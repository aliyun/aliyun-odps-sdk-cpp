/** @file timer.h
 * SDK 内部时间工具(原 apsara timing 中被 SDK 使用的部分,
 * 搬入 apsara::odps::sdk::util,实现语义保持不变)。
 * 纯头文件实现;clock_gettime 依赖 -lrt(构建已链接)。
 */

#ifndef APSARA_ODPS_SDK_UTIL_TIMER_H
#define APSARA_ODPS_SDK_UTIL_TIMER_H

#include <ctime>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <iostream>

#define ODPS_SDK_TIMER_SAFE(x) \
    do                      \
    {                       \
        int r = (x);        \
        if (r != 0)         \
        {                   \
            int e = errno;  \
            std::cerr << "ODPS_SDK_TIMER_SAFE to abort() @ " << __FILE__ << ":" <<__LINE__ << " in " << __FUNCTION__ << std::endl \
                      << "Ret:" << r << " errno:" << e << "(" << strerror(e) << ')' << std::endl; \
            abort();        \
        }                   \
    } while(false)


namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

namespace timing
{

typedef int64_t TimeInSec;
typedef int64_t TimeInMsec;
typedef int64_t TimeInUsec;
typedef int64_t TimeInNsec;

const TimeInUsec USEC_PER_SEC = 1000000;
const TimeInNsec NSEC_PER_USEC = 1000;
const TimeInNsec NSEC_PER_MSEC = 1000000;
const TimeInNsec NSEC_PER_SEC = USEC_PER_SEC * NSEC_PER_USEC;

inline TimeInNsec GetCurrentTimeInNanoSeconds()
{
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    return tm.tv_sec * NSEC_PER_SEC + tm.tv_nsec;
}

inline TimeInSec GetCurrentTimeInSeconds()
{
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    return tm.tv_sec;
}

inline TimeInUsec GetCurrentTimeInMicroSeconds()
{
    return GetCurrentTimeInNanoSeconds() / NSEC_PER_USEC;
}

inline TimeInMsec GetCurrentTimeInMilliSeconds()
{
    return GetCurrentTimeInNanoSeconds() / NSEC_PER_MSEC;
}

inline std::string GetCurrentTimeStr()
{
    time_t t = GetCurrentTimeInSeconds();
    char buffer[100];
    ctime_r(&t, buffer);
    return std::string(buffer);
}

inline std::string GetTimeStamp(TimeInSec t, const std::string& format = "%Y%m%d%H%M%S")
{
    char buffer[100];
    time_t time = t;
    struct tm timeinfo;
    localtime_r(&time, &timeinfo);
    strftime (buffer, sizeof(buffer), format.c_str(), &timeinfo);
    return std::string(buffer);
}

inline std::string GetCurrentTimeStamp(const std::string& format = "%Y%m%d%H%M%S")
{
    time_t t = GetCurrentTimeInSeconds();
    return GetTimeStamp(t, format);
}

class Timer
{
public:
    Timer(bool autoStart = true)
    {
        startTime.tv_sec = 0;
        startTime.tv_nsec = 0;
        if (autoStart)
        {
            Start();
        }
    }

    void Start()
    {
        ODPS_SDK_TIMER_SAFE(clock_gettime(CLOCK_MONOTONIC, &startTime));
    }

    void RestartTimer()
    {
        Start();
    }

    int64_t Stop()
    {
        struct timespec stopTime;
        ODPS_SDK_TIMER_SAFE(clock_gettime(CLOCK_MONOTONIC, &stopTime));
        return (int64_t)(stopTime.tv_sec - startTime.tv_sec) * 1000000
            + (stopTime.tv_nsec - startTime.tv_nsec) / 1000;
    }

    static uint64_t GetCurrentTimeInNanoSeconds()
    {
        return timing::GetCurrentTimeInNanoSeconds();
    }

    static uint64_t GetCurrentTimeInMicroSeconds()
    {
        return timing::GetCurrentTimeInMicroSeconds();
    }

    static uint64_t GetCurrentTimeInMilliSeconds()
    {
        return timing::GetCurrentTimeInMilliSeconds();
    }

    static uint64_t GetCurrentTime()
    {
        return timing::GetCurrentTimeInSeconds();
    }

    static std::string GetCurrentTimeStr()
    {
        return timing::GetCurrentTimeStr();
    }

    static std::string GetCurrentTimeStamp(
        const std::string& format = "%Y%m%d%H%M%S")
    {
        return timing::GetCurrentTimeStamp(format);
    }

    static std::string GetTimeStamp(time_t tm,
        const std::string& format = "%Y%m%d%H%M%S")
    {
        TimeInSec t = tm;
        return timing::GetTimeStamp(t, format);
    }

    static std::string ConvertToTimeStamp(const time_t& tm,
        const std::string& format = "%Y%m%d%H%M%S")
    {
        TimeInSec t = tm;
        return timing::GetTimeStamp(t, format);
    }

private:
    struct timespec startTime;
};

}  // namespace timing

typedef timing::Timer Timer;

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // APSARA_ODPS_SDK_UTIL_TIMER_H
