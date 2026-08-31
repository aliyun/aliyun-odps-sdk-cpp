#ifdef ODPS_SDK_ENABLE_ARROW
#include "common/logging.h"

#include <vector>
#include <map>
#include "tunnel/odps_meta.h"
#include "common/http_flags.h"
#include "common/http_connection.h"
#include "common/json_serialize.h"
#include "max_storage_api/table_write_stream.h"
#include "max_storage_api/request_handler.h"
#include "max_storage_api/table_schema.h"
#include "arrow/ipc/api.h"
#include "arrow/ipc/writer.h"
#include "arrow/io/memory.h"
#include "util/utils.h"
#include "tunnel/util.h"
#include "tunnel/zstd_stream.h"

using namespace std;
using namespace apsara::odps::sdk::logging;
using namespace apsara::odps::sdk::internal;

namespace apsara { namespace odps { namespace sdk { namespace max_storage_api {

#if ARROW_VERSION_MAJOR >= 13
inline std::shared_ptr<arrow::util::Codec> CreateCodecOrThrow(
    arrow::Compression::type type,
    int level,
    const std::string& errorPrefix)
{
    auto res = arrow::util::Codec::Create(type, level);
    if (!res.ok())
    {
        util::TunnelThrow(errorPrefix + res.status().ToString());
    }

    return std::move(*res);
}
#endif

struct TableCreateWriteStreamRequest
{
    std::string mStreamId;
    int64_t mStreamVersion;
};

inline void to_json(nlohmann::json& j, const TableCreateWriteStreamRequest& r)
{
    j = nlohmann::json{
        {"StreamId", r.mStreamId},
        {"StreamVersion", r.mStreamVersion},
    };
}

inline void from_json(const nlohmann::json& j, TableCreateWriteStreamRequest& r)
{
    r.mStreamId = j.at("StreamId").get<std::string>();
    r.mStreamVersion = j.at("StreamVersion").get<int64_t>();
}

struct TableCreateWriteStreamResponse
{
    TableSchema mTableSchema;
    bool mIsDeltaTable = false;
};

inline void to_json(nlohmann::json& j, const TableCreateWriteStreamResponse& r)
{
    j = nlohmann::json{
        {"TableSchema", r.mTableSchema},
        {"IsDeltaTable", r.mIsDeltaTable},
    };
}

inline void from_json(const nlohmann::json& j, TableCreateWriteStreamResponse& r)
{
    j.at("TableSchema").get_to(r.mTableSchema);
    r.mIsDeltaTable = j.value("IsDeltaTable", false);
}

struct TableGetWriteStreamRequest
{
    std::string mStreamId;
    int64_t mStreamVersion;
};

inline void to_json(nlohmann::json& j, const TableGetWriteStreamRequest& r)
{
    j = nlohmann::json{
        {"StreamId", r.mStreamId},
        {"StreamVersion", r.mStreamVersion},
    };
}

inline void from_json(const nlohmann::json& j, TableGetWriteStreamRequest& r)
{
    r.mStreamId = j.at("StreamId").get<std::string>();
    r.mStreamVersion = j.at("StreamVersion").get<int64_t>();
}

struct TableGetWriteStreamResponse
{
    std::string mWarningMessage;
    TableSchema mTableSchema;
    bool mIsDeltaTable = false;
};

inline void to_json(nlohmann::json& j, const TableGetWriteStreamResponse& r)
{
    j = nlohmann::json{
        {"WarningMessage", r.mWarningMessage},
        {"TableSchema", r.mTableSchema},
        {"IsDeltaTable", r.mIsDeltaTable},
    };
}

inline void from_json(const nlohmann::json& j, TableGetWriteStreamResponse& r)
{
    r.mWarningMessage = j.at("WarningMessage").get<std::string>();
    j.at("TableSchema").get_to(r.mTableSchema);
    r.mIsDeltaTable = j.value("IsDeltaTable", false);
}

struct TableWriteStreamResponse
{
    std::string mWarningMessage;
    // TODO: add metrics
};

inline void to_json(nlohmann::json& j, const TableWriteStreamResponse& r)
{
    j = nlohmann::json{{"WarningMessage", r.mWarningMessage}};
}

inline void from_json(const nlohmann::json& j, TableWriteStreamResponse& r)
{
    r.mWarningMessage = j.value("WarningMessage", "");
}

struct TableCloseWriteStreamRequest
{
    std::string mSessionId;
    std::string mStreamId;
    int64_t mStreamVersion;
    std::map<std::string, std::string> mFlags;
};

inline void to_json(nlohmann::json& j, const TableCloseWriteStreamRequest& r)
{
    j = nlohmann::json{
        {"SessionId", r.mSessionId},
        {"StreamId", r.mStreamId},
        {"StreamVersion", r.mStreamVersion},
        {"Flags", r.mFlags},
    };
}

inline void from_json(const nlohmann::json& j, TableCloseWriteStreamRequest& r)
{
    r.mSessionId = j.at("SessionId").get<std::string>();
    r.mStreamId = j.at("StreamId").get<std::string>();
    r.mStreamVersion = j.at("StreamVersion").get<int64_t>();
    j.at("Flags").get_to(r.mFlags);
}

struct TableCloseWriteStreamResponse
{
    std::string mWarningMessage;
};

inline void to_json(nlohmann::json& j, const TableCloseWriteStreamResponse& r)
{
    j = nlohmann::json{{"WarningMessage", r.mWarningMessage}};
}

inline void from_json(const nlohmann::json& j, TableCloseWriteStreamResponse& r)
{
    r.mWarningMessage = j.value("WarningMessage", "");
}

IArrowWriteStreamPtr TableWriteStreamBuilderImpl::Build()
{
    return std::make_shared<TableWriteStream>(*this);
}

TableWriteStream::TableWriteStream(const TableWriteStreamBuilderImpl& builder):
    mConf(builder.GetConfiguration()),
    mProject(builder.GetProject()),
    mSchemaName(builder.GetSchemaName()),
    mTable(builder.GetTable()),
    mSessionId(builder.GetSessionId()),
    mStreamId(builder.GetStreamId()),
    mStreamVersion(builder.GetStreamVersion()),
    mWriteOptions(builder.GetWriteOptions())
{
    if (builder.GetResume())
    {
        GetWriteStream();
    }
    else
    {
        CreateWriteStream();
    }
}

void TableWriteStream::Write(const arrow::RecordBatch& r)
{
    auto bfs = arrow::io::BufferOutputStream::Create().ValueUnsafe();
    auto&& options = arrow::ipc::IpcWriteOptions::Defaults();
    options.use_threads = false;
    if (mCompressOption.algorithm == CompressOption::ODPS_ZSTD)
    {
#if ARROW_VERSION_MAJOR >= 13
        options.codec = CreateCodecOrThrow(arrow::Compression::ZSTD, mCompressOption.level, "Failed to create ZSTD codec: ");
#else
        options.compression = arrow::Compression::ZSTD;
        options.compression_level = mCompressOption.level;
#endif
    }
    else if (mCompressOption.algorithm == CompressOption::ODPS_LZ4_FRAME)
    {
#if ARROW_VERSION_MAJOR >= 13
        options.codec = CreateCodecOrThrow(arrow::Compression::LZ4_FRAME, mCompressOption.level, "Failed to create LZ4-Frame codec: ");
#else
        options.compression = arrow::Compression::LZ4_FRAME;
#endif
    }
    else if (mCompressOption.algorithm != CompressOption::ODPS_RAW)
    {
        util::TunnelThrow("Unsupported compression algorithm. only support zstd & lz4_frame");
    }

    options.memory_pool = arrow::system_memory_pool();
    {
#if ARROW_VERSION_MAJOR >= 10
        auto writer = arrow::ipc::MakeStreamWriter(bfs.get(), mArrowSchema, options);
#else
        auto writer = arrow::ipc::NewStreamWriter(bfs.get(), mArrowSchema, options);
#endif
        if (!writer.ok())
        {
            util::TunnelThrow("SerializationException", "Cannot create rb writer:", writer.status().ToString());
        }
        auto status = writer.ValueUnsafe()->WriteRecordBatch(r);
        if (!status.ok())
        {
            util::TunnelThrow("SerializationException", "Cannot ser rb:", status.ToString());
        }
        status = writer.ValueUnsafe()->Close();
        if (!status.ok())
        {
            util::TunnelThrow("SerializationException", "Cannot ser rb:", status.ToString());
        }
    }

    auto buffer = bfs->Finish();

    if (!buffer.ok())
    {
        util::TunnelThrow("SerializationException", "Cannot gen buffer:", buffer.status().ToString());
    }

    auto buf = buffer.ValueUnsafe();

    mWireBytes += buf->size();

    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_WRITE,
        mProject,
        mSchemaName,
        mTable);

