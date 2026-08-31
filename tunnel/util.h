#ifndef APSARA_ODPS_TUNNEL_INTERNAL_UTIL_H
#define APSARA_ODPS_TUNNEL_INTERNAL_UTIL_H

#include "configuration.h"
#include "common/http_connection.h"
#include <cstring>
#include <nlohmann/json.hpp>
#include "odps_exception.h"
#include "util/string_util.h"
#include "util/timer.h"


namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

std::string GetRouterServer(const Configuration& conf, const std::string& project);
bool SuccessWithRetry(HttpConnectionPtr& conn, ResponsePtr& resp);

template<typename Func>
auto ReadWithRetry(uint64_t retryTimes, Func&& func) -> decltype(func())
{
    if (retryTimes == 0)
    {
        return func();
    }

    for (uint64_t attempt = 0; attempt <= retryTimes; ++attempt)
    {
        try
        {
            return func();
        }
        catch (const OdpsException& ex)
        {
            if (attempt == retryTimes)
            {
                throw;
            }
        }
    }
    throw OdpsException("ReadWithRetry: unexpected end of retry");
}

// sleep [min, max] milliseconds
void RandomSleep(int min, int max);

static const int64_t DATETIME_MAX_TICKS = 253402271999000L;
static const int64_t DATETIME_MIN_TICKS = -62135798400000L;

static inline std::string ReformatPartitionSpec(const std::string& pspec)
{
    std::string tmp;
    std::vector<std::string> parts = util::SplitString(pspec, ",/");
    bool first = true;
    for (auto part: parts)
    {
        if (!first)
        {
            tmp += ",";
        }
        else
        {
            first = false;
        }
        std::vector<std::string> kv = util::SplitString(part, "=");
        if (kv.size() != 2)
        {
            throw OdpsTunnelException("InvalidArgument", "PartitionSpec invalid: " + part);
        }
        std::string k = util::TrimString(kv[0]);
        std::string v = util::TrimString(kv[1]);
        v = util::ReplaceString(v, "'", "");
        v = util::ReplaceString(v, "\"", "");
        if (k.size() == 0 || v.size() == 0)
        {
            throw OdpsTunnelException("InvalidArgument",
                    "PartitionSpec invalid: " + part +
                    " Found PartitionKeySize:" + std::to_string(k.size()) +
                    " PartitionValueSize:" + std::to_string(v.size()));
        }
        // reformat it.
        tmp += k;
        tmp += "=";
        tmp += v;
    }
    return tmp;
}

static inline std::string CompressOptionToEncoding(const CompressOption& compress)
{
    switch (compress.algorithm)
    {
        case CompressOption::ODPS_ZLIB:
            return COMPRESS_DEFLATE;
        case CompressOption::ODPS_ZSTD:
            return COMPRESS_ZSTD;
        case CompressOption::ODPS_LZ4_FRAME:
            return COMPRESS_LZ4_FRAME;
        case CompressOption::ODPS_LZ4:
            return COMPRESS_LZ4;
        default: break;
    }
    return "";
}

static inline CompressOption EncodingToCompressOption(const std::string& compress)
{
    if (util::ToLowerCaseString(compress) == COMPRESS_DEFLATE)
    {
        return CompressOption::ZLIB_COMPRESS;
    }
    else if (util::ToLowerCaseString(compress) == COMPRESS_LZ4_FRAME)
    {
        return CompressOption::LZ4_COMPRESS;
    }
    else if (util::ToLowerCaseString(compress) == COMPRESS_LZ4)
    {
        return CompressOption::ODPS_LZ4_COMPRESS;
    }
    else if (util::ToLowerCaseString(compress) == COMPRESS_ZSTD)
    {
        return CompressOption::ZSTD_COMPRESS;
    }
    else
    {
        return CompressOption::NO_COMPRESS;
    }
}

inline CompressOption SelectCompressOption(const std::string& acceptEncodings, const std::vector<CompressOption::CompressAlgorithm>& preferredAlgorithms)
{
    const auto& encodings = util::SplitString(acceptEncodings, ",");

    for (const auto& encoding : encodings)
    {
        for (const auto& algorithm : preferredAlgorithms)
        {
            const auto& option = EncodingToCompressOption(encoding);
            if (algorithm == option.algorithm)
            {
                return option;
            }
        }
    }
    return CompressOption::NO_COMPRESS;
}

static inline bool CompressOptionCompatWithArrow(const CompressOption& compress)
{
    return compress.algorithm == CompressOption::ODPS_ZSTD
        || compress.algorithm == CompressOption::ODPS_RAW
        || compress.algorithm == CompressOption::ODPS_LZ4_FRAME
        || compress.algorithm == CompressOption::ODPS_LZ4;
}

static inline bool CompressOptionCompatWithNormal(const CompressOption& compress)
{
    return compress.algorithm == CompressOption::ODPS_ZLIB
        || compress.algorithm == CompressOption::ODPS_ZSTD
        || compress.algorithm == CompressOption::ODPS_LZ4_FRAME
        || compress.algorithm == CompressOption::ODPS_RAW;
}

static inline bool CompressOptionCompatWithStreaming(const CompressOption& compress)
{
    return compress.algorithm == CompressOption::ODPS_ZLIB
        || compress.algorithm == CompressOption::ODPS_ZSTD
        || compress.algorithm == CompressOption::ODPS_RAW;
}

static inline bool CompressOptionCompatWithUpsert(const CompressOption& compress)
{
    return compress.algorithm == CompressOption::ODPS_ZLIB
        || compress.algorithm == CompressOption::ODPS_RAW;
}

class TimerGuard
{
public:
    TimerGuard(int64_t& TotalTime) : mTotalTime(TotalTime)
    {
        mStartTime = util::Timer::GetCurrentTimeInMicroSeconds();
    }
    ~TimerGuard()
    {
        mTotalTime += (util::Timer::GetCurrentTimeInMicroSeconds() - mStartTime);
    }

private:
    int64_t mStartTime = 0;
    int64_t& mTotalTime;
};

class Metrics
{
public:
    Metrics()
    {
    }
    ~Metrics()
    {
    }

    int64_t ClientProcessCost = 0;
    int64_t NetworkCost = 0;
    int64_t TunnelProccessCost = 0;
    int64_t PanguIOCost = 0;
    int64_t ServerTotalCost = 0;
    int64_t ClientIOCost = 0;
    int64_t ServerIOCost = 0;
    int64_t RateLimitCost = 0;

};

// 与历史协议保持一致:只序列化服务端相关的四项开销
inline void to_json(nlohmann::json& j, const Metrics& m)
{
    j = nlohmann::json{
        {"ServerIOCost", m.ServerIOCost},
        {"PanguIOCost", m.PanguIOCost},
        {"RateLimitCost", m.RateLimitCost},
        {"ServerTotalCost", m.ServerTotalCost},
    };
}

inline void from_json(const nlohmann::json& j, Metrics& m)
{
    m.ServerIOCost = j.value("ServerIOCost", static_cast<int64_t>(0));
    m.PanguIOCost = j.value("PanguIOCost", static_cast<int64_t>(0));
    m.RateLimitCost = j.value("RateLimitCost", static_cast<int64_t>(0));
    m.ServerTotalCost = j.value("ServerTotalCost", static_cast<int64_t>(0));
}

std::string GetTunnelMetrics(const Metrics& metrics);


}}}}}
#endif
