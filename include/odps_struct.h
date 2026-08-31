#ifndef APSARA_ODPS_STRUCT_H
#define APSARA_ODPS_STRUCT_H

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <memory>
#include <unordered_map>

#include "limits.h"
#include "odps_exception.h"
#include "odps_types.h"

namespace apsara{ namespace odps{ namespace sdk{

class ODPSStruct : public IODPSProtoSerializable
{
public:
    /**
     *	@brief 构造函数.
     *
     *	@param typeInfo 此对象的复杂类型信息
     */
    ODPSStruct(const ODPSColumnTypeInfo& typeInfo);

    virtual ~ODPSStruct();

    virtual int64_t GetInMemorySize() const override;

    /**
     *	@brief 获得复杂类型信息
     */
    virtual const ODPSColumnTypeInfo& GetTypeInfo() const override;

    int64_t Size() const;

    std::vector<std::string> GetMembers() const;

    uint32_t GetMemberIndex(const std::string& memberName) const;

    const ODPSColumnTypeInfo& GetMemberType(uint32_t index) const;

    // get formated data
    int8_t GetTinyInt(uint32_t idx) const;

    int16_t GetSmallInt(uint32_t idx) const;

    int32_t GetInteger(uint32_t idx) const;

    int64_t GetBigInt(uint32_t idx) const;

    float GetFloat(uint32_t idx) const;

    double GetDouble(uint32_t idx) const;

    bool GetBool(uint32_t idx) const;

    std::string GetDatetime(uint32_t idx) const;

    int64_t GetDatetimeValue(uint32_t idx) const;

    std::string GetDate(uint32_t idx) const;

    int64_t GetDateValue(uint32_t idx) const;

    IntervalYearMonth GetIntervalYearMonth(uint32_t idx) const
    {
        return IntervalYearMonth(GetIntervalYearMonthValue(idx));
    }

    int64_t GetIntervalYearMonthValue(uint32_t idx) const;

    std::string GetString(uint32_t idx) const;

    std::string GetChar(uint32_t idx) const;

    std::string GetVarchar(uint32_t idx) const;

    std::string GetBinary(uint32_t idx) const;

    TimeStamp GetTimestamp(uint32_t idx) const;

    TimeStamp GetTimestampNTZ(uint32_t idx) const;

    IntervalDayTime GetIntervalDayTime(uint32_t idx) const
    {
        TimeStamp ts = GetIntervalDayTimeValue(idx);
        return IntervalDayTime(ts.GetSecond(), ts.GetNano());
    }

    TimeStamp GetIntervalDayTimeValue(uint32_t idx) const;

    std::string GetDecimal(uint32_t idx) const;

    std::shared_ptr<ODPSArray> GetArray(uint32_t idx) const;

    std::shared_ptr<ODPSMap> GetMap(uint32_t idx) const;

    std::shared_ptr<ODPSStruct> GetStruct(uint32_t idx) const;

    void SetBigIntValue(uint32_t idx, int64_t value);

    void SetTinyIntValue(uint32_t idx, int8_t value);

    void SetSmallIntValue(uint32_t idx, int16_t value);

    void SetIntegerValue(uint32_t idx, int32_t value);

    void SetIntervalYearMonthValue(uint32_t idx, int64_t value);

    void SetIntervalYearMonthValue(uint32_t idx, int64_t year, int64_t month)
    {
        return SetIntervalYearMonthValue(idx, year * 12 + month);
    }

    void SetFloatValue(uint32_t idx, float value);

    void SetDoubleValue(uint32_t idx, double value);

    void SetBoolValue(uint32_t idx, bool value);

    /* value is milliseconds after January 1, 1970 00:00:00. UTC */
    void SetDatetimeValue(uint32_t idx, int64_t value);

    /* format: YYYY-MM-DD HH:MM:SS */
    void SetDatetimeValue(uint32_t idx, const std::string& datetime);

    /* 与 1970-01-01 的偏移天数 */
    void SetDateValue(uint32_t idx, int64_t value);

    /* format: YYYY-MM-DD */
    void SetDateValue(uint32_t idx, const std::string& date);

    void SetStringValue(uint32_t idx, const std::string& str);

    void SetCharValue(uint32_t idx, const std::string& str);

    void SetVarcharValue(uint32_t idx, const std::string& str);

    void SetBinaryValue(uint32_t idx, const std::string& str);

    void SetTimestampValue(uint32_t idx, int64_t sec, int32_t ns)
    {
        SetTimestampValue(idx, TimeStamp(sec, ns));
    }

    void SetTimestampValue(uint32_t idx, const TimeStamp& ts);

    void SetTimestampNTZValue(uint32_t idx, int64_t sec, int32_t ns)
    {
        SetTimestampNTZValue(idx, TimeStamp(sec, ns));
    }

    void SetTimestampNTZValue(uint32_t idx, const TimeStamp& ts);

    void SetIntervalDayTimeValue(uint32_t idx, int64_t sec, int32_t ns)
    {
        SetIntervalDayTimeValue(idx, TimeStamp(sec, ns));
    }

    void SetIntervalDayTimeValue(uint32_t idx, const TimeStamp& ts);

    void SetDecimalValue(uint32_t idx, const std::string& dec);

    void SetArrayValue(uint32_t idx, std::shared_ptr<ODPSArray> value);

    void SetMapValue(uint32_t idx, std::shared_ptr<ODPSMap> value);

    void SetStructValue(uint32_t idx, std::shared_ptr<ODPSStruct> value);

    bool IsNull(uint32_t idx) const;

    void SetNullValue(uint32_t idx);

private:
    // we do not do overflow check: std::vector does for us.
    // on null value, will return null pointer.
    template <typename T>
    const T& Access(uint32_t index, ODPSColumnType opType) const
    {
        TypeCheck(index, opType);
        NullCheck(index);
        return *std::static_pointer_cast<typename std::decay<T>::type>(mMembers[index]);
    }

    template <typename T>
    void Rewrite(uint32_t index, ODPSColumnType opType, T value)
    {
        TypeCheck(index, opType);
        mMembers[index] = std::make_shared<typename std::decay<T>::type>(value);
    }

    template <typename T>
    std::shared_ptr<T> AccessPtr(uint32_t index, ODPSColumnType opType) const
    {
        TypeCheck(index, opType);
        NullCheck(index);
        return std::static_pointer_cast<T>(mMembers[index]);
    }

    template <typename T>
    void RewritePtr(uint32_t index, ODPSColumnType opType, std::shared_ptr<T> value)
    {
        TypeCheck(index, opType);
        mMembers[index] = value;
    }

    std::unordered_map<std::string, uint32_t> mMemberNameToIndexMap;
    std::vector<std::shared_ptr<void>> mMembers;
    ODPSColumnTypeInfo mTypeInfo;
    void TypeCheck(uint32_t index, ODPSColumnType opType) const;
    void NullCheck(uint32_t index) const;
};

}}}

#endif