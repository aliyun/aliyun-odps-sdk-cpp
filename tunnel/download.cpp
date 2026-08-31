#include "common/logging.h"
#include "util/string_util.h"

#include "odps_meta.h"
#include "common/http_connection.h"
#include "download.h"
#include "record_reader.h"
#include "connection_manager.h"
#include <typeinfo>
#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_reader.h"
#include "arrow_meta_helper.h"
#endif

using namespace std;
using namespace apsara::odps::sdk::util;

static apsara::odps::sdk::logging::Logger* sLogger = apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

namespace apsara { namespace odps { namespace sdk { namespace internal { namespace tunnel {

RequestPtr Download::BuildRequest(
        const map<string, string>& requestArgs,
        uint64_t start,
        uint64_t count,
        const std::vector<std::string>& colNames,
        const CompressOption& compress,
        ODPSTableSchemaPtr& readingSchema,
        bool disableModifiedCheck,
        uint64_t rawSize)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(mResourcePath);

    if (compress.algorithm != CompressOption::ODPS_RAW)
    {
        req->SetHeader("Accept-Encoding", CompressOptionToEncoding(compress));
    }

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }

    if (disableModifiedCheck)
    {
        req->SetParameter(PARAM_DISABLE_MODIFIED_CHECK, "true");
    }

    req->SetParameter(PARAM_DOWNLOAD_ID, mDownloadId);
    req->SetParameter("data", "");
    req->SetParameter(PARAM_ROWRANGE, "(" + std::to_string(start) + "," + std::to_string(count) + ")");

    if (rawSize > 0)
    {
        req->SetParameter(PARAM_RAW_SIZE, std::to_string(rawSize));
    }

    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    if (!colNames.empty())
    {
        std::map<std::string, uint32_t> validColumns;
        for(uint32_t i = 0; i < mSchema->GetColumnCount(); i++)
        {
            validColumns[util::ToLowerCaseString(mSchema->GetTableColumn(i).GetName())] = i;
        }
        std::string colSpec;
        ODPSTableSchemaPtr sqlSchema(new ODPSTableSchema());
        for (size_t i = 0; i < colNames.size(); i++)
        {
            std::string colName = util::TrimString(util::ToLowerCaseString(colNames[i]));
            if (colName == "")
            {
                util::TunnelThrow(INVALID_ARGUMENT, "Column name can't be empty. at position: " , i);
            }

            auto it = validColumns.find(colName);

            if (it == validColumns.end())
            {
                util::TunnelThrow(INVALID_ARGUMENT, "Column name not exist in schema:", colName);
            }

            sqlSchema->AppendColumn(mSchema->GetTableColumn(it->second));
            colSpec.append(colName);
            if (i != colNames.size() - 1)
                colSpec.append(",");
        }
        readingSchema = sqlSchema;
        req->SetParameter(PARAM_COLUMNS, colSpec);
    }
    else
    {
        readingSchema = mSchema;
    }

    if (!mPartition.empty())
    {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    for(auto& eachArg: requestArgs)
    {
        req->SetParameter(eachArg.first, eachArg.second);
    }
    return req;
}

#ifdef ODPS_SDK_ENABLE_ARROW
std::shared_ptr<arrow::Schema> Download::GetArrowSchema()
{
    if (!mArrowSchema)
    {
        mArrowSchema = SqlSchemaToArrowSchema(*mSchema);
    }
    return mArrowSchema;
}
IArrowRecordReaderPtr Download::OpenArrowReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck, const uint64_t rawSize)
{
    ODPSTableSchemaPtr readingSchema;
    if (!CompressOptionCompatWithArrow(compress))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(compress) + " is not compatible with arrow.");
    }
    auto&& requestPtr = BuildRequest(map<string, string> {{PARAM_ARROW, ""}}, start, count, colNames, compress, readingSchema, disableModifiedCheck, rawSize);
    ConnectionManagerPtr connManager = std::make_shared<ConnectionManager>(requestPtr, mConf);
    return IArrowRecordReaderPtr(new ArrowRecordReader(SqlSchemaToArrowSchema(*readingSchema), connManager, compress));
}

IBufferArrowRecordReaderPtr Download::OpenBufferArrowReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck)
{
    return OpenBufferArrowReader(start, count, bufferRecordCount, 0, colNames, compress, disableModifiedCheck);
}

IBufferArrowRecordReaderPtr Download::OpenBufferArrowReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const uint64_t rawSize, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck)
{
    DownloadInfo downloadInfo = {mConf, mProject, mTable, mPartition, mDownloadId, mSchemaName};
    return IBufferArrowRecordReaderPtr(new BufferArrowRecordReader(start, count, bufferRecordCount, rawSize, colNames, compress, downloadInfo, disableModifiedCheck));
}
#endif

