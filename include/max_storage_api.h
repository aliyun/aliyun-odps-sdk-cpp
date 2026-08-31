#ifndef APSARA_ODPS_SDK_MAX_STORAGE_API_H
#define APSARA_ODPS_SDK_MAX_STORAGE_API_H

#ifdef ODPS_SDK_ENABLE_ARROW
#include <arrow/api.h>
#else
// 未开启 arrow (WITH_ARROW=OFF) 时不引入 arrow 头文件。接口中的
// arrow 类型仅以指针/引用形式出现,前置声明即可让本头文件继续可编译;
// 对应的 arrow 流实现在该构建模式下不存在。
namespace arrow {
class RecordBatch;
class Schema;
}
#endif

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "odps_table.h"
#include "configuration.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

enum class IncrementalReadMode {
    APPEND,
    CDC
};

enum class TimeUnit {
    SECOND,
    MILLI,
    MICRO,
    NANO
};

// ArrowOptions structure
struct ArrowOptions
{
    TimeUnit mTimestampUnit = TimeUnit::NANO;
    TimeUnit mDatetimeUnit = TimeUnit::MILLI;
};

struct IncrementalReadOptions
{
    bool mEnableIncrementalRead = false;
    int64_t mStartVersion = -1;
    int64_t mEndVersion = -1;
    std::string mStartTimeStamp = "-1";
    std::string mEndTimeStamp = "-1";
    IncrementalReadMode mMode = IncrementalReadMode::APPEND;
};

struct IncrementalInfo
{
    IncrementalInfo() = default;
    IncrementalInfo(const IncrementalReadOptions& options) : mOptions(options) {}
    IncrementalReadOptions mOptions;
    int64_t mLatestVersion = -1;
};

// ReadOptions structure
struct ReadOptions
{
    int64_t mMaxBatchRows = 4096; // max value 20000
    int64_t mSkipRowNum = 0;
    int64_t mMaxBatchRawSize = 0; // 0 means no limit, min value 1MB, max value 256MB
    std::vector<std::string> mDataColumns;
    bool mDataColumnsUnordered = false;
};

// 谓词类前置声明
class IPredicate;
typedef std::shared_ptr<IPredicate> IPredicatePtr;

/**
 * @brief 谓词基类，用于过滤下推
 */
class IPredicate
{
public:
    /**
     * @brief 谓词类型枚举
     */
    enum class PredicateType
    {
        BINARY,      // 二元谓词
        UNARY,       // 一元谓词
        COMPOUND,    // 复合谓词
        IN,          // IN谓词
        CONSTANT,    // 常量
        ATTRIBUTE,   // 属性（列名）
        RAW          // 原始字符串
    };

    static const IPredicatePtr NO_PREDICATE;

    virtual ~IPredicate() = default;
    virtual PredicateType GetType() const { return mType; }
    virtual std::string ToString() const = 0;

protected:
    PredicateType mType;
    IPredicate(PredicateType type) : mType(type) {}
};

/**
 * @brief 属性谓词（列引用）
 */
class IAttribute : public IPredicate
{
public:
    static std::shared_ptr<IAttribute> Of(const std::string& value);
    virtual std::string GetValue() const = 0;

protected:
    IAttribute() : IPredicate(PredicateType::ATTRIBUTE) {}
};

/**
 * @brief 常量谓词（字面值）
 */
class IConstant : public IPredicate
{
public:
    static std::shared_ptr<IConstant> Of(const std::string& value);
    virtual std::string GetValue() const = 0;

protected:
    IConstant() : IPredicate(PredicateType::CONSTANT) {}
};

/**
 * @brief 原始谓词（未处理的字符串）
 */
class IRawPredicate : public IPredicate
{
public:
    static std::shared_ptr<IRawPredicate> Of(const std::string& rawExpr);
    virtual std::string GetRawExpr() const = 0;

protected:
    IRawPredicate() : IPredicate(PredicateType::RAW) {}
};

/**
 * @brief 二元谓词，支持各种操作符
 */
class IBinaryPredicate : public IPredicate
{
public:
    /**
     * @brief 操作符枚举
     */
    enum class Operator
    {
        EQUALS,                 // 等于
        NOT_EQUALS,             // 不等于
        GREATER_THAN,           // 大于
        LESS_THAN,              // 小于
        GREATER_THAN_OR_EQUAL,  // 大于等于
        LESS_THAN_OR_EQUAL      // 小于等于
    };

