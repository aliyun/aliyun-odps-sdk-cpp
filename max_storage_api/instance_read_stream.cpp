#ifdef ODPS_SDK_ENABLE_ARROW
#include <arrow/ipc/reader.h>

#include "common/http_message.h"
#include "max_storage_api/instance_read_stream.h"
#include "max_storage_api/commons.h"
#include "tunnel/util.h"
#include "util/utils.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

class InstanceReadRequest
{
private:
    std::vector<std::string> mColumns;

public:
    InstanceReadRequest() {}

    void SetColumns(const std::vector<std::string>& columns)
    {
        mColumns = columns;
    }

    friend void to_json(nlohmann::json& j, const InstanceReadRequest& r);
    friend void from_json(const nlohmann::json& j, InstanceReadRequest& r);
};

inline void to_json(nlohmann::json& j, const InstanceReadRequest& r)
{
    j = nlohmann::json{{REQUEST_COLUMNS, r.mColumns}};
}

inline void from_json(const nlohmann::json& j, InstanceReadRequest& r)
{
    r.mColumns = j.at(REQUEST_COLUMNS).get<std::vector<std::string>>();
}

class InstanceDirectReadRequest
{
private:
    std::vector<std::string> mColumns;
    std::string mTaskName;
    int64_t mQueryId = 0;
    bool mEnableLimit = false;

public:
    InstanceDirectReadRequest() {}

    void SetColumns(const std::vector<std::string>& columns)
    {
        mColumns = columns;
    }
    void SetTaskName(const std::string& taskName)
    {
        mTaskName = taskName;
    }
    void SetQueryId(int64_t queryId)
    {
        mQueryId = queryId;
    }
    void SetEnableLimit(bool enableLimit)
    {
        mEnableLimit = enableLimit;
    }

    friend void to_json(nlohmann::json& j, const InstanceDirectReadRequest& r);
    friend void from_json(const nlohmann::json& j, InstanceDirectReadRequest& r);
};

inline void to_json(nlohmann::json& j, const InstanceDirectReadRequest& r)
{
    j = nlohmann::json{
        {REQUEST_COLUMNS, r.mColumns},
        {REQUEST_TASK_NAME, r.mTaskName},
        {REQUEST_QUERY_ID, r.mQueryId},
        {REQUEST_ENABLE_LIMIT, r.mEnableLimit},
    };
}

inline void from_json(const nlohmann::json& j, InstanceDirectReadRequest& r)
{
    r.mColumns = j.at(REQUEST_COLUMNS).get<std::vector<std::string>>();
    r.mTaskName = j.at(REQUEST_TASK_NAME).get<std::string>();
    r.mQueryId = j.at(REQUEST_QUERY_ID).get<int64_t>();
    r.mEnableLimit = j.at(REQUEST_ENABLE_LIMIT).get<bool>();
}

IArrowReadStreamPtr InstanceReadStreamBuilderImpl::Build()
{
    return std::make_shared<InstanceReadStreamImpl>(*this);
}

IArrowReadStreamPtr InstanceDirectReadStreamBuilderImpl::Build()
{
    return std::make_shared<InstanceReadStreamImpl>(*this);
}

InstanceReadStreamImpl::InstanceReadStreamImpl(const InstanceReadStreamBuilderImpl& builder)
    : ArrowReadStreamImpl(ACTION_INSTANCE_READ),
    mConf(builder.GetConfiguration()),
    mProject(builder.GetProject()),
    mInstance(builder.GetInstance()),
    mSessionId(builder.GetSessionId()),
    mColumns(builder.GetColumns())
{
    const auto& split = builder.GetSplit();
    if (split == nullptr)
    {
        TunnelThrow("Split is null");
    }
    split->Accept(*this);
    if (mSplitContext.mSplitMode != SplitMode::ROW_OFFSET)
    {
        TunnelThrow("Only support row offset split mode");
    }
    mOffset = mSplitContext.mOffset;
    mCount = mSplitContext.mCount;
}

InstanceReadStreamImpl::InstanceReadStreamImpl(const InstanceDirectReadStreamBuilderImpl& builder)
    : ArrowReadStreamImpl(ACTION_INSTANCE_READ),
    mConf(builder.GetConfiguration()),
    mProject(builder.GetProject()),
    mInstance(builder.GetInstance()),
    mOffset(builder.GetOffset()),
    mCount(builder.GetCount()),
    mTaskName(builder.GetTaskName()),
    mQueryId(builder.GetQueryId()),
    mEnableLimit(builder.GetEnableLimit())
{
}

std::shared_ptr<ArrowBatchReader> InstanceReadStreamImpl::OpenReader()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = CreateMaxStorageApiInstanceRequest(
        mConf, ACTION_INSTANCE_READ, mProject, mInstance);

    std::string body;
    if (mSessionId.empty())
    {
        InstanceDirectReadRequest request;
        request.SetColumns(mColumns);
        request.SetTaskName(mTaskName);
        request.SetQueryId(mQueryId);
        request.SetEnableLimit(mEnableLimit);
        body = ToJsonString(request);
    }
    else
    {
        // Instance Read Session
        req->SetParameter(REQUEST_SESSION_ID, mSessionId);
        InstanceReadRequest request;
        request.SetColumns(mColumns);
        body = ToJsonString(request);
    }

    if (mOffset > 0)
    {
        req->SetParameter(REQUEST_OFFSET, std::to_string(mOffset));
    }

    if (mCount > 0)
    {
        req->SetParameter(REQUEST_COUNT, std::to_string(mCount));
    }

    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
    req->SetHeader(ACCEPT_ENCODING, COMPRESS_LZ4_FRAME + "," + COMPRESS_ZSTD);
    req->SetBody(body);
    req->SetHeader(CONTENT_LENGTH, std::to_string(req->GetBody().size()));

    return std::make_shared<ArrowBatchReader>(req, mConf);
}

void InstanceReadStreamImpl::Close()
{
    mReader.reset();
    mColumns.clear();
    mClosed = true;
}
#endif  // ODPS_SDK_ENABLE_ARROW