IRecordReaderPtr Download::OpenReader(const uint64_t start, const uint64_t count, const bool compress)
{
    std::vector<std::string> colNames;
    return OpenReader(start, count, colNames, compress);
}

IRecordReaderPtr Download::OpenReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const bool compress)
{
    CompressOption option = compress ? mConf.GetCompressOption() : CompressOption::NO_COMPRESS;
    return OpenReader(start, count, colNames, option, false);
}

IRecordReaderPtr Download::OpenReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const CompressOption& _option, bool disableModifiedCheck)
{
    ODPSTableSchemaPtr readingSchema;
    CompressOption option = _option;
    if (!CompressOptionCompatWithNormal(option))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(option) + " is not compatible with normal download.");
    }
    auto&& requestPtr = BuildRequest(map<string, string>{}, start, count, colNames, option, readingSchema, disableModifiedCheck);
    ConnectionManagerPtr connManager = std::make_shared<ConnectionManager>(requestPtr, mConf);
    return IRecordReaderPtr(new RecordReader(readingSchema, connManager, option.algorithm != CompressOption::ODPS_RAW, &option));
}

IBufferRecordReaderPtr Download::OpenBufferReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck)
{
    DownloadInfo downloadInfo = {mConf, mProject, mTable, mPartition, mDownloadId, mSchemaName};
    return IBufferRecordReaderPtr(new BufferRecordReader(start, count, bufferRecordCount, colNames, compress, downloadInfo, disableModifiedCheck));
}

void Download::Initiate()
{
    RequestPtr req(new Request());
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(mResourcePath);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }

    req->SetParameter(PARAM_ASYNC_MODE, "true");
    req->SetMethod(HTTP_METHOD_POST);
    req->SetParameter(PARAM_DOWNLOADS, "");

    if (!mPartition.empty())
    {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    if (!mConf.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConf.GetTunnelQuotaName());
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        FromJson(content);
    }
    else
    {
        resp->ReadBody(content);
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        LOG_ERROR(sLogger,("StatusCode", resp->GetStatusCode()));
        LOG_ERROR(sLogger,("create download handler failed", errorMsg));
        resp->GetError(errorMsg, mConf.tunnelEndpoint);
    }

    // status code:
    // 0 - UNKNOWN
    // 1 - INITIATED
    // 2 - CLOSED
    // 3 - EXPIRED
    // 4 - INITIATING
    while (mStatus == "initiating")
    {
        RandomSleep(5 * 1000, 30 * 1000);
        Reload();
    }
}

void Download::Reload()
{
    RequestPtr req(new Request());
    req->SetContentLength(0);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(mResourcePath);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }

    req->SetMethod(HTTP_METHOD_GET);
    req->SetParameter(PARAM_DOWNLOAD_ID, mDownloadId);

    if (mPartition.length() > 0)
    {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    if (SuccessWithRetry(conn, resp))
    {
        resp->ReadBody(content);
        FromJson(content);
    }
    else
    {
        resp->ReadBody(content);
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        LOG_ERROR(sLogger,("StatusCode", resp->GetStatusCode()));
        LOG_ERROR(sLogger,("create download handler failed", errorMsg));
        resp->GetError(errorMsg, mConf.tunnelEndpoint);
    }
}

void Download::Complete()
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetEndpoint(mConf.tunnelEndpoint);
    req->SetResourcePath(mResourcePath);
    req->SetContentLength(0);

    if (!mConf.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConf.defaultProject);
    }
    req->SetParameter(PARAM_DOWNLOAD_ID, mDownloadId);

    if (mPartition.length() > 0)
    {
        req->SetParameter(PARAM_PARTITION, mPartition);
    }

    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    resp->ReadBody(content);
    if (!resp->isSuccessful())
    {
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        LOG_ERROR(sLogger,("StatusCode",resp->GetStatusCode()));
        LOG_ERROR(sLogger,("complete download handler failed",errorMsg));
        resp->GetError(errorMsg, mConf.tunnelEndpoint);
    }
    LOG_DEBUG(sLogger,("Complete response",content)
            ("StatusCode",resp->GetStatusCode()));
}

void Download::FromJson(const string& json)
{
    LOG_DEBUG(sLogger,("fromCreateJson",json));
    CreateDownloadResult result;

    FromJsonString(result, json);

    mDownloadId = result.mDownloadId;
    mStatus = result.mStatus;
    mRecordCount = result.mRecordCount;
    mQuotaName = result.mQuotaName;

    if (!mSchema)
    {
        mSchema = result.mSchema.ToODPSTableSchema();
    }

    LOG_DEBUG(sLogger,("CreateDownloadResult",ToJsonString(result)));
}


}}}}}