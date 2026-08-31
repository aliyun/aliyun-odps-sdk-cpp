#ifndef APSARA_ODPS_EXECUTION_ENGINE_RUNTIME_TYPE_CATEGORY_H
#define APSARA_ODPS_EXECUTION_ENGINE_RUNTIME_TYPE_CATEGORY_H

#include <stdint.h>
#include <string>

namespace apsara
{
namespace odps
{

enum TypeCategory : uint16_t
{
    // Primitive Types. enum ordered in typcial usage frequency and/or size
    RT_NULL = 0x0,
    RT_VOID = 'v',
    RT_TINYINT = 'a',
    RT_SMALLINT = 't',
    RT_INT = 'i',
    RT_BIGINT = 'x',
    RT_FLOAT = 'f',
    RT_DOUBLE = 'd',
    RT_BOOLEAN = 'b',
    RT_STRING = 'S',
    RT_CHAR = 'C',
    RT_VARCHAR = 'V',
    RT_BINARY = 'B',
    RT_DATETIME = 'T',
    RT_DATE = 'D',
    RT_TIMESTAMP = 'P',
    RT_TIMESTAMP_NTZ = 'Q',
    RT_INTERVAL_YEAR_MONTH = 'Y',
    RT_INTERVAL_DAY_TIME = 'I',
    RT_DECIMAL = 'F',
    RT_DECIMAL_NEW = 'E',
    RT_DECIMAL_NEW_64 = 0x4464, // Dd
    RT_DECIMAL_NEW_128 = 0x4465, // De
    RT_DECIMAL_NEW_32 = 0x4466, // Df
    RT_DECIMAL_NEW_16 = 0x4468, // Dh
    RT_ARRAY = 'A',
    RT_MAP = 'M',
    RT_STRUCT = 'U',
    RT_JSON = 'J',
    RT_COLUMNAR_ARRAY = 'r',
    RT_COLUMNAR_MAP = 'm',
    RT_COLUMNAR_STRUCT = 's',
    RT_DICTIONARY_ENCODING_STRING = 'K',
    RT_ROARING_BITMAP_32 = 0x5269, // Ri
    RT_ROARING_BITMAP_64 = 0x5278, // Rx
    // UDT
    RT_UDTYPE = 0xF0
};

//Some UDTs' implemention which can inherit it
struct UDType {};

//It provides the interfaces if call func like IsXXXType.
struct WhichRuntimeType
{
    TypeCategory mType;

    WhichRuntimeType() : mType(static_cast<TypeCategory>(0xFFFFFFFF)) {}
    WhichRuntimeType(TypeCategory type) : mType(type) {}

    //Only implement some ones already_used, please extends others if necessary to use.
    bool IsBoolean() const {return mType == RT_BOOLEAN;}
    bool IsTimestamp() const {return mType == RT_TIMESTAMP || mType == RT_TIMESTAMP_NTZ;}
    bool IsDate() const {return mType == RT_DATE;}
    bool IsIntervalDayTime() const {return mType == RT_INTERVAL_DAY_TIME;}
    bool IsIntervalYearMonth() const {return mType == RT_INTERVAL_YEAR_MONTH;}
    bool IsDecimal() const {return mType == RT_DECIMAL;}
    bool IsDecimalVal() const {return mType == RT_DECIMAL_NEW;}
    bool IsDecimalVal128() const {return mType == RT_DECIMAL_NEW_128;}
    bool IsArray() const {return mType == RT_ARRAY || mType == RT_COLUMNAR_ARRAY;}
    bool IsMap() const {return mType == RT_MAP || mType == RT_COLUMNAR_MAP;}
    bool IsStruct() const {return mType == RT_STRUCT || mType == RT_COLUMNAR_STRUCT;}
    bool IsVariableLengthDecimal() const
    {
        return mType == RT_DECIMAL_NEW_16 || mType == RT_DECIMAL_NEW_32 ||
            mType == RT_DECIMAL_NEW_64 || mType == RT_DECIMAL_NEW_128;
    }
    bool IsJson() const {return mType == RT_JSON;}
    bool IsNullType() const {return mType == RT_NULL;}
    bool IsBinary() const {return mType == RT_BINARY;}
};

inline std::string TypeCategoryToString(TypeCategory type)
{
    if (type < 256)
    {
        return std::string(1, (char)type);
    }
    else
    {
        return std::string(1, (char)(type >> 8)) + std::string(1, (char)type);
    }
}

std::string GetName(enum TypeCategory type);

}
}

#endif