    static std::shared_ptr<IBinaryPredicate> Equals(const std::string& leftOperand, const std::string& rightOperand);
    static std::shared_ptr<IBinaryPredicate> NotEquals(const std::string& leftOperand, const std::string& rightOperand);
    static std::shared_ptr<IBinaryPredicate> GreaterThan(const std::string& leftOperand, const std::string& rightOperand);
    static std::shared_ptr<IBinaryPredicate> LessThan(const std::string& leftOperand, const std::string& rightOperand);
    static std::shared_ptr<IBinaryPredicate> GreaterThanOrEqual(const std::string& leftOperand, const std::string& rightOperand);
    static std::shared_ptr<IBinaryPredicate> LessThanOrEqual(const std::string& leftOperand, const std::string& rightOperand);

    virtual Operator GetOperator() const = 0;
    virtual std::string GetLeftOperand() const = 0;
    virtual std::string GetRightOperand() const = 0;

protected:
    IBinaryPredicate() : IPredicate(PredicateType::BINARY) {}
};

/**
 * @brief 一元谓词，支持IS NULL和IS NOT NULL操作符
 */
class IUnaryPredicate : public IPredicate
{
public:
    /**
     * @brief 操作符枚举
     */
    enum class Operator
    {
        IS_NULL,   // IS NULL
        NOT_NULL   // IS NOT NULL
    };

    static std::shared_ptr<IUnaryPredicate> IsNull(const std::string& operand);
    static std::shared_ptr<IUnaryPredicate> NotNull(const std::string& operand);

    virtual Operator GetOperator() const = 0;
    virtual std::string GetOperand() const = 0;

protected:
    IUnaryPredicate() : IPredicate(PredicateType::UNARY) {}
};

/**
 * @brief 复合谓词，支持逻辑操作符
 */
class ICompoundPredicate : public IPredicate
{
public:
    /**
     * @brief 逻辑操作符枚举
     */
    enum class Operator
    {
        AND,  // 与
        OR,   // 或
        NOT   // 非
    };

    static std::shared_ptr<ICompoundPredicate> And(const std::vector<IPredicatePtr>& predicates);
    static std::shared_ptr<ICompoundPredicate> Or(const std::vector<IPredicatePtr>& predicates);
    static std::shared_ptr<ICompoundPredicate> Not(const IPredicatePtr& predicate);

    virtual Operator GetOperator() const = 0;
    virtual const std::vector<IPredicatePtr>& GetPredicates() const = 0;
    virtual void AddPredicate(const IPredicatePtr& predicate) = 0;

protected:
    ICompoundPredicate() : IPredicate(PredicateType::COMPOUND) {}
};

/**
 * @brief IN谓词，用于集合成员检查
 */
class IInPredicate : public IPredicate
{
public:
    /**
     * @brief 操作符枚举
     */
    enum class Operator
    {
        IN,     // IN
        NOT_IN  // NOT IN
    };

    static std::shared_ptr<IInPredicate> In(const std::string& operand, const std::vector<std::string>& set);
    static std::shared_ptr<IInPredicate> NotIn(const std::string& operand, const std::vector<std::string>& set);

    virtual Operator GetOperator() const = 0;
    virtual std::string GetOperand() const = 0;
    virtual const std::vector<std::string>& GetSet() const = 0;

protected:
    IInPredicate() : IPredicate(PredicateType::IN) {}
};

// FilterOptions structure
struct FilterOptions
{
    std::vector<std::string> mRequiredDataColumns;
    std::vector<std::string> mRequiredPartitionColumns;
    std::vector<std::string> mRequiredPartitions;
    std::vector<int32_t> mRequiredBucketIds;
    IPredicatePtr mPredicate;
};

// SplitMode枚举
enum class SplitMode
{
    ROW_OFFSET,
    SIZE,
    BUCKET
};

// SplitOptions structure
struct SplitOptions
{
    SplitMode mSplitMode = SplitMode::ROW_OFFSET;
    int64_t mSplitSize = 0; // Size/模式下使用
    bool mCrossPartition = false; // 是否跨分区
    int64_t mMaxFileNum = 0; // 单Split最多文件数量
};

