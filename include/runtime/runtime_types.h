#ifndef APSARA_ODPS_EXECUTION_ENGINE_RUNTIME_TYPES_H
#define APSARA_ODPS_EXECUTION_ENGINE_RUNTIME_TYPES_H


#include <stdint.h>
#include <string>
#include <vector>
#include "runtime_type_category.h"

namespace apsara
{
namespace odps
{

enum typeNameSpecific
{
    TS_RUNTIME,
    TS_CFILE,
    TS_POT,
    TS_UDF
};

class RuntimeType;

class RuntimeTypePtr {
public:
    friend bool operator==(const RuntimeTypePtr& a, const RuntimeTypePtr& b);
    friend bool operator==(const RuntimeTypePtr& a, std::nullptr_t);
    friend bool operator==(std::nullptr_t, const RuntimeTypePtr& a);
    friend bool operator!=(const RuntimeTypePtr& a, const RuntimeTypePtr& b);
    friend bool operator!=(const RuntimeTypePtr& a, std::nullptr_t);
    friend bool operator!=(std::nullptr_t, const RuntimeTypePtr& a);
    friend bool operator<(const RuntimeTypePtr& a, const RuntimeTypePtr& b);
    friend bool operator<(const RuntimeTypePtr& a, std::nullptr_t);
    friend bool operator<(std::nullptr_t, const RuntimeTypePtr& a);

    RuntimeTypePtr() : ptr_(nullptr) {}
    RuntimeTypePtr(std::nullptr_t) : ptr_(nullptr) {}
    RuntimeTypePtr(const RuntimeType* ptr) : ptr_(ptr) {}

    const RuntimeType* operator->() const {
        return ptr_;
    }

    const RuntimeType& operator*() const {
        return *ptr_;
    }

    explicit operator bool() const {
        return ptr_ != nullptr;
    }

    RuntimeTypePtr& operator=(const RuntimeTypePtr& other) {
        ptr_ = other.ptr_;
        return *this;
    }
private:
    const RuntimeType* get() const {
        return ptr_;
    }

private:
    const RuntimeType* ptr_;
};

inline bool operator==(const RuntimeTypePtr& a, const RuntimeTypePtr& b) {
    return a.get() == b.get();
}

inline bool operator==(const RuntimeTypePtr& a, std::nullptr_t) {
    return !a.get();
}

inline bool operator==(std::nullptr_t, const RuntimeTypePtr& a) {
    return !a.get();
}

inline bool operator!=(const RuntimeTypePtr& a, const RuntimeTypePtr& b) {
    return a.get() != b.get();
}

inline bool operator!=(const RuntimeTypePtr& a, std::nullptr_t) {
    return a.get();
}

inline bool operator!=(std::nullptr_t, const RuntimeTypePtr& a) {
    return a.get();
}

inline bool operator<(const RuntimeTypePtr& a, const RuntimeTypePtr& b) {
    return a.get() < b.get();
}
inline bool operator<(const RuntimeTypePtr& a, std::nullptr_t) { return false; }

inline bool operator<(std::nullptr_t, const RuntimeTypePtr& a) {
    return a.get() ? true : false;
}

extern std::string RuntimeTypeToString(apsara::odps::RuntimeTypePtr type);

class RuntimeType : public WhichRuntimeType
{
public:
    static RuntimeTypePtr get(TypeCategory type, bool hasNull = true);

    // RT_NULL
    static RuntimeTypePtr getNull(bool hasNull = true);

    // RT_VOID
    static RuntimeTypePtr getVoid(bool hasNull = true);

    // RT_TINYINT
    static RuntimeTypePtr getTinyint(bool hasNull = true);

    // RT_SMALLINT
    static RuntimeTypePtr getSmallint(bool hasNull = true);

    // RT_INT
    static RuntimeTypePtr getInt(bool hasNull = true);

    // RT_BIGINT
    static RuntimeTypePtr getBigint(bool hasNull = true);

    // RT_FLOAT
    static RuntimeTypePtr getFloat(bool hasNull = true);

    // RT_DOUBLE
    static RuntimeTypePtr getDouble(bool hasNull = true);

    // RT_BOOLEAN
    static RuntimeTypePtr getBoolean(bool hasNull = true);

    // RT_STRING
    static RuntimeTypePtr getString(bool hasNull = true);

    // RT_BINARY
    static RuntimeTypePtr getBinary(bool hasNull = true);

    // RT_DATETIME
    static RuntimeTypePtr getDatetime(bool hasNull = true);

    // RT_DATE
    static RuntimeTypePtr getDate(bool hasNull = true);

    // RT_TIMESTAMP
    static RuntimeTypePtr getTimestamp(bool hasNull = true);

    // RT_TIMESTAMP_NTZ
    static RuntimeTypePtr getTimestampNtz(bool hasNull = true);

    // RT_INTERVAL_YEAR_MONTH
    static RuntimeTypePtr getInteralYearMonth(bool hasNull = true);

    // RT_INTERVAL_DAY_TIME
    static RuntimeTypePtr getIntervalDayTime(bool hasNull = true);

    // RT_DECIMAL
    static RuntimeTypePtr getLegacyDecimal(bool hasNull = true);

    // RT_UDT
    static RuntimeTypePtr getUDTYPE(bool hasNull = true);

    // RT_CHAR
    static RuntimeTypePtr getChar(int64_t len, bool hasNull = true);

