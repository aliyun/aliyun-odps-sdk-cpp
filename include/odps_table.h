#ifndef APSARA_ODPS_TABLE_H
#define APSARA_ODPS_TABLE_H

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <memory>
#include <memory>

#include "limits.h"
#include "error_code.h"
#include "odps_common.h"
#include "odps_exception.h"
#include "odps_types.h"
#include "odps_array.h"
#include "odps_map.h"
#include "odps_struct.h"

namespace apsara{ namespace odps{ namespace sdk{

struct SortColumn
{
    std::string mColumn;
    std::string mOrder;
};

struct TableExtendedReservedInfo
{
    bool mIsTransactional = false;
    std::string mClusterType;
    int64_t mBucketNum = -1;
    std::vector<std::string> mClusterCols;
    std::vector<SortColumn> mSortCols;
    bool mHasRowAccessPolicy = false;
    std::vector<std::string> mPrimaryKey;
};

struct PartitionExtendedReservedInfo
{
    std::string mClusterType;
    int64_t mBucketNum = -1;
    std::vector<std::string> mClusterCols;
    std::vector<SortColumn> mSortCols;
};

struct ODPSPartitionBasicInfo
{
    std::vector<std::pair<std::string, std::string>> mPartitionValues;
    int64_t mCreateTime = -1;
    int64_t mLastDDLTime = -1;
    int64_t mLastModifiedTime = -1;
    int64_t mLastAccessTime = -1;
};

struct ODPSPartitionExtendedInfo
{
    bool mIsArchived = false;
    bool mIsExstore = false;
    int mLifeCycle = -1;
    int64_t mPhysicalSize = 0;
    int64_t mFileNum = 0;
    PartitionExtendedReservedInfo mReserved;
};

struct ODPSTableExtendedInfo
{
    int64_t mFileNum = 0;
    bool mIsArchived = false;
    int64_t mPhysicalSize = 0;
    TableExtendedReservedInfo mReserved;
};

class IODPSPartition
{
public:
    virtual ~IODPSPartition() {};

    virtual std::string GetProjectName() const = 0;
    virtual std::string GetTableName() const = 0;

    virtual const std::vector<std::pair<std::string, std::string>>& GetPartitionValues() const = 0;
    virtual ODPSPartitionExtendedInfo GetExtendedInfo() = 0;
    virtual std::string ToPartitionSpec() const = 0;
    virtual int64_t GetCreateTime() const = 0;
    virtual int64_t GetLastDDLTime() const = 0;
    virtual int64_t GetLastModifiedTime() const = 0;
    virtual int64_t GetLastAccessTime() const = 0;
    virtual int64_t GetPartitionSize() const = 0;
    virtual int64_t GetPartitionRecordNum() const = 0;

    virtual void Reload() = 0;
};

using IODPSPartitionPtr = std::shared_ptr<IODPSPartition>;

class IODPSTableColumn
{
public:
    virtual ~IODPSTableColumn() {};
    virtual IODPSTableColumn* Clone() const = 0;

    /**
     *	@brief return the type enumerator of this column. good for simple checks.
     *
     *	@return the type enumerator.
     */
    virtual ODPSColumnType GetType() const { return GetTypeInfo().mType; };

    /**
     *	@brief get complete type information, other than just an enumerator.
     *
     *	@return the complete type information.
     */
    virtual const ODPSColumnTypeInfo& GetTypeInfo() const = 0;

    /**
     *	@brief set the type of this column.
     */
    virtual IODPSTableColumn& SetTypeInfo(const ODPSColumnTypeInfo& typeinfo) = 0;

    /**
     *	@brief 获得Schema中对应列的列名.
     *
     *	@return 返回对应列的列名
     */
    virtual const std::string& GetName() const = 0;
    virtual IODPSTableColumn& SetName(const std::string& name) = 0;

    /**
     *	@brief 获得Schema中列的id.
     *
     *	@return 返回对应列的id
     */
    virtual uint64_t GetColumnId() const = 0;

    /**
     *	@brief 获得Schema中对应列的默认值.
     *
     *	@return 返回对应列的默认值，若无则返回空字符串
     */
    virtual const std::string& GetDefaultValue() const = 0;
    virtual bool HasDefaultValue() const = 0;
    virtual IODPSTableColumn& SetNoDefaultValue() = 0;
    virtual IODPSTableColumn& SetDefaultValue(const std::string& defaultValue) = 0;