class ISplitVisitor;
class ISplit
{
public:
    virtual void Accept(ISplitVisitor& visitor) = 0;
    virtual ~ISplit() = default;
};
typedef std::shared_ptr<ISplit> ISplitPtr;

class ISplits
{
public:
    virtual SplitMode GetSplitMode() const = 0;
    virtual int64_t GetRecordCount() const = 0;
    virtual int32_t GetSplitCount() const = 0;
    virtual std::shared_ptr<ISplit> GetSplit(int32_t index) const = 0;
    virtual std::shared_ptr<ISplit> GetSplit(int64_t offset, int64_t count) const = 0;
    virtual ~ISplits() = default;
};
typedef std::shared_ptr<ISplits> ISplitsPtr;

/**
 * @brief TableReadStream
 */
class IArrowReadStream
{
public:
    virtual std::shared_ptr<arrow::RecordBatch> Read() = 0;
    virtual void Close() = 0;
    virtual int64_t GetWireBytes() const { return 0; }
    virtual ~IArrowReadStream() = default;
};
typedef std::shared_ptr<IArrowReadStream> IArrowReadStreamPtr;

class ITableReadStreamBuilder
{
public:
    // 设置Split
    virtual ITableReadStreamBuilder& SetSplit(const ISplitPtr& split) = 0;

    // 设置ReadOptions
    virtual ITableReadStreamBuilder& SetReadOptions(const ReadOptions& readOptions) = 0;

    virtual IArrowReadStreamPtr Build() = 0;
    virtual ~ITableReadStreamBuilder() = default;
};
typedef std::shared_ptr<ITableReadStreamBuilder> ITableReadStreamBuilderPtr;

/**
 * @brief TableReadSession
 */
class ITableReadSession
{
public:
    virtual ITableReadStreamBuilderPtr BuildTableReadStream() = 0;
    virtual std::string GetSessionId() = 0;
    virtual ISplitsPtr GetSplits() = 0;
    virtual IncrementalInfo GetIncrementalInfo() = 0;
    virtual ~ITableReadSession() = default;
};
typedef std::shared_ptr<ITableReadSession> ITableReadSessionPtr;

class ITableReadSessionBuilder
{
public:
    virtual ITableReadSessionBuilder& SetProject(const std::string& project) = 0;
    virtual ITableReadSessionBuilder& SetTable(const std::string& table) = 0;
    virtual ITableReadSessionBuilder& SetSchema(const std::string& schema) = 0;
    virtual ITableReadSessionBuilder& SetSessionId(const std::string& sessionId) = 0;

    // Structured options
    virtual ITableReadSessionBuilder& SetSplitOptions(const SplitOptions& splitOptions) = 0;
    virtual ITableReadSessionBuilder& SetArrowOptions(const ArrowOptions& arrowOptions) = 0;
    virtual ITableReadSessionBuilder& SetIncrementalReadOptions(const IncrementalReadOptions& incrementalReadOptions) = 0;
    virtual ITableReadSessionBuilder& SetFilterOptions(const FilterOptions& filterOptions) = 0;

    virtual ITableReadSessionPtr Build() = 0;
    virtual ~ITableReadSessionBuilder() = default;
};
typedef std::shared_ptr<ITableReadSessionBuilder> ITableReadSessionBuilderPtr;

struct WriteOptions
{
};

class IArrowWriteStream
{
public:
    virtual ~IArrowWriteStream(){}
    virtual void Write(const arrow::RecordBatch& r) = 0;
    virtual void Close() = 0;
    virtual const IODPSTableSchema& GetSchema() const = 0;
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() const = 0;
    virtual int64_t GetWireBytes() const { return 0; }
};

using IArrowWriteStreamPtr = std::shared_ptr<IArrowWriteStream>;

class ITableWriteStreamBuilder
{
public:
    virtual ITableWriteStreamBuilder& SetStreamId(const std::string& streamId) = 0;
    virtual ITableWriteStreamBuilder& SetStreamVersion(int64_t streamVersion) = 0;
    virtual ITableWriteStreamBuilder& SetResume(bool resume) = 0;
    virtual ITableWriteStreamBuilder& SetWriteOptions(const WriteOptions& options) = 0;
    virtual IArrowWriteStreamPtr Build() = 0;
    virtual ~ITableWriteStreamBuilder() = default;
};
typedef std::shared_ptr<ITableWriteStreamBuilder> ITableWriteStreamBuilderPtr;

