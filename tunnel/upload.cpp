#include "common/logging.h"

#include <vector>
#include <map>
#include "connection_manager.h"
#include "odps_meta.h"
#include "common/http_flags.h"
#include "common/http_connection.h"
#include "upload.h"
#include "record_writer.h"
#include "util.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_writer.h"
#include "arrow_meta_helper.h"
#endif

using namespace std;
using namespace apsara::odps::sdk::logging;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;
namespace apsara { namespace odps { namespace sdk { namespace internal { namespace tunnel {

static apsara::odps::sdk::logging::Logger* sLogger = apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

#ifdef ODPS_SDK_ENABLE_ARROW
std::shared_ptr<arrow::Schema> Upload::GetArrowSchema()
{
    if (!mArrowSchema)
    {
        mArrowSchema = SqlSchemaToArrowSchema(*schema);
    }
    return mArrowSchema;
}

IArrowRecordWriterPtr Upload::OpenArrowWriter(const uint32_t blockId, const CompressOption& compress)
{
    if (!CompressOptionCompatWithArrow(compress))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(compress) + " is not compatible with arrow.");
    }
    auto&& requestPtr = BuildRequest(map<string, string>{{PARAM_ARROW, ""}}, blockId, compress);
    ConnectionManagerPtr connManager = std::make_shared<ConnectionManager>(requestPtr, conf);
    return IArrowRecordWriterPtr(new ArrowRecordWriter(connManager, compress));
}
#endif

IRecordWriterPtr Upload::OpenWriter(const uint32_t blockId, const bool compress)
{
    CompressOption option = compress ? conf.GetCompressOption() : CompressOption::NO_COMPRESS;
    return OpenWriter(blockId, option);
}

IRecordWriterPtr Upload::OpenWriter(const uint32_t blockId, const CompressOption& _option)
{
    CompressOption option = _option;
    if (!CompressOptionCompatWithNormal(option))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(option) + " is not compatible with normal upload.");
    }
    auto&& requestPtr = BuildRequest(map<string, string>{}, blockId, option);
    ConnectionManagerPtr connManager = std::make_shared<ConnectionManager>(requestPtr, conf);
    return IRecordWriterPtr(new RecordWriter(schema, connManager, option.algorithm != CompressOption::ODPS_RAW, &option));
}

RequestPtr Upload::BuildRequest(
        const map<string, string>& requestArgs,
        uint64_t blockId, const CompressOption& compress)
{
    string path = getResourcePath();
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_PUT);
    req->SetContentLength(0);
    req->SetEndpoint(conf.tunnelEndpoint);
    req->SetResourcePath(path);

    if (compress.algorithm != CompressOption::ODPS_RAW)
    {
        req->SetHeader("Content-Encoding", CompressOptionToEncoding(compress));
    }

    if (!conf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, conf.defaultProject);
    }

    req->SetParameter(PARAM_UPLOAD_ID, uploadId);
    req->SetParameter(PARAM_BLOCK_ID, std::to_string(blockId));

    if (partition.length() > 0) {
        req->SetParameter(PARAM_PARTITION, partition);
    }

    if (!conf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, conf.GetTunnelQuotaName());
    }

    for(auto& eachArg: requestArgs)
    {
        req->SetParameter(eachArg.first, eachArg.second);
    }
    return req;
}

void Upload::Initiate()
{
    string path = getResourcePath();
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetContentLength(0);
    req->SetEndpoint(conf.tunnelEndpoint);
    req->SetResourcePath(path);

    if (mOverWriteMode)
    {
        req->SetParameter(PARAM_OVERWRITE_MODE, "true");
    }

    if (!conf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, conf.defaultProject);
    }

    if (!conf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, conf.GetTunnelQuotaName());
    }

    req->SetParameter(PARAM_UPLOADS, "");
    if (partition.length() > 0)
    {
        req->SetParameter(PARAM_PARTITION, partition);
    }

    HttpConnectionPtr conn(new HttpConnection(conf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        ParseCreateResult(content);
    }
    else
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("create upload handler failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, conf.tunnelEndpoint);
    }
}

void Upload::Reload(const std::string& getblockid)
{
    string path = getResourcePath();
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(conf.tunnelEndpoint);
    req->SetResourcePath(path);

    if (!conf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, conf.defaultProject);
    }

    req->SetParameter(PARAM_UPLOAD_ID, uploadId);
    if (partition.length() > 0) {
        req->SetParameter(PARAM_PARTITION, partition);
    }
    req->SetParameter(PARAM_GET_BLOCK_ID, getblockid);

    HttpConnectionPtr conn(new HttpConnection(conf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        ParseQueryResult(content);
    }
    else
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("reload upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, conf.tunnelEndpoint);
    }
}

