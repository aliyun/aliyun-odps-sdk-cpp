#ifndef APSARA_ODPS_TUNNEL_HASH_HELPER_H
#define APSARA_ODPS_TUNNEL_HASH_HELPER_H


#include <mutex>
#include <vector>

#include "runtime/runtime_types.h"
#include "runtime/runtime_hash.h"
#include "odps_table.h"

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

class HashHelper{

public:

    static int32_t GetHashVal(UpsertRecord& r, uint32_t index)
    {
        ODPSColumnType type = r.GetSchema()->GetTableColumn(index).GetType();
        int32_t hashVal = 0;
        switch(type)
        {
            case ODPS_TINYINT:
            {
                int8_t value = r.GetTinyInt(index);
                hashVal = RuntimeDefaultHasher::HashBigint(value);
                break;
            }
            case ODPS_BIGINT:
            {
                int64_t value = r.GetBigInt(index);
                hashVal = RuntimeDefaultHasher::HashBigint(value);
                break;
            }
            case ODPS_INTEGER:
            {
                int32_t value = r.GetInteger(index);
                hashVal = RuntimeDefaultHasher::HashBigint(value);
                break;
            }
            case ODPS_SMALLINT:
            {
                int16_t value = r.GetSmallInt(index);
                hashVal = RuntimeDefaultHasher::HashBigint(value);
                break;
            }
            case ODPS_DOUBLE:
            {
                double value = r.GetDouble(index);
                hashVal = RuntimeDefaultHasher::HashDouble(value);
                break;
            }
            case ODPS_FLOAT:
            {
                float value = r.GetFloat(index);
                hashVal = RuntimeDefaultHasher::HashFloat(value);
                break;
            }
            case ODPS_BOOLEAN:
            {
                bool value = r.GetBool(index);
                hashVal = RuntimeDefaultHasher::HashBool(value);
                break;
            }
            case ODPS_STRING:
            {
                std::string str = r.GetString(index);
                hashVal = RuntimeDefaultHasher::HashString(str.c_str(), str.length());
                break;
            }
            case ODPS_CHAR:
            {
                std::string str = r.GetChar(index);
                hashVal = RuntimeDefaultHasher::HashString(str.c_str(), str.length());
                break;
            }
            case ODPS_VARCHAR:
            {
                std::string str = r.GetVarchar(index);
                hashVal = RuntimeDefaultHasher::HashString(str.c_str(), str.length());
                break;
            }
            default:
            {
                std::string typeName = r.GetSchema()->GetTableColumn(index).GetTypeInfo().ToTypeString();
                throw OdpsTunnelException(NOT_IMPLEMENTED, "Unsupported upsert key type name: " + typeName);
            }
        }
        return hashVal;
    }

    static int32_t GetHasher(UpsertRecord& r, std::vector<std::string> hashKeys)
    {
        std::vector<int32_t> hashVals;
        const std::map<std::string, uint32_t> colName2id = r.GetColName2Id();
        for(uint32_t i = 0; i < hashKeys.size(); i++)
        {
            std::map<std::string, uint32_t>::const_iterator it = colName2id.find(hashKeys[i]);
            if (it != colName2id.end())
            {
                uint32_t index = it->second;
                hashVals.emplace_back(GetHashVal(r, index));
            }
        }  

        int32_t hashVal = 0;
        for(auto i : hashVals)
        {
            hashVal = RuntimeDefaultHasher::CombineHash(hashVal, i);
        }

        return RuntimeDefaultHasher::FinalHash(hashVal);
    }
};

}  // namespace tunnel
}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