    /**
     *	@brief get column comment on creating table.
     *	       not guarantee this can be fetched successfully (especially when using tunnel).
     *
     *	@return 返回对应列的默认值，若无则返回空字符串
     */

    virtual const std::string& GetComment() const = 0;
    virtual IODPSTableColumn& SetComment(const std::string& comment) = 0;

    virtual const std::string& GetLabel() const = 0;
    virtual IODPSTableColumn& SetLabel(const std::string& comment) = 0;

    virtual const std::vector<std::string>& GetExtendedLabels() const = 0;
    virtual IODPSTableColumn& SetExtendedLabels(const std::vector<std::string> &extendedLabels) = 0;

    virtual bool GetNullable() const = 0;
    virtual IODPSTableColumn& SetNullable(bool nullable) = 0;

    /**
     *	@brief for printing purpose. not guarantee the output format.
     */
    virtual std::string ToString() const = 0;
};
using IODPSTableColumnPtr = std::shared_ptr<IODPSTableColumn>;

/**
 *	@brief ODPS表的Schema的接口
 */
class IODPSTableSchema
{
public:
    virtual ~IODPSTableSchema() {};

    virtual IODPSTableSchema* Clone() const = 0;

    /**
     *	@brief 获得Schema的列数.
     *
     *	@return 返回Schema的列数
     */
    virtual uint32_t GetColumnCount() const = 0;

    virtual const IODPSTableColumn& GetTableColumn(uint32_t index) const = 0;

    virtual uint32_t GetPartitionLevels() const = 0;

    virtual const IODPSTableColumn& GetTablePartition(uint32_t index) const = 0;

    virtual IODPSTableSchema& AppendColumn(const IODPSTableColumn& column) = 0;
    virtual IODPSTableSchema& AppendColumns(const std::vector<IODPSTableColumnPtr> columns)
    {
        for(const auto& column: columns)
        {
            AppendColumn(*column);
        }
        return *this;
    }

    virtual IODPSTableSchema& AppendPartition(const IODPSTableColumn& partition) = 0;
    virtual IODPSTableSchema& AppendPartitions(const std::vector<IODPSTableColumnPtr> partitions)
    {
        for(const auto& partition: partitions)
        {
            AppendPartition(*partition);
        }
        return *this;
    }
    virtual std::string ToString() const = 0;

    virtual int64_t GetMaxFieldSize() const = 0;
};
using IODPSTableSchemaPtr = std::shared_ptr<IODPSTableSchema>;

class IODPSTable
{
public:
    virtual ~IODPSTable() {}


    // 表名
    virtual const std::string& GetName() = 0;
    // 所属project名
    virtual const std::string& GetProjectName() = 0;
    virtual const std::string& GetSchemaName() = 0;
    // 表的拥有者
    virtual const std::string& GetOwner() = 0;
    virtual IODPSTable& SetOwner(const std::string &owner) = 0;
    // 表的comment
    virtual const std::string& GetComment() = 0;
    // 表的id
    virtual const std::string& GetTableId() = 0;
    // 表的Record数, 若无准确数据，则返回-1
    virtual long GetRecordNum() = 0;
    // 表的生命周期值，单位:天
    virtual long GetLife() = 0;
    // 表的datahub生命周期值，单位:天
    virtual long GetHubLifecycle() = 0;
    // 内部存储大小，单位:Byte
    virtual long GetSize() = 0;
    // label
    virtual const std::string& GetTableLabel() = 0;
    // extended label
    virtual const std::vector<std::string>& GetExtendedLabel() = 0;
    // 判断表是否为外部表
    virtual bool IsExternalTable() = 0;
    // 判断表是否为虚拟视图
    virtual bool IsVirtualView() = 0;
    // 创建时间，单位:秒
    virtual long GetCreationTime() = 0;
    // 最后修改时间，单位:秒
    virtual long GetLastModifiedTime() = 0;
    // 最后一次DDL时间，单位:秒
    virtual long GetLastDDLTime() = 0;

    // 获取表Schema
    virtual IODPSTableSchemaPtr GetSchema() = 0;

    virtual std::string ToString() = 0;

