#ifndef APSARA_ODPS_EXECUTION_ENGINE_INTERVAL_H_
#define APSARA_ODPS_EXECUTION_ENGINE_INTERVAL_H_

#include <stdint.h>
#include <string>

namespace apsara
{
namespace odps
{

class RuntimeIntervalYearMonth
{
public:
    static int64_t atoi(const char* input);
    static int64_t atoi(const char* input, size_t len);
    static int32_t ValidIn(int32_t year, int32_t month);
    static int32_t ValidIn(int64_t month);
    static int32_t Negate(int64_t totalMonth);
    static int32_t GetYear(int64_t totalMonth);
    static int32_t GetMonth(int64_t totalMonth);
    static int32_t Parse(std::string s);
    static int64_t parse(const char* data, int64_t size);
    static std::string ToString(int64_t totalMonth);

    static const int32_t MIN_INTERVAL_STRING = 3;
    static const int32_t MAX_INTERVAL_STRING = 10;
    static const int32_t MAX_YEARS = 9999;
    static const int32_t MONTHS_PER_YEAR = 12;

    // caller need to provide a constructor to build the result
    template <typename R, typename CTOR>
    static R ToStringWithCtor(int64_t totalMonth, CTOR ctor)
    {
        char buf[MAX_INTERVAL_STRING];
        int32_t n;

        if (totalMonth < 0)
        {
            n = snprintf(buf, MAX_INTERVAL_STRING, "%c%d%c%d", '-',
                    std::abs((int)(totalMonth / MONTHS_PER_YEAR)), '-', std::abs((int)(totalMonth % MONTHS_PER_YEAR)));
        }
        else
        {
            n = snprintf(buf, MAX_INTERVAL_STRING, "%d%c%d",
                    std::abs((int)(totalMonth / MONTHS_PER_YEAR)), '-', std::abs((int)(totalMonth % MONTHS_PER_YEAR)));
        }
        int32_t length = n < MAX_INTERVAL_STRING ? n : MAX_INTERVAL_STRING;
        return ctor(buf, length);
    }

    RuntimeIntervalYearMonth(int64_t month): mMonth(month) { }
    RuntimeIntervalYearMonth(): mMonth(0L) { }

    bool operator > (const RuntimeIntervalYearMonth& other) const {
        return mMonth > other.mMonth;
    }

    bool operator >= (const RuntimeIntervalYearMonth& other) const {
        return mMonth >= other.mMonth;
    }

    bool operator < (const RuntimeIntervalYearMonth& other) const {
        return mMonth < other.mMonth;
    }

    bool operator <= (const RuntimeIntervalYearMonth& other) const {
        return mMonth <= other.mMonth;
    }

    bool operator == (const RuntimeIntervalYearMonth& other) const {
        return mMonth == other.mMonth;
    }

    bool operator != (const RuntimeIntervalYearMonth& other) const {
        return mMonth != other.mMonth;
    }

    RuntimeIntervalYearMonth operator - () const {
      return RuntimeIntervalYearMonth(-mMonth);
    }

    int64_t mMonth;

private:
    static const char* trim(const char* str, int64_t& len);
    static int64_t find(const char* data, int64_t len, char c);
};

class RuntimeIntervalDayTime
{
public:
    RuntimeIntervalDayTime():mTotalSeconds(0), mNanos(0) {}
    RuntimeIntervalDayTime(int64_t totalSeconds, int32_t nanos);
    RuntimeIntervalDayTime(int32_t totalSeconds)
        :mTotalSeconds(totalSeconds), mNanos(0) {};
    RuntimeIntervalDayTime(std::string daytimestr);
    RuntimeIntervalDayTime Negate() const;
    int64_t GetTotalSeconds() const;
    void Set(const RuntimeIntervalDayTime& dayTime);
    void Set(int64_t seconds, int32_t nanosecond);
    void SetNull();
    void SetSeconds(int64_t seconds);
    void SetNanos(int32_t nanos);
    void SetData(char* buf, int32_t size);
    int32_t GetNanos() const;
    int32_t GetDays() const;
    int32_t GetHours() const;
    int32_t GetMinutes() const;
    int32_t GetSeconds() const;
    int32_t GetMillis() const;
    int64_t ToMillis() const;
    int32_t GetSize() const;
    void GetData(char* buf, int32_t size) const;
    int32_t GetHash() const;
    RuntimeIntervalDayTime Plus(const RuntimeIntervalDayTime& rhs) const;
    int32_t CompareTo(const RuntimeIntervalDayTime& o);
    bool Equals(RuntimeIntervalDayTime o);
    std::string ToString() const;
    static RuntimeIntervalDayTime OfDays(int days);
    static RuntimeIntervalDayTime OfTotalHours(int hours);
    static RuntimeIntervalDayTime OfTotalMinutes(int minutes);
    static RuntimeIntervalDayTime OfTotalSeconds(long seconds);
    static RuntimeIntervalDayTime OfTotalSeconds(std::string s);
    static RuntimeIntervalDayTime OfTotalSeconds(const char* s, size_t len);

    bool operator == (const RuntimeIntervalDayTime& dec) const;
    bool operator != (const RuntimeIntervalDayTime& dec) const;
    bool operator > (const RuntimeIntervalDayTime& dec) const;
    bool operator < (const RuntimeIntervalDayTime& dec) const;
    bool operator >= (const RuntimeIntervalDayTime& dec) const;
    bool operator <= (const RuntimeIntervalDayTime& dec) const;

    RuntimeIntervalDayTime operator - () const {
        return RuntimeIntervalDayTime(-mTotalSeconds, -mNanos);
    }

    // caller need to provide a constructor to build the result
    template <typename R, typename CTOR>
    R ToStringWithCtor(CTOR ctor) const
    {
        char buf[40];
        int n;

        if (mTotalSeconds < 0 || mNanos < 0)
        {
            n = snprintf(buf, 40, "%c%d %02d:%02d:%02d.%09d",
                    '-', std::abs(GetDays()), std::abs(GetHours()), std::abs(GetMinutes()), std::abs(GetSeconds()), std::abs(GetNanos()));
        }
        else
        {
            n = snprintf(buf, 40, "%d %02d:%02d:%02d.%09d",
                        GetDays(), GetHours(), GetMinutes(), GetSeconds(), GetNanos());
        }

        int32_t length = n < 40 ? n : 40;
        return ctor(buf, length);
    }

    // To calculate NDV correctly, we need to zero out padding bytes in struct/class
    // Please ref: https://work.aone.alibaba-inc.com/issue/21525743
    // we do not need packResultBuf here because the padding bytes are continuous and at the end
    static std::pair<void*, uint32_t> GetPackedData(const RuntimeIntervalDayTime* data, char* packResultBuf) {
        uint32_t len = sizeof(int64_t)               // for mTotalSeconds
                     + sizeof(int32_t);              // for mNanos
        return std::make_pair((void*)data, len);
    }

private:
    static long x(long d, long m, long over)
    {
        if (d > over)
            return INT64_MAX;
        if (d < -over)
            return INT64_MIN;
        return d * m;
    }

    void normalize()
    {
        if (mTotalSeconds > 0 && mNanos < 0)
        {
            --mTotalSeconds;
            mNanos += 1000000000;
        }
        else if (mTotalSeconds < 0 && mNanos > 0)
        {
            ++mTotalSeconds;
            mNanos -= 1000000000;
        }
    }

    // NOTE: if you change the following members, you MUST also consider to change method GetPackedData().
    int64_t mTotalSeconds;
    int32_t mNanos;
};

} // namespace odps
} // namespace apsara

#endif

