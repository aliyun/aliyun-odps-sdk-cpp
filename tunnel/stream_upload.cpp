#include "util/address.h"
#include "common/http_connection.h"
#include "record_pack.h"
#include "stream_upload.h"
#include "util.h"
#include "util/timer.h"
#include "common/json_serialize.h"
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_meta_helper.h"
#endif

using namespace apsara::odps::sdk::util;

namespace apsara { namespace odps { namespace sdk { namespace internal { namespace tunnel {

const static int32_t SLOT_DISABLE_TIME = 60;
const static int32_t ADDR_DISABLE_MAX_NUM = 2;

static apsara::odps::sdk::logging::Logger* sLogger =
    apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

StreamUpload::StreamUpload(const Configuration& conf,
    const std::string& project,
    const std::string& table,
    const std::string& partition,
    bool createPartition,
    int32_t slotNum,
    const CompressOption& compress,
    const std::string& schemaName)
    : mP2PMode(false)
    , mProject(project)
    , mTable(table)
    , mPartition(ReformatPartitionSpec(partition))
    , mConf(conf)
    , mCurrentSlot(0)
    , mRecordPackWriter(new RecordPackWriter(this))
    , mCompressMode(compress)
    , mSchemaName(schemaName)
    , mSlotNum(slotNum)
    , mCreatePartition(createPartition)
    , mLastUpdate(-1)
{
    mConf.tunnelEndpoint = GetRouterServer(conf, project);
    Initiate();
}

StreamUpload::~StreamUpload()
{
    mRecordPackWriter->Clear();
}

IRecordPackPtr StreamUpload::CreateRecordPack()
{
    return CreateRecordPack(mCompressMode);
}
IRecordPackPtr StreamUpload::CreateRecordPack(const CompressOption& option)
{
    if (!CompressOptionCompatWithStreaming(option))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(option) + " is not compatible with streaming tunnel.");
    }
    // for some reason, std::is_convertible failed to detect inheritance more than one level.
    // help it.
    return std::make_shared<RecordPack>(
            mSchema,
            option,
            mRecordPackWriter);
}

IRecordPackPtr StreamUpload::CreateRecordPack(const CompressOption& option, size_t reserveSize)
{
    if (!CompressOptionCompatWithStreaming(option))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(option) + " is not compatible with streaming tunnel.");
    }

    // for some reason, std::is_convertible failed to detect inheritance more than one level.
    // help it.
    if (reserveSize < 1024)
    {
        reserveSize = 1024;
    }

