#include "volume_write_stream.h"

#include <sstream>

#include "common/http_flags.h"
#include "include/odps_exception.h"
#include "tunnel/util.h"
#include "util/utils.h"
#include "commons.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

class VolumeV2CreateWriteStreamRequest
{
public:
    VolumeV2CreateWriteStreamRequest() : mReplicaCount(3) {}

    void SetFile(const std::string& file) { mFile = file; }
    void SetReplicaCount(int64_t count) { mReplicaCount = count; }

    friend void to_json(nlohmann::json& j, const VolumeV2CreateWriteStreamRequest& r);
    friend void from_json(const nlohmann::json& j, VolumeV2CreateWriteStreamRequest& r);

private:
    std::string mFile;
    int64_t mReplicaCount;
};

inline void to_json(nlohmann::json& j, const VolumeV2CreateWriteStreamRequest& r)
{
    j = nlohmann::json{
        {REQUEST_FILE, r.mFile},
        {REQUEST_REPLICA_COUNT, r.mReplicaCount},
    };
}

inline void from_json(const nlohmann::json& j, VolumeV2CreateWriteStreamRequest& r)
{
    r.mFile = j.at(REQUEST_FILE).get<std::string>();
    r.mReplicaCount = j.at(REQUEST_REPLICA_COUNT).get<int64_t>();
}

class VolumeV1CreateWriteStreamRequest
{
public:
    VolumeV1CreateWriteStreamRequest() {}

    void SetFile(const std::string& file) { mFile = file; }

    friend void to_json(nlohmann::json& j, const VolumeV1CreateWriteStreamRequest& r);
    friend void from_json(const nlohmann::json& j, VolumeV1CreateWriteStreamRequest& r);

private:
    std::string mFile;
};

inline void to_json(nlohmann::json& j, const VolumeV1CreateWriteStreamRequest& r)
{
    j = nlohmann::json{{REQUEST_FILE, r.mFile}};
}

inline void from_json(const nlohmann::json& j, VolumeV1CreateWriteStreamRequest& r)
{
    r.mFile = j.at(REQUEST_FILE).get<std::string>();
}

class VolumeCreateWriteStreamResponse
{
public:
    VolumeCreateWriteStreamResponse() {}

    std::string GetStreamId() { return mStreamId; }

    friend void to_json(nlohmann::json& j, const VolumeCreateWriteStreamResponse& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateWriteStreamResponse& r);

private:
    std::string mStreamId;
};

inline void to_json(nlohmann::json& j, const VolumeCreateWriteStreamResponse& r)
{
    j = nlohmann::json{{REQUEST_STREAM_ID, r.mStreamId}};
}

inline void from_json(const nlohmann::json& j, VolumeCreateWriteStreamResponse& r)
{
    r.mStreamId = j.at(REQUEST_STREAM_ID).get<std::string>();
}

IVolumeWriteStreamPtr VolumeFSWriteStreamBuilderImpl::Build()
{
    return VolumeWriteStreamPtr(new VolumeWriteStreamImpl(*this));
}

IVolumeWriteStreamPtr VolumeWriteStreamBuilderImpl::Build()
{
    return VolumeWriteStreamPtr(new VolumeWriteStreamImpl(*this));
}

VolumeWriteStreamImpl::VolumeWriteStreamImpl(const VolumeFSWriteStreamBuilderImpl& builder)
    : mConf(builder.GetConfiguration()),
    mProject(builder.GetProject()),
    mVolume(builder.GetVolume()),
    mPath(builder.GetPath()),
    mReplicaCount(builder.GetReplicaCount()),
    mClosed(false)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
}

VolumeWriteStreamImpl::VolumeWriteStreamImpl(const VolumeWriteStreamBuilderImpl& builder)
    : mConf(builder.GetConfiguration()),
    mProject(builder.GetProject()),
    mVolume(builder.GetVolume()),
    mPartition(builder.GetPartition()),
    mSessionId(builder.GetSessionId()),
    mClosed(false)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
    mPath = mProject + "/" + mVolume + "/" + mPartition + "/" + builder.GetFile();
}

