#include "volume_download.h"

#include <iostream>
#include <string>

#include "common/http_connection.h"
#include "common/http_flags.h"
#include "serialize.h"
#include "common/logging.h"
#include "util.h"

using namespace std;
using namespace apsara::odps::sdk::logging;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;

namespace apsara { namespace odps{ namespace sdk { namespace internal { namespace tunnel {

static apsara::odps::sdk::logging::Logger* sLogger = apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal/volume_download");

VolumeDownload::VolumeDownload(const Configuration& conf,
        const std::string& project,
        const std::string& volume,
        const std::string& partition,
        const std::string& fileName,
        const std::string& downloadId
        )
   :mConfig(conf),
    mProject(project),
    mVolume(volume),
    mPartition(partition),
    mFileName(fileName),
    mResourcePath("projects/" + project + "/tunnel/downloads"),
    mDownloadId(downloadId)
{
    mConfig.tunnelEndpoint = GetRouterServer(conf, project);
    Initiate();
}

void VolumeDownload::Initiate(void)
{
    RequestPtr req(new Request());
    req->SetContentLength(0);
    req->SetEndpoint(mConfig.tunnelEndpoint);

    if (!mConfig.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConfig.defaultProject);
    }

    if (!mConfig.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConfig.GetTunnelQuotaName());
    }

    if (mDownloadId.empty())
    {
        req->SetResourcePath(mResourcePath);
        req->SetMethod(HTTP_METHOD_POST);

        req->SetParameter("type", "volumefile");
        string target = mProject + "/" + mVolume + "/" + mPartition + "/" + mFileName;
        req->SetParameter("target", target);
    }
    else
    {
        req->SetResourcePath(mResourcePath + "/" + mDownloadId);
        req->SetMethod(HTTP_METHOD_GET);
    }

    HttpConnectionPtr conn(new HttpConnection(mConfig));

    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    resp->ReadBody(content);

    if(resp->isSuccessful())
    {
        FromJson(content);
    }
    else
    {
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        LOG_ERROR(sLogger,("StatusCode",resp->GetStatusCode()));
        LOG_ERROR(sLogger,("create volume download handler failed",errorMsg));
        resp->GetError(errorMsg);
    }
}

IVolumeInputStreamPtr VolumeDownload::OpenInputStream(const uint64_t start, const uint64_t length, bool compress)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(mConfig.tunnelEndpoint);
    req->SetResourcePath(mResourcePath + "/" + mDownloadId);

    if (compress)
    {
        req->SetHeader("Accept-Encoding", "deflate");
    }

    if (!mConfig.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConfig.defaultProject);
    }
    if (!mConfig.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConfig.GetTunnelQuotaName());
    }
    req->SetParameter("data", "");

    req->SetParameter("range", "(" + std::to_string(start) + "," + std::to_string(length) + ")");
    VolumeDeserializerPtr serializer;
    HttpConnectionPtr conn(new HttpConnection(mConfig));
    conn->SetRequest(req);
    conn->Open();

    ResponsePtr resp = conn->GetResponse();
    if (resp->isSuccessful())
    {
        return IVolumeInputStreamPtr(new HttpVolumeInputStream(conn, compress));
    }
    else
    {
        string content;
        resp->ReadBody(content);
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        LOG_ERROR(sLogger,("StatusCode",resp->GetStatusCode()));
        LOG_ERROR(sLogger,("create volume output stream failed",errorMsg));
        resp->GetError(errorMsg);
    }
}

void VolumeDownload::Complete(void)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_PUT);
    req->SetEndpoint(mConfig.tunnelEndpoint);
    req->SetResourcePath(mResourcePath + "/" + mDownloadId);
    req->SetContentLength(0);

    if (!mConfig.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConfig.defaultProject);
    }
    if (!mConfig.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConfig.GetTunnelQuotaName());
    }

    HttpConnectionPtr conn(new HttpConnection(mConfig));
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();

    string content;
    resp->ReadBody(content);
    if (!resp->isSuccessful())
    {
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        LOG_ERROR(sLogger,("StatusCode",resp->GetStatusCode()));
        LOG_ERROR(sLogger,("complete volume download handler failed",errorMsg));
        resp->GetError(errorMsg);
    }
}

void VolumeDownload::FromJson(const string& jsonContent)
{
    VolumeDownloadResult result;
    FromJsonString(result, jsonContent);
    mDownloadId = result.mDownloadId;
    mStatus = result.mStatus;
    mFileLength = result.GetFileLength();
    mQuotaName = result.mQuotaName;
}

}}}}}