#ifndef APSARA_ODPS_EXECUTION_ENGINE_COMMON_RUNTIME_STRUCT_INFO_H
#define APSARA_ODPS_EXECUTION_ENGINE_COMMON_RUNTIME_STRUCT_INFO_H

#include <stdint.h>
#include <map>
#include "runtime_types.h"

namespace apsara
{
namespace odps
{

struct StructColumnInfo
{
    StructColumnInfo(int64_t dataOffset, uint32_t columnIndex)
        : mDataOffset(dataOffset)
        , mColumnIndex(columnIndex)
    {
    }

    int64_t mDataOffset;
    uint32_t mColumnIndex;
};

struct RuntimeStructInfo
{
    explicit RuntimeStructInfo(const RuntimeTypePtr type);

    virtual ~RuntimeStructInfo() {}

    inline int64_t GetOffsetByIndex(uint32_t index) const {
        return mIndex2Offset[index]->mDataOffset;
    }

    std::map<std::string, StructColumnInfo> mName2Offset;
    int64_t mFixedLength;
    std::vector<const StructColumnInfo*> mIndex2Offset;
};

struct JsonStructInfo : public RuntimeStructInfo
{
    explicit JsonStructInfo(const RuntimeTypePtr type);
    ~JsonStructInfo() {
        for (const StructColumnInfo* info : mIndex2Offset) {
            delete info;
        }
    }
};

} // namespace odps
} // namespace apsara

#endif //APSARA_ODPS_EXECUTION_ENGINE_COMMON_RUNTIME_STRUCT_INFO_H
