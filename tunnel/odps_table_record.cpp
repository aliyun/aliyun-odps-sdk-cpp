#include <string.h>
#include <time.h>
#include <sstream>
#include <string>
#include <iomanip>
#include "odps_exception.h"
#include "util/utils.h"

#include "odps_tunnel.h"
#include "util.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::util;
using namespace apsara::odps::sdk::internal::tunnel;

ODPSTableRecord::ODPSTableRecord(const std::shared_ptr<IODPSTableSchema>& schema)
    :mSchema(schema)
{
    mColCount = mSchema->GetColumnCount();
    mRawData =  new unsigned char[mColCount * sizeof(int64_t)];
    memset(mRawData, 0, mColCount * sizeof(int64_t));

    for (uint32_t i = 0; i < mColCount; i++)
    {
        mIsNullValue.push_back(true);
    }
}

ODPSTableRecord::~ODPSTableRecord()
{
    delete[] mRawData;
}

IODPSTableSchema* ODPSTableRecord::GetSchema()
{
    return mSchema.get();
}

std::string ODPSTableRecord::GetDatetime(uint32_t idx) const
{
    CheckNull(idx);
    int64_t t = *GetDatetimeValue(idx);
    // convert milliseconds to seconds
    return gmt_strftime(t / 1000, TUNNEL_DATE_TIME_FORMAT);
}

std::string ODPSTableRecord::GetDate(uint32_t idx) const
{
    CheckNull(idx);
    int64_t t = *GetDateValue(idx);
    // convert days to seconds
    return gmt_strftime(t * SECONDS_PER_DAY, TUNNEL_DATE_FORMAT);
}

const int64_t* ODPSTableRecord::GetIntValue(uint32_t idx, ODPSColumnType type) const
{
    CheckType(idx, type);

    if (mIsNullValue[idx])
    {
        return NULL;
    }

    return (int64_t*)(mRawData + idx * sizeof(int64_t));
}

const float* ODPSTableRecord::GetFloatValue(uint32_t idx) const
{
    CheckType(idx, ODPS_FLOAT);

    if (mIsNullValue[idx])
    {
        return NULL;
    }

    return (float*)(mRawData + idx * sizeof(int64_t));
}

const double* ODPSTableRecord::GetDoubleValue(uint32_t idx) const
{
    CheckType(idx, ODPS_DOUBLE);

    if (mIsNullValue[idx])
    {
        return NULL;
    }

    return (double*)(mRawData + idx * sizeof(int64_t));
}

const bool* ODPSTableRecord::GetBoolValue(uint32_t idx) const
{
    CheckType(idx, ODPS_BOOLEAN);

    if (mIsNullValue[idx])
    {
        return NULL;
    }

    return (bool*)(mRawData + idx * sizeof(int64_t));
}

const char* ODPSTableRecord::GetStringValue(uint32_t idx, uint32_t &len, ODPSColumnType type) const
{
    CheckType(idx, type);

    unsigned char *str = (mRawData + idx * sizeof(int64_t));
    std::string** s =(std::string**)str;
    if (mIsNullValue[idx])
    {
        return NULL;
    }
    else
    {
        len = (*s)->size();
        return (*s)->c_str();
    }
}

IODPSProtoSerializablePtr ODPSTableRecord::GetComplexValue(uint32_t idx, ODPSColumnType type) const
{
    CheckType(idx, type);

    if (mIsNullValue[idx])
    {
        return IODPSProtoSerializablePtr();
    }
    return mComplexColBufs.at(idx);
}

const TimeStamp* ODPSTableRecord::GetTimeValue(uint32_t idx, ODPSColumnType type) const
{
    CheckType(idx, type);

    if (mIsNullValue[idx])
    {
        return NULL;
    }

    return *((TimeStamp**)(mRawData + idx * sizeof(int64_t)));
}

void ODPSTableRecord::SetDatetimeValue(uint32_t idx, const std::string& datetime)
{
    // seconds -> milliseconds
    SetDatetimeValue(idx, gmt_strptime(datetime, TUNNEL_DATE_TIME_FORMAT) * 1000);
}

