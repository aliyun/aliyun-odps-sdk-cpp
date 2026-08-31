#include "volume_read_session.h"

#include "common/http_connection.h"
#include "common/http_flags.h"
#include "commons.h"
#include "include/odps_exception.h"
#include "tunnel/util.h"
#include "tunnel/volume_reader_writer.h"
#include "util/utils.h"
#include "volume_read_stream.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

class VolumeCreateReadSessionRequest
{
public:
    VolumeCreateReadSessionRequest() {}

    void SetFile(const std::string& file) { mFile = file; }

    friend void to_json(nlohmann::json& j, const VolumeCreateReadSessionRequest& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateReadSessionRequest& r);

private:
    std::string mFile;
};

inline void to_json(nlohmann::json& j, const VolumeCreateReadSessionRequest& r)
{
    j = nlohmann::json{{REQUEST_FILE, r.mFile}};
}

inline void from_json(const nlohmann::json& j, VolumeCreateReadSessionRequest& r)
{
    r.mFile = j.at(REQUEST_FILE).get<std::string>();
}

class VolumeCreateReadSessionResponse
{
public:
    VolumeCreateReadSessionResponse() {}

    std::string GetSessionId() { return mSessionId; }
    std::string GetStatus() { return mStatus; }
    std::string GetQuotaName() { return mQuotaName; }

    friend void to_json(nlohmann::json& j, const VolumeCreateReadSessionResponse& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateReadSessionResponse& r);

private:
    std::string mSessionId;
    std::string mStatus;
    std::string mQuotaName;
};

inline void to_json(nlohmann::json& j, const VolumeCreateReadSessionResponse& r)
{
    j = nlohmann::json{
        {REQUEST_DOWNLOAD_ID, r.mSessionId},
        {REQUEST_STATUS, r.mStatus},
        {REQUEST_QUOTA_NAME, r.mQuotaName},
    };
}

inline void from_json(const nlohmann::json& j, VolumeCreateReadSessionResponse& r)
{
    r.mSessionId = j.at(REQUEST_DOWNLOAD_ID).get<std::string>();
    r.mStatus = j.value(REQUEST_STATUS, "");
    r.mQuotaName = j.value(REQUEST_QUOTA_NAME, "");
}
using VolumeGetReadSessionResponse = VolumeCreateReadSessionResponse;

IVolumeReadSessionPtr VolumeReadSessionBuilderImpl::Build()
{
    return std::make_shared<VolumeReadSessionImpl>(*this);
}

VolumeReadSessionImpl::VolumeReadSessionImpl(const VolumeReadSessionBuilderImpl& builder)
    : mConf(builder.GetConfiguration()),
      mProject(builder.GetProject()),
      mVolume(builder.GetVolume()),
      mPartition(builder.GetPartition()),
      mSessionId(builder.GetSessionId())
{
    mPath = mProject + "/" + mVolume + "/" + mPartition + "/" + builder.GetFile();
    if (mSessionId.empty())
    {
        CreateVolumeReadSession();
    }
    else
    {
        GetVolumeReadSession(mSessionId);
    }
}

void VolumeReadSessionImpl::CreateVolumeReadSession()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(
        mConf, ACTION_VOLUME_CREATE_READ_SESSION, mProject, mVolume);
    VolumeCreateReadSessionRequest request;
    request.SetFile(mPath);
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
    VolumeCreateReadSessionResponse response;
    FromJsonString(response, responseBody);
    mSessionId = response.GetSessionId();
    mStatus = response.GetStatus();
    mQuotaName = response.GetQuotaName();
}

void VolumeReadSessionImpl::GetVolumeReadSession(const std::string& sessionId)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(
        mConf, ACTION_VOLUME_GET_READ_SESSION, mProject, mVolume);
    req->SetParameter(REQUEST_SESSION_ID, sessionId);
    VolumeCreateReadSessionRequest request;
    request.SetFile(mPath);
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
    VolumeGetReadSessionResponse response;
    FromJsonString(response, responseBody);
    mSessionId = response.GetSessionId();
    mStatus = response.GetStatus();
    mQuotaName = response.GetQuotaName();
}

IVolumeReadStreamBuilderPtr VolumeReadSessionImpl::BuildVolumeReadStream()
{
    auto builder = std::make_shared<VolumeReadStreamBuilderImpl>(mConf);
    builder->SetProject(mProject)
        .SetVolume(mVolume)
        .SetPartition(mPartition)
        .SetSessionId(mSessionId)
        .SetPath(mPath);
    return builder;
}

std::string VolumeReadSessionImpl::GetSessionId()
{
    return mSessionId;
}

std::string VolumeReadSessionImpl::GetStatus()
{
    return mStatus;
}