    virtual std::shared_ptr<Iterator<ODPSPartitionBasicInfo>> ListPartitions(int64_t batchSize = 1000) = 0;

    // 获取表的分区名称
    virtual bool GetPartitionNames(std::vector<std::string>& partNames) = 0;

    virtual IODPSPartitionPtr GetPartition(
        const std::vector<std::pair<std::string, std::string>>& partitionValues
    ) = 0;

    virtual IODPSPartitionPtr GetPartition(const std::string& partitionSpec) = 0;


    virtual ODPSTableExtendedInfo GetExtendedInfo() = 0;
};
using IODPSTablePtr = std::shared_ptr<IODPSTable>;

// forward decl
namespace internal { namespace tunnel{

    class RecordReader;
    class Upload;
    class StreamUpload;
    class UpsertSession;
    class UpsertRecord;
} }

/**
 *	@brief ODPSTableRecord类的对象表示ODPS表中一条记录
 */
class ODPSTableRecord
{
private:
    /**
     *	@brief 构造函数.
     *
     *	@param schema 初始化记录对应的schema
     */
    ODPSTableRecord(const std::shared_ptr<IODPSTableSchema>& schema);
    friend class internal::tunnel::RecordReader;
    friend class internal::tunnel::Upload;
    friend class internal::tunnel::StreamUpload;
    friend class internal::tunnel::UpsertSession;
    friend class internal::tunnel::UpsertRecord;

public:
    /**
     *	@brief 析构函数
     */
    virtual ~ODPSTableRecord();

    /**
     *	@brief 获得记录对应的schema
     *
     *	@return 返回IODPSTableSchema类型指针
     */
    IODPSTableSchema* GetSchema();

    // get formated data
    virtual int8_t GetTinyInt(uint32_t idx) const
    {
        CheckNull(idx);
        return (int8_t)*GetTinyIntValue(idx);
    }

    virtual int16_t GetSmallInt(uint32_t idx) const
    {
        CheckNull(idx);
        return (int16_t)*GetSmallIntValue(idx);
    }

    virtual int32_t GetInteger(uint32_t idx) const
    {
        CheckNull(idx);
        return (int32_t)*GetIntegerValue(idx);
    }

    virtual int64_t GetBigInt(uint32_t idx) const
    {
        CheckNull(idx);
        return *GetBigIntValue(idx);
    }

    virtual float GetFloat(uint32_t idx) const
    {
        CheckNull(idx);
        return *GetFloatValue(idx);
    }

    virtual double GetDouble(uint32_t idx) const
    {
        CheckNull(idx);
        return *GetDoubleValue(idx);
    }

    virtual bool GetBool(uint32_t idx) const
    {
        CheckNull(idx);
        return *GetBoolValue(idx);
    }

    virtual std::string GetDatetime(uint32_t idx) const;

    std::string GetDate(uint32_t idx) const;

    IntervalYearMonth GetIntervalYearMonth(uint32_t idx) const
    {
        CheckNull(idx);
        return IntervalYearMonth(*GetIntervalYearMonthValue(idx));
    }

    virtual std::string GetString(uint32_t idx) const
    {
        CheckNull(idx);
        uint32_t len;
        const char* str = GetStringValue(idx, len);
        return std::string(str, len);
    }

    std::string GetJson(uint32_t idx) const
    {
        CheckNull(idx);
        uint32_t len;
        const char* str = GetJsonValue(idx, len);
        return std::string(str, len);
    }

    virtual std::string GetChar(uint32_t idx) const
    {
        CheckNull(idx);
        uint32_t len;
        const char* str = GetCharValue(idx, len);
        return std::string(str, len);
    }

    virtual std::string GetVarchar(uint32_t idx) const
    {
        CheckNull(idx);
        uint32_t len;
        const char* str = GetVarcharValue(idx, len);
        return std::string(str, len);
    }

    virtual std::string GetBinary(uint32_t idx) const
    {
        CheckNull(idx);
        uint32_t len;
        const char* str = GetBinaryValue(idx, len);
        return std::string(str, len);
    }

    TimeStamp GetTimestamp(uint32_t idx) const
    {
        CheckNull(idx);
        return *GetTimestampValue(idx);
    }

