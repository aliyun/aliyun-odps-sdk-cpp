#include "common/http_message.h"
#include "max_storage_api/commons.h"
#include "max_storage_api/instance_read_session.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/instance_read_stream.h"
#endif
#include "include/odps_exception.h"
#include "tunnel/odps_meta.h"
#include "tunnel/util.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

class InstanceSessionRequest
{
private:
    bool mEnableLimit;

public:
    InstanceSessionRequest() : mEnableLimit(false) {}

    void SetEnableLimit(bool enableLimit) { mEnableLimit = enableLimit; }

    friend void to_json(nlohmann::json& j, const InstanceSessionRequest& r);
    friend void from_json(const nlohmann::json& j, InstanceSessionRequest& r);
};

inline void to_json(nlohmann::json& j, const InstanceSessionRequest& r)
{
    j = nlohmann::json{{REQUEST_ENABLE_LIMIT, r.mEnableLimit}};
}

inline void from_json(const nlohmann::json& j, InstanceSessionRequest& r)
{
    r.mEnableLimit = j.at(REQUEST_ENABLE_LIMIT).get<bool>();
}

struct InstanceSessionResponse
{
    std::string mSessionId;
    int64_t mRecordCount;
    std::string mStatus;
    std::string mOwner;
    std::string mInitiated;
    std::string mQuotaName;
    TableSchema mTableSchema;
    InstanceSessionResponse() : mRecordCount(0) {}
};

inline void to_json(nlohmann::json& j, const InstanceSessionResponse& r)
{
    j = nlohmann::json{
        {REQUEST_DOWNLOAD_ID, r.mSessionId},
        {REQUEST_RECORD_COUNT, r.mRecordCount},
        {REQUEST_STATUS, r.mStatus},
        {REQUEST_OWNER, r.mOwner},
        {REQUEST_INITIATED, r.mInitiated},
        {REQUEST_QUOTA_NAME, r.mQuotaName},
        {REQUEST_TABLE_SCHEMA, r.mTableSchema},
    };
}

inline void from_json(const nlohmann::json& j, InstanceSessionResponse& r)
{
    r.mSessionId = j.value(REQUEST_DOWNLOAD_ID, "");
    r.mRecordCount = j.value(REQUEST_RECORD_COUNT, static_cast<int64_t>(0));
    r.mStatus = j.value(REQUEST_STATUS, "");
    r.mOwner = j.value(REQUEST_OWNER, "");
    r.mInitiated = j.value(REQUEST_INITIATED, "");
    r.mQuotaName = j.value(REQUEST_QUOTA_NAME, "");
    j.at(REQUEST_TABLE_SCHEMA).get_to(r.mTableSchema);
}

IInstanceReadSessionPtr InstanceReadSessionBuilderImpl::Build()
{
    return std::make_shared<InstanceReadSessionImpl>(*this);
}

InstanceReadSessionImpl::InstanceReadSessionImpl(const InstanceReadSessionBuilderImpl& builder)
    : mConf(builder.GetConfiguration()),
    mProject(builder.GetProject()),
    mInstance(builder.GetInstance()),
    mSessionId(builder.GetSessionId()),
    mEnableLimit(builder.GetEnableLimit())
{
    if (mSessionId.empty())
    {
        // TODO: async mode
        CreateReadSession();

        const auto start = std::chrono::steady_clock::now();
        while (mStatus == SESSION_STATUS_INITIATING)
        {
            RandomSleep(5 * 1000, 30 * 1000);
            GetReadSession(mSessionId);

            // 检查是否超时
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

            if (elapsedTimeMs > CREATE_SESSION_TIMEOUT_MS) {
                TunnelThrow("SessionInitializationTimeout",
                        "Session initialization timeout after " + std::to_string(elapsedTimeMs) + " ms. SessionId: " + mSessionId);
            }
        }
    }
    else
    {
        GetReadSession(mSessionId);
    }
}

void InstanceReadSessionImpl::CreateReadSession()
{
    if (!mSessionId.empty())
    {
        return;
    }

    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = CreateMaxStorageApiInstanceRequest(
        mConf, ACTION_INSTANCE_CREATE_READ_SESSION, mProject, mInstance);

    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
    InstanceSessionRequest request;
    request.SetEnableLimit(mEnableLimit);
    req->SetBody(ToJsonString(request));
    req->SetHeader(ACCEPT_ENCODING, HEADER_APPLICATION_JSON);
    req->SetHeader(CONTENT_LENGTH, std::to_string(req->GetBody().size()));

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

    LoadFromJson(responseBody);
}

IInstanceReadStreamBuilderPtr InstanceReadSessionImpl::BuildInstanceReadStream()
{
#ifdef ODPS_SDK_ENABLE_ARROW
    auto builder = std::make_shared<InstanceReadStreamBuilderImpl>(mConf);
    builder->SetProject(mProject)
        .SetInstance(mInstance)
        .SetSessionId(mSessionId);
    return builder;
#else
    throw OdpsException("InstanceReadStream requires arrow support, rebuild with WITH_ARROW=ON");
#endif
}

std::string InstanceReadSessionImpl::GetSessionId()
{
    return mSessionId;
}

ISplitsPtr InstanceReadSessionImpl::GetSplits()
{
    return std::make_shared<Splits>(SplitMode::ROW_OFFSET, mRecordCount, std::vector<SplitContext>());
}

void InstanceReadSessionImpl::GetReadSession(const std::string& sessionId)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = CreateMaxStorageApiInstanceRequest(
        mConf, ACTION_INSTANCE_GET_READ_SESSION, mProject, mInstance);

    req->SetParameter(REQUEST_SESSION_ID, sessionId);
    req->SetHeader(ACCEPT_ENCODING, HEADER_APPLICATION_JSON);
    req->SetHeader(CONTENT_LENGTH, std::to_string(req->GetBody().size()));

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

    LoadFromJson(responseBody);
}

void InstanceReadSessionImpl::LoadFromJson(const std::string& json)
{
    InstanceSessionResponse response;
    FromJsonString(response, json);
    mSessionId = response.mSessionId;
    mRecordCount = response.mRecordCount;
    mStatus = response.mStatus;
    mOwner = response.mOwner;
    mInitiated = response.mInitiated;
    mQuotaName = response.mQuotaName;
    mTableSchema = response.mTableSchema;
}