std::vector<int64_t> Upload::GetBlockList()
{
    Reload();
    std::vector<int64_t> ret;
    for(typeof(blockList.begin()) iter = blockList.begin(); iter != blockList.end(); ++iter)
    {
        ret.push_back((*iter).mBlockID);
    }
    return ret;
}

void Upload::CommitUpload()
{
    string path = getResourcePath();

    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetEndpoint(conf.tunnelEndpoint);
    req->SetResourcePath(path);
    req->SetContentLength(0);

    if (!conf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, conf.defaultProject);
    }
    req->SetParameter(PARAM_UPLOAD_ID, uploadId);

    if (partition.length() > 0) {
        req->SetParameter(PARAM_PARTITION, partition);
    }

    if (!conf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, conf.GetTunnelQuotaName());
    }

    HttpConnectionPtr conn(new HttpConnection(conf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        ParseCloseResult(content);
        LOG_DEBUG(sLogger,("Complete response",content)
            ("StatusCode",resp->GetStatusCode()));
    }
    else
    {
        resp->ReadBody(content);
        std::string errorMsg = "Resource:" + req->GetResourcePath();
        errorMsg.append(",StatusCode:").append(std::to_string(resp->GetStatusCode()));
        errorMsg.append(",ErrorMessage:").append(content.empty() ? conn->TraceError() : content);
        LOG_ERROR(sLogger, ("close upload hander failed", errorMsg));
        resp->GetError(content.empty()?conn->TraceError():content, conf.tunnelEndpoint);
    }
}

void Upload::Commit()
{
    CommitUpload();
}

void Upload::Commit(const std::vector<uint32_t>& blocks)
{
    Reload();
    std::map<uint32_t, bool> serverMap;
    std::map<uint32_t, bool> clientMap;
    for(typeof(blocks.begin()) iter = blocks.begin(); iter != blocks.end(); ++iter){
        clientMap[*iter] = true;
    }
    for(typeof(blockList.begin()) iter = blockList.begin(); iter != blockList.end(); ++iter){
        serverMap[(*iter).mBlockID] = true;
    }
    if(serverMap.size() != clientMap.size()){
        throw OdpsTunnelException(INTERNAL_ERROR,"Blocks not match, server:"
              + std::to_string(serverMap.size()) + "client" + std::to_string(clientMap.size()));
    }
    for(typeof(blocks.begin()) iter = blocks.begin(); iter != blocks.end(); ++iter){
        if(serverMap.find(*iter) == serverMap.end()){
            throw OdpsTunnelException(INTERNAL_ERROR,
                  "Block not exits on server, block id is: " + std::to_string(*iter));
        }
    }

    CommitUpload();
}

void Upload::ParseCreateResult(const string& json)
{
    LOG_INFO(sLogger,("fromCreateJson",json));
    CreateUploadResult result;

    FromJsonString(result,json);

    uploadId = result.mUploadId;
    status = result.mStatus;
    schema = result.mSchema.ToODPSTableSchema();
    mQuotaName = result.mQuotaName;

    if (result.mMaxFieldSize > 0)
    {
        schema->SetMaxFieldSize(result.mMaxFieldSize);
    }

    if (mOverWriteMode && !result.mIsOverwrite)
    {
        throw OdpsTunnelException(INTERNAL_ERROR,
              "Overwrite is not supported in tunnel server");
    }

    LOG_DEBUG(sLogger,("CreateUploadResult",ToJsonString(result)));
}

void Upload::ParseQueryResult(const string& json)
{
    LOG_INFO(sLogger,("fromCreateJson",json));
    QueryUploadResult result;

    FromJsonString(result,json);

    uploadId = result.mUploadId;
    status = result.mStatus;

    if (!schema)
    {
        schema = result.mSchema.ToODPSTableSchema();
    }
    blockList = result.blockList;
    LOG_DEBUG(sLogger,("QueryUploadResult",ToJsonString(result)));
}

void Upload::ParseCloseResult(const string& json)
{
    LOG_DEBUG(sLogger,("fromCreateJson",json));
    CloseUploadResult result;

    FromJsonString(result,json);
    blockList = result.blockList;
    LOG_DEBUG(sLogger,("CloseUploadResult",ToJsonString(result)));
}

}}}}}
