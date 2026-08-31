#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/table_read_stream.h"

#include <arrow/array.h>
#include <arrow/ipc/reader.h>
#include <arrow/record_batch.h>

#include <sstream>

#include "max_storage_api/commons.h"
#include "include/odps_exception.h"
#include "util/utils.h"
#include "max_storage_api/arrow_reader.h"
#include "tunnel/util.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

class TableReadRequest
{
public:
    int64_t mMaxBatchRows = 4096;
    int64_t mSkipRowNum = 0;
    int64_t mMaxBatchRawSize = 0;
    DataFormat mDataFormat;
    std::vector<std::string> mDataColumns;
    bool mDataColumnsUnordered = false;

    void SetMaxBatchRows(int64_t maxBatchRows)
    {
        mMaxBatchRows = maxBatchRows;
    }

    void SetSkipRowNum(int64_t skipRowNum)
    {
        mSkipRowNum = skipRowNum;
    }

    void SetMaxBatchRawSize(int64_t maxBatchRawSize)
    {
        mMaxBatchRawSize = maxBatchRawSize;
    }

    void SetDataFormat(const DataFormat& dataFormat)
    {
        mDataFormat = dataFormat;
    }

    void SetDataColumns(const std::vector<std::string>& dataColumns)
    {
        mDataColumns = dataColumns;
    }

    void SetDataColumnsUnordered(bool dataColumnsUnordered)
    {
        mDataColumnsUnordered = dataColumnsUnordered;
    }
};

inline void to_json(nlohmann::json& j, const TableReadRequest& r)
{
    // 与历史行为一致:负值在序列化时收敛为合法默认值(只影响输出,不改动请求对象)
    int64_t maxBatchRawSize = r.mMaxBatchRawSize;
    if (maxBatchRawSize < 0)
    {
        maxBatchRawSize = 4096;
    }
    int64_t skipRowNum = r.mSkipRowNum;
    if (skipRowNum < 0)
    {
        skipRowNum = 0;
    }
    j = nlohmann::json{
        {"MaxBatchRows", r.mMaxBatchRows},
        {"SkipRowNum", skipRowNum},
        {"MaxBatchRawSize", maxBatchRawSize},
        {"DataFormat", r.mDataFormat},
        {"DataColumns", r.mDataColumns},
        {"DataColumnsUnordered", r.mDataColumnsUnordered},
    };
}

inline void from_json(const nlohmann::json& j, TableReadRequest& r)
{
    r.mMaxBatchRows = j.at("MaxBatchRows").get<int64_t>();
    r.mSkipRowNum = j.at("SkipRowNum").get<int64_t>();
    r.mMaxBatchRawSize = j.at("MaxBatchRawSize").get<int64_t>();
    j.at("DataFormat").get_to(r.mDataFormat);
    r.mDataColumns = j.at("DataColumns").get<std::vector<std::string>>();
    r.mDataColumnsUnordered = j.at("DataColumnsUnordered").get<bool>();
}

TableReadStreamImpl::TableReadStreamImpl(const TableReadStreamBuilderImpl& builder)
    : mConf(builder.GetConfiguration())
    , mProject(builder.GetProject())
    , mTable(builder.GetTable())
    , mSchema(builder.GetSchema())
    , mSessionId(builder.GetSessionId())
    , mReadOptions(builder.GetReadOptions())
    , mClosed(false)
    , mEof(false)
{
    const auto& split = builder.GetSplit();
    if (split == nullptr)
    {
        TunnelThrow("Split is null");
    }
    split->Accept(*this);
}

std::shared_ptr<arrow::RecordBatch> TableReadStreamImpl::Read()
{
    if (mClosed)
    {
        TunnelThrow("TableReadStream is closed");
    }

    if (mEof)
    {
        return nullptr;
    }

    if (!mReader)
    {
        mReader = OpenReader();
    }

    auto batch = mReader->Read();

    if (!batch)
    {
        mEof = true;
        mCachedWireBytes = mReader->GetWireBytes();
        mReader.reset();
    }
    else
    {
        mCachedWireBytes = mReader->GetWireBytes();
    }

    return batch;
}

void TableReadStreamImpl::Close()
{
    mClosed = true;
}

std::shared_ptr<ArrowBatchReader> TableReadStreamImpl::OpenReader()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = CreateMaxStorageApiTableRequest(
        mConf,
        ACTION_TABLE_READ,
        mProject,
        mSchema,
        mTable);

    req->SetParameter(REQUEST_SESSION_ID, mSessionId);

    // 根据SplitMode处理不同的参数
    if (mSplitContext.mSplitMode == SplitMode::ROW_OFFSET)
    {
        if (mSplitContext.mOffset < 0)
        {
            mSplitContext.mOffset = 0;
        }
        if (mSplitContext.mCount <= 0)
        {
            mSplitContext.mCount = std::numeric_limits<uint32_t>::max(); // limited by common table
        }
        req->SetParameter(REQUEST_COUNT, std::to_string(mSplitContext.mCount));
        req->SetParameter(REQUEST_OFFSET, std::to_string(mSplitContext.mOffset));
    }
    else
    {
        req->SetParameter(REQUEST_INDEX, std::to_string(mSplitContext.mIndex));
    }

    TableReadRequest readReq;
    readReq.SetMaxBatchRows(mReadOptions.mMaxBatchRows);
    readReq.SetSkipRowNum(mReadOptions.mSkipRowNum);
    readReq.SetMaxBatchRawSize(mReadOptions.mMaxBatchRawSize);
    readReq.SetDataColumns(mReadOptions.mDataColumns);
    readReq.SetDataColumnsUnordered(mReadOptions.mDataColumnsUnordered);

    std::string requestBody = ToJsonString(readReq);
    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
    req->SetHeader(CONTENT_LENGTH, std::to_string(requestBody.size()));

    // 设置压缩选项
    req->SetHeader(ACCEPT_ENCODING, COMPRESS_LZ4_FRAME + "," + COMPRESS_ZSTD);
    req->SetBody(requestBody);

    return std::make_shared<ArrowBatchReader>(req, mConf);
}

IArrowReadStreamPtr TableReadStreamBuilderImpl::Build()
{
    return std::make_shared<TableReadStreamImpl>(*this);
}


}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara
#endif  // ODPS_SDK_ENABLE_ARROW
