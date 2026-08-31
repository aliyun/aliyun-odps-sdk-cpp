#include "volume_read_stream.h"

#include "common/http_flags.h"
#include "include/odps_exception.h"
#include "tunnel/util.h"
#include "util/utils.h"
#include "max_storage_api/commons.h"
#include "common/http_connection.h"
#include "tunnel/volume_reader_writer.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

class VolumeCreateReadStreamRequest
{
public:
    VolumeCreateReadStreamRequest() {}

    void SetFile(const std::string& file) { mFile = file; }

    friend void to_json(nlohmann::json& j, const VolumeCreateReadStreamRequest& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateReadStreamRequest& r);

private:
    std::string mFile;
};

inline void to_json(nlohmann::json& j, const VolumeCreateReadStreamRequest& r)
{
    j = nlohmann::json{{REQUEST_FILE, r.mFile}};
}

inline void from_json(const nlohmann::json& j, VolumeCreateReadStreamRequest& r)
{
    r.mFile = j.at(REQUEST_FILE).get<std::string>();
}

class VolumeCreateReadStreamResponse
{
public:
    VolumeCreateReadStreamResponse() : mFileLength(-1) {}

    std::string GetStreamId() { return mStreamId; }
    int64_t GetFileLength() { return mFileLength; }

    friend void to_json(nlohmann::json& j, const VolumeCreateReadStreamResponse& r);
    friend void from_json(const nlohmann::json& j, VolumeCreateReadStreamResponse& r);

private:
    std::string mStreamId;
    int64_t mFileLength;
};

inline void to_json(nlohmann::json& j, const VolumeCreateReadStreamResponse& r)
{
    j = nlohmann::json{
        {REQUEST_STREAM_ID, r.mStreamId},
        {REQUEST_FILE_LENGTH, r.mFileLength},
    };
}

inline void from_json(const nlohmann::json& j, VolumeCreateReadStreamResponse& r)
{
    r.mStreamId = j.at(REQUEST_STREAM_ID).get<std::string>();
    r.mFileLength = j.at(REQUEST_FILE_LENGTH).get<int64_t>();
}

IVolumeReadStreamPtr VolumeFSReadStreamBuilderImpl::Build()
{
    return std::make_shared<VolumeReadStreamImpl>(*this);
}

IVolumeReadStreamPtr VolumeReadStreamBuilderImpl::Build()
{
    return std::make_shared<VolumeReadStreamImpl>(*this);
}

VolumeReadStreamImpl::VolumeReadStreamImpl(const VolumeFSReadStreamBuilderImpl& builder)
    : mConf(builder.GetConfiguration())
    , mProject(builder.GetProject())
    , mVolume(builder.GetVolume())
    , mPath(builder.GetPath())
    , mOffset(builder.GetOffset())
    , mCount(builder.GetCount())
    , mClosed(false)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
}

VolumeReadStreamImpl::VolumeReadStreamImpl(const VolumeReadStreamBuilderImpl& builder)
    : mConf(builder.GetConfiguration())
    , mProject(builder.GetProject())
    , mVolume(builder.GetVolume())
    , mPartition(builder.GetPartition())
    , mPath(builder.GetPath())
    , mOffset(builder.GetOffset())
    , mCount(builder.GetCount())
    , mSessionId(builder.GetSessionId())
    , mClosed(false)
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }
}

VolumeReadStreamImpl::~VolumeReadStreamImpl()
{
    Close();
}

int64_t VolumeReadStreamImpl::Read(char* buf, int64_t len)
{
    if (mClosed)
    {
        TunnelThrow("Volume read stream is closed");
    }

    CreateReadStream();

    if (buf == nullptr || len <= 0 || mCount == 0)
    {
        return 0;
    }

    auto input = OpenInputStream();

    int64_t total = 0;
    int64_t size = mCount > 0 ? std::min(len, mCount) : len;
    while (size > 0)
    {
        int read = input->Read(buf, size);
        if (read <= 0)
        {
            break;
        }
        else
        {
            total += read;
            size -= read;
            if (mCount > 0)
            {
                mCount -= read;
            }
            mOffset += read;
        }
    }
    return total;
}

void VolumeReadStreamImpl::Close()
{
    if (mClosed)
    {
        return;
    }
    mClosed = true;
}

void VolumeReadStreamImpl::CreateReadStream()
{
    if (mStreamId.empty())
    {
        const RequestPtr req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_CREATE_READ_STREAM, mProject, mVolume);

        VolumeCreateReadStreamRequest request;
        request.SetFile(mPath);
        const std::string& requestBody = ToJsonString(request);
        req->SetBody(requestBody);
        req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
        req->SetHeader(CONTENT_LENGTH, std::to_string(requestBody.size()));

        if(!mSessionId.empty())
        {
            req->SetParameter(REQUEST_SESSION_ID, mSessionId);
        }

        HttpConnectionPtr connection(new HttpConnection(mConf, true));
        connection->SetRequest(req);
        connection->Open();
        ResponsePtr resp = connection->GetResponse();

        if (!resp->isSuccessful())
        {
            TunnelThrow(*resp, mConf.tunnelEndpoint);
        }

        std::string responseBody;
        resp->ReadBody(responseBody);
        VolumeCreateReadStreamResponse response;
        FromJsonString(response, responseBody);
        mStreamId = response.GetStreamId();
        if (response.GetFileLength() >= 0)
        {
            if (mCount > 0 && response.GetFileLength() - mOffset > 0)
            {
                mCount = std::min(mCount, response.GetFileLength() - mOffset);
            }
            else
            {
                mCount = response.GetFileLength();
            }
        }

        connection->Close();
    }
}

std::shared_ptr<HttpVolumeInputStream> VolumeReadStreamImpl::OpenInputStream()
{
    RequestPtr req = util::CreateMaxStorageApiVolumeRequest(mConf, ACTION_VOLUME_READ, mProject, mVolume);
    req->SetParameter(REQUEST_STREAM_ID, mStreamId);

    if(!mSessionId.empty())
    {
        req->SetParameter(REQUEST_SESSION_ID, mSessionId);
    }

    if (mOffset > 0)
    {
        req->SetParameter(REQUEST_OFFSET, std::to_string(mOffset));
    }

    if (mCount > 0) {
        req->SetParameter(REQUEST_COUNT, std::to_string(mCount));
    }

    req->SetHeader(ACCEPT_ENCODING, COMPRESS_LZ4_FRAME + "," + COMPRESS_ZSTD);

    HttpConnectionPtr connection(new HttpConnection(mConf, true));
    connection->SetRequest(req);
    connection->Open();
    ResponsePtr response = connection->GetResponse();

    if (!response->isSuccessful()) {
        connection->Close();
        TunnelThrow(*response, mConf.tunnelEndpoint);
    }

    return std::make_shared<HttpVolumeInputStream>(connection, EncodingToCompressOption(response->GetHeader(CONTENT_ENCODING)));
}