void ODPSTableRecord::SetDateValue(uint32_t idx, const std::string& date)
{
    // seconds -> days
    SetDateValue(idx, gmt_strptime(date, TUNNEL_DATE_FORMAT) / SECONDS_PER_DAY);
}

void ODPSTableRecord::SetIntValue(uint32_t idx, int64_t value, ODPSColumnType type)
{
    CheckType(idx, type);

    *((int64_t*)(mRawData + idx * sizeof(int64_t))) = value;
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetFloatValue(uint32_t idx, float value)
{
    CheckType(idx, ODPS_FLOAT);

    *((float*)(mRawData + idx * sizeof(int64_t))) = value;
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetDoubleValue(uint32_t idx, double value)
{
    CheckType(idx, ODPS_DOUBLE);

    *((double*)(mRawData + idx * sizeof(int64_t))) = value;
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetBoolValue(uint32_t idx, bool value)
{
    CheckType(idx, ODPS_BOOLEAN);

    *((bool*)(mRawData + idx * sizeof(int64_t))) = value;
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetStringValue(uint32_t idx, const char* value, uint32_t len, ODPSColumnType type)
{
    CheckType(idx, type);

    if (len > mSchema->GetMaxFieldSize())
    {
        throw OdpsException(INVALID_ARGUMENT, "The string's length is more than " + std::to_string(mSchema->GetMaxFieldSize()) + " bytes.");
    }

    if (value == NULL)
    {
        throw OdpsException(INVALID_ARGUMENT, "can not set null value through SetStringValue, use SetNullValue.");
    }

    std::string* buf = &mStrColBufs[idx];
    buf->assign(value, len);
    memcpy(mRawData + idx * sizeof(int64_t), &buf, sizeof(std::string*));
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetComplexValue(uint32_t idx, IODPSProtoSerializablePtr value, ODPSColumnType type)
{
    CheckType(idx, type);
    mComplexColBufs[idx] = value;
    reinterpret_cast<int64_t*>(mRawData)[idx] = idx;
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetTimeValue(uint32_t idx, int64_t sec, int32_t ns, ODPSColumnType type)
{
    CheckType(idx, type);

    mTsColBufs[idx] = TimeStamp(sec, ns);
    TimeStamp* ts = &mTsColBufs[idx];
    memcpy(mRawData + idx * sizeof(int64_t), &ts, sizeof(TimeStamp*));
    mIsNullValue[idx] = false;
}

void ODPSTableRecord::SetNullValue(uint32_t idx)
{
    if (idx > mSchema->GetColumnCount() - 1)
    {
        throw OdpsException(INVALID_ARGUMENT, "index out of boundary.");
    }

    mIsNullValue[idx] = true;
}

bool ODPSTableRecord::IsNullValue(uint32_t idx) const
{
    return mIsNullValue[idx];
}

void ODPSTableRecord::CheckType(uint32_t idx, ODPSColumnType type) const
{
    if (mSchema->GetTableColumn(idx).GetType() != type)
    {
        util::OdpsThrow(INVALID_ARGUMENT, "column type invalid. Required column type is", GetTypeName(type));
    }
}

int64_t ODPSTableRecord::GetRecordSize() const
{
    int64_t sum = 0;
    sum += mColCount * sizeof(uint64_t);
    sum += sizeof(bool) * mIsNullValue.size();

    sum += mStrColBufs.size() * sizeof(decltype(mStrColBufs)::key_type);
    for(auto it: mStrColBufs)
    {
        sum += it.second.size();
    }

    sum += mTsColBufs.size() * sizeof(decltype(mTsColBufs)::key_type);
    sum += mTsColBufs.size() * sizeof(decltype(mTsColBufs)::value_type);

    sum += mComplexColBufs.size() * sizeof(decltype(mComplexColBufs)::key_type);
    for(auto it: mComplexColBufs)
    {
        sum += it.second->GetInMemorySize();
    }

    return sum;
}
