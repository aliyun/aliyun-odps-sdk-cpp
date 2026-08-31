#include "odps_table.h"
#include <unordered_map>
#include "error_code.h"
#include "odps_exception.h"

#include "util/utils.h"
using namespace apsara::odps::sdk::util;

using namespace std;


namespace apsara { namespace odps { namespace sdk {

ODPSArray::ODPSArray(const ODPSColumnTypeInfo& typeInfo):
    mTypeInfo(typeInfo)
{
    if (mTypeInfo.mType != ODPS_ARRAY ||
        mTypeInfo.mSubTypes.size() != 1)
    {
        throw OdpsException("Creating ODPSArray with non-array type");
    }
}

ODPSColumnType ODPSArray::GetElementType() const
{
    return mTypeInfo.mSubTypes[0].mType;
}

void ODPSArray::TypeCheck(ODPSColumnType opType) const
{
    if (opType != mTypeInfo.mSubTypes[0].mType)
    {
        throw OdpsException("Incompatible types for ODPSArray operations: array is " +
            GetTypeName(GetElementType()) + ", but operation require " + GetTypeName(opType));
    }
}

ODPSArray::~ODPSArray()
{

}

int64_t ODPSArray::GetInMemorySize() const
{
    int64_t sum = 0;
    sum += mNullArray.size() * sizeof(decltype(mNullArray)::value_type);

    ODPSColumnTypeInfo elemType = mTypeInfo.mSubTypes.at(0);
    switch(elemType.mType)
    {
        case ODPS_BIGINT:
        case ODPS_DATETIME:
        case ODPS_DATE:
        case ODPS_INTERVAL_YEAR_MONTH:
            sum += Size() * sizeof(int64_t);
        break;
        case ODPS_DOUBLE:
            sum += Size() * sizeof(double);
        break;
        case ODPS_BOOLEAN:
            sum += Size() * sizeof(bool);
        break;
        case ODPS_STRING:
        case ODPS_CHAR:
        case ODPS_VARCHAR:
        case ODPS_BINARY:
        case ODPS_DECIMAL:
            {
                for(uint32_t i = 0; i < Size(); i++)
                {
                    if (IsNull(i))
                    {
                        continue;
                    }
                    sum += Access<std::string>(i, elemType.mType).size();
                }
            }
        break;
        case ODPS_TINYINT:
            sum += Size() * sizeof(int8_t);
        break;
        case ODPS_SMALLINT:
            sum += Size() * sizeof(int16_t);
        break;
        case ODPS_INTEGER:
            sum += Size() * sizeof(int32_t);
        break;
        case ODPS_TIMESTAMP:
        case ODPS_TIMESTAMP_NTZ:
        case ODPS_INTERVAL_DAY_TIME:
            sum += Size() * sizeof(TimeStamp);
        break;
        case ODPS_FLOAT:
            sum += Size() * sizeof(float);
        break;
        case ODPS_ARRAY:
        case ODPS_MAP:
        case ODPS_STRUCT:
            {
                for(uint32_t i = 0; i < Size(); i++)
                {
                    if (IsNull(i))
                    {
                        continue;
                    }
                    sum += AccessPtr<IODPSProtoSerializable>(i, elemType.mType)->GetInMemorySize();
                }
            }
        break;
        default:
            throw OdpsException("Unimplemented size calculation for type: " + GetTypeName(elemType.mType));
    }
    return sum;
}

const ODPSColumnTypeInfo& ODPSArray::GetTypeInfo() const
{
    return mTypeInfo;
}

int64_t ODPSArray::Size() const
{
    return mArray.size();
}
#undef IFSIZE

int8_t ODPSArray::GetTinyInt(uint32_t idx) const
{
    return Access<int8_t>(idx, ODPS_TINYINT);
}

int16_t ODPSArray::GetSmallInt(uint32_t idx) const
{
    return Access<int16_t>(idx, ODPS_SMALLINT);
}

int32_t ODPSArray::GetInteger(uint32_t idx) const
{
    return Access<int32_t>(idx, ODPS_INTEGER);
}

int64_t ODPSArray::GetBigInt(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_BIGINT);
}

float ODPSArray::GetFloat(uint32_t idx) const
{
    return Access<float>(idx, ODPS_FLOAT);
}

double ODPSArray::GetDouble(uint32_t idx) const
{
    return Access<double>(idx, ODPS_DOUBLE);
}

bool ODPSArray::GetBool(uint32_t idx) const
{
    return Access<bool>(idx, ODPS_BOOLEAN);
}

int64_t ODPSArray::GetDatetimeValue(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_DATETIME);
}

int64_t ODPSArray::GetDateValue(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_DATE);
}