    return std::make_shared<RecordPack>(
            mSchema,
            option,
            mRecordPackWriter,
            reserveSize);
}

std::string StreamUpload::RecordPackWriter::WriteRecordPack(RecordPack& pack)
{
    if (mUpload)
    {
        return mUpload->OnWriteRecordPack(pack);
    }
    else
    {
        throw OdpsTunnelException(UPLOAD_CLOSED, "Upload Is Closed.");
    }
}

void StreamUpload::RecordPackWriter::Clear()
{
    std::lock_guard<std::mutex> _lock(mLock);
    mUpload = NULL;
}

std::string StreamUpload::OnWriteRecordPack(RecordPack& pack)
{
    const Slot& slot = NextSlot();
    const CompressOption& option = pack.GetCompressOption();
    std::map<std::string, std::string> headers =
        {{std::string(CONTENT_LENGTH), std::to_string(pack.GetDataSize())}};

    std::map<std::string, std::string> params =
        {{std::string(PARAM_RECORD_COUNT), std::to_string(pack.GetRecordCount())}};

    if (pack.GetEnableOffset())
    {
        params[PARAM_ENABLE_OFFSET] = "";
    }

    HttpConnectionPtr conn = OpenWriterConnection(slot,
                                                  option,
                                                  pack.GetDataSize(),
                                                  headers,
                                                  params);
    conn->Write(pack.GetData(), pack.GetDataSize());
    conn->CloseUpstream();

    ResponsePtr resp = conn->GetResponse();

    std::ostringstream oss;
    char buff[512];
    int64_t bytes;
    while(0 != (bytes = resp->ReadBody(buff, 512))) {
        oss << std::string(buff,bytes);
    }

    std::string disableServerAddr;
    if (!resp->isSuccessful()) {
        if (resp->GetStatusCode() == 502 || resp->GetStatusCode() == 504)
        {
            try
            {
                Reload(false);
            }
            catch(...)
            {
                LOG_ERROR(sLogger,("Realod session error", ""));
            }
            disableServerAddr = slot.mServerAddr;
            DisableSlot(disableServerAddr);
        }

        LOG_ERROR(sLogger,("close record writer failed", oss.str()));
        resp->GetError(oss.str());
    }

    std::string metrics = resp->GetHeader(HEADER_ODPS_TUNNEL_METRICS);
    pack.UpdateServerMetrics(metrics);

    conn->Close();
    pack.UpdateClientIOCost(conn->GetIOCost());
    if (pack.GetEnableOffset())
    {
        try
        {
            pack.SetFileName(resp->GetHeader(HEADER_ODPS_TUNNEL_FILE_NAME));
            pack.SetFileOffset(util::StringTo<int64_t>(resp->GetHeader(HEADER_ODPS_TUNNEL_FILE_OFFSET)));
            pack.SetRecordOffset(util::StringTo<int64_t>(resp->GetHeader(HEADER_ODPS_TUNNEL_RECORD_OFFSET)));
        }
        catch(const std::exception& ignore){}
    }
    UpdateSlot(slot.mSlotId,
               resp->GetHeader(HEADER_ODPS_ROUTED_SERVER),
               resp->GetHeader(HEADER_ODPS_SLOT_NUM));

    LOG_DEBUG(sLogger,("Write record block", "success"));
    return resp->GetHeader(HEADER_ODPS_REQUEST_ID);
}

void StreamUpload::UpdateSlot(int32_t id,
    const std::string& serverAddr,
    const std::string& strSlotNum)
{
    try
    {
        uint32_t slotNum = 0;
        {
            std::lock_guard<std::mutex> _lock(mLock);
            slotNum = mSlots.size();
        }
        if (util::StringTo<uint32_t>(strSlotNum) != slotNum)
        {
            Reload(true);
        }
        else
        {
            UpdateSlot(id, serverAddr);
        }
    }
    catch (std::exception& ignore) {}
}

void StreamUpload::Initiate()
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }

    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    req->SetParameter(PARAM_UPLOADS, "");
    if (!mPartition.empty())
    {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    if (mCreatePartition)
    {
        req->SetParameter(PARAM_CREATE_PARTITION, "");
    }

    if (mSlotNum > 0)
    {
        req->SetHeader(HEADER_ODPS_SLOT_NUM, std::to_string(mSlotNum));
    }

    if (mCompressMode.algorithm != CompressOption::CompressAlgorithm::ODPS_RAW)
    {
        req->SetParameter(PARAM_STORAGE_COMPRESS_MODE, CompressOptionToEncoding(mCompressMode));
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    std::string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        StreamUploadInfo info;
        FromJsonString(info, content);
        {
            std::lock_guard<std::mutex> _lock(mLock);
            mQuotaName = info.mQuotaName;
            mSchema = info.mSchema.ToODPSTableSchema();
            mUploadId = info.mSessionName;
            mStatus = info.mStatus;
        }
        LoadSlots(info.mSlots);

    }
    else
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("create upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

void StreamUpload::Reload(bool forceReload)
{
    if (!forceReload)
    {
        std::lock_guard<std::mutex> _lock(mLock);
        time_t now;
        time(&now);
        if ((int64_t)now - mLastUpdate < 30)
        {
            return;
        }
    }
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());

    req->SetParameter(PARAM_UPLOAD_ID, mUploadId);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    std::string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        StreamUploadInfo info;
        FromJsonString(info, content);
        {
            std::lock_guard<std::mutex> _lock(mLock);
            mQuotaName = info.mQuotaName;
        }
        LoadSlots(info.mSlots);

    }
    else
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("create upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

HttpConnectionPtr StreamUpload::OpenWriterConnection(const Slot& slot,
    const CompressOption& option,
    int64_t contentLength,
    const std::map<std::string, std::string>& headers,
    const std::map<std::string, std::string>& params)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_PUT);
    if (mP2PMode)
    {
        util::Address address(mConf.tunnelEndpoint);
        req->SetEndpoint(address.GetProtocol() + "://" + slot.mServerIp);
    }
    else
    {
        req->SetEndpoint(mConf.tunnelEndpoint);
    }
    req->SetResourcePath(GetResourcePath());
    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, slot.mServerAddr);
    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (mSlotNum > 0)
    {
        req->SetHeader(HEADER_ODPS_SLOT_NUM, std::to_string(mSlotNum));
    }

    if (contentLength > 0)
    {
        req->SetContentLength(contentLength);
        req->SetHeader(CONTENT_LENGTH, std::to_string(contentLength));
    }

    req->SetHeader(CONTENT_ENCODING, CompressOptionToEncoding(option));

    for (const auto& it : headers)
    {
        req->SetHeader(it.first, it.second);
    }

    req->SetParameter(PARAM_UPLOAD_ID, mUploadId);
    req->SetParameter(PARAM_SLOT_ID, std::to_string(slot.mSlotId));

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }
    for (const auto& it : params)
    {
        req->SetParameter(it.first, it.second);
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    return conn;
}

