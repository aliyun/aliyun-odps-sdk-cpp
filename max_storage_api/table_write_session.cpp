#include "common/logging.h"

#include <vector>
#include <map>
#include "tunnel/odps_meta.h"
#include "common/http_flags.h"
#include "common/http_connection.h"
#include "common/json_serialize.h"
#include "max_storage_api/table_write_session.h"
#include "max_storage_api/request_handler.h"
#include "include/odps_exception.h"
#include "tunnel/util.h"
#include "util/utils.h"

using namespace std;
using namespace apsara::odps::sdk::logging;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

namespace apsara { namespace odps { namespace sdk { namespace max_storage_api {

struct EmptyJsonObject
{
};

inline void to_json(nlohmann::json& j, const EmptyJsonObject&)
{
    j = nlohmann::json::object();
}

inline void from_json(const nlohmann::json&, EmptyJsonObject&)
{
}

struct TableCreateWriteSessionRequest
{
    std::string mPartialPartitionSpec;

    std::map<std::string, std::string> mFlags;
};

inline void to_json(nlohmann::json& j, const TableCreateWriteSessionRequest& r)
{
    j = nlohmann::json{
        {"PartialPartitionSpec", r.mPartialPartitionSpec},
        {"Flags", r.mFlags},
    };
}

inline void from_json(const nlohmann::json& j, TableCreateWriteSessionRequest& r)
{
    r.mPartialPartitionSpec = j.at("PartialPartitionSpec").get<std::string>();
    r.mFlags = j.value("Flags", std::map<std::string, std::string>());
}

struct TableCreateWriteSessionResponse
{
    std::string mWarningMessage;
    std::string mSessionId;
};

inline void to_json(nlohmann::json& j, const TableCreateWriteSessionResponse& r)
{
    j = nlohmann::json{
        {"WarningMessage", r.mWarningMessage},
        {"SessionId", r.mSessionId},
    };
}

inline void from_json(const nlohmann::json& j, TableCreateWriteSessionResponse& r)
{
    r.mWarningMessage = j.value("WarningMessage", "");
    r.mSessionId = j.at("SessionId").get<std::string>();
}

struct TableGetWriteSessionResponse
{
    std::string mWarningMessage;
    std::map<std::string, int64_t> mStreams;
};

inline void to_json(nlohmann::json& j, const TableGetWriteSessionResponse& r)
{
    j = nlohmann::json{
        {"WarningMessage", r.mWarningMessage},
        {"Streams", r.mStreams},
    };
}

inline void from_json(const nlohmann::json& j, TableGetWriteSessionResponse& r)
{
    r.mWarningMessage = j.at("WarningMessage").get<std::string>();
    j.at("Streams").get_to(r.mStreams);
}

struct TableCommitWriteResponse
{
    std::string mWarningMessage;
};

inline void to_json(nlohmann::json& j, const TableCommitWriteResponse& r)
{
    j = nlohmann::json{{"WarningMessage", r.mWarningMessage}};
}

inline void from_json(const nlohmann::json& j, TableCommitWriteResponse& r)
{
    r.mWarningMessage = j.value("WarningMessage", "");
}

ITableWriteSessionPtr TableWriteSessionBuilderImpl::Build()
{
    return std::make_shared<TableWriteSession>(*this);
}

TableWriteSession::TableWriteSession(const TableWriteSessionBuilderImpl& builder)
    : mConf(builder.GetConfiguration()),
      mProjectName(builder.GetProject()),
      mSchemaName(builder.GetSchema()),
      mTableName(builder.GetTable()),
      mPartitionSpec(builder.GetPartitionSpec()),
      mSessionId(builder.GetSessionId())
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProjectName);
    }

    if (builder.GetOverwrite())
    {
        mOptions["overwrite"] = "true";
    }

    if (mSessionId.empty())
    {
        TableRequestHandler handler(
            mConf,
            ACTION_TABLE_CREATE_WRITE_SESSION,
            mProjectName,
            mSchemaName,
            mTableName);

        TableCreateWriteSessionRequest request;
        request.mPartialPartitionSpec = mPartitionSpec;
        request.mFlags = mOptions;
        TableCreateWriteSessionResponse response;
        handler.HandleRequest(request, response);
        mSessionId = response.mSessionId;
    }
}

ITableWriteStreamBuilderPtr TableWriteSession::BuildWriteStream()
{
#ifdef ODPS_SDK_ENABLE_ARROW
    auto builder = std::make_shared<TableWriteStreamBuilderImpl>(mConf);
    builder->SetProject(mProjectName)
            .SetSchemaName(mSchemaName)
            .SetTable(mTableName)
            .SetSessionId(mSessionId);
    return builder;
#else
    throw OdpsException("TableWriteStream requires arrow support, rebuild with WITH_ARROW=ON");
#endif
}

std::map<std::string, int64_t> TableWriteSession::ListStream()
{
    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_GET_WRITE_SESSION,
        mProjectName,
        mSchemaName,
        mTableName);

    handler.SetRequestParameter(PARAM_SESSION_ID, mSessionId);

    TableGetWriteSessionResponse response;
    handler.HandleRequest(EmptyJsonObject(), response);
    return response.mStreams;
}

void TableWriteSession::Abort()
{
    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_ABORT_WRITE_SESSION,
        mProjectName,
        mSchemaName,
        mTableName);

    handler.SetRequestParameter(PARAM_SESSION_ID, mSessionId);

    EmptyJsonObject _;
    handler.HandleRequest(EmptyJsonObject(), _);
}

void TableWriteSession::Commit(const std::map<std::string, int64_t>& streams)
{
    if (!streams.empty())
    {
        auto list = ListStream();
        if (list != streams)
        {
            util::TunnelThrow("Stream list mismatch: expect " + util::ToJsonCompactString(list) + " provided " + util::ToJsonCompactString(streams));
        }
    }

    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_COMMIT_WRITE_SESSION,
        mProjectName,
        mSchemaName,
        mTableName);

    handler.SetRequestParameter(PARAM_SESSION_ID, mSessionId);

    TableCommitWriteResponse response;
    handler.HandleRequest(EmptyJsonObject(), response);
}

}}}}
