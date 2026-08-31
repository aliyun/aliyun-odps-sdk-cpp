#pragma once

#include <atomic>
#include <array>
#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <iomanip>

#include "tool.h"

namespace benchmark_util {

// ============================================================================
// Latency Histogram - Lock-free latency distribution tracking
// ============================================================================
struct LatencyHistogram
{
    // Bucket boundaries in milliseconds: <1, 1-2, 2-5, 5-10, 10-20, 20-50,
    // 50-100, 100-200, 200-500, 500-1000, 1000-2000, 2000-5000, >5000
    static constexpr size_t NUM_BUCKETS = 13;

    std::array<std::atomic<int64_t>, NUM_BUCKETS> counts{};
    std::atomic<int64_t> minLatency{INT64_MAX};
    std::atomic<int64_t> maxLatency{0};

    static int64_t GetBucketBound(size_t index)
    {
        static const int64_t bounds[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, INT64_MAX};
        return bounds[index];
    }

    void Record(int64_t latencyMs)
    {
        // Update min/max atomically
        int64_t currentMin = minLatency.load(std::memory_order_relaxed);
        while (latencyMs < currentMin &&
               !minLatency.compare_exchange_weak(currentMin, latencyMs, std::memory_order_relaxed)) {}

        int64_t currentMax = maxLatency.load(std::memory_order_relaxed);
        while (latencyMs > currentMax &&
               !maxLatency.compare_exchange_weak(currentMax, latencyMs, std::memory_order_relaxed)) {}

        // Find bucket and increment
        for (size_t i = 0; i < NUM_BUCKETS; i++)
        {
            if (latencyMs < GetBucketBound(i))
            {
                counts[i].fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
        counts[NUM_BUCKETS - 1].fetch_add(1, std::memory_order_relaxed);
    }

    void Reset()
    {
        for (auto& c : counts) c.store(0, std::memory_order_relaxed);
        minLatency.store(INT64_MAX, std::memory_order_relaxed);
        maxLatency.store(0, std::memory_order_relaxed);
    }

    int64_t GetTotalCount() const
    {
        int64_t total = 0;
        for (const auto& c : counts) total += c.load(std::memory_order_relaxed);
        return total;
    }

    void Print(const std::string& prefix = "") const
    {
        int64_t total = GetTotalCount();
        if (total == 0) return;

        tool::Println(prefix, "Latency Distribution Histogram:");
        tool::Println(prefix, "  Min Latency:", minLatency.load(), "ms");
        tool::Println(prefix, "  Max Latency:", maxLatency.load(), "ms");

        int64_t prevBound = 0;
        for (size_t i = 0; i < NUM_BUCKETS; i++)
        {
            int64_t count = counts[i].load(std::memory_order_relaxed);
            if (count > 0)
            {
                double pct = 100.0 * count / total;
                std::string range;
                int64_t bound = GetBucketBound(i);
                if (i == NUM_BUCKETS - 1)
                    range = ">=" + std::to_string(GetBucketBound(i-1)) + "ms";
                else if (i == 0)
                    range = "<" + std::to_string(bound) + "ms";
                else
                    range = std::to_string(prevBound) + "-" + std::to_string(bound) + "ms";

                // Build a simple bar
                int barLen = std::min(50, (int)(pct / 2));
                std::string bar(barLen, '#');

                tool::Println(prefix, "  ", std::setw(12), range, ": ",
                        std::setw(8), count, " (", std::fixed, std::setprecision(1), pct, "%) ", bar);
            }
            prevBound = GetBucketBound(i);
        }
    }
};

// ============================================================================
// Metrics - Optimized with per-thread latency buffers (lock-free recording)
// ============================================================================
static constexpr int MAX_THREADS = 1024;

struct Metrics
{
    // Atomic counters - lock-free
    std::atomic_int_fast64_t totalOps{0};
    std::atomic_int_fast64_t totalRows{0};
    std::atomic_int_fast64_t totalSize{0};
    std::atomic_int_fast64_t totalWireSize{0};
    std::atomic_int_fast64_t totalLatencyMs{0};
    std::atomic_int_fast64_t errorCount{0};

    // Sub-operation timing in MICROSECONDS for higher precision
    std::atomic_int_fast64_t totalSessionCreateTimeUs{0};
    std::atomic_int_fast64_t totalStreamCreateTimeUs{0};
    std::atomic_int_fast64_t totalGenTimeUs{0};
    std::atomic_int_fast64_t totalWriteTimeUs{0};
    std::atomic_int_fast64_t totalCommitTimeUs{0};

    std::atomic_int_fast64_t totalSplitGetTimeUs{0};
    std::atomic_int_fast64_t totalReadStreamCreateTimeUs{0};
    std::atomic_int_fast64_t totalReadTimeUs{0};
    std::atomic_int_fast64_t totalReadCloseTimeUs{0};

    // Coordinated Omission tracking
    std::atomic_int_fast64_t coordinatedOmissionMs{0};
    std::atomic_int_fast64_t correctedLatencyMs{0};

    int64_t wallClockTimeMs{0};
    int64_t startTimeMs{0};

    // Per-thread latency buffers - no lock contention during recording
    std::vector<std::vector<int64_t>> perThreadLatencies;
    std::atomic<int> registeredThreadCount{0};

    // Latency histogram - lock-free
    LatencyHistogram histogram;

    // Cached sorted latencies and percentiles (computed once at end)
    mutable std::vector<int64_t> cachedSortedLatencies;
    mutable bool latenciesSorted{false};
    mutable std::mutex sortMutex;

    void Initialize(int maxThreads)
    {
        perThreadLatencies.resize(maxThreads);
        for (auto& v : perThreadLatencies)
        {
            v.reserve(100000);  // Pre-allocate to avoid reallocations
        }
    }

    int RegisterThread()
    {
        return registeredThreadCount.fetch_add(1, std::memory_order_relaxed);
    }

    void Reset()
    {
        totalOps = 0;
        totalRows = 0;
        totalSize = 0;
        totalWireSize = 0;
        totalLatencyMs = 0;
        errorCount = 0;
        totalSessionCreateTimeUs = 0;
        totalStreamCreateTimeUs = 0;
        totalGenTimeUs = 0;
        totalWriteTimeUs = 0;
        totalCommitTimeUs = 0;
        totalSplitGetTimeUs = 0;
        totalReadStreamCreateTimeUs = 0;
        totalReadTimeUs = 0;
        totalReadCloseTimeUs = 0;
        coordinatedOmissionMs = 0;
        correctedLatencyMs = 0;
        wallClockTimeMs = 0;
        startTimeMs = 0;

        for (auto& v : perThreadLatencies) v.clear();
        registeredThreadCount = 0;
        histogram.Reset();

        std::lock_guard<std::mutex> lock(sortMutex);
        cachedSortedLatencies.clear();
        latenciesSorted = false;
    }

    // Lock-free recording for per-thread buffers
    void RecordOp(int threadId, int64_t latencyMs, int64_t rows, int64_t size, int64_t wireSize = 0)
    {
        totalOps.fetch_add(1, std::memory_order_relaxed);
        totalRows.fetch_add(rows, std::memory_order_relaxed);
        totalSize.fetch_add(size, std::memory_order_relaxed);
        totalWireSize.fetch_add(wireSize, std::memory_order_relaxed);
        totalLatencyMs.fetch_add(latencyMs, std::memory_order_relaxed);

        // Lock-free: each thread writes to its own buffer
        if (threadId >= 0 && threadId < (int)perThreadLatencies.size())
        {
            perThreadLatencies[threadId].push_back(latencyMs);
        }

        // Lock-free histogram update
        histogram.Record(latencyMs);
    }

    void RecordWriteOp(int threadId, int64_t latencyMs, int64_t rows, int64_t size,
                       int64_t sessionCreateUs, int64_t streamCreateUs,
                       int64_t genUs, int64_t writeUs, int64_t commitUs,
                       int64_t wireSize = 0)
    {
        RecordOp(threadId, latencyMs, rows, size, wireSize);
        totalSessionCreateTimeUs.fetch_add(sessionCreateUs, std::memory_order_relaxed);
        totalStreamCreateTimeUs.fetch_add(streamCreateUs, std::memory_order_relaxed);
        totalGenTimeUs.fetch_add(genUs, std::memory_order_relaxed);
        totalWriteTimeUs.fetch_add(writeUs, std::memory_order_relaxed);
        totalCommitTimeUs.fetch_add(commitUs, std::memory_order_relaxed);
    }

    void RecordReadOp(int threadId, int64_t latencyMs, int64_t rows, int64_t size,
                      int64_t splitGetUs, int64_t streamCreateUs,
                      int64_t readUs, int64_t closeUs,
                      int64_t wireSize = 0)
    {
        RecordOp(threadId, latencyMs, rows, size, wireSize);
        totalSplitGetTimeUs.fetch_add(splitGetUs, std::memory_order_relaxed);
        totalReadStreamCreateTimeUs.fetch_add(streamCreateUs, std::memory_order_relaxed);
        totalReadTimeUs.fetch_add(readUs, std::memory_order_relaxed);
        totalReadCloseTimeUs.fetch_add(closeUs, std::memory_order_relaxed);
    }

    void RecordError()
    {
        errorCount.fetch_add(1, std::memory_order_relaxed);
    }

    double GetQPS() const
    {
        if (wallClockTimeMs <= 0) return 0.0;
        return (double)totalOps.load() / ((double)wallClockTimeMs / 1000.0);
    }

    double GetAvgLatency() const
    {
        if (totalOps.load() == 0) return 0.0;
        return (double)totalLatencyMs.load() / (double)totalOps.load() / 1000.0;
    }

    double GetThroughput() const
    {
        if (wallClockTimeMs <= 0) return 0.0;
        return (double)totalSize.load() / ((double)wallClockTimeMs / 1000.0);
    }

    double GetWireThroughput() const
    {
        if (wallClockTimeMs <= 0) return 0.0;
        return (double)totalWireSize.load() / ((double)wallClockTimeMs / 1000.0);
    }

    double GetRPS() const
    {
        if (wallClockTimeMs <= 0) return 0.0;
        return (double)totalRows.load() / ((double)wallClockTimeMs / 1000.0);
    }

    // Merge all per-thread latencies and sort (called once at end)
    void PrepareLatenciesForPercentile() const
    {
        std::lock_guard<std::mutex> lock(sortMutex);
        if (latenciesSorted) return;

        cachedSortedLatencies.clear();
        size_t totalSize = 0;
        for (const auto& v : perThreadLatencies) totalSize += v.size();
        cachedSortedLatencies.reserve(totalSize);

        for (const auto& v : perThreadLatencies)
        {
            cachedSortedLatencies.insert(cachedSortedLatencies.end(), v.begin(), v.end());
        }

        std::sort(cachedSortedLatencies.begin(), cachedSortedLatencies.end());
        latenciesSorted = true;
    }

    double GetPercentileLatency(double percentile) const
    {
        PrepareLatenciesForPercentile();

        std::lock_guard<std::mutex> lock(sortMutex);
        if (cachedSortedLatencies.empty()) return 0.0;

        size_t index = (size_t)(cachedSortedLatencies.size() * percentile);
        if (index >= cachedSortedLatencies.size()) index = cachedSortedLatencies.size() - 1;

        return (double)cachedSortedLatencies[index] / 1000.0;
    }

    double GetLatencyStdDev() const
    {
        PrepareLatenciesForPercentile();

        std::lock_guard<std::mutex> lock(sortMutex);
        if (cachedSortedLatencies.size() < 2) return 0.0;

        double mean = (double)totalLatencyMs.load() / cachedSortedLatencies.size();
        double sumSquaredDiff = 0.0;
        for (int64_t lat : cachedSortedLatencies)
        {
            double diff = lat - mean;
            sumSquaredDiff += diff * diff;
        }
        return std::sqrt(sumSquaredDiff / cachedSortedLatencies.size()) / 1000.0;
    }

    int64_t GetMinLatency() const
    {
        return histogram.minLatency.load(std::memory_order_relaxed);
    }

    int64_t GetMaxLatency() const
    {
        return histogram.maxLatency.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// TestResult - Snapshot of test results
// ============================================================================
struct TestResult
{
    int threadCount;

    int64_t totalOps;
    int64_t totalRows;
    int64_t totalSize;
    int64_t totalWireSize;
    int64_t wallClockTimeMs;
    int64_t errorCount;

    double qps;
    double rps;
    double throughput;
    double wireThroughput;
    double avgLatency;
    double p50Latency;
    double p90Latency;
    double p95Latency;
    double p99Latency;
    double p999Latency;
    double minLatency;
    double maxLatency;
    double stdDevLatency;

    TestResult() : threadCount(0), totalOps(0), totalRows(0), totalSize(0),
                   totalWireSize(0), wallClockTimeMs(0), errorCount(0), qps(0), rps(0),
                   throughput(0), wireThroughput(0), avgLatency(0), p50Latency(0), p90Latency(0),
                   p95Latency(0), p99Latency(0), p999Latency(0),
                   minLatency(0), maxLatency(0), stdDevLatency(0) {}

    static TestResult FromMetrics(int threads, const Metrics& metrics)
    {
        TestResult result;
        result.threadCount = threads;
        result.totalOps = metrics.totalOps.load();
        result.totalRows = metrics.totalRows.load();
        result.totalSize = metrics.totalSize.load();
        result.totalWireSize = metrics.totalWireSize.load();
        result.wallClockTimeMs = metrics.wallClockTimeMs;
        result.errorCount = metrics.errorCount.load();
        result.qps = metrics.GetQPS();
        result.rps = metrics.GetRPS();
        result.throughput = metrics.GetThroughput();
        result.wireThroughput = metrics.GetWireThroughput();
        result.avgLatency = metrics.GetAvgLatency();

        // Compute percentiles once (triggers merge and sort)
        result.p50Latency = metrics.GetPercentileLatency(0.50);
        result.p90Latency = metrics.GetPercentileLatency(0.90);
        result.p95Latency = metrics.GetPercentileLatency(0.95);
        result.p99Latency = metrics.GetPercentileLatency(0.99);
        result.p999Latency = metrics.GetPercentileLatency(0.999);
        result.minLatency = (double)metrics.GetMinLatency() / 1000.0;
        result.maxLatency = (double)metrics.GetMaxLatency() / 1000.0;
        result.stdDevLatency = metrics.GetLatencyStdDev();
        return result;
    }

    double GetQPS() const { return qps; }
    double GetAvgLatency() const { return avgLatency; }
    double GetThroughput() const { return throughput; }
    double GetWireThroughput() const { return wireThroughput; }
    double GetRPS() const { return rps; }
};

} // namespace benchmark_util