int64_t ODPSArray::GetIntervalYearMonthValue(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_INTERVAL_YEAR_MONTH);
}

std::string ODPSArray::GetString(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_STRING);
}

std::string ODPSArray::GetChar(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_CHAR);
}

std::string ODPSArray::GetVarchar(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_VARCHAR);
}

std::string ODPSArray::GetBinary(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_BINARY);
}

TimeStamp ODPSArray::GetTimestamp(uint32_t idx) const
{
    return Access<TimeStamp>(idx, ODPS_TIMESTAMP);
}

TimeStamp ODPSArray::GetTimestampNTZ(uint32_t idx) const
{
    return Access<TimeStamp>(idx, ODPS_TIMESTAMP_NTZ);
}

TimeStamp ODPSArray::GetIntervalDayTimeValue(uint32_t idx) const
{
    return Access<TimeStamp>(idx, ODPS_INTERVAL_DAY_TIME);
}

std::string ODPSArray::GetDatetime(uint32_t idx) const
{
    return util::gmt_strftime(GetDatetimeValue(idx) / 1000, util::TUNNEL_DATE_TIME_FORMAT);
}

std::string ODPSArray::GetDate(uint32_t idx) const
{
    return util::gmt_strftime(GetDateValue(idx) * util::SECONDS_PER_DAY, util::TUNNEL_DATE_FORMAT);
}

std::string ODPSArray::GetDecimal(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_DECIMAL);
}

shared_ptr<ODPSArray> ODPSArray::GetArray(uint32_t idx) const
{
    return AccessPtr<ODPSArray>(idx, ODPS_ARRAY);
}

shared_ptr<ODPSMap> ODPSArray::GetMap(uint32_t idx) const
{
    return AccessPtr<ODPSMap>(idx, ODPS_MAP);
}

shared_ptr<ODPSStruct> ODPSArray::GetStruct(uint32_t idx) const
{
    return AccessPtr<ODPSStruct>(idx, ODPS_STRUCT);
}

void ODPSArray::SetDatetimeValue(uint32_t idx, const std::string& datetime)
{
    SetDatetimeValue(idx, util::gmt_strptime(datetime, util::TUNNEL_DATE_TIME_FORMAT) * 1000);
}

void ODPSArray::SetDateValue(uint32_t idx, const std::string& date)
{
    SetDateValue(idx, util::gmt_strptime(date, util::TUNNEL_DATE_FORMAT) / util::SECONDS_PER_DAY);
}

void ODPSArray::SetTinyIntValue(uint32_t idx, int8_t value)
{
    Rewrite<int8_t>(idx, ODPS_TINYINT, value);
}

void ODPSArray::SetSmallIntValue(uint32_t idx, int16_t value)
{
    Rewrite<int16_t>(idx, ODPS_SMALLINT, value);
}

void ODPSArray::SetIntegerValue(uint32_t idx, int32_t value)
{
    Rewrite<int32_t>(idx, ODPS_INTEGER, value);
}

void ODPSArray::SetBigIntValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_BIGINT, value);
}

void ODPSArray::SetFloatValue(uint32_t idx, float value)
{
    Rewrite<float>(idx, ODPS_FLOAT, value);
}

void ODPSArray::SetDoubleValue(uint32_t idx, double value)
{
    Rewrite<double>(idx, ODPS_DOUBLE, value);
}

void ODPSArray::SetBoolValue(uint32_t idx, bool value)
{
    Rewrite<bool>(idx, ODPS_BOOLEAN, value);
}

void ODPSArray::SetDatetimeValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_DATETIME, value);
}

void ODPSArray::SetDateValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_DATE, value);
}

void ODPSArray::SetIntervalYearMonthValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_INTERVAL_YEAR_MONTH, value);
}

void ODPSArray::SetStringValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_STRING, value);
}

void ODPSArray::SetCharValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_CHAR, value);
}

void ODPSArray::SetVarcharValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_VARCHAR, value);
}

void ODPSArray::SetBinaryValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_BINARY, value);
}

void ODPSArray::SetTimestampValue(uint32_t idx, const TimeStamp& value)
{
    Rewrite<const TimeStamp&>(idx, ODPS_TIMESTAMP, value);
}

void ODPSArray::SetTimestampNTZValue(uint32_t idx, const TimeStamp& value)
{
    Rewrite<const TimeStamp&>(idx, ODPS_TIMESTAMP_NTZ, value);
}

