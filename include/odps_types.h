#ifndef APSARA_ODPS_TYPES_H
#define APSARA_ODPS_TYPES_H

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <memory>

#include "limits.h"
#include "odps_exception.h"

namespace apsara{ namespace odps{ namespace sdk{

/**
 *	@brief ODPS表支持的数据类型
 */
enum ODPSColumnType
{
    ODPS_UNKNOWN = -1,
    ODPS_BIGINT = 0,	/**< 8字节有符号整型*/
    ODPS_DOUBLE = 1,	/**< 8字节双精度浮点型*/
    ODPS_BOOLEAN = 2,	/**< 布尔型*/
    ODPS_DATETIME = 3,	/**< 日期类型，取值范围是0001-01-01 00:00:00 ~ 9999-12-31 23:59:59*/
    ODPS_STRING = 4,	/**< 字符串类型*/
    ODPS_DECIMAL = 5,
    ODPS_TINYINT = 6,
    ODPS_SMALLINT = 7,
    ODPS_INTEGER = 8,
    ODPS_CHAR = 9,
    ODPS_VARCHAR = 10,
    ODPS_BINARY = 11,
    ODPS_DATE = 12,
    ODPS_TIMESTAMP = 13,
    ODPS_FLOAT = 14,
    ODPS_INTERVAL_YEAR_MONTH = 15,
    ODPS_INTERVAL_DAY_TIME = 16,
    ODPS_ARRAY = 17,
    ODPS_MAP = 18,
    ODPS_STRUCT = 19,
    ODPS_JSON = 20,
    ODPS_TIMESTAMP_NTZ = 21
};

/**
 *	@brief 复杂类型的额外信息
 */
struct ODPSColumnTypeInfo
{
    ODPSColumnType mType = ODPS_BIGINT;
    // for decimal
    bool IsLegacyDecimal() const { return mType == ODPS_DECIMAL && mPrecision == 54 && mScale == 18; }
    int32_t mPrecision = 0;
    int32_t mScale = 0;
    // for char & varchar
    int32_t mSpecifiedLength = 0;
    // for struct members
    std::string mMemberName;
    // for complex types
    std::vector<ODPSColumnTypeInfo> mSubTypes;
    // nullable
    bool mNullable = false;
    // cid
    uint64_t mColumnId = 0;
    // default value
    std::string mDefaultValue;
    bool mHasDefaultValue = false;

    ODPSColumnTypeInfo() {}
    ODPSColumnTypeInfo(const ODPSColumnTypeInfo&) = default;
    ODPSColumnTypeInfo(ODPSColumnType type): mType(type) {}
    ODPSColumnTypeInfo(ODPSColumnType type, int32_t specifiedLength):
        mType(type), mSpecifiedLength(specifiedLength) {}
    ODPSColumnTypeInfo(ODPSColumnType type, const std::string& memberName):
        mType(type), mMemberName(memberName) {}
    ODPSColumnTypeInfo(ODPSColumnType type, int32_t precision, int32_t scale):
        mType(type), mPrecision(precision), mScale(scale) {}
    static ODPSColumnTypeInfo ParseTypeInfoString(const std::string& infoString);
    std::string ToTypeString() const;
private:
    // good for recursive parsing
    ODPSColumnTypeInfo(std::vector<std::string>::const_iterator& begin,
        std::vector<std::string>::const_iterator end);
};

class TimeStamp
{
public:
    TimeStamp() : second(0), nano(0) {}
    TimeStamp(int64_t sec, int32_t ns = 0);

    int64_t GetSecond() const { return second; }
    int32_t GetNano() const { return nano; }
    bool operator==(const TimeStamp& another) const
    {
        return second == another.second && nano == another.nano;
    }
    bool operator<(const TimeStamp& another) const
    {
        if (second > another.second)
        {
            return false;
        }
        return second < another.second || nano < another.nano;
    }

    virtual std::string ToString() const;

private:
    int64_t second;
    int32_t nano;
};

class IntervalDayTime : public TimeStamp
{
public:
    IntervalDayTime() : TimeStamp() {}
    IntervalDayTime(int64_t sec) : TimeStamp(sec) {}
    IntervalDayTime(int64_t sec, int32_t ns) : TimeStamp(sec, ns) {}

    /* override */
    virtual std::string ToString() const;
};

class IntervalYearMonth
{
public:
    IntervalYearMonth() : months(0) {}
    IntervalYearMonth(int64_t total) : months(total) {}
    IntervalYearMonth(int64_t y, int64_t m) : months(y * 12 + m) {}

    int64_t GetYears() const { return months / 12; }
    int64_t GetMonths() const { return months % 12; }

    std::string ToString() const;

private:
    int64_t months;
};

/**
 *	@brief 可序列化复杂类型的基类
 */
class IODPSProtoSerializable
{
public:
    virtual const ODPSColumnTypeInfo& GetTypeInfo() const = 0;
    // returns the in-memory size of this object. best-effort accurate.
    virtual int64_t GetInMemorySize() const { return 0; }
    virtual ~IODPSProtoSerializable() {}
};

using IODPSProtoSerializablePtr = std::shared_ptr<IODPSProtoSerializable>;

// forward declarations
class ODPSArray;
class ODPSMap;
class ODPSStruct;

static inline std::string GetTypeName(ODPSColumnType type)
{
    switch(type)
    {
        case ODPS_BIGINT:
            return "BIGINT";
        case ODPS_DOUBLE:
            return "DOUBLE";
        case ODPS_BOOLEAN:
            return "BOOLEAN";
        case ODPS_DATETIME:
            return "DATETIME";
        case ODPS_STRING:
            return "STRING";
        case ODPS_DECIMAL:
            return "DECIMAL";
        case ODPS_TINYINT:
            return "TINYINT";
        case ODPS_SMALLINT:
            return "SMALLINT";
        case ODPS_INTEGER:
            return "INT";
        case ODPS_CHAR:
            return "CHAR";
        case ODPS_VARCHAR:
            return "VARCHAR";
        case ODPS_BINARY:
            return "BINARY";
        case ODPS_DATE:
            return "DATE";
        case ODPS_TIMESTAMP:
            return "TIMESTAMP";
        case ODPS_TIMESTAMP_NTZ:
            return "TIMESTAMP_NTZ";
        case ODPS_FLOAT:
            return "FLOAT";
        case ODPS_INTERVAL_YEAR_MONTH:
            return "INTERVAL_YEAR_MONTH";
        case ODPS_INTERVAL_DAY_TIME:
            return "INTERVAL_DAY_TIME";
        case ODPS_MAP:
            return "MAP";
        case ODPS_ARRAY:
            return "ARRAY";
        case ODPS_STRUCT:
            return "STRUCT";
        case ODPS_JSON:
            return "JSON";
        default:
            throw OdpsException("UnknownColumnType", "Unknown column type:" + std::to_string(type));
    }
}

}}}

#endif
