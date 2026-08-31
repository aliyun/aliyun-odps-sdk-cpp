#ifndef APSARA_ODPS_MAX_STORAGE_API_TABLE_READ_STREAM_H
#define APSARA_ODPS_MAX_STORAGE_API_TABLE_READ_STREAM_H
#ifdef ODPS_SDK_ENABLE_ARROW

#include <memory>
#include "include/max_storage_api.h"
#include "max_storage_api/arrow_read_stream.h"
#include "max_storage_api/arrow_reader.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/split.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace max_storage_api
{

IMPLEMENT_BUILDER(TableReadStreamBuilderImpl, ITableReadStreamBuilder, IArrowReadStreamPtr,
    DEFINE_BUILDER_PARAM(TableReadStreamBuilderImpl, Project, std::string);
    DEFINE_BUILDER_PARAM(TableReadStreamBuilderImpl, Table, std::string);
    DEFINE_BUILDER_PARAM(TableReadStreamBuilderImpl, Schema, std::string);
    DEFINE_BUILDER_PARAM(TableReadStreamBuilderImpl, SessionId, std::string);
    DEFINE_BUILDER_PARAM(ITableReadStreamBuilder, Split, ISplitPtr);
    DEFINE_BUILDER_PARAM(ITableReadStreamBuilder, ReadOptions, ReadOptions);
);

class TableReadStreamImpl : public IArrowReadStream, public ISplitVisitor
{
public:
    TableReadStreamImpl(const TableReadStreamBuilderImpl& builder);
    ~TableReadStreamImpl() = default;

public:
    virtual std::shared_ptr<arrow::RecordBatch> Read() override;
    virtual void Close() override;
    virtual int64_t GetWireBytes() const override { return mCachedWireBytes; }

public:
    virtual void Visit(const SplitContext& context) override
    {
        mSplitContext = context;
    }

private:
    std::shared_ptr<ArrowBatchReader> OpenReader();

private:
    Configuration mConf;
    std::string mProject;
    std::string mTable;
    std::string mSchema;
    std::string mSessionId;
    SplitContext mSplitContext;
    ReadOptions mReadOptions;

    std::shared_ptr<ArrowBatchReader> mReader;
    bool mClosed;
    bool mEof;
    int64_t mCachedWireBytes = 0;
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // ODPS_SDK_ENABLE_ARROW
#endif