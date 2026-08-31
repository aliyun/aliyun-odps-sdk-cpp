#ifndef __ODPS_UTIL_BASE_H_2017_02_15__
#define __ODPS_UTIL_BASE_H_2017_02_15__

#include <string>
#include <stdint.h>

namespace apsara{
namespace odps{

class TimeZone;
typedef TimeZone* TIMEZONE_OFFSET;

extern "C" struct TimeZone* tz_open(const char *tzname);
extern "C" void tz_close(struct TimeZone* tzf);
extern "C" const char* tz_get_name(const struct TimeZone* tzf);
extern "C" struct tm* tz_localtime_r(const time_t* t, struct tm* tp, TimeZone* tzf);
extern "C" time_t tz_mktime (struct tm *tp, TimeZone* tzf);
extern "C" const char* tz_dump_tm(const struct tm* tt, char* buf, size_t buflen);

#define ODPS_MAX_TIME 253402387200l//10000-01-02 08:00:00 yday   1 wday 0 isdst  0 gmtoff 28800
#define ODPS_MIN_TIME -62167305600l//   -1-12-31 08:05:52 yday 364 wday 5 isdst  0 gmtoff 29152

static const TIMEZONE_OFFSET TIMEZONE_CHINA = NULL;
static const int NANOPS = 1000000000;

enum FUXI_EVENT_TYPE
{
    DATA_MOVEMENT_EVENT,
    COMPOSITE_DATA_MOVEMENT_EVENT,
    COMPOSITE_ROUTED_DATA_MOVEMENT_EVENT,
    VERTEX_MANAGER_EVENT,
    BATCH_DATA_MOVEMENT_EVENT
};

struct FormatDetail
{
    TIMEZONE_OFFSET tzoff;
};

#define SAFE_TZOFF(fmt) ((fmt) == NULL ? apsara::odps::TIMEZONE_CHINA : (fmt)->tzoff)
#define MEMCPY_AND_MOVE(dst, src, size) \
    do { \
        memcpy(dst, src, size); \
        dst += size; \
    } while(0)

#define MEMCPY_AND_MOVE_SRC(dst, src, size) \
    do { \
        memcpy(dst, src, size); \
        src += size; \
    } while(0)

struct TokenInfo
{
    bool isToken;
    std::string token;
    std::string errInfo;
};

inline bool IsDigit(char ch)
{
    return (ch >= '0' and ch <= '9');
}

int NextToken(const char* ps, size_t pslen, uint32_t& index, const char* specialChars, bool isPeek, TokenInfo& ret);
const char* TrimRight(char* input, const char* chars = " \r\n\t");
const char* TrimLeft(char* input, const char* chars = " \r\n\t");
const char* Trim(char* input, const char* chars = " \r\n\t");
const std::string& Trim(std::string& str, const char* chars = " \r\n\t");

const std::string& HexDump(const void* data, size_t len, std::string& content);
std::string HexDump(const void* data, size_t len);
std::string HexDump(const std::string& data);

inline std::string HexDump(const void* data, size_t len)
{
    std::string buf;
    return HexDump(data, len, buf);
}

inline std::string HexDump(const std::string& data)
{
    return HexDump(data.data(), data.size());
}

const std::string& QuotedPrintableEncode(const void* data, size_t len, std::string& content);
std::string QuotedPrintableEncode(const void* data, size_t len);
std::string QuotedPrintableEncode(const std::string& data);
const std::string& Replace(const std::string& source, const std::string& match, const std::string& repstr, std::string& output);
const std::string& ReplaceSelf(std::string& source, const std::string& match, const std::string& repstr);
void QuoteAppend(const std::string& input, std::string& output);

inline std::string QuotedPrintableEncode(const void* data, size_t len)
{
    std::string buf;
    return QuotedPrintableEncode(data, len, buf);
}

inline std::string QuotedPrintableEncode(const std::string& data)
{
    return QuotedPrintableEncode(data.data(), data.size());
}

int64_t GetFileSize(const char* fname);

}
}

#endif
