#include "max_storage_api/table_read_session.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/table_read_stream.h"
#endif
#include "max_storage_api/commons.h"
#include "tunnel/util.h"
#include "util/utils.h"
#include "common/http_connection.h"
#include "common/http_flags.h"
#include "common/odps_table_schema.h"
#include "common/json_serialize.h"
#include "include/odps_exception.h"

using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;
using namespace apsara::odps::sdk::internal;

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

// 在本地定义需要的结构体
struct SplitOptionsImpl
{
public:
    std::string mSplitMode;
    uint64_t mSplitNumber;
    bool mCrossPartition;

    SplitOptionsImpl()
        : mSplitMode("RowOffset"),mSplitNumber(100)
        , mCrossPartition(false)
    {
    }

    void SetSplitMode(const std::string& splitMode)
    {
        mSplitMode = splitMode;
    }
    void SetSplitNumber(const uint64_t& splitNumber)
    {
        mSplitNumber = splitNumber;
    }
    void SetCrossPartition(const bool& crossPartition)
    {
        mCrossPartition = crossPartition;
    }
};

inline void to_json(nlohmann::json& j, const SplitOptionsImpl& o)
{
    j = nlohmann::json{
        {"SplitMode", o.mSplitMode},
        {"SplitNumber", o.mSplitNumber},
        {"CrossPartition", o.mCrossPartition},
    };
}

inline void from_json(const nlohmann::json& j, SplitOptionsImpl& o)
{
    o.mSplitMode = j.at("SplitMode").get<std::string>();
    o.mSplitNumber = j.at("SplitNumber").get<uint64_t>();
    o.mCrossPartition = j.at("CrossPartition").get<bool>();
}

struct ArrowOptionsImpl
{
public:
    TimeUnit mTimestampUnit = TimeUnit::NANO;
    TimeUnit mDatetimeUnit = TimeUnit::MILLI;

    void SetTimestampUnit(const TimeUnit& timestampUnit)
    {
        mTimestampUnit = timestampUnit;
    }

    void SetDatetimeUnit(const TimeUnit& datetimeUnit)
    {
        mDatetimeUnit = datetimeUnit;
    }

    friend void to_json(nlohmann::json& j, const ArrowOptionsImpl& o);
    friend void from_json(const nlohmann::json& j, ArrowOptionsImpl& o);

private:
    static std::string TimeUnitToString(TimeUnit unit)
    {
        switch (unit) {
            case TimeUnit::SECOND: return "second";
            case TimeUnit::MILLI: return "milli";
            case TimeUnit::MICRO: return "micro";
            case TimeUnit::NANO: return "nano";
            default: return "nano";
        }
    }

    static TimeUnit StringToTimeUnit(const std::string& unit)
    {
        if (unit == "second") {
            return TimeUnit::SECOND;
        } else if (unit == "milli") {
            return TimeUnit::MILLI;
        } else if (unit == "micro") {
            return TimeUnit::MICRO;
        } else {
            return TimeUnit::NANO;
        }
    }
};

inline void to_json(nlohmann::json& j, const ArrowOptionsImpl& o)
{
    j = nlohmann::json{
        {"TimestampUnit", ArrowOptionsImpl::TimeUnitToString(o.mTimestampUnit)},
        {"DatetimeUnit", ArrowOptionsImpl::TimeUnitToString(o.mDatetimeUnit)},
    };
}

inline void from_json(const nlohmann::json& j, ArrowOptionsImpl& o)
{
    o.mTimestampUnit = ArrowOptionsImpl::StringToTimeUnit(j.value("TimestampUnit", std::string("nano")));
    o.mDatetimeUnit = ArrowOptionsImpl::StringToTimeUnit(j.value("DatetimeUnit", std::string("milli")));
}