    TimeStamp GetTimestampNTZ(uint32_t idx) const
    {
        CheckNull(idx);
        return *GetTimestampNTZValue(idx);
    }

    IntervalDayTime GetIntervalDayTime(uint32_t idx) const
    {
        CheckNull(idx);
        const TimeStamp* ts = GetIntervalDayTimeValue(idx);
        return IntervalDayTime(ts->GetSecond(), ts->GetNano());
    }

    std::string GetDecimal(uint32_t idx) const
    {
        CheckNull(idx);
        uint32_t len;
        const char* str = GetDecimalValue(idx, len);
        return std::string(str, len);
    }

    // get raw data
    /**
     *	@brief 取得指定BIGINT列的值
     *
     *	@param idx 指定列的索引值
     *	@return 返回指定列的BIGINT值的指针
     *　如果是空则返回NULL指针
     */
    const int64_t* GetBigIntValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_BIGINT);
    }

    /**
     *  @brief 取得指定TINYINT列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的TINYINT值的指针
     *　如果是空则返回NULL指针
     */
    const int64_t* GetTinyIntValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_TINYINT);
    }

    /**
     *  @brief 取得指定SMALLINT列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的SMALLINT值的指针
     *　如果是空则返回NULL指针
     */
    const int64_t* GetSmallIntValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_SMALLINT);
    }

    /**
     *  @brief 取得指定INTEGER列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的INTEGER值的指针
     *　如果是空则返回NULL指针
     */
    const int64_t* GetIntegerValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_INTEGER);
    }

    /**
     *  @brief 取得指定INTERVAL_YEAR_MONTH列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的INTERVAL_YEAR_MONTH值的指针
     *　如果是空则返回NULL指针
     */
    const int64_t* GetIntervalYearMonthValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_INTERVAL_YEAR_MONTH);
    }

    const int64_t* GetIntValue(uint32_t idx, ODPSColumnType type) const;

    /**
     *  @brief 取得指定FLOAT列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的FLOAT值的指针
     *  如果是空则返回NULL指针
     */
    const float* GetFloatValue(uint32_t idx) const;

    /**
     *	@brief 取得指定DOUBLE列的值
     *
     *	@param idx 指定列的索引值
     *	@return 返回指定列的DOUBLE值的指针
     *	如果是空则返回NULL指针
     */
    const double* GetDoubleValue(uint32_t idx) const;

    /**
     *	@brief 获得指定BOOLEAN列的值
     *
     *	@param idx 指定列的索引值
     *	@return 返回指定列的BOOLEAN值的指针
     *	如果是空则返回NULL指针
     */
    const bool* GetBoolValue(uint32_t idx) const;

    /* return milliseconds after January 1, 1970 00:00:00. */
    /**
     *	@brief 获得指定DATETIME列的值
     *
     *	@param idx 指定列的索引值
     *	@return 返回指定列的Unix时间戳毫秒值的指针
     *	如果是空则返回NULL指针
     */
    const int64_t* GetDatetimeValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_DATETIME);
    }

    /* 与 1970-01-01 00:00:00 UTC 的偏移天数 */
    /**
     *  @brief 获得指定DATE列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的Unix时间戳毫秒值的指针
     *  如果是空则返回NULL指针
     */
    virtual const int64_t* GetDateValue(uint32_t idx) const
    {
        return GetIntValue(idx, ODPS_DATE);
    }

    /**
     *	@brief 取得指定STRING列的值
     *
     *	@param idx 指定列的索引值
     *	@param len 字符串长度
     *	@return 返回指定列的字符串的指针，如果是空则返回NULL指针
     */
    const char* GetStringValue(uint32_t idx, uint32_t &len) const
    {
        return GetStringValue(idx, len, ODPS_STRING);
    }

    /**
     *	@brief 取得指定JSON列的值
     *
     *	@param idx 指定列的索引值
     *	@param len json字符串长度
     *	@return 返回指定列的字符串的指针，如果是空则返回NULL指针
     */
    const char* GetJsonValue(uint32_t idx, uint32_t &len) const
    {
        return GetStringValue(idx, len, ODPS_JSON);
    }

    /**
     *  @brief 取得指定CHAR列的值
     *
     *  @param idx 指定列的索引值
     *  @param len 字符串长度
     *  @return 返回指定列的字符串的指针，如果是空则返回NULL指针
     */
    const char* GetCharValue(uint32_t idx, uint32_t &len) const
    {
        return GetStringValue(idx, len, ODPS_CHAR);
    }

    /**
     *  @brief 取得指定VARCHAR列的值
     *
     *  @param idx 指定列的索引值
     *  @param len 字符串长度
     *  @return 返回指定列的字符串的指针，如果是空则返回NULL指针
     */
    const char* GetVarcharValue(uint32_t idx, uint32_t &len) const
    {
        return GetStringValue(idx, len, ODPS_VARCHAR);
    }

    /**
     *  @brief 取得指定BINARY列的值
     *
     *  @param idx 指定列的索引值
     *  @param len 字符串长度
     *  @return 返回指定列的字符串的指针，如果是空则返回NULL指针
     */
    const char* GetBinaryValue(uint32_t idx, uint32_t &len) const
    {
        return GetStringValue(idx, len, ODPS_BINARY);
    }

    const char* GetStringValue(uint32_t idx, uint32_t &len, ODPSColumnType type) const;

    IODPSProtoSerializablePtr GetComplexValue(uint32_t idx, ODPSColumnType type) const;

    virtual std::shared_ptr<ODPSArray> GetArrayValue(uint32_t idx) const
    {
        return std::dynamic_pointer_cast<ODPSArray>(GetComplexValue(idx, ODPS_ARRAY));
    }

    virtual std::shared_ptr<ODPSMap> GetMapValue(uint32_t idx) const
    {
        return std::dynamic_pointer_cast<ODPSMap>(GetComplexValue(idx, ODPS_MAP));
    }

    virtual std::shared_ptr<ODPSStruct> GetStructValue(uint32_t idx) const
    {
        return std::dynamic_pointer_cast<ODPSStruct>(GetComplexValue(idx, ODPS_STRUCT));
    }

    /**
     *  @brief 取得指定TIMESTAMP列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的指针，如果是空则返回NULL指针
     */
    virtual const TimeStamp* GetTimestampValue(uint32_t idx) const
    {
        return GetTimeValue(idx, ODPS_TIMESTAMP);
    }

    /**
     *  @brief 取得指定TIMESTAMPNTZ列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的指针，如果是空则返回NULL指针
     */
    const TimeStamp* GetTimestampNTZValue(uint32_t idx) const
    {
        return GetTimeValue(idx, ODPS_TIMESTAMP_NTZ);
    }

    /**
     *  @brief 取得指定INTERVAL_DAY_TIME列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的指针，如果是空则返回NULL指针
     */
    virtual const TimeStamp* GetIntervalDayTimeValue(uint32_t idx) const
    {
        return GetTimeValue(idx, ODPS_INTERVAL_DAY_TIME);
    }

    const TimeStamp* GetTimeValue(uint32_t idx, ODPSColumnType type) const;

    /**
     *  @brief 取得指定DECIMAL列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的指针，如果是空则返回NULL指针
     */
    virtual const char* GetDecimalValue(uint32_t idx, uint32_t &len) const
    {
        return GetStringValue(idx, len, ODPS_DECIMAL);
    }

    /**
     *	@brief 设置指定BIGINT列的值
     *
     *	@param idx 指定列的索引值
     *	@param value 指定赋值内容
     */
    virtual void SetBigIntValue(uint32_t idx, int64_t value)
    {
        SetIntValue(idx, value, ODPS_BIGINT);
    }

    /**
     *  @brief 设置指定TINYINT列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetTinyIntValue(uint32_t idx, int8_t value)
    {
        SetIntValue(idx, value, ODPS_TINYINT);
    }

    /**
     *  @brief 设置指定SMALLINT列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetSmallIntValue(uint32_t idx, int16_t value)
    {
        SetIntValue(idx, value, ODPS_SMALLINT);
    }

    /**
     *  @brief 设置指定INTEGER列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetIntegerValue(uint32_t idx, int32_t value)
    {
        SetIntValue(idx, value, ODPS_INTEGER);
    }

    /**
     *  @brief 设置指定INTERVAL_YEAR_MONTH列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    void SetIntervalYearMonthValue(uint32_t idx, int64_t value)
    {
        SetIntValue(idx, value, ODPS_INTERVAL_YEAR_MONTH);
    }

    /**
     *  @brief 设置指定INTERVAL_YEAR_MONTH列的值
     *
     *  @param idx 指定列的索引值
     *  @param year 年
     *  @param month 月
     */
    void SetIntervalYearMonthValue(uint32_t idx, int64_t year, int64_t month)
    {
        SetIntervalYearMonthValue(idx, year * 12 + month);
    }

    void SetIntValue(uint32_t idx, int64_t value, ODPSColumnType type);

    /**
     *  @brief 设置指定FLOAT列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetFloatValue(uint32_t idx, float value);

    /**
     *	@brief 设置指定DOUBLE列的值
     *
     *	@param idx 指定列的索引值
     *	@param value 指定赋值内容
     */
    virtual void SetDoubleValue(uint32_t idx, double value);

    /**
     *	@brief 设置指定BOOLEAN列的值
     *
     *	@param idx 指定列的索引值
     *	@param value 指定赋值内容
     */
    virtual void SetBoolValue(uint32_t idx, bool value);

    /* value is milliseconds after January 1, 1970 00:00:00. UTC */
    /**
     *	@brief 设置指定DATETIME列的值
     *
     *	@param idx 指定列的索引值
     *	@param value 指定赋值的Unix时间戳毫秒值
     */
    virtual void SetDatetimeValue(uint32_t idx, int64_t value)
    {
        SetIntValue(idx, value, ODPS_DATETIME);
    }

    /* format: YYYY-MM-DD HH:MM:SS */
    /**
     *	@brief 设置指定DATETIME列的值
     *
     *	@param idx 指定列的索引值
     *	@param datetime 时间字符串
     */
    virtual void SetDatetimeValue(uint32_t idx, const std::string& datetime);

    /* 与 1970-01-01 的偏移天数 */
    /**
     *	@brief 设置指定DATE列的值
     *
     *	@param idx 指定列的索引值
     *	@param value 指定赋值的天数
     */
    virtual void SetDateValue(uint32_t idx, int64_t value)
    {
        SetIntValue(idx, value, ODPS_DATE);
    }

    /* format: YYYY-MM-DD */
    /**
     *	@brief 设置指定DATE列的值
     *
     *	@param idx 指定列的索引值
     *	@param date 日期字符串
     */
    virtual void SetDateValue(uint32_t idx, const std::string& date);

    /* set string value, value is c style string with '\0' at end.*/
    /**
     *	@brief 设置指定STRING列的值
     *
     *	@param idx 指定列的索引值
     *	@param value 指定赋值内容
     *	@param len 数据长度
     */
    virtual void SetStringValue(uint32_t idx, const char* value, uint32_t len)
    {
        SetStringValue(idx, value, len, ODPS_STRING);
    }

    void SetJsonValue(uint32_t idx, const char* value, uint32_t len)
    {
        SetStringValue(idx, value, len, ODPS_JSON);
    }

    /* set string value, value is c style string with '\0' at end.*/
    /**
     *	@brief 设置指定STRING列的值
     *
     *	@param idx 指定列的索引值
     *	@param str 指定赋值内容
     */
    virtual void SetStringValue(uint32_t idx, const std::string& str)
    {
        SetStringValue(idx, str.c_str(), str.size());
    }

    /**
     *	@brief 设置指定JSON列的值
     *
     *	@param idx 指定列的索引值
     *	@param str 指定赋值内容
     */
    void SetJsonValue(uint32_t idx, const std::string& str)
    {
        SetJsonValue(idx, str.c_str(), str.size());
    }

    /* set string value, value is c style string with '\0' at end.*/
    /**
     *  @brief 设置指定CHAR列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetCharValue(uint32_t idx, const char* value, uint32_t len)
    {
        SetStringValue(idx, value, len, ODPS_CHAR);
    }

    /* set string value, value is c style string with '\0' at end.*/
    /**
     *  @brief 设置指定CHAR列的值
     *
     *  @param idx 指定列的索引值
     *  @param str 指定赋值内容
     */
    virtual void SetCharValue(uint32_t idx, const std::string& str)
    {
        SetCharValue(idx, str.c_str(), str.size());
    }

    /* set string value, value is c style string with '\0' at end.*/
    /**
     *  @brief 设置指定VARCHAR列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetVarcharValue(uint32_t idx, const char* value, uint32_t len)
    {
        SetStringValue(idx, value, len, ODPS_VARCHAR);
    }

    /* set string value, value is c style string with '\0' at end.*/
    /**
     *  @brief 设置指定VARCHAR列的值
     *
     *  @param idx 指定列的索引值
     *  @param str 指定赋值内容
     */
    virtual void SetVarcharValue(uint32_t idx, const std::string& str)
    {
        SetVarcharValue(idx, str.c_str(), str.size());
    }

    /**
     *  @brief 设置指定BINARY列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetBinaryValue(uint32_t idx, const char* value, uint32_t len)
    {
        SetStringValue(idx, value, len, ODPS_BINARY);
    }

    void SetStringValue(uint32_t idx, const char* value, uint32_t len, ODPSColumnType type);

    void SetComplexValue(uint32_t idx, IODPSProtoSerializablePtr value, ODPSColumnType type);

    virtual void SetArrayValue(uint32_t idx, std::shared_ptr<ODPSArray> value)
    {
        SetComplexValue(idx, value, ODPS_ARRAY);
    }

    virtual void SetMapValue(uint32_t idx, std::shared_ptr<ODPSMap> value)
    {
        SetComplexValue(idx, value, ODPS_MAP);
    }

    virtual void SetStructValue(uint32_t idx, std::shared_ptr<ODPSStruct> value)
    {
        SetComplexValue(idx, value, ODPS_STRUCT);
    }

    /**
     *  @brief 设置指定TIMESTAMP列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetTimestampValue(uint32_t idx, int64_t sec, int32_t ns)
    {
        SetTimeValue(idx, sec, ns, ODPS_TIMESTAMP);
    }

    void SetTimestampNTZValue(uint32_t idx, int64_t sec, int32_t ns)
    {
        SetTimeValue(idx, sec, ns, ODPS_TIMESTAMP_NTZ);
    }

    /**
     *  @brief 设置指定INTERVALDAYTIME列的值
     *
     *  @param idx 指定列的索引值
     *  @param value 指定赋值内容
     */
    virtual void SetIntervalDayTimeValue(uint32_t idx, int64_t sec, int32_t ns)
    {
        SetTimeValue(idx, sec, ns, ODPS_INTERVAL_DAY_TIME);
    }

    void SetTimeValue(uint32_t idx, int64_t sec, int32_t ns, ODPSColumnType type);

    /**
     *  @brief 取得指定DECIMAL列的值
     *
     *  @param idx 指定列的索引值
     *  @return 返回指定列的指针，如果是空则返回NULL指针
     */
    virtual void SetDecimalValue(uint32_t idx, const std::string& dec)
    {
        SetStringValue(idx, dec.c_str(), dec.size(), ODPS_DECIMAL);
    }

    /**
     *	@brief 设置指定值为NULL
     *
     *	@param idx 指定列的索引值
     */
    virtual void SetNullValue(uint32_t idx);

    /* column value is null.*/
    /**
     *	@brief 判断指定列是否为NULL
     *
     *	@param idx 指定列的索引值
     *	@return 返回判断结果
     */
    virtual bool IsNullValue(uint32_t idx) const;

    /**
     *	@brief 获得 Record 的逻辑大小。
     */
    virtual int64_t GetRecordSize() const;

private:
    void CheckType(uint32_t idx, ODPSColumnType type) const;
    void CheckNull(uint32_t idx) const
    {
        if (IsNullValue(idx))
        {
            throw OdpsException(INTERNAL_ERROR, "Accessing null value");
        }
    }

private:
    //do not support copy & assgin
    ODPSTableRecord(const ODPSTableRecord&);

    ODPSTableRecord& operator=(const ODPSTableRecord&);

    std::shared_ptr<IODPSTableSchema> mSchema;
    unsigned char* mRawData;
    uint32_t mColCount;
    std::map<uint32_t, std::string> mStrColBufs;
    std::map<uint32_t, TimeStamp> mTsColBufs;
    std::map<uint32_t, IODPSProtoSerializablePtr> mComplexColBufs;
    std::vector<bool> mIsNullValue;
};

typedef std::shared_ptr<ODPSTableRecord> ODPSTableRecordPtr;

}}}
#endif
