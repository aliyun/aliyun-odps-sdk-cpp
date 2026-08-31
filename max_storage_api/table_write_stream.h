#pragma once

#ifdef ODPS_SDK_ENABLE_ARROW

#include <stdint.h>
#include <sstream>
#include <memory>
#include <functional>
#include <vector>

#include "max_storage_api.h"
#include "tunnel/arrow_meta_helper.h"
#include "tunnel/odps_meta.h"
#include "tunnel/util.h"
#include "tunnel/serialize.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/commons.h"
#include "max_storage_api/table_schema.h"
namespace apsara{ namespace odps{ namespace sdk { namespace max_storage_api {

IMPLEMENT_BUILDER(TableWriteStreamBuilderImpl, ITableWriteStreamBuilder, IArrowWriteStreamPtr,
    DEFINE_BUILDER_PARAM(TableWriteStreamBuilderImpl, Project, std::string);
    DEFINE_BUILDER_PARAM(TableWriteStreamBuilderImpl, Table, std::string);
    DEFINE_BUILDER_PARAM(TableWriteStreamBuilderImpl, SchemaName, std::string);
    DEFINE_BUILDER_PARAM(TableWriteStreamBuilderImpl, SessionId, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteStreamBuilder, StreamId, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteStreamBuilder, StreamVersion, int64_t);
    DEFINE_BUILDER_PARAM(ITableWriteStreamBuilder, Resume, bool);
    DEFINE_BUILDER_PARAM(ITableWriteStreamBuilder, WriteOptions, WriteOptions);
);

class TableWriteStream : public IArrowWriteStream
{
public:
    TableWriteStream(const TableWriteStreamBuilderImpl& builder);

    virtual ~TableWriteStream() {}
    void Write(const arrow::RecordBatch& r) override;
    void Close() override;
    const IODPSTableSchema& GetSchema() const override { return mTableSchema; }
    std::shared_ptr<arrow::Schema> GetArrowSchema() const override { return mArrowSchema; }
    int64_t GetWireBytes() const override { return mWireBytes; }

private:
    void CreateWriteStream();
    void GetWriteStream();
    void CloseWriteStream();

    template<typename ResponseType>
    void Load(const ResponseType& response)
    {
        mTableSchema = response.mTableSchema;
        mArrowSchema = internal::tunnel::SqlSchemaToArrowSchema(mTableSchema);
    }

private:
    Configuration mConf;
    std::string mProject;
    std::string mSchemaName;
    std::string mTable;
    std::string mSessionId;
    std::string mStreamId;
    int64_t mStreamVersion;
    TableSchema mTableSchema;
    WriteOptions mWriteOptions;
    std::shared_ptr<arrow::Schema> mArrowSchema;
    std::string mStatus;
    std::string mToken;
    CompressOption mCompressOption = CompressOption::ZSTD_COMPRESS;
    int64_t mWireBytes = 0;
};

}}}}

#endif  // ODPS_SDK_ENABLE_ARROW