void StreamUpload::LoadSlots(const std::map<int32_t, std::string>& slots)
{
    if (slots.empty())
    {
        throw OdpsTunnelException(EMPTY_SLOT_MAP, "Empty slot map");
    }

    std::lock_guard<std::mutex> _lock(mLock);

    std::map<std::string, int64_t> addrDisableTimeMap;
    for (const auto& slot : mSlots)
    {
        addrDisableTimeMap[slot.mServerAddr] = slot.mDisableTime;
    }

    mSlots.clear();
    for (uint32_t i = 0; i < slots.size(); ++i)
    {
        try
        {
            const auto& serverAddr = slots.at(i);
            mSlots.push_back(Slot(i, serverAddr));
        }
        catch (std::out_of_range& e)
        {
            throw OdpsTunnelException(SLOT_NOT_FOUND, "Slot not found:" + std::to_string(i));
        }
    }

    for (const auto& iter : addrDisableTimeMap)
    {
        if (util::timing::GetCurrentTimeInSeconds() - iter.second < SLOT_DISABLE_TIME)
        {
            if (CheckSlots(mSlots, iter.first))
            {
                for(size_t i = 0; i < mSlots.size(); i++)
                {
                    if (mSlots.at(i).mServerAddr == iter.first)
                    {
                        mSlots.at(i).mDisableTime = iter.second;
                    }
                }
            }
        }
    }

    mCurrentSlot = (uint32_t)(rand() % slots.size());
    time_t now;
    time(&now);
    mLastUpdate = now;
}

Slot StreamUpload::NextSlot()
{
    std::lock_guard<std::mutex> _lock(mLock);
    if (mSlots.empty())
    {
        throw OdpsTunnelException(EMPTY_SLOT_MAP, "Empty slot map");
    }

    do
    {
        ++mCurrentSlot;
        mCurrentSlot = (mCurrentSlot % mSlots.size());
    } while (mSlots.at(mCurrentSlot).mDisableTime != -1 && util::timing::GetCurrentTimeInSeconds() - mSlots.at(mCurrentSlot).mDisableTime < SLOT_DISABLE_TIME);

    return mSlots.at(mCurrentSlot);
}

void StreamUpload::UpdateSlot(int32_t id, const std::string& serverAddr)
{
    std::lock_guard<std::mutex> _lock(mLock);
    try
    {
        if (mSlots.at(id).mServerAddr != serverAddr)
        {
            mSlots.at(id) = Slot(id, serverAddr);
        }
    }
    catch(std::out_of_range& ingore) {}
}