class ITableWriteSession
{
public:
    virtual ~ITableWriteSession() {}
    virtual std::string GetID() = 0;
    virtual ITableWriteStreamBuilderPtr BuildWriteStream() = 0;
    virtual void Commit() = 0;
    virtual void Commit(const std::map<std::string, int64_t>& streams) = 0;
    virtual std::map<std::string, int64_t> ListStream() = 0;
    virtual void Abort() = 0;
};

using ITableWriteSessionPtr = std::shared_ptr<ITableWriteSession>;

class ITableWriteSessionBuilder
{
public:
    virtual ITableWriteSessionBuilder& SetProject(const std::string& project) = 0;
    virtual ITableWriteSessionBuilder& SetSchema(const std::string& schema) = 0;
    virtual ITableWriteSessionBuilder& SetTable(const std::string& table) = 0;
    // partition spec example: pt1=a/pt2=b, only support '/' as partition delimiter
    virtual ITableWriteSessionBuilder& SetPartitionSpec(const std::string& partitionSpec) = 0;
    virtual ITableWriteSessionBuilder& SetSessionId(const std::string& sessionId) = 0;
    virtual ITableWriteSessionBuilder& SetOverwrite(bool overwrite) = 0;
    virtual ITableWriteSessionPtr Build() = 0;
    virtual ~ITableWriteSessionBuilder() = default;
};
typedef std::shared_ptr<ITableWriteSessionBuilder> ITableWriteSessionBuilderPtr;


/**
 * @brief InstanceRead
 */
class IInstanceDirectReadStreamBuilder
{
public:
    virtual IInstanceDirectReadStreamBuilder& SetProject(const std::string& project) = 0;
    virtual IInstanceDirectReadStreamBuilder& SetInstance(const std::string& instance) = 0;
    virtual IInstanceDirectReadStreamBuilder& SetTaskName(const std::string& taskName) = 0;
    virtual IInstanceDirectReadStreamBuilder& SetQueryId(int64_t queryId) = 0;
    virtual IInstanceDirectReadStreamBuilder& SetOffset(int64_t offset) = 0;
    virtual IInstanceDirectReadStreamBuilder& SetCount(int64_t count) = 0;
    virtual IInstanceDirectReadStreamBuilder& SetEnableLimit(bool enableLimit) = 0;
    virtual IArrowReadStreamPtr Build() = 0;
    virtual ~IInstanceDirectReadStreamBuilder() = default;
};
typedef std::shared_ptr<IInstanceDirectReadStreamBuilder> IInstanceDirectReadStreamBuilderPtr;

class IInstanceReadStreamBuilder
{
public:
    virtual IInstanceReadStreamBuilder& SetSplit(const ISplitPtr& split) = 0;
    virtual IInstanceReadStreamBuilder& SetColumns(const std::vector<std::string>& columns) = 0;
    virtual IArrowReadStreamPtr Build() = 0;
    virtual ~IInstanceReadStreamBuilder() = default;
};
typedef std::shared_ptr<IInstanceReadStreamBuilder> IInstanceReadStreamBuilderPtr;

class IInstanceReadSession
{
public:
    virtual IInstanceReadStreamBuilderPtr BuildInstanceReadStream() = 0;
    virtual std::string GetSessionId() = 0;
    virtual ISplitsPtr GetSplits() = 0;
    virtual ~IInstanceReadSession() = default;
};

typedef std::shared_ptr<IInstanceReadSession> IInstanceReadSessionPtr;

class IInstanceReadSessionBuilder
{
public:
    virtual IInstanceReadSessionBuilder& SetProject(const std::string& project) = 0;
    virtual IInstanceReadSessionBuilder& SetInstance(const std::string& instance) = 0;
    virtual IInstanceReadSessionBuilder& SetEnableLimit(bool enableLimit) = 0;
    virtual IInstanceReadSessionBuilder& SetSessionId(const std::string& id) = 0;
    virtual IInstanceReadSessionPtr Build() = 0;
    virtual ~IInstanceReadSessionBuilder() = default;
};

