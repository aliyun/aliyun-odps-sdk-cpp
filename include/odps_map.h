#ifndef APSARA_ODPS_MAP_H
#define APSARA_ODPS_MAP_H

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
#include "odps_array.h"

namespace apsara{ namespace odps{ namespace sdk{

class ODPSMap : public IODPSProtoSerializable
{
private:
    template <typename KeyType>
    using KeyIndexMapType = std::map<KeyType, uint32_t>;
    template <typename KeyType>
    using KeyNullMapType = std::map<KeyType, bool>;

    ODPSColumnTypeInfo mValueArrayType;
    ODPSColumnType mKeyCType;
    ODPSColumnTypeInfo mTypeInfo;
    std::shared_ptr<void> mKeyToArrayIndexMap;
    std::shared_ptr<ODPSArray> mUnderlyingArray;
    // ctype check
    template <typename KeyType>
    void KeyCTypeCheck() const
    {
        throw OdpsTunnelException("Unsupported key type for ODPSMap.");
    }

    template <typename KeyType>
    KeyIndexMapType<KeyType>& CastToKeyIndexMap()
    {
        KeyCTypeCheck<typename std::decay<KeyType>::type>();
        return *std::static_pointer_cast<KeyIndexMapType<KeyType>>(mKeyToArrayIndexMap);
    }

    template <typename KeyType>
    const KeyIndexMapType<KeyType>& CastToKeyIndexMap() const
    {
        KeyCTypeCheck<typename std::decay<KeyType>::type>();
        return *std::static_pointer_cast<KeyIndexMapType<KeyType>>(mKeyToArrayIndexMap);
    }

    template <typename KeyType>
    typename KeyIndexMapType<KeyType>::iterator FindIndex(KeyType key)
    {
        return CastToKeyIndexMap<KeyType>().find(key);
    }

    template <typename KeyType>
    typename KeyIndexMapType<KeyType>::const_iterator FindIndex(KeyType key) const
    {
        return CastToKeyIndexMap<KeyType>().find(key);
    }

    template <typename KeyType>
    uint32_t FindIndexOrFail(KeyType key) const
    {
        auto& keyIndexMap = CastToKeyIndexMap<KeyType>();
        auto it = keyIndexMap.find(key);
        if (it == keyIndexMap.end())
        {
            throw OdpsTunnelException("Key not in map.");
        }
        return it->second;
    }

    template <typename KeyType>
    uint32_t FindIndexOrAppend(KeyType key)
    {
        auto& keyIndexMap = CastToKeyIndexMap<KeyType>();
        auto it = keyIndexMap.find(key);
        if (it == keyIndexMap.end())
        {
            uint32_t newIndex = mUnderlyingArray->Size();
            mUnderlyingArray->AppendNull();
            keyIndexMap[key] = newIndex;
            return newIndex;
        }
        return it->second;
    }

public:
    /**
     *	@brief 构造函数.
     *
     *	@param typeInfo 此对象的复杂类型信息
     */
    ODPSMap(const ODPSColumnTypeInfo& typeInfo);

    virtual ~ODPSMap();

    virtual int64_t GetInMemorySize() const override;

    /**
     *	@brief 获得复杂类型信息
     */
    virtual const ODPSColumnTypeInfo& GetTypeInfo() const override;

    ODPSColumnType GetKeyCType() const { return mKeyCType; }

    const ODPSColumnTypeInfo& GetKeyTypeInfo() const;

    const ODPSColumnTypeInfo& GetValueTypeInfo() const;

    std::shared_ptr<ODPSArray> GetKeys() const;
    std::shared_ptr<ODPSArray> GetValues() const { return mUnderlyingArray; };

    void Recover(std::shared_ptr<ODPSArray> keyArray, std::shared_ptr<ODPSArray> valueArray);

    template<typename KeyType>
    bool Contains(KeyType key) const
    {
        return FindIndex(key) != CastToKeyIndexMap<KeyType>().end();
    }

    template<typename KeyType>
    bool ContainsAndNotNull(KeyType key) const
    {
        const KeyIndexMapType<KeyType>& indexMap = CastToKeyIndexMap<KeyType>();
        auto indexIt = FindIndex(key);
        if (indexIt == indexMap.end())
        {
            return false;
        }
        return !(mUnderlyingArray->IsNull(indexIt->second));
    }

