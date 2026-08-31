#include <string>
#include "util/string_util.h"
#include "common/http_connection.h"
#include "tunnel/upsert_stream.h"
#include "tunnel/upsert.h"
#include "util.h"
#include "util/timer.h"
#include "common/http_connection.h"
#include "record_pack.h"
#include "tunnel/odps_meta.h"

using namespace apsara::odps::sdk::util;

namespace apsara { namespace odps { namespace sdk { namespace internal { namespace tunnel {

static apsara::odps::sdk::logging::Logger* sLogger =
    apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

UpsertSession::UpsertSession(const Configuration& conf,
    const std::string& project,
    const std::string& table,
    const std::string& partition,
    const std::string& upsertId,
     const std::string& schemaName)
    : mUpsertId(upsertId)
    , mProject(project)
    , mTable(table)
    , mPartition(ReformatPartitionSpec(partition))
    , mConf(conf)
    , mUpsertRecordPackWriter(new UpsertRecordPackWriter(this))
    , mSchemaName(schemaName)
    , mCommitTimeout(120 * 1000)
{
    mConf.tunnelEndpoint = GetRouterServer(conf, project);
    if("" == upsertId)
    {
        Initiate();
    }
    else
    {
        Reload();
    }
}

UpsertSession::~UpsertSession()
{
    mUpsertRecordPackWriter->Clear();
}

void UpsertSession::Initiate()
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
    if (!mPartition.empty())
    {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }
    req->SetParameter(PARAM_SLOT_NUM, std::to_string(mSlotNum));

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();
    std::string content;
    resp->ReadBody(content);
    if (resp->isSuccessful())
    {
        ParseUpsertResult(content);
    }
    else
    {
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("Initiate upsert handler failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}


void UpsertSession::Reload()
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());

    req->SetParameter(PARAM_UPSERT_ID, mUpsertId);
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
    ResponsePtr resp = conn->GetResponse();
    std::string content;
    resp->ReadBody(content);
    if (resp->isSuccessful())
    {
        ParseUpsertResult(content);
    }
    else
    {
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("Reload upsert handler failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

void UpsertSession::ParseUpsertResult(const std::string& content)
{
    UpsertInfo info;
    FromJsonString(info, content);
    mSchema = info.mSchema.ToODPSTableSchema();
    for(auto field : info.mSchema.fields)
    {
        mName2Id[field.GetName()] = field.GetColumnId();
    }
    mRecordSchema = info.mSchema;
    mHashKeys = info.mHashKeys;
    mSupportPartialUpadte = info.mSupportPartialUpadte;
    mRecordSchema.addField(ODPSTunnelTableColumn("__version", "BIGINT"));
    mRecordSchema.addField(ODPSTunnelTableColumn("__app_version", "BIGINT"));
    mRecordSchema.addField(ODPSTunnelTableColumn("__operation", "TINYINT"));
    ODPSColumnTypeInfo columnTypeInfo(ODPSColumnType::ODPS_ARRAY);
    columnTypeInfo.mSubTypes.push_back(ODPSColumnTypeInfo(ODPSColumnType::ODPS_BIGINT));
    mRecordSchema.addField(ODPSTunnelTableColumn("__key_cols", columnTypeInfo));
    mRecordSchema.addField(ODPSTunnelTableColumn("__value_cols", columnTypeInfo));
    mUpsertId = info.mUpsertId;
    mStatus = info.mStatus;
    LoadSlots(info.mSlots);
}

void UpsertSession::Commit(bool async)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());
    req->SetContentLength(0);

    req->SetParameter(PARAM_UPSERT_ID, mUpsertId);
    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, mBuckets.at(0).mServerAddr);
    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }

