#ifndef APSARA_ODPS_ARRAY_H
#define APSARA_ODPS_ARRAY_H

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <memory>

#include "limits.h"
#include "odps_exception.h"
#include "odps_types.h"

namespace apsara{ namespace odps{ namespace sdk{

class ODPSArray : public IODPSProtoSerializable
{
public:
    /**
     *	@brief 构造函数.
     *
     *	@param typeInfo 复杂类型信息
     */
    ODPSArray(const ODPSColumnTypeInfo& typeInfo);

    virtual ~ODPSArray();

    /**
     *	@brief 获得复杂类型信息
     */
    virtual const ODPSColumnTypeInfo& GetTypeInfo() const override;

    int64_t Size() const;

    int8_t GetTinyInt(uint32_t idx) const;

    int16_t GetSmallInt(uint32_t idx) const;

    int32_t GetInteger(uint32_t idx) const;

    int64_t GetBigInt(uint32_t idx) const;

    float GetFloat(uint32_t idx) const;

    double GetDouble(uint32_t idx) const;

    bool GetBool(uint32_t idx) const;

    std::string GetDatetime(uint32_t idx) const;

    std::string GetDate(uint32_t idx) const;

    int64_t GetDatetimeValue(uint32_t idx) const;

    int64_t GetDateValue(uint32_t idx) const;

    int64_t GetIntervalYearMonthValue(uint32_t idx) const;

    IntervalYearMonth GetIntervalYearMonth(uint32_t idx) const
    {
        return IntervalYearMonth(GetIntervalYearMonthValue(idx));
    }

    std::string GetString(uint32_t idx) const;

    std::string GetChar(uint32_t idx) const;

    std::string GetVarchar(uint32_t idx) const;

    std::string GetBinary(uint32_t idx) const;

    TimeStamp GetTimestamp(uint32_t idx) const;

    TimeStamp GetTimestampNTZ(uint32_t idx) const;

    TimeStamp GetIntervalDayTimeValue(uint32_t idx) const;

    IntervalDayTime GetIntervalDayTime(uint32_t idx) const
    {
        TimeStamp ts = GetIntervalDayTimeValue(idx);
        return IntervalDayTime(ts.GetSecond(), ts.GetNano());
    }

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
        SetIntervalYearMonthValue(idx, year * 12 + month);
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

    void SetIntervalDayTimeValue(uint32_t idx, const TimeStamp& idt);

    void SetDecimalValue(uint32_t idx, const std::string& dec);

    void SetArrayValue(uint32_t idx, std::shared_ptr<ODPSArray> value);

    void SetMapValue(uint32_t idx, std::shared_ptr<ODPSMap> value);

    void SetStructValue(uint32_t idx, std::shared_ptr<ODPSStruct> value);

    void AppendBigIntValue(int64_t value);

    void AppendTinyIntValue(int8_t value);

    void AppendSmallIntValue(int16_t value);

    void AppendIntegerValue(int32_t value);

    void AppendIntervalYearMonthValue(int64_t value);

    void AppendIntervalYearMonthValue(int64_t year, int64_t month)
    {
        AppendIntervalYearMonthValue(year * 12 + month);
    }

    void AppendFloatValue(float value);

    void AppendDoubleValue(double value);

    void AppendBoolValue(bool value);

    void AppendDatetimeValue(int64_t value);

    void AppendDatetimeValue(const std::string& datetime);

    void AppendDateValue(int64_t value);

    void AppendDateValue(const std::string& date);

    void AppendStringValue(const std::string& str);

    void AppendCharValue(const std::string& str);

    void AppendVarcharValue(const std::string& str);

    void AppendBinaryValue(const std::string& str);

    void AppendTimestampValue(int64_t sec, int32_t ns)
    {
        AppendTimestampValue(TimeStamp(sec, ns));
    }

    void AppendTimestampValue(const TimeStamp& ts);

    void AppendTimestampNTZValue(int64_t sec, int32_t ns)
    {
        AppendTimestampNTZValue(TimeStamp(sec, ns));
    }

    void AppendTimestampNTZValue(const TimeStamp& ts);

    void AppendIntervalDayTimeValue(int64_t sec, int32_t ns)
    {
        AppendIntervalDayTimeValue(TimeStamp(sec, ns));
    }

    void AppendIntervalDayTimeValue(const TimeStamp& ts);

    void AppendDecimalValue(const std::string& dec);

    void AppendArrayValue(std::shared_ptr<ODPSArray> value);

    void AppendMapValue(std::shared_ptr<ODPSMap> value);

    void AppendStructValue(std::shared_ptr<ODPSStruct> value);

    ODPSColumnType GetElementType() const;

    bool IsNull(uint32_t index) const { return mNullArray[index]; }

    void SetNull(uint32_t index) { mNullArray[index] = true; }

    void AppendNull();

    virtual int64_t GetInMemorySize() const override;

private:
    void TypeCheck(ODPSColumnType opType) const;

    void NullCheck(uint32_t index) const
    {
        if (IsNull(index))
        {
            throw OdpsTunnelException("Access Null value at index " + std::to_string(index));
        }
    }

    void SetNotNull(uint32_t index)
    {
        mNullArray[index] = false;
    }

    // we do not do overflow check: std::vector does for us.
    template <typename T>
    const T& Access(uint32_t index, ODPSColumnType opType) const
    {
        TypeCheck(opType);
        NullCheck(index);
        return *std::static_pointer_cast<T>(mArray[index]);
    }

    template <typename T>
    typename std::shared_ptr<T> AccessPtr(uint32_t index, ODPSColumnType opType) const
    {
        TypeCheck(opType);
        NullCheck(index);
        return std::static_pointer_cast<T>(mArray[index]);
    }

    template <typename T>
    void Rewrite(uint32_t index, ODPSColumnType opType, T value)
    {
        TypeCheck(opType);
        SetNotNull(index);
        if (!mArray[index])
        {
            mArray[index] = std::make_shared<typename std::decay<T>::type>(value);
        }
        else
        {
            *std::static_pointer_cast<typename std::decay<T>::type>(mArray[index]) = value;
        }
    }

    template <typename T>
    void RewritePtr(uint32_t index, ODPSColumnType opType, std::shared_ptr<T> value)
    {
        TypeCheck(opType);
        SetNotNull(index);
        mArray[index] = value;
    }

    template <typename T>
    void Append(T t, ODPSColumnType opType)
    {
        TypeCheck(opType);
        mArray.push_back(std::make_shared<typename std::decay<T>::type>(t));
        mNullArray.push_back(false);
    }

    void AppendPtr(std::shared_ptr<void> t, ODPSColumnType opType)
    {
        TypeCheck(opType);
        mArray.push_back(t);
        mNullArray.push_back(false);
    }

    ODPSColumnTypeInfo mTypeInfo;
    std::vector<bool> mNullArray;

    // shared_ptr<void>'s type erasure feature ensure everything is correctly destructed.
    std::vector<std::shared_ptr<void>> mArray;
};

}}}

#endif