typedef std::shared_ptr<IInstanceReadSessionBuilder> IInstanceReadSessionBuilderPtr;

/**
 * @brief VolumeWrite
 */

class IVolumeWriteStream
{
public:
    virtual void Write(const char* buf, int64_t len) = 0;
    virtual void Close() = 0;
    virtual ~IVolumeWriteStream() = default;
};

typedef std::shared_ptr<IVolumeWriteStream> IVolumeWriteStreamPtr;

class IVolumeFSWriteStreamBuilder
{
public:
    virtual IVolumeFSWriteStreamBuilder& SetProject(const std::string& project) = 0;
    virtual IVolumeFSWriteStreamBuilder& SetVolume(const std::string& volume) = 0;
    virtual IVolumeFSWriteStreamBuilder& SetPath(const std::string& path) = 0;
    virtual IVolumeFSWriteStreamBuilder& SetReplicaCount(int64_t replicaCount) = 0;
    virtual IVolumeWriteStreamPtr Build() = 0;
    virtual ~IVolumeFSWriteStreamBuilder() = default;
};
typedef std::shared_ptr<IVolumeFSWriteStreamBuilder> IVolumeFSWriteStreamBuilderPtr;

class IVolumeWriteStreamBuilder
{
public:
    virtual IVolumeWriteStreamBuilder& SetFile(const std::string& file) = 0;
    virtual IVolumeWriteStreamPtr Build() = 0;
    virtual ~IVolumeWriteStreamBuilder() = default;
};
typedef std::shared_ptr<IVolumeWriteStreamBuilder> IVolumeWriteStreamBuilderPtr;

class IVolumeWriteSession
{
public:
    virtual IVolumeWriteStreamBuilderPtr BuildWriteStream() = 0;
    virtual void Commit() = 0;
    virtual void Abort() = 0;
    virtual std::string GetSessionId() = 0;
    virtual std::string GetStatus() = 0;
    virtual ~IVolumeWriteSession() = default;
};

typedef std::shared_ptr<IVolumeWriteSession> IVolumeWriteSessionPtr;

class IVolumeWriteSessionBuilder
{
public:
    virtual IVolumeWriteSessionBuilder& SetProject(const std::string& project) = 0;
    virtual IVolumeWriteSessionBuilder& SetVolume(const std::string& volume) = 0;
    virtual IVolumeWriteSessionBuilder& SetPartition(const std::string& partition) = 0;
    virtual IVolumeWriteSessionBuilder& SetSessionId(const std::string& sessionId) = 0;
    virtual IVolumeWriteSessionPtr Build() = 0;
};

typedef std::shared_ptr<IVolumeWriteSessionBuilder> IVolumeWriteSessionBuilderPtr;

/**
 * @brief VolumeRead
 */

class IVolumeReadStream
{
public:
    virtual int64_t Read(char* buf, int64_t len) = 0;
    virtual void Close() = 0;
    virtual ~IVolumeReadStream() = default;
};

typedef std::shared_ptr<IVolumeReadStream> IVolumeReadStreamPtr;

class IVolumeFSReadStreamBuilder
{
public:
    virtual IVolumeFSReadStreamBuilder& SetProject(const std::string& project) = 0;
    virtual IVolumeFSReadStreamBuilder& SetVolume(const std::string& volume) = 0;
    virtual IVolumeFSReadStreamBuilder& SetPath(const std::string& path) = 0;
    virtual IVolumeFSReadStreamBuilder& SetOffset(int64_t offset) = 0;
    virtual IVolumeFSReadStreamBuilder& SetCount(int64_t count) = 0;
    virtual IVolumeReadStreamPtr Build() = 0;
    virtual ~IVolumeFSReadStreamBuilder() = default;
};
typedef std::shared_ptr<IVolumeFSReadStreamBuilder> IVolumeFSReadStreamBuilderPtr;

class IVolumeReadStreamBuilder
{
public:
    virtual IVolumeReadStreamBuilder& SetOffset(int64_t offset) = 0;
    virtual IVolumeReadStreamBuilder& SetCount(int64_t count) = 0;
    virtual IVolumeReadStreamPtr Build() = 0;
    virtual ~IVolumeReadStreamBuilder() = default;
};
typedef std::shared_ptr<IVolumeReadStreamBuilder> IVolumeReadStreamBuilderPtr;