struct IncrementalReadOptionsImpl : public IncrementalReadOptions
{
public:
    IncrementalReadOptionsImpl() = default;
    IncrementalReadOptionsImpl(const IncrementalReadOptions& options) : IncrementalReadOptions(options) {}

    friend void to_json(nlohmann::json& j, const IncrementalReadOptionsImpl& o);
    friend void from_json(const nlohmann::json& j, IncrementalReadOptionsImpl& o);

private:
    static std::string ModeToString(IncrementalReadMode mode) {
        switch (mode) {
            case IncrementalReadMode::APPEND: return "append";
            case IncrementalReadMode::CDC: return "cdc";
            default: return "append";
        }
    }
    static IncrementalReadMode StringToMode(const std::string& mode) {
        if (mode == "cdc") {
            return IncrementalReadMode::CDC;
        } else {
            return IncrementalReadMode::APPEND;
        }
    }
};

inline void to_json(nlohmann::json& j, const IncrementalReadOptionsImpl& o)
{
    j = nlohmann::json{
        {"StartVersion", o.mStartVersion},
        {"EndVersion", o.mEndVersion},
        {"StartTimeStamp", o.mStartTimeStamp},
        {"EndTimeStamp", o.mEndTimeStamp},
        {"Mode", IncrementalReadOptionsImpl::ModeToString(o.mMode)},
        {"EnableIncrementalRead", o.mEnableIncrementalRead},
    };
}

inline void from_json(const nlohmann::json& j, IncrementalReadOptionsImpl& o)
{
    o.mStartVersion = j.at("StartVersion").get<int64_t>();
    o.mEndVersion = j.at("EndVersion").get<int64_t>();
    o.mStartTimeStamp = j.at("StartTimeStamp").get<std::string>();
    o.mEndTimeStamp = j.at("EndTimeStamp").get<std::string>();
    o.mMode = IncrementalReadOptionsImpl::StringToMode(j.at("Mode").get<std::string>());
    o.mEnableIncrementalRead = j.value("EnableIncrementalRead", false);
}

/**
 * @brief 创建读取会话请求类
 */
class TableCreateReadSessionRequest
{
private:
    std::vector<std::string> mRequiredDataColumns;
    std::vector<std::string> mRequiredPartitionColumns;
    std::vector<std::string> mRequiredPartitions;
    std::vector<int32_t> mRequiredBucketIds;
    SplitOptionsImpl mSplitOptions;
    ArrowOptionsImpl mArrowOptions;
    std::string mFilterPredicate;
    bool mFilterPredicateFallback = false;
    bool mAccquireDownload = true;
    int32_t mSplitMaxFileNum = 0;
    bool mSupportHybridStream = false;
    bool mEnableLargeString = false;
    IncrementalReadOptionsImpl mIncrementalReadOptions;
    bool mIncrementalRead = false;
    bool mSupportSaveToPangu = false;

