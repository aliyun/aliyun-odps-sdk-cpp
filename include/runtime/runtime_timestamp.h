#ifndef APSARA_ODPS_EXECUTION_ENGINE_TIMESTAMP_H_
#define APSARA_ODPS_EXECUTION_ENGINE_TIMESTAMP_H_

#include <stdint.h>
#include <assert.h>
#include <cstdlib>
#include "odps_util_time.h"

namespace apsara
{
namespace odps
{

class RuntimeTimestamp
{
public:
    RuntimeTimestamp():mSecond(0), mNano(0)
    {}

    RuntimeTimestamp(int64_t second, int32_t nano)
        : mSecond(second), mNano(nano)
    {}

    RuntimeTimestamp(int32_t second)
        : mSecond(second), mNano(0)
    {}

    RuntimeTimestamp(const std::string& timestr, TIMEZONE_OFFSET tzoff)
    {
        ValueOf(timestr, tzoff);
    }

    RuntimeTimestamp& operator = (const RuntimeTimestamp& other)
    {
        if (&other == this)
        {
            return *this;
        }

        mSecond = other.mSecond;
        mNano = other.mNano;
        return *this;
    }

    bool operator == (const RuntimeTimestamp& dec) const
    {
        if (&dec == this)
            return true;
        return mSecond == dec.mSecond ?
            (mNano == dec.mNano ? true : false) : false;
    }

    bool operator != (const RuntimeTimestamp& dec) const
    {
        if (&dec == this)
            return false;
        return mSecond == dec.mSecond ?
            (mNano == dec.mNano ? false : true) : true;
    }

    bool operator > (const RuntimeTimestamp& dec) const
    {
        if (&dec == this)
            return false;
        return mSecond > dec.mSecond ?
            true :
            (mSecond == dec.mSecond && mNano > dec.mNano) ?
            true : false;
    }

    bool operator < (const RuntimeTimestamp& dec) const
    {
        if (&dec == this)
            return false;
        return mSecond < dec.mSecond ?
            true :
            (mSecond == dec.mSecond && mNano < dec.mNano) ?
            true : false;
    }

    bool operator >= (const RuntimeTimestamp& dec) const
    {
        if (&dec == this)
            return true;
        return mSecond > dec.mSecond ? true :
            mSecond == dec.mSecond && mNano >= dec.mNano ?
            true : false;
    }

    bool operator <= (const RuntimeTimestamp& dec) const
    {
        if (&dec == this)
            return true;
        return mSecond < dec.mSecond ? true :
            mSecond == dec.mSecond && mNano <= dec.mNano ?
            true : false;
    }

    void Set(const int64_t& second, const int32_t& nano)
    {
        mSecond = second;
        mNano = nano;
    }

    void Set(const RuntimeTimestamp& val)
    {
        if (&val == this)
        {
            return;
        }
        mSecond = val.mSecond;
        mNano = val.mNano;
    }

    void SetNull()
    {
        mSecond = 0x8000000000000000;
        mNano = 0;
    }

    int32_t GetMillis() const
    {
        return mNano / 1000000;
    }

    int64_t GetTime() const
    {
        return mSecond * 1000 + mNano / 1000000;
    }

    void SetTime(int64_t time)
    {
        mSecond = time / 1000;
        mNano = (time % 1000) * 1000000;
        if (mNano < 0)
        {
            mNano = 1000000000 + mNano;
            mSecond -= 1;
        }
    }

    double GetDouble() const
    {
        return (double)mSecond + (double)mNano / 1000000000;
    }

    int64_t GetSeconds() const
    {
        return mSecond;
    }

    void SetSeconds(int64_t second)
    {
        mSecond = second;
    }

    int32_t GetNanos() const
    {
        return mNano;
    }

    void SetNanos(int32_t nano)
    {
        mNano = nano;
    }

    int32_t GetSize() const
    {
        return sizeof(int64_t) + sizeof(int32_t);
    }