    handler.SetRequestParameter(PARAM_SESSION_ID, mSessionId);
    handler.SetRequestParameter(PARAM_STREAM_ID, mStreamId);
    handler.SetRequestParameter(PARAM_STREAM_VERSION, std::to_string(mStreamVersion));
    handler.SetRequestParameter(REQUEST_COUNT, std::to_string(r.num_rows()));
    handler.SetRequestHeader(HEADER_ODPS_MAX_STORAGE_ROUTE_TOKEN, mToken);

    TableWriteStreamResponse response;
    handler.HandleRequest(reinterpret_cast<const char*>(buf->data()), buf->size(), response);
    mToken = handler.GetToken();
}

void TableWriteStream::Close()
{
    CloseWriteStream();
}

void TableWriteStream::CreateWriteStream()
{
    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_CREATE_WRITE_STREAM,
        mProject,
        mSchemaName,
        mTable);

    handler.SetRequestParameter("SessionId", mSessionId);

    TableCreateWriteStreamRequest request;
    request.mStreamId = mStreamId;
    request.mStreamVersion = mStreamVersion;
    TableCreateWriteStreamResponse response;
    handler.HandleRequest(request, response);
    Load(response);
    mToken = handler.GetToken();
    mCompressOption = tunnel::SelectCompressOption(handler.GetResponseHeader(ACCEPT_ENCODING), {CompressOption::ODPS_LZ4_FRAME, CompressOption::ODPS_ZSTD});
}

void TableWriteStream::GetWriteStream()
{
    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_GET_WRITE_STREAM,
        mProject,
        mSchemaName,
        mTable);

    handler.SetRequestParameter(PARAM_SESSION_ID, mSessionId);

    TableGetWriteStreamRequest request;
    request.mStreamId = mStreamId;
    request.mStreamVersion = mStreamVersion;
    TableGetWriteStreamResponse response;
    handler.HandleRequest(request, response);
    Load(response);
    mToken = handler.GetToken();
}

void TableWriteStream::CloseWriteStream()
{
    TableRequestHandler handler(
        mConf,
        ACTION_TABLE_CLOSE_WRITE_STREAM,
        mProject,
        mSchemaName,
        mTable);

    handler.SetRequestParameter(PARAM_SESSION_ID, mSessionId);
    handler.SetRequestHeader(HEADER_ODPS_MAX_STORAGE_ROUTE_TOKEN, mToken);

    TableCloseWriteStreamRequest request;
    request.mStreamId = mStreamId;
    request.mStreamVersion = mStreamVersion;
    TableCloseWriteStreamResponse response;
    handler.HandleRequest(request, response);
}

}}}}
#endif  // ODPS_SDK_ENABLE_ARROW