    friend void to_json(nlohmann::json& j, const TableCreateReadSessionRequest& r);

public:
    void SetRequiredDataColumns(const std::vector<std::string>& requiredDataColumns)
    {
        mRequiredDataColumns = requiredDataColumns;
    }
    void SetRequiredPartitionColumns(const std::vector<std::string>& requiredPartitionColumns)
    {
        mRequiredPartitionColumns = requiredPartitionColumns;
    }
    void SetRequiredPartitions(const std::vector<std::string>& requiredPartitions)
    {
        mRequiredPartitions = requiredPartitions;
    }
    void SetRequiredBucketIds(const std::vector<int32_t>& requiredBucketIds)
    {
        mRequiredBucketIds = requiredBucketIds;
    }
    void SetSplitOptions(const SplitOptionsImpl& splitOptions)
    {
        mSplitOptions = splitOptions;
    }
    void SetArrowOptions(const ArrowOptionsImpl& arrowOptions)
    {
        mArrowOptions = arrowOptions;
    }
    void SetFilterPredicate(const std::string& filterPredicate)
    {
        mFilterPredicate = filterPredicate;
    }
    void SetFilterPredicateFallback(const bool& filterPredicateFallback)
    {
        mFilterPredicateFallback = filterPredicateFallback;
    }
    void SetAccquireDownload(const bool& accquireDownload)
    {
        mAccquireDownload = accquireDownload;
    }
    void SetSplitMaxFileNum(const int32_t& splitMaxFileNum)
    {
        mSplitMaxFileNum = splitMaxFileNum;
    }
    void SetSupportHybridStream(const bool& supportHybridStream)
    {
        mSupportHybridStream = supportHybridStream;
    }
    void SetEnableLargeString(const bool& enableLargeString)
    {
        mEnableLargeString = enableLargeString;
    }
    void SetIncrementalReadOptions(const IncrementalReadOptionsImpl& incrementalReadOptions)
    {
        mIncrementalReadOptions = incrementalReadOptions;
    }
    void SetIncrementalRead(const bool& incrementalRead)
    {
        mIncrementalRead = incrementalRead;
    }
    void SetSupportSaveToPangu(const bool& supportSaveToPangu)
    {
        mSupportSaveToPangu = supportSaveToPangu;
    }
};

inline void to_json(nlohmann::json& j, const TableCreateReadSessionRequest& r)
{
    j = nlohmann::json{
        {"RequiredDataColumns", r.mRequiredDataColumns},
        {"RequiredPartitionColumns", r.mRequiredPartitionColumns},
        {"RequiredPartitions", r.mRequiredPartitions},
        {"RequiredBucketIds", r.mRequiredBucketIds},
        {"FilterPredicate", r.mFilterPredicate},
        {"FilterPredicateFallback", r.mFilterPredicateFallback},
        {"SplitOptions", r.mSplitOptions},
        {"ArrowOptions", r.mArrowOptions},
        {"SplitMaxFileNum", r.mSplitMaxFileNum},
        {"AccquireDownload", r.mAccquireDownload},
        {"SupportHybridStream", r.mSupportHybridStream},
        {"EnableLargeString", r.mEnableLargeString},
        {"IncrementalReadOptions", r.mIncrementalReadOptions},
        {"IncrementalRead", r.mIncrementalRead},
        {"SupportSaveToPangu", r.mSupportSaveToPangu},
    };
}

/**
 * @brief 创建读取会话响应类
 */
class TableCreateReadSessionResponse
{
public:
    std::string mSessionId;
    int64_t mExpirationTime;
    std::string mSessionStatus;
    TableSchema mTableSchema;

    int64_t mSplitsCount;
    std::vector<int32_t> mSplitBucketId;
    int64_t mRecordCount;
    std::string mErrorMessage;
    std::vector<DataFormat> mSupportedDataFormat;
    bool mEnableLargeString = false;
    int64_t mLatestVersion = 0;
    IncrementalReadOptionsImpl mIncrementalReadOptions;
    std::string mSessionType = "batch_read";
    std::string mSplitMode;

    TableCreateReadSessionResponse(){};

    std::shared_ptr<Splits> GetSplits()
    {
        SplitMode mode;
        std::vector<SplitContext> splits;
        if (mSplitMode == "RowOffset")
        {
            mode = SplitMode::ROW_OFFSET;
        }
        else if (mSplitMode == "Bucket")
        {
            mode = SplitMode::BUCKET;
            for (size_t i = 0; i < mSplitBucketId.size(); ++i)
            {
                SplitContext context;
                context.mSplitMode = mode;
                context.mIndex = i;
                context.mBucketId = mSplitBucketId[i];
                splits.push_back(context);
            }
        }
        else
        {
            mode = SplitMode::SIZE;
            for (int64_t i = 0; i < mSplitsCount; i++)
            {
                SplitContext context;
                context.mSplitMode = mode;
                context.mIndex = i;
                splits.push_back(context);
            }
        }
        return std::make_shared<Splits>(mode, mRecordCount, splits);
    }
};

