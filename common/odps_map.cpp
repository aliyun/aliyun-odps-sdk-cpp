#include "error_code.h"
#include "odps_exception.h"
#include "odps_map.h"
#include "util/utils.h"
#include <utility>
#include <vector>
#include <algorithm>

#include "util/utils.h"
using namespace apsara::odps::sdk::util;

using namespace std;

namespace apsara { namespace odps { namespace sdk {

static std::string MapKeyTypeToCTypeString(ODPSColumnType keytype)
{
    switch(keytype)
    {
        case ODPS_BIGINT:
            return "int64_t";
        case ODPS_FLOAT:
            return "float";
        case ODPS_DOUBLE:
            return "double";
        case ODPS_STRING:
            return "std::string";
        case ODPS_BOOLEAN:
            return "bool";
        case ODPS_TIMESTAMP:
            return "TimeStamp";
        default:
            return "UNKNOWN";
    }
}


#define KeyCTypeCheckGenerator(CTYPE, KEYTYPE) \
template <> void ODPSMap::KeyCTypeCheck<CTYPE>() const \
{ \
if (mKeyCType != KEYTYPE) \
    { \
        throw OdpsTunnelException("KeyType mismatch for ODPSMap: require " + \
                                  MapKeyTypeToCTypeString(KEYTYPE) + \
                                  " found " + typeid(CTYPE).name()); \
    } \
}

// supported types are:
KeyCTypeCheckGenerator(int64_t, ODPS_BIGINT);
KeyCTypeCheckGenerator(float, ODPS_FLOAT);
KeyCTypeCheckGenerator(double, ODPS_DOUBLE);
KeyCTypeCheckGenerator(std::string, ODPS_STRING);
KeyCTypeCheckGenerator(bool, ODPS_BOOLEAN);

#undef KeyCTypeCheckGenerator

template <> void ODPSMap::KeyCTypeCheck<TimeStamp>() const
{
if (mKeyCType != ODPS_TIMESTAMP && mKeyCType != ODPS_TIMESTAMP_NTZ)
    {
        throw OdpsTunnelException("KeyType mismatch for ODPSMap: require " +
                                  MapKeyTypeToCTypeString(ODPS_TIMESTAMP) +
                                  " found " + typeid(TimeStamp).name());
    }
}

static bool MapKeyTypeSupported(ODPSColumnType ctype)
{
    switch(ctype)
    {
        case ODPS_BIGINT:
        case ODPS_FLOAT:
        case ODPS_DOUBLE:
        case ODPS_STRING:
        case ODPS_BOOLEAN:
        case ODPS_TIMESTAMP:
        case ODPS_TIMESTAMP_NTZ:
            return true;
        default:
            return false;
    }
}

#define GET_KEY_INDEX_MAP_SIZE(KIMAP) \
    ((KIMAP).size() * (sizeof(std::decay<decltype(KIMAP)>::type::key_type) + sizeof(std::decay<decltype(KIMAP)>::type::value_type)))

int64_t ODPSMap::GetInMemorySize() const
{
    int64_t sum = 0;
    sum += mUnderlyingArray->GetInMemorySize();
    switch(mKeyCType)
    {
        case ODPS_BIGINT:
            {
                const auto& kiMap = CastToKeyIndexMap<int64_t>();
                sum += GET_KEY_INDEX_MAP_SIZE(kiMap);
                break;
            }
        case ODPS_FLOAT:
            {
                const auto& kiMap = CastToKeyIndexMap<float>();
                sum += GET_KEY_INDEX_MAP_SIZE(kiMap);
                break;
            }
        case ODPS_DOUBLE:
            {
                const auto& kiMap = CastToKeyIndexMap<double>();
                sum += GET_KEY_INDEX_MAP_SIZE(kiMap);
                break;
            }
        case ODPS_STRING:
            {
                const auto& kiMap = CastToKeyIndexMap<string>();
                sum += kiMap.size() * sizeof(std::decay<decltype(kiMap)>::type::value_type);
                for(auto it: kiMap)
                {
                    sum += it.first.size();
                }
                break;
            }
        case ODPS_BOOLEAN:
            {
                const auto& kiMap = CastToKeyIndexMap<bool>();
                sum += GET_KEY_INDEX_MAP_SIZE(kiMap);
                break;
            }
        case ODPS_TIMESTAMP:
            {
                const auto& kiMap = CastToKeyIndexMap<TimeStamp>();
                sum += GET_KEY_INDEX_MAP_SIZE(kiMap);
                break;
            }
        case ODPS_TIMESTAMP_NTZ:
            {
                const auto& kiMap = CastToKeyIndexMap<TimeStamp>();
                sum += GET_KEY_INDEX_MAP_SIZE(kiMap);
                break;
            }
        default:
            break;
    }
    return sum;
}

const ODPSColumnTypeInfo& ODPSMap::GetKeyTypeInfo() const
{
    return mTypeInfo.mSubTypes[0];
}

const ODPSColumnTypeInfo& ODPSMap::GetValueTypeInfo() const
{
    return mTypeInfo.mSubTypes[1];
}

#define CASE_MAP_CREATE(KEYCTYPE, CTYPE) \
    case KEYCTYPE: \
    mKeyToArrayIndexMap = make_shared<KeyIndexMapType<CTYPE>>(); \
    break;

ODPSMap::ODPSMap(const ODPSColumnTypeInfo& typeinfo):
    mTypeInfo(typeinfo)
{
    if (mTypeInfo.mType != ODPS_MAP ||
        mTypeInfo.mSubTypes.size() != 2)
    {
        throw OdpsTunnelException("Creating ODPSMap with non-map type");
    }
    if (!MapKeyTypeSupported(GetKeyTypeInfo().mType))
    {
        throw OdpsTunnelException("Unsupported Key type for ODPSMap: " + GetTypeName(GetKeyTypeInfo().mType));
    }
    mKeyCType = GetKeyTypeInfo().mType;
    mValueArrayType.mType = ODPS_ARRAY;
    mValueArrayType.mSubTypes.push_back(GetValueTypeInfo());
    mUnderlyingArray = std::make_shared<ODPSArray>(mValueArrayType);
    // create maps
    switch(mKeyCType)
    {
        CASE_MAP_CREATE(ODPS_BIGINT, int64_t);
        CASE_MAP_CREATE(ODPS_DOUBLE, double);
        CASE_MAP_CREATE(ODPS_FLOAT, float);
        CASE_MAP_CREATE(ODPS_TIMESTAMP, TimeStamp);
        CASE_MAP_CREATE(ODPS_TIMESTAMP_NTZ, TimeStamp);
        CASE_MAP_CREATE(ODPS_BOOLEAN, bool);
        CASE_MAP_CREATE(ODPS_STRING, std::string);
        default:
        throw OdpsTunnelException("Should not reach here");
    }
}

ODPSMap::~ODPSMap() {}

const ODPSColumnTypeInfo& ODPSMap::GetTypeInfo() const
{
    return mTypeInfo;
}

int64_t ODPSMap::Size() const
{
    return mUnderlyingArray->Size();
}

#define GETKEYS_IMPL(KEYTYPE, SCHEMA_STRING, SIGNTYPE, CASTTYPE) \
{ \
    using IndexKeyPair = pair<uint32_t, KEYTYPE>; \
    vector<IndexKeyPair> indices; \
    for(auto i: CastToKeyIndexMap<KEYTYPE>()) \
    { \
        indices.emplace_back(i.second, i.first); \
    } \
    std::sort(indices.begin(), indices.end(), [](const IndexKeyPair& a, const IndexKeyPair& b) { return a.first < b.first; }); \
    ODPSColumnTypeInfo _typeinfo = ODPSColumnTypeInfo::ParseTypeInfoString(SCHEMA_STRING); \
    std::shared_ptr<ODPSArray> array = make_shared<ODPSArray>(_typeinfo); \
    for(auto i: indices) \
    { \
        array->Append##SIGNTYPE##Value(CASTTYPE(i.second)); \
    } \
    return array; \
}


shared_ptr<ODPSArray> ODPSMap::GetKeys() const
{
    switch(GetKeyTypeInfo().mType)
    {
        case ODPS_BIGINT:
            GETKEYS_IMPL(int64_t, "ARRAY<BIGINT>", BigInt, int64_t);
            break;
        case ODPS_DOUBLE:
            GETKEYS_IMPL(double, "ARRAY<DOUBLE>", Double, double);
            break;
        case ODPS_FLOAT:
            GETKEYS_IMPL(float, "ARRAY<FLOAT>", Float, float);
            break;
        case ODPS_DATETIME:
            GETKEYS_IMPL(int64_t, "ARRAY<DATETIME>", Datetime, int64_t);
            break;
        case ODPS_TIMESTAMP:
            GETKEYS_IMPL(TimeStamp, "ARRAY<TIMESTAMP>", Timestamp, TimeStamp);
            break;
        case ODPS_TIMESTAMP_NTZ:
            GETKEYS_IMPL(TimeStamp, "ARRAY<TIMESTAMP_NTZ>", TimestampNTZ, TimeStamp);
            break;
        case ODPS_BOOLEAN:
            GETKEYS_IMPL(bool, "ARRAY<BOOLEAN>", Bool, bool);
            break;
        case ODPS_STRING:
            GETKEYS_IMPL(std::string, "ARRAY<STRING>", String, std::string);
            break;
        default:
            throw OdpsTunnelException("Unsupported Map Keytype: " + GetTypeName(GetKeyTypeInfo().mType));
    }
}

#define RECOVER_KEYS_IMPL(ODPSTYPE, CTYPE, SIGNTYPE) \
case ODPSTYPE: \
{ \
    for(uint32_t i = 0; i < keyArray->Size(); i++) \
    { \
        KeyIndexMapType<CTYPE>& indexMap = CastToKeyIndexMap<CTYPE>(); \
        indexMap[keyArray->Get##SIGNTYPE(i)] = i; \
    } \
} \
break

void ODPSMap::Recover(std::shared_ptr<ODPSArray> keyArray, std::shared_ptr<ODPSArray> valueArray)
{
    // we are not re-appending these values. instead, we just reset data structures..
    mUnderlyingArray = valueArray;
    switch(GetKeyTypeInfo().mType)
    {
        RECOVER_KEYS_IMPL(ODPS_BIGINT, int64_t, BigInt);
        RECOVER_KEYS_IMPL(ODPS_DOUBLE, double, Double);
        RECOVER_KEYS_IMPL(ODPS_FLOAT, float, Float);
        RECOVER_KEYS_IMPL(ODPS_DATETIME, int64_t, DatetimeValue);
        RECOVER_KEYS_IMPL(ODPS_TIMESTAMP, TimeStamp, Timestamp);
        RECOVER_KEYS_IMPL(ODPS_TIMESTAMP_NTZ, TimeStamp, TimestampNTZ);
        RECOVER_KEYS_IMPL(ODPS_BOOLEAN, bool, Bool);
        RECOVER_KEYS_IMPL(ODPS_STRING, std::string, String);
        default:
            throw OdpsTunnelException("Unsupported Map Keytype: " + GetTypeName(GetKeyTypeInfo().mType));
    }
}

}}}