#include "volume_upload.h"
#include "error_code.h"
#include <string>
#include "util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

VolumeUpload::VolumeUpload(const Configuration& conf,
        const std::string& project,
        const std::string& volume,
        const std::string& partition,
        const std::string& uploadId)
    :mConfig(conf),
     mProject(project),
     mVolume(volume),
     mPartition(partition),
     mResourcePath("projects/" + project + "/tunnel/uploads"),
     mUploadId(uploadId)
{
    mConfig.tunnelEndpoint = GetRouterServer(conf, project);
    Initiate();
}

IVolumeOutputStreamPtr VolumeUpload::OpenOutputStream(const std::string& fileName, const bool compress)
{
    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_POST);
    req->SetEndpoint(mConfig.tunnelEndpoint);
    req->SetResourcePath(mResourcePath + "/" + mUploadId);
    req->SetContentLength(0);
    req->SetHeader(TRANSFER_ENCODING, CHUNKED);

    if (!mConfig.defaultProject.empty())
    {
        req->SetParameter(PARAM_CURR_PROJECT, mConfig.defaultProject);
    }
    if (!mConfig.GetTunnelQuotaName().empty())
    {
        req->SetParameter(PARAM_QUOTA_NAME, mConfig.GetTunnelQuotaName());
    }
    req->SetParameter(PARAM_BLOCK_ID, fileName);

    if (compress)
    {
        req->SetHeader("Content-Encoding", "deflate");
    }

    HttpConnectionPtr conn(new HttpConnection(mConfig));
    conn->SetRequest(req);
    conn->Open();
    return IVolumeOutputStreamPtr(new HttpVolumeOutputStream(conn, compress));
}

void VolumeUpload::Commit(const std::vector<std::string>& clientFileList)
{
    Initiate();

    std::set<std::string> serverFileList;
    for (typeof(mFileList.begin()) iter = mFileList.begin();
        iter != mFileList.end(); ++iter)
    {
        serverFileList.insert(iter->mFileName);
    }

    if(serverFileList.size() != clientFileList.size()){
        throw OdpsTunnelException(INTERNAL_ERROR,"Blocks not match, server:"
              + std::to_string(serverFileList.size()) + "client" + std::to_string(clientFileList.size()));
    }

    for (typeof(clientFileList.begin()) iter = clientFileList.begin();
        iter != clientFileList.end(); ++iter)
    {
        if(serverFileList.find(*iter) == serverFileList.end())
        {
            throw OdpsTunnelException(INTERNAL_ERROR,
                  "Block not exits on server, block id is: " + (*iter));
        }
    }

    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_PUT);
    req->SetEndpoint(mConfig.tunnelEndpoint);
    req->SetResourcePath(mResourcePath + "/" + mUploadId);
    req->SetContentLength(0);
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
    if (resp->isSuccessful())
    {
        FromJson(content);
    }
    else
    {
        const std::string& errorMsg = content.empty()?conn->TraceError():content;
        resp->GetError(errorMsg);
    }
}

void VolumeUpload::Initiate()
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

    if (mUploadId.empty())
    {
        req->SetResourcePath(mResourcePath);
        req->SetMethod(HTTP_METHOD_POST);

        req->SetParameter("type", "volumefile");
        string target = mProject + "/" + mVolume + "/" + mPartition + "/";
        req->SetParameter("target", target);
    }
    else
    {
        req->SetResourcePath(mResourcePath + "/" + mUploadId);
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
        resp->GetError(errorMsg);
    }

}

void VolumeUpload::FromJson(const string& jsonContent)
{
    nlohmann::json result = nlohmann::json::parse(jsonContent);

    nlohmann::json::iterator iter = result.find("UploadID");
    if (iter != result.end())
    {
        mUploadId = iter->get<std::string>();
    }

    iter = result.find("Status");
    if (iter != result.end())
    {
        mStatus = iter->get<std::string>();
    }

    iter = result.find("QuotaName");
    if (iter != result.end())
    {
        mQuotaName = iter->get<std::string>();
    }

    iter = result.find("FileList");
    if (iter != result.end())
    {
        iter->get_to(mFileList);
    }
}