inline void to_json(nlohmann::json& j, const TableCreateReadSessionResponse& r)
{
    j = nlohmann::json{
        {"SessionId", r.mSessionId},
        {"ExpirationTime", r.mExpirationTime},
        {"SessionStatus", r.mSessionStatus},
        {"TableSchema", r.mTableSchema},
        {"SplitsCount", r.mSplitsCount},
        {"SplitBucketId", r.mSplitBucketId},
        {"RecordCount", r.mRecordCount},
        {"Message", r.mErrorMessage},
        {"SessionType", r.mSessionType},
        {"SupportedDataFormat", r.mSupportedDataFormat},
        {"EnableLargeString", r.mEnableLargeString},
        {"IncrementalReadOptions", r.mIncrementalReadOptions},
        {"LatestVersion", r.mLatestVersion},
        {"SplitMode", r.mSplitMode},
    };
}

inline void from_json(const nlohmann::json& j, TableCreateReadSessionResponse& r)
{
    r.mSessionId = j.at("SessionId").get<std::string>();
    r.mExpirationTime = j.at("ExpirationTime").get<int64_t>();
    r.mSessionStatus = j.at("SessionStatus").get<std::string>();
    j.at("TableSchema").get_to(r.mTableSchema);
    r.mSplitsCount = j.at("SplitsCount").get<int64_t>();
    r.mSplitBucketId = j.at("SplitBucketId").get<std::vector<int32_t>>();
    r.mRecordCount = j.at("RecordCount").get<int64_t>();
    r.mErrorMessage = j.at("Message").get<std::string>();
    r.mSessionType = j.at("SessionType").get<std::string>();
    j.at("SupportedDataFormat").get_to(r.mSupportedDataFormat);
    r.mEnableLargeString = j.at("EnableLargeString").get<bool>();
    j.at("IncrementalReadOptions").get_to(r.mIncrementalReadOptions);
    r.mLatestVersion = j.at("LatestVersion").get<int64_t>();
    r.mSplitMode = j.at("SplitMode").get<std::string>();
}

using TableGetReadSessionResponse = TableCreateReadSessionResponse;