    if (!mPartition.empty()) {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }
    CommitRequest(req);
    if(!async)
    {
        int i = 1;
        int64_t strart = util::timing::GetCurrentTimeInMilliSeconds();

        while (util::ToLowerCaseString(mStatus) == "committing" ||
            util::ToLowerCaseString(mStatus) == "normal")
        {
            if (util::timing::GetCurrentTimeInMilliSeconds() - strart > mCommitTimeout) {
                throw OdpsTunnelException(REQUEST_TIMEOUT, "Commit session timeout");
            }
            usleep(i * 1000);
            CommitRequest(req);
            if (i < 16)
            {
                i = i * 2;
            }
        }
        if (util::ToLowerCaseString(mStatus) != "committed")
        {
            throw OdpsTunnelException(STATUS_ERROR, "Commit session failed, status:" + GetStatus());
        }
    }
}

void UpsertSession::Abort()
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_DELETE);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());
    req->SetContentLength(0);
    req->SetParameter(PARAM_UPSERT_ID, mUpsertId);
    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, mBuckets.at(0).mServerAddr);

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
    ResponsePtr resp = conn->GetResponse();
    std::string content;
    resp->ReadBody(content);
    if (!resp->isSuccessful())
    {
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("Abort upsert handler failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

void UpsertSession::LoadSlots(const std::vector<UpsertSlot>& slots)
{
    if (slots.empty())
    {
        throw OdpsTunnelException(EMPTY_SLOT_MAP, "Empty slot map");
    }
    std::lock_guard<std::mutex> _lock(mLock);
    mBuckets.clear();
    for (uint32_t i = 0; i < slots.size(); ++i)
    {
        try
        {
            const UpsertSlot& upsertSlot = slots.at(i);

            std::vector<int32_t> buckets = upsertSlot.mBuckets;
            for(uint32_t i = 0; i < buckets.size(); ++i)
            {
                mBuckets.insert(std::make_pair(buckets[i], Slot(upsertSlot.mSlotId, upsertSlot.mWorkerAddr)));
            }
            for (const auto& bucket : mBuckets)
            {
                if (static_cast<uint32_t>(bucket.first) < 0 || static_cast<uint32_t>(bucket.first) >= mBuckets.size()) {
                    throw OdpsTunnelException(INVALID_BUCKET_VALUE, "Invalid bucket value:" + std::to_string(bucket.first));
                }
            }
        }
        catch (std::out_of_range& e)
        {
            throw OdpsTunnelException(SLOT_NOT_FOUND, "Slot not found:" + std::to_string(i));
        }
    }
}

void UpsertSession::CommitRequest(const RequestPtr& req)
{
    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    std::string content;
    resp->ReadBody(content);
    if (resp->isSuccessful())
    {
        UpsertInfo info;
        FromJsonString(info, content);
        LoadSlots(info.mSlots);
        mStatus = info.mStatus;
    }
    else
    {
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("Commit upsert handler failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, mConf.tunnelEndpoint);
    }
}

ODPSTableRecordPtr UpsertSession::CreateUpsertRecord()
{
    return ODPSTableRecordPtr(new UpsertRecord(mRecordSchema.ToODPSTableSchema()));
}

IUpsertStreamPtr UpsertSession::CreateUpsertStream(const CompressOption& option)
{
    if (!CompressOptionCompatWithUpsert(option))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(option) + " is not compatible with upsert tunnel.");
    }
    return std::make_shared<UpsertStream>(option, this);
}

std::string UpsertSession::UpsertRecordPackWriter::WriteRecordPack(RecordPack& pack)
{
    if (mUpsert)
    {
        return mUpsert->OnWriteRecordPack(pack);
    }
    else
    {
        throw OdpsTunnelException(UPLOAD_CLOSED, "Upsert Is Closed.");
    }
}

void UpsertSession::UpsertRecordPackWriter::Clear()
{
    std::lock_guard<std::mutex> _lock(mLock);
    mUpsert = NULL;
}

std::string UpsertSession::OnWriteRecordPack(RecordPack& pack)
{
    int32_t bucketId = pack.GetBucketId();
    const Slot& slot = GetSlot(bucketId);
    const CompressOption& option = pack.GetCompressOption();
    std::map<std::string, std::string> headers =
        {{std::string(CONTENT_LENGTH), std::to_string(pack.GetDataSize())}};

    std::map<std::string, std::string> params =
    {{std::string(PARAM_RECORD_COUNT), std::to_string(pack.GetRecordCount())}};

    HttpConnectionPtr conn = OpenWriterConnection(slot, bucketId, option, pack.GetDataSize(), headers, params);
    conn->Write(pack.GetData(), pack.GetDataSize());
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
        if (resp->GetStatusCode() == 502 || resp->GetStatusCode() == 504)
        {
            try
            {
                Reload();
            }
            catch(...)
            {
                LOG_ERROR(sLogger,("Realod session error", ""));
            }
        }
        LOG_ERROR(sLogger,("close record writer failed", oss.str()));
        resp->GetError(oss.str());
    }
    conn->Close();

    try
    {
        UpdateSlot(slot.mSlotId, resp->GetHeader(HEADER_ODPS_ROUTED_SERVER));
    }
    catch (std::exception& ignore) {}

    return resp->GetHeader(HEADER_ODPS_REQUEST_ID);
}

HttpConnectionPtr UpsertSession::OpenWriterConnection(const Slot& slot,
    int32_t bucketNum,
    const CompressOption& option,
    int64_t contentLength,
    const std::map<std::string, std::string>& headers,
    const std::map<std::string, std::string>& params)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_PUT);
    req->SetEndpoint(GetConf().tunnelEndpoint);
    req->SetResourcePath(GetResourcePath());
    req->SetHeader(HEADER_ODPS_ROUTED_SERVER, slot.mServerAddr);

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

    req->SetParameter(PARAM_BUCKET_ID, std::to_string(bucketNum));
    req->SetParameter(PARAM_UPSERT_ID, GetUpsertId());
    req->SetParameter(PARAM_SLOT_ID, std::to_string(slot.mSlotId));

    if (!GetConf().defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, GetConf().defaultProject);
    }
    if (!GetPartition().empty()) {
        req->SetParameter(PARAM_PARTITION, GetPartition());
    }
    for (const auto& it : params)
    {
        req->SetParameter(it.first, it.second);
    }

    HttpConnectionPtr conn(new HttpConnection(GetConf()));
    conn->SetRequest(req);
    conn->Open();
    return conn;
}

Slot UpsertSession::GetSlot(int32_t bucketNum)
{
    std::lock_guard<std::mutex> _lock(mLock);
    return mBuckets.at(bucketNum);
}

void UpsertSession::UpdateSlot(int32_t id, const std::string& serverAddr)
{
    std::lock_guard<std::mutex> _lock(mLock);
    try
    {
        mBuckets.at(id) = Slot(id, serverAddr);
    }
    catch(std::out_of_range& ingore) {}
}

}}}}}