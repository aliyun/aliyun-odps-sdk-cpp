#ifndef RUNTIME_HASH_H
#define RUNTIME_HASH_H

#include "include/runtime/runtime_interval.h"
#include "include/runtime/runtime_timestamp.h"


#define __TRUE 0x172ba9c7
#define __FALSE -0x3a59cb12

/*
 * MAKE SURE this is corresponding with task/sql_task/execution_engine/ir/hash_ir.cpp
 */
namespace apsara
{
namespace odps
{

/* 
 * step1: get hash value for every column, if it's null, hash value is 0
 * step2: use CombineHash for all hash values you get from step1, and you will get one new hash value
 * step3: use FinalHash for the new hash value, and this is the final result
 */
class RuntimeDefaultHasher
{

public:

static int32_t CombineHash(int32_t hash1, int32_t hash2)
{
    return hash1 + hash2;
}

static int32_t FinalHash(int32_t hashVal)
{
    return hashVal ^ (hashVal >> 8);
}


static int32_t HashBigint(int64_t a)
{
    a = (~a) + (a << 18);
    a ^= (a >> 31);
    a *= 21;
    a ^= (a >> 11);
    a += (a << 6);
    a ^= (a >> 22);
    return (int32_t)a;
};

static int32_t HashFloat(float a)
{
    return HashBigint(*(reinterpret_cast<int32_t*>(&a)));
};

static int32_t HashDouble(double a)
{
    return HashBigint(*(reinterpret_cast<int64_t*>(&a)));
};

static int32_t HashBool(bool a)
{
    return a ? __TRUE : __FALSE;
};

static int32_t HashString(const char *key, size_t len)
{
    int32_t hash = 0;
    for(uint32_t i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
};

// static int32_t HashDecimal(const apsara::odps::RuntimeDecimal& value)
// {
//     return value.GetHash();
// };

// static int32_t HashDecimalNew(const apsara::odps::RuntimeDecimalVal& value)
// {
//     return value.GetHash();
// };

static int32_t HashTimestamp4Row(const apsara::odps::RuntimeTimestamp& value)
{
    int64_t seconds = value.GetSeconds();
    seconds <<= 30;  // the nanosecond part fits in 30 bits
    seconds |= value.GetNanos();
    return HashBigint(seconds);
};


static int32_t HashIntervalDayTime4Row(const apsara::odps::RuntimeIntervalDayTime& value)
{
    int64_t seconds = value.GetTotalSeconds();
    seconds <<= 30;  // the nanosecond part fits in 30 bits
    seconds |= value.GetNanos();
    return HashBigint(seconds);
};

// static int32_t HashArray(const apsara::odps::RuntimeArray& value)
// {
//     return value.GetHash();
// }

// static int32_t HashMap(const apsara::odps::RuntimeMap& value)
// {
//     return value.GetHash();
// }

// static int32_t HashStruct(const apsara::odps::RuntimeStruct& value)
// {
//     return value.GetHash();
// }


private:
RuntimeDefaultHasher(){};

};
}
}

#endif
