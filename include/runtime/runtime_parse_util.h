#ifndef APSARA_ODPS_UTIL_RUNTIME_PARSE_UTIL_H
#define APSARA_ODPS_UTIL_RUNTIME_PARSE_UTIL_H

// #include <include/runtime_date.h>
// #include <include/runtime_timestamp.h>
// #include <include/runtime_interval.h>
// #include <include/odps_util_base.h>

#include "untime_date.h"
#include "runtime_timestamp.h"
#include "runtime_interval.h"
#include "odps_util_base.h"

#include <limits>
#include <cmath>
#include <system_error>
#include <float.h>
#include <limits.h>

namespace apsara { namespace odps { namespace util {

#if !defined CLANG && (defined(__GNUC__) && __GNUC__ < 7)
template <typename T>
inline bool __builtin_mul_overflow(T const & a, T const& b, T* r) {
    if (a == 0 || b == 0) {
        *r = 0;
        return false;
    }

    constexpr T min = std::numeric_limits<T>::min();
    constexpr T max = std::numeric_limits<T>::max();
    long lr = (long)a * b;
    if (lr < min || lr > max) {
        return true;
    }

    *r = (T)lr;
    return false;
}

template <typename T>
inline bool __builtin_add_overflow(T a, T b, T* r) {
    constexpr T min = std::numeric_limits<T>::min();
    constexpr T max = std::numeric_limits<T>::max();
    long lr = (long)a + b;
    if (lr < min || lr > max) {
        return true;
    }

    *r = (T)lr;
    return false;
}

#ifndef __x86_64__
inline bool __builtin_mul_overflow(long a, long b, long* r) {
    if (a == 0 || b == 0) {
        *r = 0;
        return false;
    }

    bool asign = a > 0;  // sign of a
    bool bsign = b > 0;  // sign of b

    bool overflow = false;
    if (asign == bsign) {
        if (asign) {  // a > 0
            overflow = b > LONG_MAX / a;
        } else {  // a < 0
            overflow = b < LONG_MAX / a;
        }
    } else {
        if (asign) {  // a > 0
            overflow = b < (LONG_MIN / a);
        } else {  // a < 0
            overflow = a < (LONG_MIN / b);
        }
    }

    if (overflow) {
        return true;
    }

    *r = a * b;
    return false;
}

inline bool __builtin_add_overflow(long a, long b, long* r) {
    if ((a > 0 && b > LONG_MAX - a) || (a < 0 && b < LONG_MIN - a)) {
        return true;
    }
    *r = a + b;
    return false;
}

#else

inline bool __builtin_mul_overflow(long a, long b, long* r) {
    bool overflow;
    long result;
    __asm__ (
        "movq %2, %%rax; imul %3, %%rax; movq %%rax, %0; seto %1;" :
        "=r"(result), "=r"(overflow) :
        "r"(a),"r"(b) :
        "rax"
    );
    *r = result;
    return overflow;
}

inline bool __builtin_mul_overflow(int a, int b, int* r) {
    bool overflow;
    int result;
    __asm__ (
        "movl %2, %%eax; imul %3, %%eax; movl %%eax, %0; seto %1;" :
        "=r"(result), "=r"(overflow) :
        "r"(a),"r"(b) :
        "rax"
    );
    *r = result;
    return overflow;
}

inline bool __builtin_add_overflow(long a, long b, long* r) {
    bool overflow;
    long result;
    __asm__ (
        "movq %2, %%rax; addq %3, %%rax; movq %%rax, %0; seto %1;" :
        "=r"(result), "=r"(overflow) :
        "r"(a),"r"(b) :
        "rax"
    );
    *r = result;
    return overflow;
}

inline bool __builtin_add_overflow(int a, int b, int* r) {
    bool overflow;
    int result;
    __asm__ (
        "movl %2, %%eax; addl %3, %%eax; movl %%eax, %0; seto %1;" :
        "=r"(result), "=r"(overflow) :
        "r"(a),"r"(b) :
        "rax"
    );
    *r = result;
    return overflow;
}

#endif

#endif

template<typename T>
inline
bool raiseAndAdd(T& val, int base, int c) {
  return !(__builtin_mul_overflow(val, (T)base, &val) || __builtin_add_overflow(val, (T) c, &val));
}

template <typename InIt>
struct ParseResult {
    InIt ptr;
    std::errc ec;
};

template <typename InIt>
inline InIt trim(InIt begin, InIt end) {
    while (begin != end && *begin <= ' ' && *begin >= 0x0) {
        ++begin;
    }
    return begin;
}

template <typename InIt, typename T>
inline ParseResult<InIt> parseInteger(InIt begin, InIt end, T& v) {
    static_assert(std::is_integral<T>::value, "implementation bug");
    static_assert(std::is_signed<T>::value, "implementation bug");

    ParseResult<InIt> r;
    InIt p = begin;
    int sign = 1;
    if (p != end) {
        if (*p == '-') {
            sign = -1;
            ++p;
        } else if (*p == '+') {
            ++p;
        }
    }

    if (p == end || !isdigit(*p)) {
        r.ptr = begin;
        r.ec = std::errc::invalid_argument;
        return r;
    }
    v = 0;
    if (sign == 1) {
        do {
            if (!raiseAndAdd(v, 10, *p - '0')) {
                r.ptr = begin;
                r.ec = std::errc::result_out_of_range;
                return r;
            }
            ++p;
        } while (p != end && isdigit(*p));
    } else {
        do {
            if (!raiseAndAdd(v, 10, '0' - *p)) {
                r.ptr = begin;
                r.ec = std::errc::result_out_of_range;
                return r;
            }
            ++p;
        } while (p != end && isdigit(*p));
    }
    r.ec = std::errc();
    r.ptr = p;
    return r;
}

template <typename InIt>
inline ParseResult<InIt> parseSmallint(InIt begin, InIt end, int16_t& v) {
    return parseInteger(begin, end, v);
}

template <typename InIt>
inline ParseResult<InIt> parseTinyint(InIt begin, InIt end, int8_t& v) {
    return parseInteger(begin, end, v);
}

template <typename InIt>
inline ParseResult<InIt> parseBigint(InIt begin, InIt end, int64_t& v) {
    return parseInteger(begin, end, v);
}

template <typename InIt>
inline ParseResult<InIt> parseInt(InIt begin, InIt end, int32_t& v) {
    return parseInteger(begin, end, v);
}

ParseResult<char const*> parseDouble0(char const* begin, char const* end, double& v);

inline ParseResult<char const*> parseDouble(char const* begin, char const* end, double& v) {
    ParseResult<char const*> r;
    r.ptr = trim(begin, end);
    if (r.ptr == end) {
        r.ec = std::errc::invalid_argument;
        return r;
    }

    r = parseDouble0(r.ptr, end, v);
    if (r.ec == std::errc()) {
        r.ptr = trim(r.ptr, end);
        if (r.ptr != end) {
            r.ec = std::errc::invalid_argument;
        }
    }
    return r;
}

inline ParseResult<char const*> parseFloat(char const* begin, char const* end, float& v) {
    double d;
    ParseResult<char const*> r = parseDouble(begin, end, d);
    double ad = std::abs(d);
    if (ad > FLT_MAX || (ad < FLT_MIN && ad > 0.0)) {
        r.ec = std::errc::invalid_argument;
    }
    v = (float)d;
    return r;
}

template <typename InIt>
inline ParseResult<InIt> parseBoolean(InIt begin, InIt end, bool& value, bool extended = false) {
    ParseResult<InIt> r;
    if (begin != end) {
        if (*begin == 't' || *begin == 'T') {
            ++begin;
            if (begin != end) {
                if (*begin == 'r' || *begin == 'R') {
                    ++begin;
                    if (begin != end && (*begin == 'u' || *begin == 'U')) {
                        ++begin;
                        if (begin != end && (*begin == 'e' || *begin == 'E')) {
                            ++begin;
                            if (begin == end) {
                                value = true;
                                r.ptr = end;
                                r.ec = std::errc();
                                return r;
                            }
                        }
                    }
                }
            } else if (extended) {
                value = true;
                r.ptr = end;
                r.ec = std::errc();
                return r;
            }
        } else if (*begin == 'f' || *begin == 'F') {
            ++begin;
            if (begin != end) {
                if (*begin == 'a' || *begin == 'A') {
                    ++begin;
                    if (begin != end && (*begin == 'l' || *begin == 'L')) {
                        ++begin;
                        if (begin != end && (*begin == 's' || *begin == 'S')) {
                            ++begin;
                            if (begin != end && (*begin == 'e' || *begin == 'E')) {
                                ++begin;
                                if (begin == end) {
                                    value = false;
                                    r.ptr = end;
                                    r.ec = std::errc();
                                    return r;
                                }
                            }
                        }
                    }
                }
            } else if (extended) {
                value = false;
                r.ptr = end;
                r.ec = std::errc();
                return r;
            }
        } else if (extended) {
            if (*begin == '1') {
                ++begin;
                if (begin == end) {
                    value = true;
                    r.ptr = end;
                    r.ec = std::errc();
                    return r;
                }
            } else if (*begin == '0') {
                ++begin;
                if (begin == end) {
                    value = false;
                    r.ptr = end;
                    r.ec = std::errc();
                    return r;
                }
            }
        }
    }
    r.ec = std::errc::invalid_argument;
    r.ptr = begin;
    return r;
}

template <typename InIt>
ParseResult<InIt> parseYearMonthDay(InIt begin, InIt end, int32_t& year, int32_t& month, int32_t& day) {
    auto r = parseInt(begin, end, year);
    auto p = r.ptr;
    if (r.ec != std::errc() || year <= 0 || year > 9999) {
        return r;
    }
    if (p == end || *p != '-') {
        return r;
    }
    ++p;

    r = parseInt(p, end, month);
    p = r.ptr;
    if (r.ec != std::errc() || p == end || *p != '-') {
        return r;
    }
    ++p;

    r = parseInt(p, end, day);
    if (r.ec != std::errc()) {
        return r;
    }
    return r;
}

template <typename InIt>
ParseResult<InIt> parseHourMinuteSecond(InIt begin, InIt end, bool strict, int32_t& hour, int32_t& minute, int32_t& second) {
    auto r = parseInt(begin, end, hour);
    if (r.ec != std::errc()) {
        return r;
    }
    if (strict && (hour < 0 || hour > 23)) {
        r.ec = std::errc::invalid_argument;
        return r;
    }
    auto p = r.ptr;
    if (p == end || *p != ':') {
        r.ec = std::errc::invalid_argument;
        return r;
    }
    ++p;

    r = parseInt(p, end, minute);
    if (r.ec != std::errc()) {
        return r;
    }
    if (strict && (minute < 0 || minute > 59)) {
        r.ec = std::errc::invalid_argument;
        return r;
    }
    p = r.ptr;
    if (p == end || *p != ':') {
        r.ec = std::errc::invalid_argument;
        return r;
    }
    ++p;

    r = parseInt(p, end, second);
    if (r.ec != std::errc()) {
        return r;
    }
    if (strict && (second < 0 || second > 59)) {
        r.ec = std::errc::invalid_argument;
        return r;
    }

    return r;
}

template <typename InIt>
ParseResult<InIt> parseMillis(InIt begin, InIt end, int32_t& millis) {
    auto r = parseInt(begin, end, millis);
    if (r < 0 || r > 999) {
        r.ec = std::errc::invalid_argument;
    }
    size_t digitCount = r.ptr - begin;
    for (int i=digitCount; i<3; ++i) {
        millis *= 10;
    }
    return r;
}

template <typename InIt>
ParseResult<InIt> parseNanos(InIt begin, InIt end, int32_t& nanos) {
    auto r = parseInt(begin, end, nanos);
    if (nanos < 0 || nanos > 999999999) {
        r.ec = std::errc::invalid_argument;
    }
    size_t digitCount = r.ptr - begin;
    for (int i=digitCount; i<9; ++i) {
        nanos *= 10;
    }
    return r;
}

template <typename InIt>
ParseResult<InIt> parseDate(InIt begin, InIt end, RuntimeDate& result) {
    tm t;
    auto r = parseYearMonthDay(begin, end, t.tm_year, t.tm_mon, t.tm_mday);
    if (r.ec != std::errc()) {
        return r;
    }

    static const int MAX_MONTH = 12, MAX_DAY = 31;
    if (!(t.tm_year >= 1 &&
        (t.tm_mon >= 1 && t.tm_mon <= MAX_MONTH) &&
        (t.tm_mday >= 1 && t.tm_mday <= MAX_DAY))) {
        r.ec = std::errc::result_out_of_range;
        return r;
    }

    t.tm_year -= 1900;
    --t.tm_mon;
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    t.tm_gmtoff = 0;
    t.tm_zone = nullptr;

    static TIMEZONE_OFFSET tzGMT = GetTimeZoneOffset("GMT");
    result.mSecond = MakeTime(&t, tzGMT);
    return r;
}

inline
ParseResult<char const*> parseTimestamp(char const* begin, char const* end, RuntimeTimestamp& result, TIMEZONE_OFFSET tzOff) {
    ParseResult<char const*> r;
    int64_t seconds;
    int32_t nanos;
    tm t;
    if (RuntimeTimestamp::ValueOf(begin, end, t, nanos)) {
        seconds = MakeTime(&t, tzOff);
        result.Set(seconds, nanos);
        r.ec = std::errc();
        r.ptr = end;
    } else {
        r.ec = std::errc::invalid_argument;
        r.ptr = end;
    }
    return r;
}

template <typename InIt>
ParseResult<InIt> parseIntervalDayTime(InIt begin, InIt end, RuntimeIntervalDayTime& result) {
    int32_t days;
    auto r = parseInt(begin, end, days);
    if (r.ec != std::errc()) {
        return r;
    }

    InIt p = r.ptr;
    if (p == end || *p != ' ') {
        r.ec = std::errc::invalid_argument;
        return r;
    }
    ++p;

    int32_t hour, minute, second;
    r = parseHourMinuteSecond(p, end, true, hour,minute, second);
    if (r.ec != std::errc()) {
        return r;
    }

    int32_t nanos = 0;
    p = r.ptr;
    if (p != end) {
        if (*p != '.') {
            r.ec = std::errc::invalid_argument;
            return r;
        }
        ++p;
        r = parseNanos(p, end, nanos);
        if (r.ec != std::errc()) {
            return r;
        }
    }
    if (r.ptr != end) {
        r.ec = std::errc::invalid_argument;
        return r;
    }

    int64_t seconds = (int64_t)days * 86400;
    int64_t hmsSeconds = (int64_t)hour * 3600 + (int64_t)minute * 60 + (int64_t)second;
    if (seconds > 0) {
        seconds += hmsSeconds;
    } else {
        seconds -= hmsSeconds;
        nanos = -nanos;
    }
    result.Set(seconds, nanos);
    return r;
}

template <typename InIt>
ParseResult<InIt> parseIntervalYearMonth(InIt begin, InIt end, RuntimeIntervalYearMonth& result) {
    int32_t years;
    auto r = parseInt(begin, end, years);
    if (r.ec != std::errc()) {
        return r;
    }

    if (years < -RuntimeIntervalYearMonth::MAX_YEARS || years > RuntimeIntervalYearMonth::MAX_YEARS) {
        r.ec = std::errc::invalid_argument;
        return r;
    }

    InIt p = r.ptr;
    if (p == end || *p != '-') {
        r.ec = std::errc::invalid_argument;
        return r;
    }
    ++p;

    int32_t month;
    r = parseInt(p, end, month);
    if (r.ec != std::errc()) {
        return r;
    }
    if (month < 0 || month > 11) {
        r.ec = std::errc::invalid_argument;
        return r;
    }

    int64_t months = (int64_t)years * 12;
    if (months >= 0) {
        months += month;
    } else {
        months -= month;
    }

    result.mMonth = months;
    return r;
}

}}}

#endif