    int64_t Size() const;

    template<typename KeyType>
    std::vector<KeyType> Keys() const
    {
        std::vector<KeyType> keys;
        for(auto x: CastToKeyIndexMap<KeyType>())
        {
            keys.push_back(x.first);
        }
        return keys;
    }

    template<typename KeyType>
    int8_t GetTinyInt(KeyType key) const
    {
        return mUnderlyingArray->GetTinyInt(FindIndexOrFail(key));
    }

    template<typename KeyType>
    int16_t GetSmallInt(KeyType key) const
    {
        return mUnderlyingArray->GetSmallInt(FindIndexOrFail(key));
    }

    template<typename KeyType>
    int32_t GetInteger(KeyType key) const
    {
        return mUnderlyingArray->GetInteger(FindIndexOrFail(key));
    }

    template<typename KeyType>
    int64_t GetBigInt(KeyType key) const
    {
        return mUnderlyingArray->GetBigInt(FindIndexOrFail(key));
    }

    template<typename KeyType>
    float GetFloat(KeyType key) const
    {
        return mUnderlyingArray->GetFloat(FindIndexOrFail(key));
    }

    template<typename KeyType>
    double GetDouble(KeyType key) const
    {
        return mUnderlyingArray->GetDouble(FindIndexOrFail(key));
    }

