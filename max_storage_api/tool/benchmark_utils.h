#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>

#include "tool.h"
#include "arrow/api.h"

namespace benchmark_util {

// ============================================================================
// High-resolution timer for sub-operation timing (microsecond precision)
// ============================================================================
inline int64_t SteadyTimeMicros()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// ============================================================================
// System Resource Monitor - CPU and Memory usage tracking
// ============================================================================
struct SystemResourceMetrics
{
    std::atomic<double> cpuUsagePercent{0.0};
    std::atomic<int64_t> memoryUsedKB{0};
    std::atomic<int64_t> peakMemoryKB{0};

    // Average and peak CPU tracking across all samples
    std::atomic<double> peakCpuPercent{0.0};
    double totalCpuPercent{0.0};    // Only accessed from sampling thread
    int64_t cpuSampleCount{0};      // Only accessed from sampling thread

    // For CPU calculation
    int64_t lastUserTime{0};
    int64_t lastSystemTime{0};
    int64_t lastTimestamp{0};

    void Sample()
    {
        // Read memory from /proc/self/status
        std::ifstream statusFile("/proc/self/status");
        if (statusFile.is_open())
        {
            std::string line;
            while (std::getline(statusFile, line))
            {
                if (line.find("VmRSS:") == 0)
                {
                    int64_t rss = 0;
                    sscanf(line.c_str(), "VmRSS: %ld kB", &rss);
                    memoryUsedKB.store(rss, std::memory_order_relaxed);

                    int64_t currentPeak = peakMemoryKB.load(std::memory_order_relaxed);
                    while (rss > currentPeak &&
                           !peakMemoryKB.compare_exchange_weak(currentPeak, rss, std::memory_order_relaxed)) {}
                }
            }
        }

        // Read CPU from /proc/self/stat
        std::ifstream statFile("/proc/self/stat");
        if (statFile.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(statFile)),
                                std::istreambuf_iterator<char>());

            // Parse utime and stime (fields 14 and 15, 1-indexed)
            std::istringstream iss(content);
            std::string token;
            int64_t utime = 0, stime = 0;
            for (int i = 1; i <= 15; i++)
            {
                iss >> token;
                if (i == 14) utime = std::stoll(token);
                if (i == 15) stime = std::stoll(token);
            }

            int64_t now = tool::CurrentTimeMillis();
            if (lastTimestamp > 0)
            {
                int64_t deltaTime = now - lastTimestamp;
                int64_t deltaCpu = (utime - lastUserTime + stime - lastSystemTime);

                // Convert jiffies to ms (assuming 100 Hz = 10ms per jiffy)
                double cpuMs = deltaCpu * 10.0;
                double cpuPct = (deltaTime > 0) ? (cpuMs / deltaTime * 100.0) : 0.0;
                cpuUsagePercent.store(cpuPct, std::memory_order_relaxed);

                // Track peak CPU
                double currentPeak = peakCpuPercent.load(std::memory_order_relaxed);
                while (cpuPct > currentPeak &&
                       !peakCpuPercent.compare_exchange_weak(currentPeak, cpuPct, std::memory_order_relaxed)) {}

                // Track average CPU (safe: Sample() is called sequentially)
                totalCpuPercent += cpuPct;
                cpuSampleCount++;
            }

            lastUserTime = utime;
            lastSystemTime = stime;
            lastTimestamp = now;
        }
    }

    double GetAvgCpuUsage() const
    {
        return cpuSampleCount > 0 ? totalCpuPercent / cpuSampleCount : 0.0;
    }

    double GetPeakCpuUsage() const
    {
        return peakCpuPercent.load(std::memory_order_relaxed);
    }

    void Reset()
    {
        cpuUsagePercent.store(0.0, std::memory_order_relaxed);
        memoryUsedKB.store(0, std::memory_order_relaxed);
        peakMemoryKB.store(0, std::memory_order_relaxed);
        peakCpuPercent.store(0.0, std::memory_order_relaxed);
        totalCpuPercent = 0.0;
        cpuSampleCount = 0;
        lastUserTime = 0;
        lastSystemTime = 0;
        lastTimestamp = 0;
    }
};

// ============================================================================
// Pre-generated Data Pool - Avoid data generation in critical path
// ============================================================================
class DataPool
{
public:
    using DataGenerator = std::function<std::shared_ptr<arrow::RecordBatch>(int64_t)>;

    void Initialize(DataGenerator generator, int64_t rowsPerBatch, int poolSize)
    {
        mBatches.clear();
        mBatches.reserve(poolSize);
        for (int i = 0; i < poolSize; i++)
        {
            mBatches.push_back(generator(rowsPerBatch));
        }
        mInitialized = true;
    }

    std::shared_ptr<arrow::RecordBatch> GetNext()
    {
        if (!mInitialized || mBatches.empty()) return nullptr;
        size_t idx = mIndex.fetch_add(1, std::memory_order_relaxed) % mBatches.size();
        return mBatches[idx];
    }

    bool IsInitialized() const { return mInitialized; }

    void Clear()
    {
        mBatches.clear();
        mIndex.store(0, std::memory_order_relaxed);
        mInitialized = false;
    }

private:
    std::vector<std::shared_ptr<arrow::RecordBatch>> mBatches;
    std::atomic<size_t> mIndex{0};
    bool mInitialized{false};
};

// ============================================================================
// Rate Limiter - Token bucket algorithm for QPS control
// ============================================================================
class RateLimiter
{
public:
    RateLimiter(double ratePerSecond)
        : mRate(ratePerSecond)
        , mTokens(ratePerSecond)  // Start with full bucket
        , mLastRefillTime(std::chrono::steady_clock::now())
    {}

    void Acquire()
    {
        if (mRate <= 0) return;  // No limit

        std::unique_lock<std::mutex> lock(mMutex);

        while (true)
        {
            Refill();

            if (mTokens >= 1.0)
            {
                mTokens -= 1.0;
                return;
            }

            // Calculate wait time
            double tokensNeeded = 1.0 - mTokens;
            double waitSeconds = tokensNeeded / mRate;
            auto waitDuration = std::chrono::microseconds((int64_t)(waitSeconds * 1000000));

            lock.unlock();
            std::this_thread::sleep_for(waitDuration);
            lock.lock();
        }
    }

    void SetRate(double ratePerSecond)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRate = ratePerSecond;
    }

private:
    void Refill()
    {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - mLastRefillTime).count();
        mTokens = std::min(mRate, mTokens + elapsed * mRate);  // Cap at rate (1 second worth)
        mLastRefillTime = now;
    }

    double mRate;
    double mTokens;
    std::chrono::steady_clock::time_point mLastRefillTime;
    std::mutex mMutex;
};

} // namespace benchmark_util