class IVolumeReadSession
{
public:
    virtual IVolumeReadStreamBuilderPtr BuildVolumeReadStream() = 0;
    virtual std::string GetSessionId() = 0;
    virtual std::string GetStatus() = 0;
    virtual ~IVolumeReadSession() = default;
};

typedef std::shared_ptr<IVolumeReadSession> IVolumeReadSessionPtr;

class IVolumeReadSessionBuilder
{
public:
    virtual IVolumeReadSessionBuilder& SetProject(const std::string& project) = 0;
    virtual IVolumeReadSessionBuilder& SetVolume(const std::string& volume) = 0;
    virtual IVolumeReadSessionBuilder& SetPartition(const std::string& partition) = 0;
    virtual IVolumeReadSessionBuilder& SetFile(const std::string& file) = 0;
    virtual IVolumeReadSessionBuilder& SetSessionId(const std::string& sessionId) = 0;
    virtual IVolumeReadSessionPtr Build() = 0;
};

typedef std::shared_ptr<IVolumeReadSessionBuilder> IVolumeReadSessionBuilderPtr;

/**
 * @brief TablePreview
 */
class ITablePreviewStreamBuilder
{
public:
    virtual ITablePreviewStreamBuilder& SetProject(const std::string& project) = 0;
    virtual ITablePreviewStreamBuilder& SetTable(const std::string& table) = 0;
    virtual ITablePreviewStreamBuilder& SetSchema(const std::string& schema) = 0;
    virtual ITablePreviewStreamBuilder& SetPartition(const std::string& partition) = 0;
    virtual ITablePreviewStreamBuilder& SetColumns(const std::vector<std::string>& columns) = 0;
    virtual ITablePreviewStreamBuilder& SetLimit(int64_t limit) = 0;
    virtual IArrowReadStreamPtr Build() = 0;
    virtual ~ITablePreviewStreamBuilder() = default;
};

typedef std::shared_ptr<ITablePreviewStreamBuilder> ITablePreviewStreamBuilderPtr;

/**
 * @brief MaxStorage API 主入口类，类似于OdpsTunnel
 */
class MaxStorageApi
{
public:
    /**
     * @brief 使用Configuration类来初始化MaxStorageApi
     *
     * @param conf 配置信息
     */
    void Init(const Configuration& conf) { this->mConf = conf; }

    /**
     * @brief 创建 VolumeWriteStream 构建器
     * @return VolumeFSWriteStreamBuilder 对象
     */
    IVolumeFSWriteStreamBuilderPtr BuildVolumeFSWriteStream();

    /**
     * @brief 创建 VolumeReadStream 构建器
     * @return VolumeFSReadStreamBuilder 对象
     */
    IVolumeFSReadStreamBuilderPtr BuildVolumeFSReadStream();

    /**
     * @brief 创建 Preview 构建器
     * @return PreviewBuilder 对象
     */
    ITablePreviewStreamBuilderPtr BuildTablePreviewStream();
    /**
     * @brief 创建 InstanceSession 构建器
     * @return InstanceSessionBuilder 对象
     */
    IInstanceReadSessionBuilderPtr BuildInstanceReadSession();
    /**
     * @brief 创建 InstanceStream 构建器
     * @return InstanceStreamBuilder 对象
     */
    IInstanceDirectReadStreamBuilderPtr BuildInstanceDirectReadStream();
    /**
     * @brief 创建 TableReadSession 构建器
     * @return TableReadSessionBuilder 对象
     */
    ITableReadSessionBuilderPtr BuildTableReadSession();
    /**
     * @brief 创建 TableWriteSession 构建器
     * @return TableWriteSessionBuilder 对象
     */
    ITableWriteSessionBuilderPtr BuildTableWriteSession();
    /**
     * @brief 创建 VolumeReadSession 构建器
     * @return VolumeReadSessionBuilder 对象
     */
    IVolumeReadSessionBuilderPtr BuildVolumeReadSession();
    /**
     * @brief 创建 VolumeWriteSession 构建器
     * @return VolumeWriteSessionBuilder 对象
     */
    IVolumeWriteSessionBuilderPtr BuildVolumeWriteSession();

private:
    Configuration mConf;
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