void StreamUpload::DisableSlot(const std::string& disableServerAddr)
{
    if (!disableServerAddr.empty())
    {
        std::lock_guard<std::mutex> _lock(mLock);
        if (CheckSlots(mSlots, disableServerAddr))
        {
            int64_t disableTime = util::timing::GetCurrentTimeInSeconds();
            for(size_t i = 0; i < mSlots.size(); i++)
            {
                if (mSlots.at(i).mServerAddr == disableServerAddr)
                {
                    mSlots.at(i).mDisableTime = disableTime;
                }
            }
        }
    }
}

bool StreamUpload::CheckSlots(const SlotListType& slots, const std::string& addr)
{
    int disableSlotNum = 0;
    std::set<std::string> disableAddrSet;

    for (size_t i = 0; i < slots.size(); i++)
    {
        if (util::timing::GetCurrentTimeInSeconds() - slots.at(i).mDisableTime < SLOT_DISABLE_TIME ||  slots.at(i).mServerAddr == addr)
        {
            disableSlotNum++;
            disableAddrSet.insert(slots.at(i).mServerAddr);
        }
    }

    return slots.size() - disableSlotNum > 0 && disableAddrSet.size() <= ADDR_DISABLE_MAX_NUM;
}

#ifdef ODPS_SDK_ENABLE_ARROW

std::shared_ptr<arrow::Schema> StreamUpload::GetArrowSchema()
{
    if (!mArrowSchema)
    {
        mArrowSchema = SqlSchemaToArrowSchema(*mSchema);
    }
    return mArrowSchema;
}


void StreamUpload::OnCreateEOSClient(const std::string& clientId)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());

    req->SetParameter(PARAM_UPLOAD_ID, mUploadId);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    req->SetParameter(PARAM_TYPE, "eos_create_client");
    req->SetParameter(PARAM_CLIENT_ID, clientId);

    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, mSlots.at(0).mServerAddr);

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    std::string content;
    if (!SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("create upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

struct EOSScheduleClientResponse
{
    std::string mScheduledServer;
};

inline void to_json(nlohmann::json& j, const EOSScheduleClientResponse& r)
{
    j = nlohmann::json{{"scheduled_server", r.mScheduledServer}};
}

inline void from_json(const nlohmann::json& j, EOSScheduleClientResponse& r)
{
    r.mScheduledServer = j.at("scheduled_server").get<std::string>();
}

std::string StreamUpload::OnScheduleEOSClient(const std::string& clientId)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());

    req->SetParameter(PARAM_UPLOAD_ID, mUploadId);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    req->SetParameter(PARAM_TYPE, "eos_get_client");
    req->SetParameter(PARAM_CLIENT_ID, clientId);

    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, mSlots.at(0).mServerAddr);

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    std::string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        EOSScheduleClientResponse schedResp;
        FromJsonString(schedResp, content);
        return schedResp.mScheduledServer;
    }
    else
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("create upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

void StreamUpload::OnReleaseEOSClient(const std::string& clientId, const std::string& server)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());

    req->SetParameter(PARAM_UPLOAD_ID, mUploadId);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    req->SetParameter(PARAM_TYPE, "eos_release_client");
    req->SetParameter(PARAM_CLIENT_ID, clientId);

    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, server);

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    std::string content;
    if (!SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("create upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

std::string StreamUpload::OnWriteEOSData(
        const std::string& server,
        const std::string& client,
        const std::string& data,
        const CompressOption& option,
        const EOSInfo& offset)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_PUT);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());
    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, server);
    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (mSlotNum > 0)
    {
        req->SetHeader(HEADER_ODPS_SLOT_NUM, std::to_string(mSlotNum));
    }

    if (data.size() > 0)
    {
        req->SetContentLength(data.size());
        req->SetHeader(CONTENT_LENGTH, std::to_string(data.size()));
    }

    req->SetHeader(CONTENT_ENCODING, CompressOptionToEncoding(option));
    req->SetParameter(PARAM_UPLOAD_ID, mUploadId);
    req->SetParameter(PARAM_TYPE, "eos_upload");
    req->SetParameter(PARAM_CLIENT_ID, client);
    req->SetParameter(PARAM_SEQUENCE_ID, std::to_string(offset.mSequenceId));
    req->SetParameter(PARAM_SEQUENCE_OFFSET, std::to_string(offset.mSequenceOffset));

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    conn->Write(data.data(), data.size());
    conn->CloseUpstream();

    ResponsePtr resp = conn->GetResponse();

    std::ostringstream oss;
    char buff[512];
    int64_t bytes;
    while(0 != (bytes = resp->ReadBody(buff, 512)))
    {
        oss << std::string(buff,bytes);
    }

    if (!resp->isSuccessful())
    {
        resp->GetError(oss.str());
    }
    conn->Close();
    LOG_DEBUG(sLogger,("Write record block", "success"));
    return resp->GetHeader(HEADER_ODPS_REQUEST_ID);
}

