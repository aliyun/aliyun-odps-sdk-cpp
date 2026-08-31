#include "odps_table.h"
#include <unordered_map>
#include "error_code.h"
#include "odps_exception.h"
#include "util/utils.h"
#include "odps_struct.h"
#include <iostream>
#include <algorithm>

#include "util/utils.h"
using namespace apsara::odps::sdk::util;

using namespace std;

namespace apsara { namespace odps { namespace sdk {

static std::string StringToUpper(std::string str)
{
    transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::toupper(c); });
    return str;
}

ODPSStruct::ODPSStruct(const ODPSColumnTypeInfo& typeInfo):
    mTypeInfo(typeInfo)
{
    if (mTypeInfo.mType != ODPS_STRUCT ||
        mTypeInfo.mSubTypes.size() == 0)
    {
        throw OdpsTunnelException("Creating ODPSStruct with non-struct type");
    }
    for (size_t i = 0; i < mTypeInfo.mSubTypes.size(); i++)
    {
        mMemberNameToIndexMap[mTypeInfo.mSubTypes[i].mMemberName] = i;
        mMembers.push_back(std::shared_ptr<void>());
    }
}

int64_t ODPSStruct::GetInMemorySize() const
{
    int64_t sum = 0;
    for(auto it : mMemberNameToIndexMap)
    {
        sum += it.first.size();
        sum += sizeof(it.second);
    }
    for(uint32_t index = 0; index < mTypeInfo.mSubTypes.size(); index ++)
    {
        if (IsNull(index))
        {
            continue;
        }
        ODPSColumnTypeInfo elemType = mTypeInfo.mSubTypes[index];
        switch(elemType.mType)
        {
            case ODPS_BIGINT:
            case ODPS_DATETIME:
            case ODPS_DATE:
            case ODPS_INTERVAL_YEAR_MONTH:
                sum += sizeof(int64_t);
            break;
            case ODPS_DOUBLE:
                sum += sizeof(double);
            break;
            case ODPS_BOOLEAN:
                sum += sizeof(bool);
            break;
            case ODPS_STRING:
            case ODPS_CHAR:
            case ODPS_VARCHAR:
            case ODPS_BINARY:
            case ODPS_DECIMAL:
                sum += Access<std::string>(index, elemType.mType).size();
            break;
            case ODPS_TINYINT:
                sum += sizeof(int8_t);
            break;
            case ODPS_SMALLINT:
                sum += sizeof(int16_t);
            break;
            case ODPS_INTEGER:
                sum += sizeof(int32_t);
            break;
            case ODPS_TIMESTAMP:
            case ODPS_TIMESTAMP_NTZ:
            case ODPS_INTERVAL_DAY_TIME:
                sum += sizeof(TimeStamp);
            break;
            case ODPS_FLOAT:
                sum += sizeof(float);
            break;
            case ODPS_ARRAY:
            case ODPS_MAP:
            case ODPS_STRUCT:
                sum += AccessPtr<IODPSProtoSerializable>(index, elemType.mType)->GetInMemorySize();
            break;
            default:
                throw OdpsTunnelException("Unimplemented size calculation for type: " + GetTypeName(elemType.mType));
        }
    }
    return sum;
}

uint32_t ODPSStruct::GetMemberIndex(const std::string& memberName) const
{
    auto it = mMemberNameToIndexMap.find(StringToUpper(memberName));
    if (it == mMemberNameToIndexMap.end())
    {
        throw OdpsTunnelException("No such member in struct: " + memberName);
    }
    return it->second;
}

const ODPSColumnTypeInfo& ODPSStruct::GetMemberType(uint32_t index) const
{
    return mTypeInfo.mSubTypes[index];
}

void ODPSStruct::TypeCheck(uint32_t index, ODPSColumnType opType) const
{
    if (opType != GetMemberType(index).mType)
    {
        throw OdpsTunnelException("Incompatible types for ODPSStruct operations: member is " +
            GetTypeName(GetMemberType(index).mType) + ", but operation require " + GetTypeName(opType));
    }
}

ODPSStruct::~ODPSStruct()
{

}

const ODPSColumnTypeInfo& ODPSStruct::GetTypeInfo() const
{
    return mTypeInfo;
}

int64_t ODPSStruct::Size() const
{
    return mTypeInfo.mSubTypes.size();
}

void ODPSStruct::NullCheck(uint32_t index) const
{
    if(!(mMembers[index]))
    {
        throw OdpsTunnelException("Accessing null member");
    }
}

bool ODPSStruct::IsNull(uint32_t idx) const
{
    return mMembers[idx].get() == nullptr;
}

void ODPSStruct::SetNullValue(uint32_t idx)
{
    mMembers[idx] = shared_ptr<void>();
}

int8_t ODPSStruct::GetTinyInt(uint32_t idx) const
{
    return Access<int8_t>(idx, ODPS_TINYINT);
}

int16_t ODPSStruct::GetSmallInt(uint32_t idx) const
{
    return Access<int16_t>(idx, ODPS_SMALLINT);
}

int32_t ODPSStruct::GetInteger(uint32_t idx) const
{
    return Access<int32_t>(idx, ODPS_INTEGER);
}

int64_t ODPSStruct::GetBigInt(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_BIGINT);
}

float ODPSStruct::GetFloat(uint32_t idx) const
{
    return Access<float>(idx, ODPS_FLOAT);
}

double ODPSStruct::GetDouble(uint32_t idx) const
{
    return Access<double>(idx, ODPS_DOUBLE);
}

bool ODPSStruct::GetBool(uint32_t idx) const
{
    return Access<bool>(idx, ODPS_BOOLEAN);
}

int64_t ODPSStruct::GetDatetimeValue(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_DATETIME);
}

int64_t ODPSStruct::GetDateValue(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_DATE);
}