TableReadSessionImpl::TableReadSessionImpl(const TableReadSessionBuilderImpl& builder)
    : mConf(builder.GetConfiguration())
    , mProject(builder.GetProject())
    , mTable(builder.GetTable())
    , mSchemaName(builder.GetSchema())
    , mSessionId(builder.GetSessionId())
    , mSplitOptions(builder.GetSplitOptions())
    , mArrowOptions(builder.GetArrowOptions())
    , mFilterOptions(builder.GetFilterOptions())
    , mIncrementalInfo(builder.GetIncrementalReadOptions())
{
    if (mSessionId.empty())
    {
        CreateReadSession();

        const auto start = std::chrono::steady_clock::now();
        while (mStatus == SESSION_STATUS_INIT)
        {
            RandomSleep(5 * 1000, 30 * 1000);
            GetReadSession();

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
        GetReadSession();
    }
}

ITableReadStreamBuilderPtr TableReadSessionImpl::BuildTableReadStream()
{
#ifdef ODPS_SDK_ENABLE_ARROW
    auto builder = std::make_shared<TableReadStreamBuilderImpl>(mConf);
    builder->SetProject(mProject)
            .SetTable(mTable)
            .SetSchema(mSchemaName)
            .SetSessionId(mSessionId);
    return builder;
#else
    throw OdpsException("TableReadStream requires arrow support, rebuild with WITH_ARROW=ON");
#endif
}

std::string TableReadSessionImpl::GetSessionId()
{
    return mSessionId;
}

ISplitsPtr TableReadSessionImpl::GetSplits()
{
    return mSplits;
}

IncrementalInfo TableReadSessionImpl::GetIncrementalInfo()
{
    return mIncrementalInfo;
}

void TableReadSessionImpl::CreateReadSession()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = CreateMaxStorageApiTableRequest(
        mConf,
        ACTION_TABLE_CREATE_READ_SESSION,
        mProject,
        mSchemaName,
        mTable);

    TableCreateReadSessionRequest sessionReq;

    sessionReq.SetRequiredDataColumns(mFilterOptions.mRequiredDataColumns);
    sessionReq.SetRequiredPartitionColumns(mFilterOptions.mRequiredPartitionColumns);
    sessionReq.SetRequiredPartitions(mFilterOptions.mRequiredPartitions);
    sessionReq.SetRequiredBucketIds(mFilterOptions.mRequiredBucketIds);

    // 转换SplitOptions类型
    SplitOptionsImpl splitOptions;
    switch (mSplitOptions.mSplitMode) {
        case SplitMode::ROW_OFFSET:
            splitOptions.SetSplitMode("RowOffset");
            break;
        case SplitMode::SIZE:
            splitOptions.SetSplitMode("Size");
            break;
        case SplitMode::BUCKET:
            splitOptions.SetSplitMode("Bucket");
            break;
        default:
            splitOptions.SetSplitMode("RowOffset");
            break;
    }

    splitOptions.SetSplitNumber(mSplitOptions.mSplitSize);
    splitOptions.SetCrossPartition(mSplitOptions.mCrossPartition);
    sessionReq.SetSplitOptions(splitOptions);
    sessionReq.SetSplitMaxFileNum(mSplitOptions.mMaxFileNum);

    // 转换ArrowOptions类型
    ArrowOptionsImpl arrowOptions;
    arrowOptions.SetTimestampUnit(mArrowOptions.mTimestampUnit);
    arrowOptions.SetDatetimeUnit(mArrowOptions.mDatetimeUnit);
    sessionReq.SetArrowOptions(arrowOptions);

    // 设置谓词
    if (mFilterOptions.mPredicate) {
        sessionReq.SetFilterPredicate(mFilterOptions.mPredicate->ToString());
    }

    // TODO: 实现谓词过滤回退
    // sessionReq.SetFilterPredicateFallback(mBuilder.GetFilterPredicateFallback());

    // 转换IncrementalReadOptions类型
    IncrementalReadOptionsImpl incrementalReadOptions(mIncrementalInfo.mOptions);
    sessionReq.SetIncrementalReadOptions(incrementalReadOptions);
    sessionReq.SetIncrementalRead(incrementalReadOptions.mEnableIncrementalRead);

    std::string requestBody = ToJsonString(sessionReq);
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

    LoadSession(responseBody);
}

void TableReadSessionImpl::GetReadSession()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = CreateMaxStorageApiTableRequest(
        mConf,
        ACTION_TABLE_GET_READ_SESSION,
        mProject,
        mSchemaName,
        mTable);

    req->SetParameter(REQUEST_SESSION_ID, mSessionId);

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

    LoadSession(responseBody);
}

void TableReadSessionImpl::LoadSession(const std::string& json)
{
    TableCreateReadSessionResponse response;
    FromJsonString(response, json);
    mSessionId = response.mSessionId;
    mStatus = response.mSessionStatus;
    mSplits = response.GetSplits();
    mTableSchema = response.mTableSchema;
    mIncrementalInfo.mOptions = response.mIncrementalReadOptions;
    mIncrementalInfo.mLatestVersion = response.mLatestVersion;
    if (mStatus != SESSION_STATUS_NORMAL && mStatus != SESSION_STATUS_INIT)
    {
        TunnelThrow("InvalidSessionStatus", "SessionId:" + mSessionId + ", Status:" + mStatus + ", " + response.mErrorMessage);
    }
}

ITableReadSessionPtr TableReadSessionBuilderImpl::Build()
{
    return std::make_shared<TableReadSessionImpl>(*this);
}

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara