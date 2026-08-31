#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <vector>

#include "tool.h"
#include "benchmark_metrics.h"
#include "benchmark_utils.h"
#include "arrow/api.h"

namespace benchmark_util {

// ============================================================================
// Report Configuration - Passed to reporter functions
// ============================================================================
struct ReportConfig
{
    std::string projectName;
    std::string tableName;
    std::string schema;
    std::string partitionSpec;
    std::string mode;
    int threadCount;
    int64_t rowsPerRun;
    double rateLimit;
    int64_t warmupSeconds;
    bool useDataPool;
    bool enableCoordinatedOmission;
    bool enableSystemMetrics;
    std::string lastWriteSessionId;
    std::string lastReadSessionId;
    std::shared_ptr<arrow::Schema> tableSchema;
    SystemResourceMetrics* systemMetrics;
};

// ============================================================================
// Schema Formatting
// ============================================================================
inline std::string FormatSchemaInfo(const std::shared_ptr<arrow::Schema>& schema)
{
    std::ostringstream oss;
    oss << "Schema (" << schema->num_fields() << " fields):\n";
    for (int i = 0; i < schema->num_fields(); i++)
    {
        auto field = schema->field(i);
        oss << "  [" << i << "] " << field->name()
            << " : " << field->type()->ToString()
            << (field->nullable() ? " (nullable)" : " (not null)") << "\n";
    }
    return oss.str();
}

// ============================================================================
// Print Metrics to Console
// ============================================================================
inline void PrintMetrics(const Metrics& metrics, const ReportConfig& config, const std::string& prefix = "")
{
    using tool::Println;
    using tool::DoubleSizeUnitFormatter;

    Println(prefix, "==========================================");
    Println(prefix, "Metrics Summary:");
    Println(prefix, "==========================================");
    Println(prefix, "Test Configuration:");
    Println(prefix, "  Project:", config.projectName);
    Println(prefix, "  Table:", config.tableName);
    Println(prefix, "  Schema:", config.schema);
    if (!config.partitionSpec.empty())
    {
        Println(prefix, "  Partition:", config.partitionSpec);
    }
    Println(prefix, "  Mode:", config.mode);
    Println(prefix, "  Threads:", config.threadCount);
    Println(prefix, "  Rows per run:", config.rowsPerRun);
    if (config.rateLimit > 0)
    {
        Println(prefix, "  Rate Limit:", std::fixed, std::setprecision(1), config.rateLimit, "ops/s/thread");
    }
    if (config.mode == "write" && !config.lastWriteSessionId.empty())
    {
        Println(prefix, "  Last Write Session ID:", config.lastWriteSessionId);
    }
    if (config.mode == "read" && !config.lastReadSessionId.empty())
    {
        Println(prefix, "  Read Session ID:", config.lastReadSessionId);
    }
    Println(prefix, "");
    Println(prefix, "Table Schema:");
    if (config.tableSchema)
    {
        Println(prefix, FormatSchemaInfo(config.tableSchema));
    }
    else
    {
        Println(prefix, "  Schema information not available");
    }
    Println(prefix, "");
    Println(prefix, "Performance Metrics:");
    Println(prefix, "  Total Ops:", metrics.totalOps.load());
    Println(prefix, "  Total Rows:", metrics.totalRows.load());
    Println(prefix, "  Total Memory Size:", DoubleSizeUnitFormatter(metrics.totalSize.load(), ""));
    Println(prefix, "  Total Wire Size:", DoubleSizeUnitFormatter(metrics.totalWireSize.load(), ""));
    Println(prefix, "  Wall Clock Time:", metrics.wallClockTimeMs, "ms");
    Println(prefix, "  QPS:", std::fixed, std::setprecision(2), metrics.GetQPS());
    Println(prefix, "  RPS:", std::fixed, std::setprecision(2), metrics.GetRPS());
    Println(prefix, "  Throughput (Memory):", std::fixed, std::setprecision(2), DoubleSizeUnitFormatter(metrics.GetThroughput(), "/s"));
    Println(prefix, "  Throughput (Wire):", std::fixed, std::setprecision(2), DoubleSizeUnitFormatter(metrics.GetWireThroughput(), "/s"));
    Println(prefix, "");
    Println(prefix, "Latency Statistics:");
    Println(prefix, "  Avg Latency:", std::fixed, std::setprecision(4), metrics.GetAvgLatency(), "s");
    Println(prefix, "  Min Latency:", std::fixed, std::setprecision(4), (double)metrics.GetMinLatency() / 1000.0, "s");
    Println(prefix, "  Max Latency:", std::fixed, std::setprecision(4), (double)metrics.GetMaxLatency() / 1000.0, "s");

    // Show N/A for Std Dev when there's only 1 operation
    int64_t totalOps = metrics.totalOps.load();
    if (totalOps <= 1)
    {
        Println(prefix, "  Std Dev: N/A (single operation)");
    }
    else
    {
        Println(prefix, "  Std Dev:", std::fixed, std::setprecision(4), metrics.GetLatencyStdDev(), "s");
    }

    Println(prefix, "  P50 Latency:", std::fixed, std::setprecision(4), metrics.GetPercentileLatency(0.50), "s");
    Println(prefix, "  P90 Latency:", std::fixed, std::setprecision(4), metrics.GetPercentileLatency(0.90), "s");
    Println(prefix, "  P95 Latency:", std::fixed, std::setprecision(4), metrics.GetPercentileLatency(0.95), "s");
    Println(prefix, "  P99 Latency:", std::fixed, std::setprecision(4), metrics.GetPercentileLatency(0.99), "s");
    Println(prefix, "  P999 Latency:", std::fixed, std::setprecision(4), metrics.GetPercentileLatency(0.999), "s");
    Println(prefix, "  Errors:", metrics.errorCount.load());
    Println(prefix, "");

    // Print latency histogram
    metrics.histogram.Print(prefix);
    Println(prefix, "");

    // Write mode detailed breakdown (times in microseconds, display in ms)
    if (totalOps > 0 && metrics.totalSessionCreateTimeUs.load() > 0)
    {
        Println(prefix, "  Detailed Time Breakdown - Write Mode (per operation):");
        Println(prefix, "    Avg Session Create Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalSessionCreateTimeUs.load() / totalOps / 1000.0, "ms");
        Println(prefix, "    Avg Stream Create Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalStreamCreateTimeUs.load() / totalOps / 1000.0, "ms");

        // Add note for data pool usage
        double avgGenTimeMs = (double)metrics.totalGenTimeUs.load() / totalOps / 1000.0;
        if (config.useDataPool)
        {
            Println(prefix, "    Avg Data Gen Time:", std::fixed, std::setprecision(3),
                    avgGenTimeMs, "ms (via data pool)");
        }
        else
        {
            Println(prefix, "    Avg Data Gen Time:", std::fixed, std::setprecision(3),
                    avgGenTimeMs, "ms");
        }

        Println(prefix, "    Avg Write Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalWriteTimeUs.load() / totalOps / 1000.0, "ms");
        Println(prefix, "    Avg Commit Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalCommitTimeUs.load() / totalOps / 1000.0, "ms");

        double writeOnlyThroughput = metrics.totalWriteTimeUs.load() > 0 ?
            (double)metrics.totalSize.load() / ((double)metrics.totalWriteTimeUs.load() / 1000000.0) : 0.0;
        Println(prefix, "  Write-only Throughput (excludes session/commit):",
                std::fixed, std::setprecision(2), DoubleSizeUnitFormatter(writeOnlyThroughput, "/s"));
    }

    // Read mode detailed breakdown (times in microseconds, display in ms)
    if (totalOps > 0 && metrics.totalReadTimeUs.load() > 0)
    {
        Println(prefix, "  Detailed Time Breakdown - Read Mode (per operation):");
        Println(prefix, "    Avg Split Get Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalSplitGetTimeUs.load() / totalOps / 1000.0, "ms");
        Println(prefix, "    Avg Stream Create Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalReadStreamCreateTimeUs.load() / totalOps / 1000.0, "ms");
        Println(prefix, "    Avg Read Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalReadTimeUs.load() / totalOps / 1000.0, "ms");
        Println(prefix, "    Avg Close Time:", std::fixed, std::setprecision(3),
                (double)metrics.totalReadCloseTimeUs.load() / totalOps / 1000.0, "ms");

        double readOnlyThroughput = metrics.totalReadTimeUs.load() > 0 ?
            (double)metrics.totalSize.load() / ((double)metrics.totalReadTimeUs.load() / 1000000.0) : 0.0;
        Println(prefix, "  Read-only Throughput (excludes split/stream/close):",
                std::fixed, std::setprecision(2), DoubleSizeUnitFormatter(readOnlyThroughput, "/s"));
    }

    // Coordinated Omission Statistics
    if (config.enableCoordinatedOmission && metrics.coordinatedOmissionMs.load() > 0)
    {
        Println(prefix, "");
        Println(prefix, "Coordinated Omission Statistics:");
        Println(prefix, "  Total Coordinated Omission:", metrics.coordinatedOmissionMs.load(), "ms");
        Println(prefix, "  Avg Coordinated Omission:", std::fixed, std::setprecision(2),
                (double)metrics.coordinatedOmissionMs.load() / totalOps, "ms/op");
        if (totalOps > 0)
        {
            double correctedAvgLatency = (double)metrics.correctedLatencyMs.load() / totalOps / 1000.0;
            Println(prefix, "  Corrected Avg Latency:", std::fixed, std::setprecision(4), correctedAvgLatency, "s");
        }
    }

    // System Resource Statistics - show avg and peak CPU
    if (config.enableSystemMetrics && config.systemMetrics)
    {
        Println(prefix, "");
        Println(prefix, "System Resource Statistics:");
        Println(prefix, "  Current Memory (RSS):", config.systemMetrics->memoryUsedKB.load(), "KB");
        Println(prefix, "  Peak Memory (RSS):", config.systemMetrics->peakMemoryKB.load(), "KB");
        Println(prefix, "  Avg CPU Usage:", std::fixed, std::setprecision(1), config.systemMetrics->GetAvgCpuUsage(), "%");
        Println(prefix, "  Peak CPU Usage:", std::fixed, std::setprecision(1), config.systemMetrics->GetPeakCpuUsage(), "%");
    }
}

// ============================================================================
// JSON Report Generator
// ============================================================================
inline void WriteJsonReport(const Metrics& metrics, const ReportConfig& config, const std::string& filename)
{
    using tool::Println;
    using tool::Errorln;

    std::ofstream jsonFile(filename);
    if (!jsonFile.is_open())
    {
        Errorln("Failed to open JSON report file:", filename);
        return;
    }

    jsonFile << "{\n";

    // Test Configuration
    jsonFile << "  \"test_config\": {\n";
    jsonFile << "    \"project\": \"" << config.projectName << "\",\n";
    jsonFile << "    \"table\": \"" << config.tableName << "\",\n";
    jsonFile << "    \"schema\": \"" << config.schema << "\",\n";
    jsonFile << "    \"partition\": \"" << config.partitionSpec << "\",\n";
    jsonFile << "    \"mode\": \"" << config.mode << "\",\n";
    jsonFile << "    \"threads\": " << config.threadCount << ",\n";
    jsonFile << "    \"rows_per_run\": " << config.rowsPerRun << ",\n";
    jsonFile << "    \"rate_limit\": " << std::fixed << std::setprecision(1) << config.rateLimit << ",\n";
    jsonFile << "    \"warmup_seconds\": " << config.warmupSeconds << ",\n";
    jsonFile << "    \"use_data_pool\": " << (config.useDataPool ? "true" : "false") << ",\n";
    jsonFile << "    \"enable_coordinated_omission\": " << (config.enableCoordinatedOmission ? "true" : "false") << ",\n";
    jsonFile << "    \"enable_system_metrics\": " << (config.enableSystemMetrics ? "true" : "false") << "\n";
    jsonFile << "  },\n";

    // Performance Summary
    jsonFile << "  \"performance_summary\": {\n";
    jsonFile << "    \"total_ops\": " << metrics.totalOps.load() << ",\n";
    jsonFile << "    \"total_rows\": " << metrics.totalRows.load() << ",\n";
    jsonFile << "    \"total_memory_size_bytes\": " << metrics.totalSize.load() << ",\n";
    jsonFile << "    \"total_wire_size_bytes\": " << metrics.totalWireSize.load() << ",\n";
    jsonFile << "    \"wall_clock_time_ms\": " << metrics.wallClockTimeMs << ",\n";
    jsonFile << "    \"qps\": " << std::fixed << std::setprecision(2) << metrics.GetQPS() << ",\n";
    jsonFile << "    \"rps\": " << std::fixed << std::setprecision(2) << metrics.GetRPS() << ",\n";
    jsonFile << "    \"memory_throughput_bytes_per_sec\": " << std::fixed << std::setprecision(2) << metrics.GetThroughput() << ",\n";
    jsonFile << "    \"wire_throughput_bytes_per_sec\": " << std::fixed << std::setprecision(2) << metrics.GetWireThroughput() << ",\n";
    jsonFile << "    \"error_count\": " << metrics.errorCount.load() << "\n";
    jsonFile << "  },\n";

    // Latency Statistics
    jsonFile << "  \"latency_stats\": {\n";
    jsonFile << "    \"avg_ms\": " << std::fixed << std::setprecision(4) << metrics.GetAvgLatency() * 1000.0 << ",\n";
    jsonFile << "    \"min_ms\": " << std::fixed << std::setprecision(4) << (double)metrics.GetMinLatency() << ",\n";
    jsonFile << "    \"max_ms\": " << std::fixed << std::setprecision(4) << (double)metrics.GetMaxLatency() << ",\n";
    jsonFile << "    \"std_dev_ms\": " << std::fixed << std::setprecision(4) << metrics.GetLatencyStdDev() * 1000.0 << ",\n";
    jsonFile << "    \"p50_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.50) * 1000.0 << ",\n";
    jsonFile << "    \"p90_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.90) * 1000.0 << ",\n";
    jsonFile << "    \"p95_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.95) * 1000.0 << ",\n";
    jsonFile << "    \"p99_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.99) * 1000.0 << ",\n";
    jsonFile << "    \"p999_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.999) * 1000.0 << "\n";
    jsonFile << "  },\n";

    // Time Breakdown (times stored in microseconds, output in ms)
    jsonFile << "  \"time_breakdown\": {\n";
    int64_t ops = metrics.totalOps.load();
    if (ops > 0)
    {
        if (metrics.totalSessionCreateTimeUs.load() > 0)
        {
            // Write mode breakdown
            jsonFile << "    \"avg_session_create_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalSessionCreateTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_stream_create_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalStreamCreateTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_data_gen_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalGenTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_write_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalWriteTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_commit_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalCommitTimeUs.load() / ops / 1000.0 << "\n";
        }
        else if (metrics.totalReadTimeUs.load() > 0)
        {
            // Read mode breakdown
            jsonFile << "    \"avg_split_get_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalSplitGetTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_stream_create_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalReadStreamCreateTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_read_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalReadTimeUs.load() / ops / 1000.0 << ",\n";
            jsonFile << "    \"avg_close_ms\": " << std::fixed << std::setprecision(3)
                     << (double)metrics.totalReadCloseTimeUs.load() / ops / 1000.0 << "\n";
        }
    }
    jsonFile << "  },\n";

    // Coordinated Omission (if enabled)
    jsonFile << "  \"coordinated_omission\": {\n";
    jsonFile << "    \"enabled\": " << (config.enableCoordinatedOmission ? "true" : "false") << ",\n";
    jsonFile << "    \"total_omission_ms\": " << metrics.coordinatedOmissionMs.load() << ",\n";
    if (ops > 0 && metrics.coordinatedOmissionMs.load() > 0)
    {
        jsonFile << "    \"avg_omission_ms\": " << std::fixed << std::setprecision(2)
                 << (double)metrics.coordinatedOmissionMs.load() / ops << ",\n";
        jsonFile << "    \"corrected_avg_latency_ms\": " << std::fixed << std::setprecision(2)
                 << (double)metrics.correctedLatencyMs.load() / ops << "\n";
    }
    else
    {
        jsonFile << "    \"avg_omission_ms\": 0,\n";
        jsonFile << "    \"corrected_avg_latency_ms\": 0\n";
    }
    jsonFile << "  },\n";

    // System Resources (if enabled) - include avg and peak CPU
    jsonFile << "  \"system_resources\": {\n";
    jsonFile << "    \"enabled\": " << (config.enableSystemMetrics ? "true" : "false") << ",\n";
    if (config.systemMetrics)
    {
        jsonFile << "    \"memory_used_kb\": " << config.systemMetrics->memoryUsedKB.load() << ",\n";
        jsonFile << "    \"peak_memory_kb\": " << config.systemMetrics->peakMemoryKB.load() << ",\n";
        jsonFile << "    \"avg_cpu_usage_percent\": " << std::fixed << std::setprecision(1)
                 << config.systemMetrics->GetAvgCpuUsage() << ",\n";
        jsonFile << "    \"peak_cpu_usage_percent\": " << std::fixed << std::setprecision(1)
                 << config.systemMetrics->GetPeakCpuUsage() << "\n";
    }
    else
    {
        jsonFile << "    \"memory_used_kb\": 0,\n";
        jsonFile << "    \"peak_memory_kb\": 0,\n";
        jsonFile << "    \"avg_cpu_usage_percent\": 0,\n";
        jsonFile << "    \"peak_cpu_usage_percent\": 0\n";
    }
    jsonFile << "  },\n";

    // Latency Histogram
    jsonFile << "  \"latency_histogram\": [\n";
    const LatencyHistogram& hist = metrics.histogram;
    int64_t prevBound = 0;
    bool first = true;
    for (size_t i = 0; i < LatencyHistogram::NUM_BUCKETS; i++)
    {
        int64_t count = hist.counts[i].load(std::memory_order_relaxed);
        if (count > 0)
        {
            if (!first) jsonFile << ",\n";
            first = false;

            int64_t bound = LatencyHistogram::GetBucketBound(i);
            std::string range;
            if (i == LatencyHistogram::NUM_BUCKETS - 1)
                range = ">=" + std::to_string(LatencyHistogram::GetBucketBound(i-1));
            else if (i == 0)
                range = "<" + std::to_string(bound);
            else
                range = std::to_string(prevBound) + "-" + std::to_string(bound);

            jsonFile << "    {\"range_ms\": \"" << range << "\", \"count\": " << count << "}";
        }
        prevBound = LatencyHistogram::GetBucketBound(i);
    }
    jsonFile << "\n  ]\n";

    jsonFile << "}\n";
    jsonFile.close();

    Println("JSON report saved to:", filename);
}

// ============================================================================
// JSON Report for Linear Scaling Test
// ============================================================================
inline void WriteScalingJsonReport(const std::vector<TestResult>& results, const ReportConfig& config, const std::string& filename)
{
    using tool::Println;
    using tool::Errorln;

    if (results.empty()) return;

    std::ofstream f(filename);
    if (!f.is_open())
    {
        Errorln("Failed to open scaling JSON report file:", filename);
        return;
    }

    f << "{\n";
    f << "  \"report_type\": \"linear_scaling\",\n";

    // Test config
    f << "  \"test_config\": {\n";
    f << "    \"project\": \"" << config.projectName << "\",\n";
    f << "    \"table\": \"" << config.tableName << "\",\n";
    f << "    \"mode\": \"" << config.mode << "\",\n";
    f << "    \"min_threads\": " << results.front().threadCount << ",\n";
    f << "    \"max_threads\": " << results.back().threadCount << "\n";
    f << "  },\n";

    // Results array
    f << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); i++)
    {
        const auto& r = results[i];
        if (i > 0) f << ",\n";
        f << "    {\n";
        f << "      \"threads\": " << r.threadCount << ",\n";
        f << "      \"total_ops\": " << r.totalOps << ",\n";
        f << "      \"total_rows\": " << r.totalRows << ",\n";
        f << "      \"total_size_bytes\": " << r.totalSize << ",\n";
        f << "      \"total_wire_size_bytes\": " << r.totalWireSize << ",\n";
        f << "      \"wall_clock_time_ms\": " << r.wallClockTimeMs << ",\n";
        f << "      \"qps\": " << std::fixed << std::setprecision(2) << r.GetQPS() << ",\n";
        f << "      \"rps\": " << std::fixed << std::setprecision(2) << r.GetRPS() << ",\n";
        f << "      \"throughput_bytes_per_sec\": " << std::fixed << std::setprecision(2) << r.GetThroughput() << ",\n";
        f << "      \"avg_latency_s\": " << std::fixed << std::setprecision(4) << r.GetAvgLatency() << ",\n";
        f << "      \"p50_latency_s\": " << std::fixed << std::setprecision(4) << r.p50Latency << ",\n";
        f << "      \"p90_latency_s\": " << std::fixed << std::setprecision(4) << r.p90Latency << ",\n";
        f << "      \"p95_latency_s\": " << std::fixed << std::setprecision(4) << r.p95Latency << ",\n";
        f << "      \"p99_latency_s\": " << std::fixed << std::setprecision(4) << r.p99Latency << ",\n";
        f << "      \"p999_latency_s\": " << std::fixed << std::setprecision(4) << r.p999Latency << ",\n";
        f << "      \"min_latency_s\": " << std::fixed << std::setprecision(4) << r.minLatency << ",\n";
        f << "      \"max_latency_s\": " << std::fixed << std::setprecision(4) << r.maxLatency << ",\n";
        f << "      \"error_count\": " << r.errorCount << "\n";
        f << "    }";
    }
    f << "\n  ]\n";
    f << "}\n";
    f.close();

    Println("Scaling JSON report saved to:", filename);
}

// ============================================================================
// Helper: Indent each line of a JSON string
// ============================================================================
inline std::string IndentJson(const std::string& json, const std::string& prefix)
{
    std::ostringstream out;
    std::istringstream in(json);
    std::string line;
    bool first = true;
    while (std::getline(in, line))
    {
        if (!first) out << "\n";
        first = false;
        if (!line.empty())
        {
            out << prefix << line;
        }
    }
    return out.str();
}

// ============================================================================
// Build JSON string for a single benchmark case (normal mode)
// ============================================================================
inline std::string BuildBenchmarkCaseJson(const Metrics& metrics, const ReportConfig& config, const std::string& suiteName)
{
    std::ostringstream j;

    j << "{\n";
    j << "  \"suite_name\": \"" << suiteName << "\",\n";
    j << "  \"type\": \"benchmark\",\n";

    // Test Configuration
    j << "  \"test_config\": {\n";
    j << "    \"project\": \"" << config.projectName << "\",\n";
    j << "    \"table\": \"" << config.tableName << "\",\n";
    j << "    \"schema\": \"" << config.schema << "\",\n";
    j << "    \"partition\": \"" << config.partitionSpec << "\",\n";
    j << "    \"mode\": \"" << config.mode << "\",\n";
    j << "    \"threads\": " << config.threadCount << ",\n";
    j << "    \"rows_per_run\": " << config.rowsPerRun << ",\n";
    j << "    \"rate_limit\": " << std::fixed << std::setprecision(1) << config.rateLimit << ",\n";
    j << "    \"warmup_seconds\": " << config.warmupSeconds << ",\n";
    j << "    \"use_data_pool\": " << (config.useDataPool ? "true" : "false") << ",\n";
    j << "    \"enable_coordinated_omission\": " << (config.enableCoordinatedOmission ? "true" : "false") << ",\n";
    j << "    \"enable_system_metrics\": " << (config.enableSystemMetrics ? "true" : "false") << "\n";
    j << "  },\n";

    // Performance Summary
    j << "  \"performance_summary\": {\n";
    j << "    \"total_ops\": " << metrics.totalOps.load() << ",\n";
    j << "    \"total_rows\": " << metrics.totalRows.load() << ",\n";
    j << "    \"total_memory_size_bytes\": " << metrics.totalSize.load() << ",\n";
    j << "    \"total_wire_size_bytes\": " << metrics.totalWireSize.load() << ",\n";
    j << "    \"wall_clock_time_ms\": " << metrics.wallClockTimeMs << ",\n";
    j << "    \"qps\": " << std::fixed << std::setprecision(2) << metrics.GetQPS() << ",\n";
    j << "    \"rps\": " << std::fixed << std::setprecision(2) << metrics.GetRPS() << ",\n";
    j << "    \"memory_throughput_bytes_per_sec\": " << std::fixed << std::setprecision(2) << metrics.GetThroughput() << ",\n";
    j << "    \"wire_throughput_bytes_per_sec\": " << std::fixed << std::setprecision(2) << metrics.GetWireThroughput() << ",\n";
    j << "    \"error_count\": " << metrics.errorCount.load() << "\n";
    j << "  },\n";

    // Latency Statistics
    j << "  \"latency_stats\": {\n";
    j << "    \"avg_ms\": " << std::fixed << std::setprecision(4) << metrics.GetAvgLatency() * 1000.0 << ",\n";
    j << "    \"min_ms\": " << std::fixed << std::setprecision(4) << (double)metrics.GetMinLatency() << ",\n";
    j << "    \"max_ms\": " << std::fixed << std::setprecision(4) << (double)metrics.GetMaxLatency() << ",\n";
    j << "    \"std_dev_ms\": " << std::fixed << std::setprecision(4) << metrics.GetLatencyStdDev() * 1000.0 << ",\n";
    j << "    \"p50_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.50) * 1000.0 << ",\n";
    j << "    \"p90_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.90) * 1000.0 << ",\n";
    j << "    \"p95_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.95) * 1000.0 << ",\n";
    j << "    \"p99_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.99) * 1000.0 << ",\n";
    j << "    \"p999_ms\": " << std::fixed << std::setprecision(4) << metrics.GetPercentileLatency(0.999) * 1000.0 << "\n";
    j << "  },\n";

    // Time Breakdown
    j << "  \"time_breakdown\": {\n";
    int64_t ops = metrics.totalOps.load();
    if (ops > 0)
    {
        if (metrics.totalSessionCreateTimeUs.load() > 0)
        {
            j << "    \"avg_session_create_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalSessionCreateTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_stream_create_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalStreamCreateTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_data_gen_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalGenTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_write_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalWriteTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_commit_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalCommitTimeUs.load() / ops / 1000.0 << "\n";
        }
        else if (metrics.totalReadTimeUs.load() > 0)
        {
            j << "    \"avg_split_get_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalSplitGetTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_stream_create_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalReadStreamCreateTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_read_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalReadTimeUs.load() / ops / 1000.0 << ",\n";
            j << "    \"avg_close_ms\": " << std::fixed << std::setprecision(3)
              << (double)metrics.totalReadCloseTimeUs.load() / ops / 1000.0 << "\n";
        }
    }
    j << "  },\n";

    // Coordinated Omission
    j << "  \"coordinated_omission\": {\n";
    j << "    \"enabled\": " << (config.enableCoordinatedOmission ? "true" : "false") << ",\n";
    j << "    \"total_omission_ms\": " << metrics.coordinatedOmissionMs.load() << ",\n";
    if (ops > 0 && metrics.coordinatedOmissionMs.load() > 0)
    {
        j << "    \"avg_omission_ms\": " << std::fixed << std::setprecision(2)
          << (double)metrics.coordinatedOmissionMs.load() / ops << ",\n";
        j << "    \"corrected_avg_latency_ms\": " << std::fixed << std::setprecision(2)
          << (double)metrics.correctedLatencyMs.load() / ops << "\n";
    }
    else
    {
        j << "    \"avg_omission_ms\": 0,\n";
        j << "    \"corrected_avg_latency_ms\": 0\n";
    }
    j << "  },\n";

    // System Resources
    j << "  \"system_resources\": {\n";
    j << "    \"enabled\": " << (config.enableSystemMetrics ? "true" : "false") << ",\n";
    if (config.systemMetrics)
    {
        j << "    \"memory_used_kb\": " << config.systemMetrics->memoryUsedKB.load() << ",\n";
        j << "    \"peak_memory_kb\": " << config.systemMetrics->peakMemoryKB.load() << ",\n";
        j << "    \"avg_cpu_usage_percent\": " << std::fixed << std::setprecision(1)
          << config.systemMetrics->GetAvgCpuUsage() << ",\n";
        j << "    \"peak_cpu_usage_percent\": " << std::fixed << std::setprecision(1)
          << config.systemMetrics->GetPeakCpuUsage() << "\n";
    }
    else
    {
        j << "    \"memory_used_kb\": 0,\n";
        j << "    \"peak_memory_kb\": 0,\n";
        j << "    \"avg_cpu_usage_percent\": 0,\n";
        j << "    \"peak_cpu_usage_percent\": 0\n";
    }
    j << "  },\n";

    // Latency Histogram
    j << "  \"latency_histogram\": [\n";
    const LatencyHistogram& hist = metrics.histogram;
    int64_t prevBound = 0;
    bool first = true;
    for (size_t i = 0; i < LatencyHistogram::NUM_BUCKETS; i++)
    {
        int64_t count = hist.counts[i].load(std::memory_order_relaxed);
        if (count > 0)
        {
            if (!first) j << ",\n";
            first = false;

            int64_t bound = LatencyHistogram::GetBucketBound(i);
            std::string range;
            if (i == LatencyHistogram::NUM_BUCKETS - 1)
                range = ">=" + std::to_string(LatencyHistogram::GetBucketBound(i-1));
            else if (i == 0)
                range = "<" + std::to_string(bound);
            else
                range = std::to_string(prevBound) + "-" + std::to_string(bound);

            j << "    {\"range_ms\": \"" << range << "\", \"count\": " << count << "}";
        }
        prevBound = LatencyHistogram::GetBucketBound(i);
    }
    j << "\n  ]\n";
    j << "}";

    return j.str();
}

// ============================================================================
// Build JSON string for a linear scaling test case
// ============================================================================
inline std::string BuildScalingCaseJson(const std::vector<TestResult>& results, const ReportConfig& config, const std::string& suiteName)
{
    if (results.empty()) return "";

    std::ostringstream j;

    j << "{\n";
    j << "  \"suite_name\": \"" << suiteName << "\",\n";
    j << "  \"type\": \"linear_scaling\",\n";

    // Test config
    j << "  \"test_config\": {\n";
    j << "    \"project\": \"" << config.projectName << "\",\n";
    j << "    \"table\": \"" << config.tableName << "\",\n";
    j << "    \"mode\": \"" << config.mode << "\",\n";
    j << "    \"min_threads\": " << results.front().threadCount << ",\n";
    j << "    \"max_threads\": " << results.back().threadCount << "\n";
    j << "  },\n";

    // Results array
    j << "  \"scaling_results\": [\n";
    for (size_t i = 0; i < results.size(); i++)
    {
        const auto& r = results[i];
        if (i > 0) j << ",\n";
        j << "    {\n";
        j << "      \"threads\": " << r.threadCount << ",\n";
        j << "      \"total_ops\": " << r.totalOps << ",\n";
        j << "      \"total_rows\": " << r.totalRows << ",\n";
        j << "      \"total_size_bytes\": " << r.totalSize << ",\n";
        j << "      \"total_wire_size_bytes\": " << r.totalWireSize << ",\n";
        j << "      \"wall_clock_time_ms\": " << r.wallClockTimeMs << ",\n";
        j << "      \"qps\": " << std::fixed << std::setprecision(2) << r.GetQPS() << ",\n";
        j << "      \"rps\": " << std::fixed << std::setprecision(2) << r.GetRPS() << ",\n";
        j << "      \"throughput_bytes_per_sec\": " << std::fixed << std::setprecision(2) << r.GetThroughput() << ",\n";
        j << "      \"avg_latency_s\": " << std::fixed << std::setprecision(4) << r.GetAvgLatency() << ",\n";
        j << "      \"p50_latency_s\": " << std::fixed << std::setprecision(4) << r.p50Latency << ",\n";
        j << "      \"p90_latency_s\": " << std::fixed << std::setprecision(4) << r.p90Latency << ",\n";
        j << "      \"p95_latency_s\": " << std::fixed << std::setprecision(4) << r.p95Latency << ",\n";
        j << "      \"p99_latency_s\": " << std::fixed << std::setprecision(4) << r.p99Latency << ",\n";
        j << "      \"p999_latency_s\": " << std::fixed << std::setprecision(4) << r.p999Latency << ",\n";
        j << "      \"min_latency_s\": " << std::fixed << std::setprecision(4) << r.minLatency << ",\n";
        j << "      \"max_latency_s\": " << std::fixed << std::setprecision(4) << r.maxLatency << ",\n";
        j << "      \"error_count\": " << r.errorCount << "\n";
        j << "    }";
    }
    j << "\n  ]\n";
    j << "}";

    return j.str();
}

// ============================================================================
// Write Unified Report (all cases in one file)
// ============================================================================
inline void WriteUnifiedReport(const std::vector<std::string>& caseJsonStrings, const std::string& filename)
{
    using tool::Println;
    using tool::Errorln;

    std::ofstream f(filename);
    if (!f.is_open())
    {
        Errorln("Failed to open unified report file:", filename);
        return;
    }

    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf;
    localtime_r(&tt, &tmBuf);
    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02dT%02d:%02d:%02d",
             tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
             tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

    f << "{\n";
    f << "  \"report_time\": \"" << timeBuf << "\",\n";
    f << "  \"total_cases\": " << caseJsonStrings.size() << ",\n";
    f << "  \"cases\": [\n";
    for (size_t i = 0; i < caseJsonStrings.size(); i++)
    {
        if (i > 0) f << ",\n";
        f << IndentJson(caseJsonStrings[i], "    ");
    }
    f << "\n  ]\n";
    f << "}\n";
    f.close();

    Println("Unified report saved to:", filename);
}

} // namespace benchmark_util