IEOSStreamPtr StreamUpload::CreateEOSStream(const std::string& clientId, const CompressOption& opt)
{
    return std::make_shared<EOSStream>(this, clientId, opt);
}

EOSStream::EOSStream(StreamUpload* upload, const std::string& clientId, const CompressOption& opt):
    mRecordPack(opt),
    mStreamUpload(upload),
    mClientId(clientId)
{
    mStreamUpload->OnCreateEOSClient(clientId);
    TryReschedule();
}

void EOSStream::TryReschedule()
{
    const int64_t RESCHEDULE_INTERVAL_SECONDS = 15;
    int retried = 0;
    std::exception_ptr lastEx;
    while (mScheduledServer.empty() || util::timing::GetCurrentTimeInSeconds() - mLastRescheduleTimestamp > RESCHEDULE_INTERVAL_SECONDS)
    {
        if (retried > 10)
        {
            if (lastEx)
            {
                std::rethrow_exception(lastEx);
            }
            util::OdpsThrow(INTERNAL_ERROR, "Cannot properly schedule a server for client", mClientId);
        }
        if (retried > 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        try
        {
            mLastRescheduleTimestamp = util::timing::GetCurrentTimeInSeconds();
            mScheduledServer = mStreamUpload->OnScheduleEOSClient(mClientId);
        }
        catch (const std::exception& e)
        {
            lastEx = std::current_exception();
            if (std::string(e.what()).find("ServiceChanged") != std::string::npos)
            {
                mStreamUpload->Reload(true);
            }
        }
        retried++;
    }
}

FlushResult EOSStream::Write(const arrow::RecordBatch& r, const EOSInfo& offset)
{
    TryReschedule();
    mRecordPack.Reset();
    mRecordPack.Append(r);
    mRecordPack.Complete();
    FlushResult result;
    result.mFlushSize = mRecordPack.GetDataSize();
    result.mRecordCount = r.num_rows();
    result.mTraceId = mStreamUpload->OnWriteEOSData(mScheduledServer, mClientId, mRecordPack.Data(), mRecordPack.GetCompressOption(), offset);
    return result;
}

FlushResult EOSStream::WritePack(const ArrowRecordPack& r, const EOSInfo& offset)
{
    TryReschedule();
    FlushResult result;
    result.mFlushSize = r.GetDataSize();
    result.mRecordCount = r.GetRecordCount();
    result.mTraceId = mStreamUpload->OnWriteEOSData(mScheduledServer, mClientId, r.Data(), r.GetCompressOption(), offset);
    return result;
}

EOSInfo EOSStream::Tell()
{
    TryReschedule();
    try
    {
        mStreamUpload->OnWriteEOSData(mScheduledServer, mClientId, std::string{}, mRecordPack.GetCompressOption(), EOSInfo{});
        throw OdpsException("Server returned undesirable result.");
    }
    catch (const ExactlyOnceAlreadyWrittenException& ex)
    {
        return EOSInfo{ex.GetSequenceId(), ex.GetSequenceOffset()};
    }
}

void EOSStream::Release()
{
    TryReschedule();
    mStreamUpload->OnReleaseEOSClient(mClientId, mScheduledServer);
}

#endif

}}}}}