    template<typename KeyType>
    bool GetBool(KeyType key) const
    {
        return mUnderlyingArray->GetBool(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetDatetime(KeyType key) const
    {
        return mUnderlyingArray->GetDatetime(FindIndexOrFail(key));
    }

    template<typename KeyType>
    int64_t GetDatetimeValue(KeyType key) const
    {
        return mUnderlyingArray->GetDatetimeValue(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetDate(KeyType key) const
    {
        return mUnderlyingArray->GetDate(FindIndexOrFail(key));
    }

    template<typename KeyType>
    int64_t GetDateValue(KeyType key) const
    {
        return mUnderlyingArray->GetDateValue(FindIndexOrFail(key));
    }

    template<typename KeyType>
    IntervalYearMonth GetIntervalYearMonth(KeyType key) const
    {
        return mUnderlyingArray->GetIntervalYearMonth(FindIndexOrFail(key));
    }

    template<typename KeyType>
    int64_t GetIntervalYearMonthValue(KeyType key) const
    {
        return mUnderlyingArray->GetIntervalYearMonthValue(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetString(KeyType key) const
    {
        return mUnderlyingArray->GetString(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetChar(KeyType key) const
    {
        return mUnderlyingArray->GetChar(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetVarchar(KeyType key) const
    {
        return mUnderlyingArray->GetVarchar(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetBinary(KeyType key) const
    {
        return mUnderlyingArray->GetBinary(FindIndexOrFail(key));
    }

    template<typename KeyType>
    TimeStamp GetTimestamp(KeyType key) const
    {
        return mUnderlyingArray->GetTimestamp(FindIndexOrFail(key));
    }

    template<typename KeyType>
    TimeStamp GetTimestampNTZ(KeyType key) const
    {
        return mUnderlyingArray->GetTimestampNTZ(FindIndexOrFail(key));
    }

    template<typename KeyType>
    TimeStamp GetIntervalDayTimeValue(KeyType key) const
    {
        return mUnderlyingArray->GetIntervalDayTimeValue(FindIndexOrFail(key));
    }

    template<typename KeyType>
    IntervalDayTime GetIntervalDayTime(KeyType key) const
    {
        return mUnderlyingArray->GetIntervalDayTime(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::string GetDecimal(KeyType key) const
    {
        return mUnderlyingArray->GetDecimal(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::shared_ptr<ODPSArray> GetArray(KeyType key) const
    {
        return mUnderlyingArray->GetArray(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::shared_ptr<ODPSMap> GetMap(KeyType key) const
    {
        return mUnderlyingArray->GetMap(FindIndexOrFail(key));
    }

    template<typename KeyType>
    std::shared_ptr<ODPSStruct> GetStruct(KeyType key) const
    {
        return mUnderlyingArray->GetStruct(FindIndexOrFail(key));
    }

    template<typename KeyType>
    void SetTinyIntValue(KeyType key, int8_t value)
    {
        mUnderlyingArray->SetTinyIntValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetSmallIntValue(KeyType key, int16_t value)
    {
        mUnderlyingArray->SetSmallIntValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetIntegerValue(KeyType key, int32_t value)
    {
        mUnderlyingArray->SetIntegerValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetBigIntValue(KeyType key, int64_t value)
    {
        mUnderlyingArray->SetBigIntValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetFloatValue(KeyType key, float value)
    {
        mUnderlyingArray->SetFloatValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetDoubleValue(KeyType key, double value)
    {
        mUnderlyingArray->SetDoubleValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetBoolValue(KeyType key, bool value)
    {
        mUnderlyingArray->SetBoolValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetDatetimeValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetDatetimeValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetDatetimeValue(KeyType key, int64_t value)
    {
        mUnderlyingArray->SetDatetimeValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetDateValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetDateValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetDateValue(KeyType key, int64_t value)
    {
        mUnderlyingArray->SetDateValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetIntervalYearMonthValue(KeyType key, int64_t value)
    {
        mUnderlyingArray->SetIntervalYearMonthValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetIntervalYearMonthValue(KeyType key, int64_t year, int64_t month)
    {
        mUnderlyingArray->SetIntervalYearMonthValue(FindIndexOrAppend(key), year, month);
    }

    template<typename KeyType>
    void SetStringValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetStringValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetCharValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetCharValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetVarcharValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetVarcharValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetBinaryValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetBinaryValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetTimestampValue(KeyType key, const TimeStamp& value)
    {
        mUnderlyingArray->SetTimestampValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetTimestampValue(KeyType key, int64_t sec, int64_t ns)
    {
        mUnderlyingArray->SetTimestampValue(FindIndexOrAppend(key), sec, ns);
    }

    template<typename KeyType>
    void SetTimestampNTZValue(KeyType key, const TimeStamp& value)
    {
        mUnderlyingArray->SetTimestampNTZValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetTimestampNTZValue(KeyType key, int64_t sec, int64_t ns)
    {
        mUnderlyingArray->SetTimestampNTZValue(FindIndexOrAppend(key), sec, ns);
    }

    template<typename KeyType>
    void SetIntervalDayTimeValue(KeyType key, const TimeStamp& value)
    {
        mUnderlyingArray->SetIntervalDayTimeValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetIntervalDayTimeValue(KeyType key, int64_t sec, int64_t ns)
    {
        mUnderlyingArray->SetIntervalDayTimeValue(FindIndexOrAppend(key), sec, ns);
    }

    template<typename KeyType>
    void SetDecimalValue(KeyType key, const std::string& value)
    {
        mUnderlyingArray->SetDecimalValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetArrayValue(KeyType key, std::shared_ptr<ODPSArray> value)
    {
        mUnderlyingArray->SetArrayValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetMapValue(KeyType key, std::shared_ptr<ODPSMap> value)
    {
        mUnderlyingArray->SetMapValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetStructValue(KeyType key, std::shared_ptr<ODPSStruct> value)
    {
        mUnderlyingArray->SetStructValue(FindIndexOrAppend(key), value);
    }

    template<typename KeyType>
    void SetNullValue(KeyType key)
    {
        mUnderlyingArray->SetNull(FindIndexOrAppend(key));
    }

};

#define KeyCTypeCheckFwdDeclGenerator(CTYPE) \
template <> void ODPSMap::KeyCTypeCheck<CTYPE>() const ;

KeyCTypeCheckFwdDeclGenerator(int64_t);
KeyCTypeCheckFwdDeclGenerator(float);
KeyCTypeCheckFwdDeclGenerator(double);
KeyCTypeCheckFwdDeclGenerator(std::string);
KeyCTypeCheckFwdDeclGenerator(bool);

#undef KeyCTypeCheckFwdDeclGenerator

template <> void ODPSMap::KeyCTypeCheck<TimeStamp>() const ;

}}}

#endif