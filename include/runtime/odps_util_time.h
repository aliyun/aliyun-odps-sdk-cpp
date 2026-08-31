#ifndef __ODPS_UTIL_TIME_H_20170215_
#define __ODPS_UTIL_TIME_H_20170215_

#include <time.h>
#include <string>
#include "odps_util_base.h"

namespace apsara{
namespace odps{

//functions
time_t MakeTimeGmt(struct tm* tt);
time_t MakeTime(struct tm* tt, const TIMEZONE_OFFSET tzoff);
time_t MakeTime(struct tm* tt);

TIMEZONE_OFFSET GetTimeZoneOffset(const std::string& tz);
std::string GetTimeZoneName(TIMEZONE_OFFSET tz);
std::string GetTimeZoneName(const FormatDetail* fmt);

struct tm* TimeZoneTime(time_t t, const TIMEZONE_OFFSET tzoff, struct tm* tt);
struct tm* GmtTime(time_t t, struct tm* tt);

void FormatTime(time_t t, int32_t nano, const TIMEZONE_OFFSET tzoff, char* buf, size_t buflen);
std::string FormatTime(time_t t, int32_t nano, const TIMEZONE_OFFSET tzoff);
std::string FormatTime(time_t t, const TIMEZONE_OFFSET tzoff);

void FormatDate(time_t t, char* buf, size_t buflen);
std::string FormatDate(time_t t);

std::string FormatDayTime(time_t t, int32_t nano);
std::string FormatYearMonth(int64_t month);
std::string DumpTm(struct tm* tt);

void AddDay(int year, int month, int day, int64_t deltaDay, int& endYear, int& endMonth, int& endDay);
int64_t ToEpochDay(int year, int month, int day);
bool IsLeapYear(int year);
void OfEpochDay(long epochDay, int& year, int& month, int& day);
static int64_t DAYS_PER_CYCLE = 146097; // The number of days in a 400 year cycle.
/*
* The number of days from year zero to year 1970.
* There are five 400 year cycles from year zero to 2000.
* There are 7 leap years from 1970 to 2000.
*/
static int64_t DAYS_0000_TO_1970 = (DAYS_PER_CYCLE * 5L) - (30L * 365L + 7L);

//implements

inline std::string FormatDate(time_t t)
{
    char buf[256];
    FormatDate(t, buf, sizeof(buf));
    return buf;
}

// caller need to provide a constructor to build the output pair
template <typename CTOR>
inline std::pair<char const*, size_t> FormatDate(time_t t, CTOR ctor)
{
    char buf[256];
    FormatDate(t, buf, sizeof(buf));
    return ctor(buf);
}

inline std::string FormatTime(time_t t, int32_t nano, const TIMEZONE_OFFSET tzoff)
{
    char buf[256];
    FormatTime(t, nano, tzoff, buf, sizeof(buf));
    return buf;
}

// caller need to provide a constructor to build the output pair
template <typename CTOR>
inline std::pair<char const*, size_t> FormatTime(time_t t, int32_t nano, const TIMEZONE_OFFSET tzoff, CTOR ctor)
{
    char buf[256];
    FormatTime(t, nano, tzoff, buf, sizeof(buf));
    return ctor(buf);
}

inline std::string FormatTime(time_t t, const TIMEZONE_OFFSET tzoff)
{
    return FormatTime(t, 0, tzoff);
}

inline time_t MakeTimeGmt(struct tm* t)
{
    return timegm(t);
}

inline time_t MakeTime(struct tm* tt)
{
    return MakeTime(tt, TIMEZONE_CHINA);
}

inline struct tm* GmtTime(time_t t, struct tm* tt)
{
    return gmtime_r(&t, tt);
}

inline std::string DumpTm(struct tm* tt)
{
    char buf[64];
    return tz_dump_tm(tt, buf, sizeof(buf));
}

inline void AddDay(int year, int month, int day, int64_t deltaDay, int& endYear, int& endMonth, int& endDay) {
    int64_t epochDay = ToEpochDay(year, month, day);
    int64_t endEpochDay = epochDay + deltaDay;
    OfEpochDay(endEpochDay, endYear, endMonth, endDay);
}
// copy from jdk
// jdk1.8.0_151.jdk/Contents/Home/src.zip!/java/time/LocalDate.java
inline int64_t ToEpochDay(int year, int month, int day) {
    int64_t y = year;
    int64_t m = month;
    int64_t total = 0;
    total += 365 * y;
    if (y >= 0) {
        total += (y + 3) / 4 - (y + 99) / 100 + (y + 399) / 400;
    } else {
        total -= y / -4 - y / -100 + y / -400;
    }
    total += ((367 * m - 362) / 12);
    total += day - 1;
    if (m > 2) {
        total--;
        if (IsLeapYear(year) == false) {
            total--;
        }
    }
    return total - DAYS_0000_TO_1970;
}

inline bool IsLeapYear(int year) {
    return ((year & 3) == 0) && ((year % 100) != 0 || (year % 400) == 0);
}


// copy from jdk
// jdk1.8.0_151.jdk/Contents/Home/src.zip!/java/time/LocalDate.java
inline void OfEpochDay(int64_t epochDay, int& year, int& month, int& day) {
    int64_t zeroDay = epochDay + DAYS_0000_TO_1970;
    // find the march-based year
    zeroDay -= 60;  // adjust to 0000-03-01 so leap day is at end of four year cycle
    int64_t adjust = 0;
    if (zeroDay < 0) {
        // adjust negative years to positive for calculation
        int64_t adjustCycles = (zeroDay + 1) / DAYS_PER_CYCLE - 1;
        adjust = adjustCycles * 400;
        zeroDay += -adjustCycles * DAYS_PER_CYCLE;
    }
    int64_t yearEst = (400 * zeroDay + 591) / DAYS_PER_CYCLE;
    int64_t doyEst = zeroDay - (365 * yearEst + yearEst / 4 - yearEst / 100 + yearEst / 400);
    if (doyEst < 0) {
        // fix estimate
        yearEst--;
        doyEst = zeroDay - (365 * yearEst + yearEst / 4 - yearEst / 100 + yearEst / 400);
    }
    yearEst += adjust;  // reset any negative year
    int64_t marchDoy0 = (int64_t) doyEst;

    // convert march-based values back to january-based
    int64_t marchMonth0 = (marchDoy0 * 5 + 2) / 153;
    month = (marchMonth0 + 2) % 12 + 1;
    day = marchDoy0 - (marchMonth0 * 306 + 5) / 10 + 1;
    yearEst += marchMonth0 / 10;

    // check year now we are certain it is correct
    year = yearEst;
}


}
}
#endif