void ODPSArray::SetIntervalDayTimeValue(uint32_t idx, const TimeStamp& value)
{
    Rewrite<const TimeStamp&>(idx, ODPS_INTERVAL_DAY_TIME, value);
}

void ODPSArray::SetDecimalValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_DECIMAL, value);
}

void ODPSArray::SetArrayValue(uint32_t idx, shared_ptr<ODPSArray> value)
{
    RewritePtr<ODPSArray>(idx, ODPS_ARRAY, value);
}

void ODPSArray::SetMapValue(uint32_t idx, shared_ptr<ODPSMap> value)
{
    RewritePtr<ODPSMap>(idx, ODPS_MAP, value);
}

void ODPSArray::SetStructValue(uint32_t idx, shared_ptr<ODPSStruct> value)
{
    RewritePtr<ODPSStruct>(idx, ODPS_STRUCT, value);
}

void ODPSArray::AppendDatetimeValue(const std::string& datetime)
{
    AppendDatetimeValue(util::gmt_strptime(datetime, util::TUNNEL_DATE_TIME_FORMAT) * 1000);
}

void ODPSArray::AppendDateValue(const std::string& date)
{
    AppendDateValue(util::gmt_strptime(date, util::TUNNEL_DATE_FORMAT) / util::SECONDS_PER_DAY);
}

void ODPSArray::AppendTinyIntValue(int8_t value)
{
    Append<int8_t>(value, ODPS_TINYINT);
}

void ODPSArray::AppendSmallIntValue(int16_t value)
{
    Append<int16_t>(value, ODPS_SMALLINT);
}

void ODPSArray::AppendIntegerValue(int32_t value)
{
    Append<int32_t>(value, ODPS_INTEGER);
}

void ODPSArray::AppendBigIntValue(int64_t value)
{
    Append<int64_t>(value, ODPS_BIGINT);
}

void ODPSArray::AppendFloatValue(float value)
{
    Append<float>(value, ODPS_FLOAT);
}

void ODPSArray::AppendDoubleValue(double value)
{
    Append<double>(value, ODPS_DOUBLE);
}

void ODPSArray::AppendBoolValue(bool value)
{
    Append<bool>(value, ODPS_BOOLEAN);
}

void ODPSArray::AppendDatetimeValue(int64_t value)
{
    Append<int64_t>(value, ODPS_DATETIME);
}

void ODPSArray::AppendDateValue(int64_t value)
{
    Append<int64_t>(value, ODPS_DATE);
}

void ODPSArray::AppendIntervalYearMonthValue(int64_t value)
{
    Append<int64_t>(value, ODPS_INTERVAL_YEAR_MONTH);
}

void ODPSArray::AppendStringValue(const std::string& value)
{
    Append<const std::string&>(value, ODPS_STRING);
}

void ODPSArray::AppendCharValue(const std::string& value)
{
    Append<const std::string&>(value, ODPS_CHAR);
}

void ODPSArray::AppendVarcharValue(const std::string& value)
{
    Append<const std::string&>(value, ODPS_VARCHAR);
}

void ODPSArray::AppendBinaryValue(const std::string& value)
{
    Append<const std::string&>(value, ODPS_BINARY);
}

void ODPSArray::AppendTimestampValue(const TimeStamp& value)
{
    Append<const TimeStamp&>(value, ODPS_TIMESTAMP);
}

void ODPSArray::AppendTimestampNTZValue(const TimeStamp& value)
{
    Append<const TimeStamp&>(value, ODPS_TIMESTAMP_NTZ);
}

void ODPSArray::AppendIntervalDayTimeValue(const TimeStamp& value)
{
    Append<const TimeStamp&>(value, ODPS_INTERVAL_DAY_TIME);
}

void ODPSArray::AppendDecimalValue(const std::string& value)
{
    Append<const std::string&>(value, ODPS_DECIMAL);
}

void ODPSArray::AppendArrayValue(shared_ptr<ODPSArray> value)
{
    AppendPtr(value, ODPS_ARRAY);
}

void ODPSArray::AppendMapValue(shared_ptr<ODPSMap> value)
{
    AppendPtr(value, ODPS_MAP);
}

void ODPSArray::AppendStructValue(shared_ptr<ODPSStruct> value)
{
    AppendPtr(value, ODPS_STRUCT);
}

void ODPSArray::AppendNull()
{
    mNullArray.push_back(true);
    mArray.push_back(shared_ptr<void>());
}

}}}