    void GetData(char* buf, int32_t size) const
    {
        assert(size >= GetSize());
        *(int64_t*)(buf) = mSecond;
        *(int32_t*)(buf + sizeof(mSecond)) = mNano;
    }

    void SetData(char* buf, int32_t size)
    {
        if (buf == NULL || size == 0)
        {
            SetNull();
        }
        else
        {
            SetSeconds(*(int64_t*)(buf));
            SetNanos(*(int32_t*)(buf + sizeof(int64_t)));
        }
    }

    int32_t GetHash() const
    {
       int64_t seconds = GetSeconds();
       seconds <<= 30;  // the nanosecond part fits in 30 bits
       seconds |= GetNanos();
       return (int) ((((unsigned long long)seconds) >> 32) ^ seconds);
    }

    bool ValueOf(const std::string& timestr, apsara::odps::TIMEZONE_OFFSET tzoff)
    {
        return ValueOf(timestr, mSecond, mNano, tzoff);
    }

    std::string ToString(const apsara::odps::TIMEZONE_OFFSET tzoff) const
    {
        return FormatTime(mSecond, mNano, tzoff);
    }

    static RuntimeTimestamp GetCurrentTimestamp()
    {
        struct timespec ts;
        RuntimeTimestamp time;
        clock_gettime(CLOCK_REALTIME, &ts);
        time.Set(ts.tv_sec, ts.tv_nsec);
        return time;
    }

    static bool ValueOf(char const* begin, char const* end, tm& resultTm,
            int32_t& resultNano, bool enableDateOnlyFormat = false);

    static bool ValueOf(const std::string& timestr, int64_t& result_s, int32_t& result_ns, apsara::odps::TIMEZONE_OFFSET tzoff)
    {
        char const* begin = timestr.data();
        char const* end = begin + timestr.size();
        tm resultTm;
        bool res = ValueOf(begin, end, resultTm, result_ns);
        if (!res) {
            return false;
        }
        result_s = MakeTime(&resultTm, tzoff);
        return true;
    }

    // To calculate NDV correctly, we need to zero out padding bytes in struct/class
    // Please ref: https://work.aone.alibaba-inc.com/issue/21525743
    // we do not need packResultBuf here because the padding bytes are continuous and at the end
    static std::pair<void*, uint32_t> GetPackedData(const RuntimeTimestamp* data, char* packResultBuf) {
        uint32_t len = sizeof(int64_t)               // for mSecond
                     + sizeof(int32_t);              // for mNano
        return std::make_pair((void*)data, len);
    }

protected:
    // NOTE: if you change the following members, you MUST also consider to change method GetPackedData().
    int64_t mSecond;
    int32_t mNano;
};

class RuntimeTimestampNtz : public RuntimeTimestamp
{
public:
    RuntimeTimestampNtz():RuntimeTimestamp()
    {}

    RuntimeTimestampNtz(int64_t second, int32_t nano)
        : RuntimeTimestamp(second, nano)
    {}

    RuntimeTimestampNtz(int32_t second)
        : RuntimeTimestamp(second)
    {}

    RuntimeTimestampNtz(const std::string& timestr)
    {
        ValueOf(timestr);
    }

    bool ValueOf(const std::string& timestr)
    {
        return RuntimeTimestampNtz::ValueOf(timestr, mSecond, mNano);
    }

    static bool ValueOf(const std::string& timestr, int64_t& result_s, int32_t& result_ns)
    {
        char const* begin = timestr.data();
        char const* end = begin + timestr.size();
        tm resultTm;
        bool res = RuntimeTimestamp::ValueOf(begin, end, resultTm, result_ns);
        if (!res) {
            return false;
        }
        result_s = MakeTimeGmt(&resultTm);
        return true;
    }

    std::string ToString() const
    {
        static TIMEZONE_OFFSET tzoff = GetTimeZoneOffset("GMT");
        return FormatTime(mSecond, mNano, tzoff);
    }

};

struct RuntimeTimestampWithoutPadding
{
    int32_t data[3];
};

} // namespace odps
} // namespace apsara

#endif
