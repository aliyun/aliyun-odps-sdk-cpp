#ifndef APSARA_ODPS_SDK_MAX_STORAGE_API_COMMONS_H
#define APSARA_ODPS_SDK_MAX_STORAGE_API_COMMONS_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace max_storage_api
{

const std::string PATH_MAX_STORAGE_API_V2 = "api/storage/v2";
const std::string ACTION_TABLE_PREVIEW = "TablePreview";
const std::string ACTION_VOLUME_CREATE_WRITE_STREAM = "VolumeCreateWriteStream";
const std::string ACTION_VOLUME_WRITE = "VolumeWrite";
const std::string ACTION_VOLUME_CLOSE_WRITE_STREAM = "VolumeCloseWriteStream";
const std::string ACTION_VOLUME_CREATE_READ_STREAM = "VolumeCreateReadStream";
const std::string ACTION_VOLUME_READ = "VolumeRead";
const std::string ACTION_INSTANCE_CREATE_READ_SESSION = "InstanceCreateReadSession";
const std::string ACTION_INSTANCE_GET_READ_SESSION = "InstanceGetReadSession";
const std::string ACTION_INSTANCE_READ = "InstanceRead";
const std::string ACTION_TABLE_CREATE_READ_SESSION = "TableCreateReadSession";
const std::string ACTION_TABLE_GET_READ_SESSION = "TableGetReadSession";
const std::string ACTION_TABLE_READ = "TableRead";
const std::string ACTION_VOLUME_CREATE_READ_SESSION = "VolumeCreateReadSession";
const std::string ACTION_VOLUME_GET_READ_SESSION = "VolumeGetReadSession";
const std::string ACTION_VOLUME_CREATE_WRITE_SESSION = "VolumeCreateWriteSession";
const std::string ACTION_VOLUME_GET_WRITE_SESSION = "VolumeGetWriteSession";
const std::string ACTION_VOLUME_COMMIT_WRITE_SESSION = "VolumeCommitWriteSession";
const std::string ACTION_VOLUME_ABORT_WRITE_SESSION = "VolumeAbortWriteSession";
const std::string ACTION_TABLE_CREATE_WRITE_SESSION = "TableCreateWriteSession";
const std::string ACTION_TABLE_GET_WRITE_SESSION = "TableGetWriteSession";
const std::string ACTION_TABLE_COMMIT_WRITE_SESSION = "TableCommitWriteSession";
const std::string ACTION_TABLE_ABORT_WRITE_SESSION = "TableAbortWriteSession";
const std::string ACTION_TABLE_CREATE_WRITE_STREAM = "TableCreateWriteStream";
const std::string ACTION_TABLE_GET_WRITE_STREAM = "TableGetWriteStream";
const std::string ACTION_TABLE_WRITE = "TableWrite";
const std::string ACTION_TABLE_CLOSE_WRITE_STREAM = "TableCloseWriteStream";

const std::string TARGET_PREFIX_PROJECT = "projects";
const std::string TARGET_PREFIX_VOLUMES = "volumes";
const std::string TARGET_PREFIX_SCHEMAS = "schemas";
const std::string TARGET_PREFIX_TABLES = "tables";
const std::string TARGET_PREFIX_BLOBS = "blobs";
const std::string TARGET_PREFIX_INSTANCES = "instances";

const std::string PARAM_SESSION_ID = "SessionId";
const std::string PARAM_STREAM_ID = "StreamId";
const std::string PARAM_STREAM_VERSION = "StreamVersion";

const std::string REQUEST_LIMIT = "Limit";
const std::string REQUEST_PARTITION = "Partition";
const std::string REQUEST_COLUMNS = "Columns";
const std::string REQUEST_STREAM_ID = "StreamId";
const std::string REQUEST_DOWNLOAD_ID = "DownloadID";
const std::string REQUEST_UPLOAD_ID = "UploadID";
const std::string REQUEST_SESSION_ID = "SessionId";
const std::string REQUEST_RECORD_COUNT = "RecordCount";
const std::string REQUEST_OWNER = "Owner";
const std::string REQUEST_INITIATED = "Initiated";
const std::string REQUEST_QUOTA_NAME = "QuotaName";
const std::string REQUEST_TABLE_SCHEMA = "TableSchema";
const std::string REQUEST_STATUS = "Status";
const std::string REQUEST_OFFSET = "Offset";
const std::string REQUEST_FILE = "File";
const std::string REQUEST_FILE_LENGTH = "FileLength";
const std::string REQUEST_COUNT = "Count";
const std::string REQUEST_REPLICA_COUNT = "ReplicaCount";
const std::string REQUEST_ENABLE_LIMIT = "EnableLimit";
const std::string REQUEST_IS_ARROW = "IsArrow";
const std::string REQUEST_SCHEMA_WRITE_IN_PB_STREAM = "SchemaWriteInPbStream";
const std::string REQUEST_INDEX = "Index";
const std::string REQUEST_TASK_NAME = "TaskName";
const std::string REQUEST_QUERY_ID = "QueryId";

const std::string SESSION_STATUS_INIT = "INIT";
const std::string SESSION_STATUS_INITIATING = "initiating";
const std::string SESSION_STATUS_NORMAL = "NORMAL";

const int64_t CREATE_SESSION_TIMEOUT_MS = 600 * 1000;

struct Column
{
public:
    std::string mName;
    std::string mType;
    std::string mComment;
    bool mNullable;
    std::string mColumnId;
};

inline void to_json(nlohmann::json& j, const Column& c)
{
    j = nlohmann::json{
        {"Name", c.mName},
        {"Type", c.mType},
        {"Comment", c.mComment},
        {"Nullable", c.mNullable},
    };
}

inline void from_json(const nlohmann::json& j, Column& c)
{
    c.mName = j.at("Name").get<std::string>();
    c.mType = j.at("Type").get<std::string>();
    c.mComment = j.at("Comment").get<std::string>();
    c.mNullable = j.at("Nullable").get<bool>();
}

// 数据格式描述(会话请求/响应 JSON 使用)。与 arrow 无关,
// 从 table_read_stream.h 迁移至此,保证 WITH_ARROW=OFF 时会话代码可编译。
struct DataFormat
{
public:
    std::string mType = "Arrow";
    std::string mVersion = "V5";

    DataFormat() = default;
    DataFormat(const std::string& type, const std::string& version) : mType(type), mVersion(version) {}
};

inline void to_json(nlohmann::json& j, const DataFormat& f)
{
    j = nlohmann::json{{"Type", f.mType}, {"Version", f.mVersion}};
}

inline void from_json(const nlohmann::json& j, DataFormat& f)
{
    f.mType = j.at("Type").get<std::string>();
    f.mVersion = j.at("Version").get<std::string>();
}

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif