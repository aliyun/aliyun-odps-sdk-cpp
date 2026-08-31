#ifndef APSARA_ODPS_MAX_STORAGE_API_PREVIEW_STREAM_H
#define APSARA_ODPS_MAX_STORAGE_API_PREVIEW_STREAM_H
#ifdef ODPS_SDK_ENABLE_ARROW

#include <arrow/api.h>
#include <arrow/ipc/reader.h>

#include "common/http_connection.h"
#include "include/max_storage_api.h"
#include "max_storage_api/arrow_read_stream.h"
#include "max_storage_api/common_macro_define.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace max_storage_api
{

IMPLEMENT_BUILDER(TablePreviewStreamBuilderImpl, ITablePreviewStreamBuilder, IArrowReadStreamPtr,
    DEFINE_BUILDER_PARAM(ITablePreviewStreamBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(ITablePreviewStreamBuilder, Schema, std::string);
    DEFINE_BUILDER_PARAM(ITablePreviewStreamBuilder, Table, std::string);
    DEFINE_BUILDER_PARAM(ITablePreviewStreamBuilder, Partition, std::string);
    DEFINE_BUILDER_PARAM(ITablePreviewStreamBuilder, Columns, std::vector<std::string>);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(ITablePreviewStreamBuilder, Limit, int64_t, 0);
);

class TablePreviewStreamImpl : public ArrowReadStreamImpl
{
public:
    TablePreviewStreamImpl(
        const Configuration& conf,
        const TablePreviewStreamBuilderImpl& builder);

public:
    virtual void Close() override;

private:
    virtual std::shared_ptr<ArrowBatchReader> OpenReader() override;

private:
    Configuration mConf;
    std::string mProject;
    std::string mTable;
    std::string mSchema;
    std::string mPartition;
    std::vector<std::string> mColumns;
    int64_t mLimit;
};

typedef std::shared_ptr<TablePreviewStreamImpl> TablePreviewStreamPtr;

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // ODPS_SDK_ENABLE_ARROW
#endif