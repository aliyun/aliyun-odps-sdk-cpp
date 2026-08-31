#include "volume_write_session.h"
#include "volume_write_stream.h"
#include "common/http_connection.h"
#include "common/http_flags.h"
#include "commons.h"
#include "include/odps_exception.h"
#include "tunnel/util.h"
#include "tunnel/volume_reader_writer.h"
#include "util/utils.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

class VolumeCreateWriteSessionRequest
{
public:
    VolumeCreateWriteSessionRequest() {}

    void SetFile(const std::string& file) { mFile = file; }

    friend void to_json(nlohmann::json& j, const VolumeCreateWriteSessionRequest& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateWriteSessionRequest& r);

private:
    std::string mFile;
};

inline void to_json(nlohmann::json& j, const VolumeCreateWriteSessionRequest& r)
{
    j = nlohmann::json{{REQUEST_FILE, r.mFile}};
}

inline void from_json(const nlohmann::json& j, VolumeCreateWriteSessionRequest& r)
{
    r.mFile = j.at(REQUEST_FILE).get<std::string>();
}

class VolumeCreateWriteSessionResponse
{
public:
    VolumeCreateWriteSessionResponse() {}

    std::string GetSessionId() { return mSessionId; }
    std::string GetStatus() { return mStatus; }
    std::string GetQuotaName() { return mQuotaName; }

    friend void to_json(nlohmann::json& j, const VolumeCreateWriteSessionResponse& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateWriteSessionResponse& r);

private:
    std::string mSessionId;
    std::string mStatus;
    std::string mQuotaName;
};

inline void to_json(nlohmann::json& j, const VolumeCreateWriteSessionResponse& r)
{
    j = nlohmann::json{
        {REQUEST_UPLOAD_ID, r.mSessionId},
        {REQUEST_STATUS, r.mStatus},
        {REQUEST_QUOTA_NAME, r.mQuotaName},
    };
}

inline void from_json(const nlohmann::json& j, VolumeCreateWriteSessionResponse& r)
{
    r.mSessionId = j.at(REQUEST_UPLOAD_ID).get<std::string>();
    r.mStatus = j.value(REQUEST_STATUS, "");
    r.mQuotaName = j.value(REQUEST_QUOTA_NAME, "");
}
using VolumeGetWriteSessionResponse = VolumeCreateWriteSessionResponse;

IVolumeWriteSessionPtr VolumeWriteSessionBuilderImpl::Build()
{
    return std::make_shared<VolumeWriteSessionImpl>(*this);
}

VolumeWriteSessionImpl::VolumeWriteSessionImpl(
    const VolumeWriteSessionBuilderImpl& builder)
    : mConf(builder.GetConfiguration()),
      mProject(builder.GetProject()),
      mVolume(builder.GetVolume()),
      mPartition(builder.GetPartition()),
      mSessionId(builder.GetSessionId())
{
    if (mSessionId.empty())
    {
        CreateVolumeWriteSession();
    }
    else
    {
        GetVolumeWriteSession(mSessionId);
    }
}
void VolumeWriteSessionImpl::CreateVolumeWriteSession()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(
        mConf, ACTION_VOLUME_CREATE_WRITE_SESSION, mProject, mVolume);
    VolumeCreateWriteSessionRequest request;
    std::string path = mProject+"/"+mVolume+"/"+mPartition+"/";
    request.SetFile(path);
    std::string requestBody = ToJsonString(request);
    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
    req->SetHeader(CONTENT_LENGTH, std::to_string(requestBody.size()));
    req->SetBody(requestBody);
    HttpConnectionPtr conn = std::make_shared<HttpConnection>(mConf);
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();
    if (!resp->isSuccessful())
    {
        TunnelThrow(*resp, mConf.tunnelEndpoint);
    }
    std::string responseBody;
    resp->ReadBody(responseBody);
    conn->Close();
    VolumeCreateWriteSessionResponse response;
    FromJsonString(response, responseBody);
    mSessionId = response.GetSessionId();
    mStatus = response.GetStatus();
    mQuotaName = response.GetQuotaName();
}

void VolumeWriteSessionImpl::GetVolumeWriteSession(const std::string& sessionId)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(
        mConf, ACTION_VOLUME_GET_WRITE_SESSION, mProject, mVolume);
    req->SetParameter(REQUEST_SESSION_ID, sessionId);
    VolumeCreateWriteSessionRequest request;
    const std::string& path = "/" + mProject + "/" + mVolume + "/" + mPartition;
    request.SetFile(path);
    const std::string& requestBody = ToJsonString(request);
    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
    req->SetHeader(CONTENT_LENGTH, std::to_string(requestBody.size()));
    req->SetBody(requestBody);
    HttpConnectionPtr conn = std::make_shared<HttpConnection>(mConf);
    conn->SetRequest(req);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();
    if (!resp->isSuccessful())
    {
        TunnelThrow(*resp, mConf.tunnelEndpoint);
    }
    std::string responseBody;
    resp->ReadBody(responseBody);
    conn->Close();
    VolumeGetWriteSessionResponse response;
    FromJsonString(response, responseBody);
    mSessionId = response.GetSessionId();
    mStatus = response.GetStatus();
    mQuotaName = response.GetQuotaName();
}

IVolumeWriteStreamBuilderPtr VolumeWriteSessionImpl::BuildWriteStream()
{
    auto builder = std::make_shared<VolumeWriteStreamBuilderImpl>(mConf);
    builder->SetProject(mProject)
        .SetVolume(mVolume)
        .SetPartition(mPartition)
        .SetSessionId(mSessionId);
    return builder;
}

void VolumeWriteSessionImpl::Commit()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_COMMIT_WRITE_SESSION, mProject, mVolume);
    req->SetParameter(REQUEST_SESSION_ID, mSessionId);
    HttpConnectionPtr connection(new HttpConnection(mConf));
    connection->SetRequest(req);
    connection->Open();
    ResponsePtr resp = connection->GetResponse();
    if (!resp->isSuccessful())
    {
        TunnelThrow(*resp, mConf.tunnelEndpoint);
    }
    connection->Close();
}
void VolumeWriteSessionImpl::Abort()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_ABORT_WRITE_SESSION, mProject, mVolume);
    req->SetParameter(REQUEST_SESSION_ID, mSessionId);
    HttpConnectionPtr connection(new HttpConnection(mConf));
    connection->SetRequest(req);
    connection->Open();
    ResponsePtr resp = connection->GetResponse();
    if (!resp->isSuccessful())
    {
        TunnelThrow(*resp, mConf.tunnelEndpoint);
    }
    connection->Close();
}

std::string VolumeWriteSessionImpl::GetSessionId()
{
    return mSessionId;
}

std::string VolumeWriteSessionImpl::GetStatus()
{
    return mStatus;
}