VolumeWriteStreamImpl::~VolumeWriteStreamImpl()
{
    Close();
}

void VolumeWriteStreamImpl::Write(const char* buf, int64_t len)
{
    if (mClosed)
    {
        TunnelThrow("Volume write stream is closed");
    }

    CreateWriteStream();

    if (buf == nullptr || len <= 0)
    {
        return;
    }

    const auto& output = OpenOutputStream();

    output->Write(buf, len);
    output->Close();
}

void VolumeWriteStreamImpl::Close()
{
    if (mClosed)
    {
        return;
    }

    CloseWriteStream();

    mClosed = true;
}

void VolumeWriteStreamImpl::CreateWriteStream()
{
    if (mStreamId.empty())
    {
        const RequestPtr& req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_CREATE_WRITE_STREAM, mProject, mVolume);
        std::string requestBody;
        if(mSessionId.empty())
        {
            VolumeV2CreateWriteStreamRequest request;
            request.SetFile(mPath);
            request.SetReplicaCount(mReplicaCount);
            requestBody = ToJsonString(request);
        }
        else
        {   req->SetParameter(REQUEST_SESSION_ID, mSessionId);
            VolumeV1CreateWriteStreamRequest request;
            request.SetFile(mPath);
            requestBody = ToJsonString(request);
        }
        req->SetBody(requestBody);
        req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
        req->SetHeader(CONTENT_LENGTH, std::to_string(requestBody.size()));

        HttpConnectionPtr connection(new HttpConnection(mConf, true));
        connection->SetRequest(req);
        connection->Open();

        ResponsePtr resp = connection->GetResponse();

        if (!resp->isSuccessful())
        {
            TunnelThrow(*resp, mConf.tunnelEndpoint);
        }

        mCompress = SelectCompressOption(resp->GetHeader(ACCEPT_ENCODING), {CompressOption::ODPS_LZ4_FRAME, CompressOption::ODPS_ZSTD});

        std::string responseBody;
        resp->ReadBody(responseBody);

        VolumeCreateWriteStreamResponse response;
        FromJsonString(response, responseBody);
        mStreamId = response.GetStreamId();
        connection->Close();
    }
}

std::shared_ptr<internal::tunnel::HttpVolumeOutputStream> VolumeWriteStreamImpl::OpenOutputStream()
{
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_WRITE, mProject, mVolume);
    if (!mSessionId.empty())
    {
        req->SetParameter(REQUEST_SESSION_ID, mSessionId);
    }
    req->SetParameter(REQUEST_STREAM_ID, mStreamId);
    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_OCTET_STREAM);
    req->SetHeader(TRANSFER_ENCODING, CHUNKED);

    if (mCompress.algorithm != CompressOption::ODPS_RAW)
    {
        req->SetHeader(CONTENT_ENCODING, CompressOptionToEncoding(mCompress));
    }

    HttpConnectionPtr connection(new HttpConnection(mConf, true));
    connection->SetRequest(req);
    connection->Open();
    return std::make_shared<HttpVolumeOutputStream>(connection, mCompress);
}

void VolumeWriteStreamImpl::CloseWriteStream()
{
    if (!mSessionId.empty())
    {
        return;
    }
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_CLOSE_WRITE_STREAM, mProject, mVolume);
    req->SetParameter(REQUEST_STREAM_ID, mStreamId);
    HttpConnectionPtr connection(new HttpConnection(mConf, true));
    connection->SetRequest(req);
    connection->Open();

    ResponsePtr resp = connection->GetResponse();

    if (!resp->isSuccessful())
    {
        TunnelThrow(*resp, mConf.tunnelEndpoint);
    }
    connection->Close();
}
