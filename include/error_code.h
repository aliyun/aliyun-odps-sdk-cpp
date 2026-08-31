#ifndef APSARA_ODPS_ERRORCODE_H
#define APSARA_ODPS_ERRORCODE_H
#include <string>

namespace apsara { namespace odps { namespace sdk {

/**
 * 拒绝访问。
 */
const std::string ACCESS_DENIED = "AccessDenied";

/**
 * 参数格式错误。
 */
const std::string INVALID_ARGUMENT = "InvalidArgument";

/**
 * Access ID不存在。
 */
const std::string INVALID_ACCESS_KEY_ID = "InvalidAccessKeyId";

/**
 * 无效的 Block ID。
 */
const std::string INVALID_BLOCK_ID = "InvalidBlockId";

/**
 * 无效的 URI。
 */
const std::string INVALID_URI = "InvalidURI";

/**
 * 无效的 RowRange。
 */
const std::string INVALID_ROWRANGE = "InvalidRowRange";

/**
 * 无效的 Partition描述。
 */
const std::string INVALID_PARTITION_SPEC = "InvalidPartitionSpec";

/**
 * Configuration 内部发生错误。
 */
const std::string INTERNAL_ERROR = "InternalError";

/**
 * 缺少内容长度。
 */
const std::string MISSING_CONTENT_LENGTH = "MissingContentLength";

/**
 * Table 不存在。
 */
const std::string NO_SUCH_TABLE = "NoSuchTable";


/**
 * requested object is not exist(could be table or some other stuff)
 */
const std::string NO_SUCH_OBJECT= "NoSuchObject";

/**
 * 无法处理的方法。
 */
const std::string NOT_IMPLEMENTED = "NotImplemented";

/**
 * 预处理错误。
 */
const std::string PRECONDITION_FAILED = "PreconditionFailed";

/**
 * 发起请求的时间和服务器时间超出15分钟。
 */
const std::string REQUEST_TIME_TOO_SKEWED = "RequestTimeTooSkewed";

/**
 * 请求超时。
 */
const std::string REQUEST_TIMEOUT = "RequestTimeout";

/**
 * 连接错误。
 */
const std::string CONNECTION_ERROR = "ConnectionError";

/**
 * 签名错误。
 */
const std::string SIGNATURE_DOES_NOT_MATCH = "SignatureDoesNotMatch";

/**
 * 连接数超quota限制。
 */
const std::string FLOW_EXCEEDED = "FlowExceeded";

/**
 * 数据序列化或反序列化错误。
 */
const std::string MALFORMED_DATA_STREAM = "MalformedDataStream";

/**
 * 上传或者下载过程中数据源发生变化。
 */
const std::string TABLE_MODIFIED = "TableModified";

/**
 * 路由信息为空
 */
const std::string EMPTY_SLOT_MAP = "EmptySlotMap";

/**
 * Upload对象已经释放
 */
const std::string UPLOAD_CLOSED = "UploadIsClosed";

/**
 * UpsertStream对象已经释放
 */
const std::string UPSERT_STREAM_CLOSED = "UpsertStreamIsClosed";

/**
 * UpsertStream状态为error
 */
const std::string UPSERT_STREAM_ERROR = "UpsertStreamError";

/**
 * Padk对象没有成功flush
 */
const std::string PACK_NOT_FLUSHED = "PackNotFlushed";

/**
 * 路由信息为空
 */
const std::string SLOT_NOT_FOUND = "SlotNotFound";

/**
 * 桶数值不合法
 */
const std::string INVALID_BUCKET_VALUE = "InvalidBucketValue";

/**
 * 桶数值不合法
 */
const std::string STATUS_ERROR = "StatusError";

/**
 * 数据类型错误、不符合列约束或存储过程错误
 */
const std::string DATA_STORE_ERROR = "DataStoreError";

const std::string ALREADY_WRITTEN = "AlreadyWritten";

const std::string LOCAL_ERROR = "LocalError";

const std::string ARROW_ERROR = "ArrowError";

}}}
#endif
