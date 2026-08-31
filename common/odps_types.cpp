#include <sstream>
#include <iomanip>
#include "odps_types.h"
#include "common/odps_column.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::util;

namespace apsara{
namespace odps {
namespace sdk {

TimeStamp::TimeStamp(int64_t sec, int32_t ns)
{
    if (ns != 0)
    {
        sec += ns / NANOS_PER_SECOND;
        ns = ns % NANOS_PER_SECOND;
        if (sec > 0 && ns < 0)
        {
            --sec;
            ns = ns + NANOS_PER_SECOND;
        }
        else if (sec < 0 && ns > 0)
        {
            ++sec;
            ns = ns - NANOS_PER_SECOND;
        }
    }
    second = sec;
    nano = ns;
}

std::string TimeStamp::ToString() const
{
    std::ostringstream oss;
    oss << gmt_strftime(second, TUNNEL_DATE_TIME_FORMAT) << ".";
    oss << std::setfill('0') << std::setw(9);
    oss << nano;
    return oss.str();
}

std::string IntervalDayTime::ToString() const
{
    std::ostringstream oss;
    int64_t sec = GetSecond();
    int32_t ns = GetNano();
    if (sec < 0 || ns < 0)
    {
        oss << "-";
        sec = -sec;
        ns = -ns;
    }

    // days
    oss << sec / SECONDS_PER_DAY << " ";
    sec = sec % SECONDS_PER_DAY;

    // pad leading zeros
    oss << std::setfill('0') << std::setw(2);

    // hours
    oss << sec / SECONDS_PER_HOUR << ":";
    sec = sec % SECONDS_PER_HOUR;

    // minutes
    oss << sec / SECONDS_PER_MINUTE << ":";
    sec = sec % SECONDS_PER_MINUTE;

    // seconds
    oss << sec << ":";

    // nanos
    oss << std::setw(9) << ns;

    return oss.str();
}

std::string IntervalYearMonth::ToString() const
{
    std::ostringstream oss;
    if (months < 0)
    {
        oss << "-";
    }
    oss << GetYears() << "-" << GetMonths();
    return oss.str();
}


}
}
}