std::string ODPSStruct::GetDatetime(uint32_t idx) const
{
    return util::gmt_strftime(GetDatetimeValue(idx) / 1000, util::TUNNEL_DATE_TIME_FORMAT);
}

std::string ODPSStruct::GetDate(uint32_t idx) const
{
    return util::gmt_strftime(GetDateValue(idx) * util::SECONDS_PER_DAY, util::TUNNEL_DATE_FORMAT);
}

int64_t ODPSStruct::GetIntervalYearMonthValue(uint32_t idx) const
{
    return Access<int64_t>(idx, ODPS_INTERVAL_YEAR_MONTH);
}

std::string ODPSStruct::GetString(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_STRING);
}

std::string ODPSStruct::GetChar(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_CHAR);
}

std::string ODPSStruct::GetVarchar(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_VARCHAR);
}

std::string ODPSStruct::GetBinary(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_BINARY);
}

TimeStamp ODPSStruct::GetTimestamp(uint32_t idx) const
{
    return Access<TimeStamp>(idx, ODPS_TIMESTAMP);
}

TimeStamp ODPSStruct::GetTimestampNTZ(uint32_t idx) const
{
    return Access<TimeStamp>(idx, ODPS_TIMESTAMP_NTZ);
}

TimeStamp ODPSStruct::GetIntervalDayTimeValue(uint32_t idx) const
{
    return Access<TimeStamp>(idx, ODPS_INTERVAL_DAY_TIME);
}

std::string ODPSStruct::GetDecimal(uint32_t idx) const
{
    return Access<std::string>(idx, ODPS_DECIMAL);
}

shared_ptr<ODPSArray> ODPSStruct::GetArray(uint32_t idx) const
{
    return AccessPtr<ODPSArray>(idx, ODPS_ARRAY);
}

shared_ptr<ODPSMap> ODPSStruct::GetMap(uint32_t idx) const
{
    return AccessPtr<ODPSMap>(idx, ODPS_MAP);
}

shared_ptr<ODPSStruct> ODPSStruct::GetStruct(uint32_t idx) const
{
    return AccessPtr<ODPSStruct>(idx, ODPS_STRUCT);
}

void ODPSStruct::SetTinyIntValue(uint32_t idx, int8_t value)
{
    Rewrite<int8_t>(idx, ODPS_TINYINT, value);
}

void ODPSStruct::SetSmallIntValue(uint32_t idx, int16_t value)
{
    Rewrite<int16_t>(idx, ODPS_SMALLINT, value);
}

void ODPSStruct::SetIntegerValue(uint32_t idx, int32_t value)
{
    Rewrite<int32_t>(idx, ODPS_INTEGER, value);
}

void ODPSStruct::SetBigIntValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_BIGINT, value);
}

void ODPSStruct::SetFloatValue(uint32_t idx, float value)
{
    Rewrite<float>(idx, ODPS_FLOAT, value);
}

void ODPSStruct::SetDoubleValue(uint32_t idx, double value)
{
    Rewrite<double>(idx, ODPS_DOUBLE, value);
}

void ODPSStruct::SetBoolValue(uint32_t idx, bool value)
{
    Rewrite<bool>(idx, ODPS_BOOLEAN, value);
}

void ODPSStruct::SetDatetimeValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_DATETIME, value);
}

void ODPSStruct::SetDateValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_DATE, value);
}

void ODPSStruct::SetIntervalYearMonthValue(uint32_t idx, int64_t value)
{
    Rewrite<int64_t>(idx, ODPS_INTERVAL_YEAR_MONTH, value);
}

void ODPSStruct::SetStringValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_STRING, value);
}

void ODPSStruct::SetCharValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_CHAR, value);
}

void ODPSStruct::SetVarcharValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_VARCHAR, value);
}

void ODPSStruct::SetBinaryValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_BINARY, value);
}

void ODPSStruct::SetTimestampValue(uint32_t idx, const TimeStamp& value)
{
    Rewrite<const TimeStamp&>(idx, ODPS_TIMESTAMP, value);
}

void ODPSStruct::SetTimestampNTZValue(uint32_t idx, const TimeStamp& value)
{
    Rewrite<const TimeStamp&>(idx, ODPS_TIMESTAMP_NTZ, value);
}

void ODPSStruct::SetIntervalDayTimeValue(uint32_t idx, const TimeStamp& value)
{
    Rewrite<const TimeStamp&>(idx, ODPS_INTERVAL_DAY_TIME, value);
}

void ODPSStruct::SetDecimalValue(uint32_t idx, const std::string& value)
{
    Rewrite<const std::string&>(idx, ODPS_DECIMAL, value);
}

void ODPSStruct::SetArrayValue(uint32_t idx, std::shared_ptr<ODPSArray> value)
{
    RewritePtr<ODPSArray>(idx, ODPS_ARRAY, value);
}

void ODPSStruct::SetMapValue(uint32_t idx, std::shared_ptr<ODPSMap> value)
{
    RewritePtr<ODPSMap>(idx, ODPS_MAP, value);
}

void ODPSStruct::SetStructValue(uint32_t idx, std::shared_ptr<ODPSStruct> value)
{
    RewritePtr<ODPSStruct>(idx, ODPS_STRUCT, value);
}

void ODPSStruct::SetDatetimeValue(uint32_t idx, const std::string& datetime)
{
    SetDatetimeValue(idx, util::gmt_strptime(datetime, util::TUNNEL_DATE_TIME_FORMAT) * 1000);
}

void ODPSStruct::SetDateValue(uint32_t idx, const std::string& date)
{
    SetDateValue(idx, util::gmt_strptime(date, util::TUNNEL_DATE_FORMAT) / util::SECONDS_PER_DAY);
}

}}}