    // RT_VARCHAR
    static RuntimeTypePtr getVarchar(int64_t len, bool hasNull = true);

    // RT_CHAR/RT_VARCHAR
    static RuntimeTypePtr getCharacter(TypeCategory type, int64_t len, bool hasNull = true);

    // RT_DECIMAL_NEW/RT_DECIMAL_NEW_64/RT_DECIMAL_NEW_128/RT_DECIMAL_NEW_32/RT_DECIMAL_NEW_16
    static RuntimeTypePtr getDecimal(TypeCategory type, int32_t precision, int32_t scale, bool hasNull = true);

    // RT_ARRAY
    static RuntimeTypePtr getArray(RuntimeTypePtr subType, bool hasNull = true, bool isColumnar = false);

    // RT_MAP
    static RuntimeTypePtr getMap(RuntimeTypePtr keyType, RuntimeTypePtr valueType,
            bool hasNull = true, bool isColumnar = false);

    // RT_STRUCT
    static RuntimeTypePtr getStruct(const std::vector<std::pair<std::string, RuntimeTypePtr> >& subTypes,
            bool hasNull = true, bool isColumnar = false);

    // RT_JSON
    static RuntimeTypePtr getJson(RuntimeTypePtr staticType = nullptr, bool hasNull = true);

    // RT_ARRAY/RT_MAP/RT_STRUCT
    static RuntimeTypePtr getComplex(TypeCategory type,
            const std::vector<std::pair<std::string, RuntimeTypePtr> >& subTypes,
            bool hasNull = true, bool isColumnar = false);

    // RT_DICTIONARY_ENCODING_STRING
    static RuntimeTypePtr getDictionaryEncodingString(bool hasNull = true);

    // RT_ROARING_BITMAP_32
    static RuntimeTypePtr getRoaringBitmap32(bool hasNull = true);

    // RT_ROARING_BITMAP_64
    static RuntimeTypePtr getRoaringBitmap64(bool hasNull = true);

    // get has null or has not null type
    static RuntimeTypePtr get(const RuntimeTypePtr typePtr, bool hasNull);

    // convert RT_COLUMNAR_(ARRAY|MAP|STRUCT) to RT_(ARRAY|MAP|STRUCT), only used for runtime
    static RuntimeTypePtr removeColumnarAttribute(RuntimeTypePtr type);

    // convert RT_(ARRAY|MAP|STRUCT) to RT_COLUMNAR_(ARRAY|MAP|STRUCT), only used for runtime
    static RuntimeTypePtr addColumnarAttribute(RuntimeTypePtr type);

    // convert RT_STRING to RT_DICTIONARY_ENCODING_STRING, only used for runtime
    static RuntimeTypePtr addEncodedAttribute(RuntimeTypePtr type);

public:
    bool operator==(const RuntimeType& b) const;

    bool operator!=(const RuntimeType& b) const;

    // not like operator ==, operator < compare hasNull
    bool operator<(const RuntimeType& b) const;

    static bool IsOwnMemoryType(RuntimeTypePtr type);

    //It's legacy method which Only is used for streamline, it's abandoned in next version
    bool IsVarLen() const;
    bool isComplexType() const;
    bool isColumnarType() const {
        return mType == RT_COLUMNAR_ARRAY || mType == RT_COLUMNAR_MAP || mType == RT_COLUMNAR_STRUCT;
    }
    bool isJsonType() const {
        return mType == RT_JSON;
    }

    std::string const& GetSubTypeName(uint32_t idx) const { return mSubTypes[idx].first; }

    RuntimeTypePtr GetSubType(uint32_t idx) const { return mSubTypes[idx].second; }

    uint32_t GetSubTypesCount() const { return mSubTypes.size(); }

    // used for RT_JSON
    RuntimeTypePtr GetStaticType() const {
        if (mType != RT_JSON) {
            abort();
        }
        return mSubTypes.size() ? mSubTypes[0].second : nullptr;
    }

    /* used for decimal new start */
    // high bits used to store precision and low bits store scale, precision range [1, 38], scale range [0, precision]
    int32_t GetPrecision() const { return int32_t(mTypeInfo >> 32); }

    int32_t GetScale() const { return int32_t(mTypeInfo & 0x00000000FFFFFFFF); }

    // used for char and varchar
    uint32_t GetMaxLength() const { return static_cast<uint32_t>(mTypeInfo); }

    void SetMaxLength(uint32_t maxLength) { mTypeInfo = maxLength; }

    bool IsNullable() const { return mHasNull; }

    std::string ToString() const { return RuntimeTypeToString(this); }

private:
    RuntimeType();

    explicit RuntimeType(TypeCategory type, bool hasNull = true);

    RuntimeType(TypeCategory type, int64_t typeInfo, bool hasNull = true);

    RuntimeType(TypeCategory type, int32_t precision, int32_t scale, bool hasNull = true);

    void SetPrecisionAndScale(int32_t precision, int32_t scale);

    void SetPrecision(int32_t precision);

    void SetScale(int32_t scale);

    static uint64_t GetTypeInfo(int32_t precision, int32_t scale);

    static RuntimeTypePtr insertAndGetComplex(apsara::odps::RuntimeType&& t);

//  TODO: modify class member to private
public:
    bool mHasNull;
    int64_t mTypeInfo;
    std::vector<std::pair<std::string, RuntimeTypePtr> > mSubTypes;

    friend class RuntimeTypePool;
};

} // namespace odps
} // namespace apsara
#endif
