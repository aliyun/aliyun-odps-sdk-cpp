#pragma once

#include <stdint.h>
#include <sstream>
#include <memory>
#include <functional>
#include <vector>

#include "max_storage_api.h"
#include "tunnel/odps_meta.h"
#include "tunnel/util.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/commons.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/table_write_stream.h"
#endif

namespace apsara{ namespace odps{ namespace sdk { namespace max_storage_api {

IMPLEMENT_BUILDER(TableWriteSessionBuilderImpl, ITableWriteSessionBuilder, ITableWriteSessionPtr,
    DEFINE_BUILDER_PARAM(ITableWriteSessionBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteSessionBuilder, Schema, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteSessionBuilder, Table, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteSessionBuilder, PartitionSpec, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteSessionBuilder, SessionId, std::string);
    DEFINE_BUILDER_PARAM(ITableWriteSessionBuilder, Overwrite, bool);
);

using Options = std::map<std::string, std::string>;

class TableWriteSession: public ITableWriteSession
{
public:
    TableWriteSession(const TableWriteSessionBuilderImpl& builder);

    virtual ~TableWriteSession() {}
    virtual std::string GetID() override { return mSessionId; }
    virtual std::map<std::string, int64_t> ListStream() override;

    virtual ITableWriteStreamBuilderPtr BuildWriteStream() override;
    virtual void Abort() override;
    virtual void Commit() override { Commit({}); }
    virtual void Commit(const std::map<std::string, int64_t>& streams) override;

private:
    Configuration mConf;
    std::string mProjectName;
    std::string mSchemaName;
    std::string mTableName;
    std::string mPartitionSpec;
    std::string mSessionId;
    Options mOptions;
};

}}}}
