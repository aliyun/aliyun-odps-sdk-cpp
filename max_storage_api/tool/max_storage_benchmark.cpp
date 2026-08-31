#include <stdlib.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <vector>
#include <chrono>
#include <array>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <memory>
#include <sstream>
#include <sys/resource.h>
#include <unistd.h>

#include "tool.h"
#include "util/string_util.h"
#include "include/configuration.h"
#include "include/odps_api.h"
#include "max_storage_api.h"
#include "test/common/test_util.h"
#include "arrow/api.h"
#include "boost/program_options.hpp"
#include "benchmark_data_generator.h"
#include "benchmark_utils.h"
#include "benchmark_metrics.h"
#include "benchmark_reporter.h"
#include "benchmark_config_parser.h"

#include <set>

namespace po = boost::program_options;

using namespace tool;
using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;
using namespace benchmark_util;

static int64_t MAX_RUNS = 1000000;
static bool ENABLE_MAX_RUNS = false;
static std::atomic_int_fast64_t sRunsCounter(0);
static std::atomic<bool> sStopFlag{false};

static std::string gProjectName;
static std::string gTableName;
static std::string gSchema = "default";
static std::string gPartitionSpec;
static std::string gMode;
static int gThreadCount = 1;
static int64_t gRowsPerRun = 1000;
static std::shared_ptr<arrow::Schema> gTableSchema;
static MaxStorageApi* gMaxStorageApi = nullptr;
static ITableReadSessionPtr gReadSession = nullptr;
static ITableWriteSessionPtr gSharedWriteSession = nullptr;  // Global shared session for all threads
static int64_t gSharedSessionCreateTimeUs = 0;  // Time taken to create global shared session
static int64_t gSharedSessionCommitTimeUs = 0;  // Time taken to commit global shared session
static int64_t gTotalRecordCount = 0;
static Metrics* gMetrics = nullptr;
static std::string gLastWriteSessionId;
static std::string gLastReadSessionId;

static bool gVerbose = false;              // Enable verbose per-operation logging
static int64_t gLogInterval = 0;           // Log every N operations (0 = disable)
static int64_t gWarmupSeconds = 0;         // Warmup duration in seconds
static double gRateLimit = 0.0;            // Max ops per second per thread (0 = unlimited)
static int64_t gMonitorInterval = 0;       // Realtime monitor interval in seconds (0 = disable)
static std::unique_ptr<RateLimiter> gRateLimiter;
static std::atomic_int_fast64_t gOpCounter{0};  // Global operation counter for logging

// Performance optimization features
static DataPool gDataPool;                       // Pre-generated data pool
static bool gUseDataPool = true;                 // Always enabled for write mode
static int gDataPoolSize = 100;                  // Fixed pool size
static SystemResourceMetrics gSystemMetrics;     // System resource monitoring
static bool gEnableSystemMetrics = true;         // Always enabled
static bool gEnableCoordinatedOmission = true;   // Always enabled
static bool gSessionReuse = true;                 // Session reuse mode (default: enabled)
static int gMaxRetries = 3;                       // Max retries for transient errors

// ============================================================================
// Helper: Reset all global state for running a new benchmark case
// ============================================================================
static void ResetGlobalState()
{
    sRunsCounter = 0;
    sStopFlag = false;
    gOpCounter = 0;
    MAX_RUNS = 1000000;
    ENABLE_MAX_RUNS = false;
    gMetrics = nullptr;
    gMaxStorageApi = nullptr;
    gReadSession = nullptr;
    gSharedWriteSession = nullptr;
    gSharedSessionCreateTimeUs = 0;
    gSharedSessionCommitTimeUs = 0;
    gTableSchema = nullptr;
    gLastWriteSessionId = "";
    gLastReadSessionId = "";
    gTotalRecordCount = 0;
    gProjectName = "";
    gTableName = "";
    gSchema = "default";
    gPartitionSpec = "";
    gMode = "";
    gThreadCount = 1;
    gRowsPerRun = 1000;
    gVerbose = false;
    gLogInterval = 0;
    gWarmupSeconds = 0;
    gRateLimit = 0.0;
    gMonitorInterval = 0;
    gRateLimiter.reset();
    gDataPool.Clear();
    gSystemMetrics.Reset();
    gSessionReuse = true;
}

int64_t CalculateRecordBatchSize(const std::shared_ptr<arrow::RecordBatch>& batch)
{
    int64_t totalSize = 0;
    for (int i = 0; i < batch->num_columns(); i++)
    {
        auto array = batch->column(i);
        auto buffers = array->data()->buffers;
        for (const auto& buffer : buffers)
        {
            if (buffer)
            {
                totalSize += buffer->size();
            }
        }
    }
    return totalSize;
}

std::string ArrowTypeToOdpsTypeString(const std::shared_ptr<arrow::DataType>& type)
{
    switch (type->id())
    {
        case arrow::Type::INT8:
            return "tinyint";
        case arrow::Type::INT16:
            return "smallint";
        case arrow::Type::INT32:
            return "int";
        case arrow::Type::INT64:
            return "bigint";
        case arrow::Type::BOOL:
            return "boolean";
        case arrow::Type::FLOAT:
            return "float";
        case arrow::Type::DOUBLE:
            return "double";
        case arrow::Type::STRING:
            return "string";
        case arrow::Type::BINARY:
            return "binary";
        case arrow::Type::DATE32:
            return "date";
        case arrow::Type::TIMESTAMP:
        {
            auto timestamp_type = std::static_pointer_cast<arrow::TimestampType>(type);
            if (timestamp_type->unit() == arrow::TimeUnit::MILLI)
            {
                return "datetime";
            }
            else
            {
                return "timestamp";
            }
        }
        case arrow::Type::DECIMAL:
        {
            auto decimal_type = std::static_pointer_cast<arrow::DecimalType>(type);
            return "decimal(" + std::to_string(decimal_type->precision()) + "," + std::to_string(decimal_type->scale()) + ")";
        }
        case arrow::Type::LIST:
        {
            auto list_type = std::static_pointer_cast<arrow::ListType>(type);
            std::string element_type = ArrowTypeToOdpsTypeString(list_type->value_type());
            return "array<" + element_type + ">";
        }
        case arrow::Type::MAP:
        {
            auto map_type = std::static_pointer_cast<arrow::MapType>(type);
            std::string key_type = ArrowTypeToOdpsTypeString(map_type->key_type());
            std::string value_type = ArrowTypeToOdpsTypeString(map_type->item_type());
            return "map<" + key_type + "," + value_type + ">";
        }
        case arrow::Type::STRUCT:
        {
            auto struct_type = std::static_pointer_cast<arrow::StructType>(type);
            std::string result = "struct<";
            for (int i = 0; i < struct_type->num_fields(); i++)
            {
                if (i > 0) result += ",";
                result += struct_type->field(i)->name() + ":" + ArrowTypeToOdpsTypeString(struct_type->field(i)->type());
            }
            result += ">";
            return result;
        }
        default:
            return "string";
    }
}

std::shared_ptr<arrow::RecordBatch> GenerateTestData(const std::shared_ptr<arrow::Schema>& schema, int64_t count)
{
    int64_t base_val = 0;

    arrow::MemoryPool* pool = arrow::default_memory_pool();
    std::vector<std::shared_ptr<arrow::Array>> built_arrays;
    built_arrays.reserve(schema->num_fields());

    std::vector<std::string> odps_types;
    odps_types.reserve(schema->num_fields());
    for (int i = 0; i < schema->num_fields(); i++)
    {
        odps_types.push_back(ArrowTypeToOdpsTypeString(schema->field(i)->type()));
    }

    for (int i = 0; i < schema->num_fields(); i++)
    {
        auto field = schema->field(i);
        const std::string& odps_type = odps_types[i];

        std::shared_ptr<arrow::ArrayBuilder> builder = GenerateArrayBuilderBasedOnTypeInfo(odps_type, pool);

        GenerateDataIntoBuilderBasedOnTypeInfo(builder.get(), odps_type, count, i, base_val);

        std::shared_ptr<arrow::Array> array;
        arrow::Status status = builder->Finish(&array);
        if (!status.ok())
        {
            throw std::runtime_error("Failed to finish array builder: " + status.ToString());
        }

        built_arrays.push_back(array);
    }

    std::shared_ptr<arrow::RecordBatch> record_batch = arrow::RecordBatch::Make(schema, count, built_arrays);

    return record_batch;
}

void WriteThread(int threadId)
{
    // For coordinated omission correction
    int64_t expectedIntervalMs = (gRateLimit > 0) ? (int64_t)(1000.0 / gRateLimit) : 0;
    int64_t lastOpEndTime = CurrentTimeMillis();

    // Session reuse: use the global shared session (created by main thread)
    if (gSessionReuse && gSharedWriteSession == nullptr)
    {
        ParaErrorln("WriteThread", threadId, "Error: gSharedWriteSession is null in session reuse mode");
        if (gMetrics) gMetrics->RecordError();
        return;
    }

    for(int64_t i = sRunsCounter.fetch_add(1); ((i < MAX_RUNS) || !ENABLE_MAX_RUNS) && !sStopFlag.load(); i = sRunsCounter.fetch_add(1))
    {
        // Rate limiting
        if (gRateLimiter)
        {
            gRateLimiter->Acquire();
        }

        try
        {
            int64_t totalStartTm = CurrentTimeMillis();

            // Coordinated omission detection
            int64_t coordinatedOmission = 0;
            if (gEnableCoordinatedOmission && expectedIntervalMs > 0 && i > 0)
            {
                int64_t expectedStartTime = lastOpEndTime + expectedIntervalMs;
                if (totalStartTm > expectedStartTime)
                {
                    coordinatedOmission = totalStartTm - expectedStartTime;
                }
            }

            // Use microsecond timing for sub-operations
            int64_t genStartUs = SteadyTimeMicros();
            std::shared_ptr<arrow::RecordBatch> recordBatch;
            if (gUseDataPool && gDataPool.IsInitialized())
            {
                recordBatch = gDataPool.GetNext();
            }
            else
            {
                recordBatch = GenerateTestData(gTableSchema, gRowsPerRun);
            }
            int64_t genEndUs = SteadyTimeMicros();
            int64_t genTimeUs = genEndUs - genStartUs;

            // --- Session handling ---
            int64_t sessionCreateTimeUs = 0;
            ITableWriteSessionPtr session;
            std::string sessionId;

            if (gSessionReuse)
            {
                // Use the global shared session (created by main thread)
                session = gSharedWriteSession;
                sessionId = session->GetID();
                sessionCreateTimeUs = gSharedSessionCreateTimeUs;
            }
            else
            {
                // Original: create new session each iteration
                int64_t sessionStartUs = SteadyTimeMicros();
                auto builder = gMaxStorageApi->BuildTableWriteSession();
                (*builder).SetProject(gProjectName)
                          .SetSchema(gSchema)
                          .SetTable(gTableName);
                if (!gPartitionSpec.empty())
                {
                    (*builder).SetPartitionSpec(gPartitionSpec);
                }
                session = (*builder).Build();
                sessionId = session->GetID();
                gLastWriteSessionId = sessionId;
                int64_t sessionEndUs = SteadyTimeMicros();
                sessionCreateTimeUs = sessionEndUs - sessionStartUs;
            }

            // --- Stream creation (always per-iteration, unique ID) ---
            int64_t streamStartUs = SteadyTimeMicros();
            std::string streamId = gSessionReuse
                ? ("stream-t" + std::to_string(threadId) + "-" + std::to_string(i))
                : ("stream-" + std::to_string(i));
            auto stream = session->BuildWriteStream()
                            ->SetStreamId(streamId)
                            .SetStreamVersion(1)
                            .Build();
            int64_t streamEndUs = SteadyTimeMicros();
            int64_t streamCreateTimeUs = streamEndUs - streamStartUs;

            // --- Write + Close ---
            int64_t writeStartUs = SteadyTimeMicros();
            stream->Write(*recordBatch);
            stream->Close();
            int64_t writeEndUs = SteadyTimeMicros();
            int64_t writeTimeUs = writeEndUs - writeStartUs;

            // --- Commit (only in non-reuse mode) ---
            int64_t commitTimeUs = 0;
            if (!gSessionReuse)
            {
                int64_t commitStartUs = SteadyTimeMicros();
                session->Commit({});
                int64_t commitEndUs = SteadyTimeMicros();
                commitTimeUs = commitEndUs - commitStartUs;
            }
            else
            {
                // In reuse mode, use the global shared session commit time
                commitTimeUs = gSharedSessionCommitTimeUs;
            }

            int64_t totalEndTm = CurrentTimeMillis();
            int64_t totalTime = totalEndTm - totalStartTm;
            lastOpEndTime = totalEndTm;

            // Corrected latency includes coordinated omission
            int64_t correctedLatency = totalTime + coordinatedOmission;

            int64_t rows = recordBatch->num_rows();
            int64_t size = CalculateRecordBatchSize(recordBatch);
            int64_t wireSize = stream->GetWireBytes();

            if (gMetrics)
            {
                gMetrics->RecordWriteOp(threadId, totalTime, rows, size,
                                       sessionCreateTimeUs, streamCreateTimeUs,
                                       genTimeUs, writeTimeUs, commitTimeUs,
                                       wireSize);
                if (gEnableCoordinatedOmission && coordinatedOmission > 0)
                {
                    gMetrics->coordinatedOmissionMs.fetch_add(coordinatedOmission, std::memory_order_relaxed);
                    gMetrics->correctedLatencyMs.fetch_add(correctedLatency, std::memory_order_relaxed);
                }
            }

            // Conditional logging based on configuration
            int64_t opIdx = gOpCounter.fetch_add(1, std::memory_order_relaxed);
            bool shouldLog = gVerbose && (gLogInterval == 0 || (gLogInterval > 0 && opIdx % gLogInterval == 0));

            if (shouldLog)
            {
                // Convert microseconds to milliseconds for display
                double genTimeMs = genTimeUs / 1000.0;
                double sessionCreateTimeMs = sessionCreateTimeUs / 1000.0;
                double streamCreateTimeMs = streamCreateTimeUs / 1000.0;
                double writeTimeMs = writeTimeUs / 1000.0;
                double commitTimeMs = commitTimeUs / 1000.0;

                double writeSpeed = (writeTimeUs > 0 && size > 0) ? (double)size / ((double)writeTimeUs / 1000000.0) : 0;
                double totalSpeed = (totalTime > 0 && size > 0) ? (double)size / ((double)totalTime / (double)1000) : 0;

                ParaErrorln("WriteThread Run", i,
                        "Session ID:", sessionId,
                        "Stream ID:", streamId,
                        "Total Time:", totalTime, "ms",
                        "(Gen:", std::fixed, std::setprecision(3), genTimeMs, "ms",
                        "Session:", std::fixed, std::setprecision(3), sessionCreateTimeMs, "ms",
                        "Stream:", std::fixed, std::setprecision(3), streamCreateTimeMs, "ms",
                        "Write:", std::fixed, std::setprecision(3), writeTimeMs, "ms",
                        "Commit:", std::fixed, std::setprecision(3), commitTimeMs, "ms)",
                        "Write-only Speed:", DoubleSizeUnitFormatter(writeSpeed),
                        "Total Speed:", DoubleSizeUnitFormatter(totalSpeed),
                        "Rows:", rows,
                        "Sz:", DoubleSizeUnitFormatter(size, ""));
            }

            if (!ENABLE_MAX_RUNS)
            {
                break;
            }
        }
        catch (const std::exception& e)
        {
            if (gMetrics)
            {
                gMetrics->RecordError();
            }
            ParaErrorln("WriteThread Run", i, "Error:", e.what());
            if (!ENABLE_MAX_RUNS)
            {
                break;
            }
        }
    }
}

void ReadThread(int threadId)
{
    if (gTotalRecordCount == 0)
    {
        ParaErrorln("ReadThread: No record in table, perf test stopped");
        return;
    }

    auto splits = gReadSession->GetSplits();
    std::string sessionId = gReadSession->GetSessionId();

    // For coordinated omission correction
    int64_t expectedIntervalMs = (gRateLimit > 0) ? (int64_t)(1000.0 / gRateLimit) : 0;
    int64_t lastOpEndTime = CurrentTimeMillis();

    for(int64_t i = sRunsCounter.fetch_add(1); ((i < MAX_RUNS) || !ENABLE_MAX_RUNS) && !sStopFlag.load(); i = sRunsCounter.fetch_add(1))
    {
        // Rate limiting
        if (gRateLimiter)
        {
            gRateLimiter->Acquire();
        }

        try
        {
            int64_t totalStartTm = CurrentTimeMillis();

            // Coordinated omission detection
            int64_t coordinatedOmission = 0;
            if (gEnableCoordinatedOmission && expectedIntervalMs > 0 && i > 0)
            {
                int64_t expectedStartTime = lastOpEndTime + expectedIntervalMs;
                if (totalStartTm > expectedStartTime)
                {
                    coordinatedOmission = totalStartTm - expectedStartTime;
                }
            }

            // Use microsecond timing for sub-operations
            int64_t splitStartUs = SteadyTimeMicros();
            int64_t offset = (i * gRowsPerRun) % gTotalRecordCount;
            int64_t rowsToRead = gRowsPerRun;
            if (offset + rowsToRead > gTotalRecordCount)
            {
                rowsToRead = gTotalRecordCount - offset;
                if (rowsToRead <= 0)
                {
                    if (!ENABLE_MAX_RUNS)
                    {
                        break;
                    }
                    continue;
                }
            }
            auto split = splits->GetSplit(offset, rowsToRead);
            int64_t splitEndUs = SteadyTimeMicros();
            int64_t splitGetTimeUs = splitEndUs - splitStartUs;

            int64_t streamStartUs = SteadyTimeMicros();
            auto readStream = gReadSession->BuildTableReadStream()->SetSplit(split).Build();
            int64_t streamEndUs = SteadyTimeMicros();
            int64_t streamCreateTimeUs = streamEndUs - streamStartUs;

            int64_t readStartUs = SteadyTimeMicros();
            int64_t rowsRead = 0;
            int64_t sizeRead = 0;
            int retryCount = 0;
            bool readSuccess = false;

            while (!readSuccess && retryCount <= gMaxRetries) {
                try {
                    rowsRead = 0;
                    sizeRead = 0;
                    while (true) {
                        auto batch = readStream->Read();
                        if (batch == nullptr) {
                            break;
                        }
                        rowsRead += batch->num_rows();
                        sizeRead += CalculateRecordBatchSize(batch);
                    }
                    readSuccess = true;
                } catch (const std::exception& e) {
                    retryCount++;
                    if (retryCount <= gMaxRetries) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100 * retryCount));
                        // Recreate read stream for retry
                        readStream = gReadSession->BuildTableReadStream()->SetSplit(split).Build();
                    } else {
                        throw;
                    }
                }
            }
            int64_t readEndUs = SteadyTimeMicros();
            int64_t readTimeUs = readEndUs - readStartUs;

            int64_t closeStartUs = SteadyTimeMicros();
            readStream->Close();
            int64_t closeEndUs = SteadyTimeMicros();
            int64_t closeTimeUs = closeEndUs - closeStartUs;

            int64_t totalEndTm = CurrentTimeMillis();
            int64_t totalTime = totalEndTm - totalStartTm;
            lastOpEndTime = totalEndTm;

            // Corrected latency includes coordinated omission
            int64_t correctedLatency = totalTime + coordinatedOmission;

            int64_t wireSize = readStream->GetWireBytes();

            if (gMetrics)
            {
                gMetrics->RecordReadOp(threadId, totalTime, rowsRead, sizeRead,
                                      splitGetTimeUs, streamCreateTimeUs,
                                      readTimeUs, closeTimeUs,
                                      wireSize);
                if (gEnableCoordinatedOmission && coordinatedOmission > 0)
                {
                    gMetrics->coordinatedOmissionMs.fetch_add(coordinatedOmission, std::memory_order_relaxed);
                    gMetrics->correctedLatencyMs.fetch_add(correctedLatency, std::memory_order_relaxed);
                }
            }

            // Conditional logging based on configuration
            int64_t opIdx = gOpCounter.fetch_add(1, std::memory_order_relaxed);
            bool shouldLog = gVerbose && (gLogInterval == 0 || (gLogInterval > 0 && opIdx % gLogInterval == 0));

            if (shouldLog)
            {
                // Convert microseconds to milliseconds for display
                double splitGetTimeMs = splitGetTimeUs / 1000.0;
                double streamCreateTimeMs = streamCreateTimeUs / 1000.0;
                double readTimeMs = readTimeUs / 1000.0;
                double closeTimeMs = closeTimeUs / 1000.0;

                double readSpeed = (readTimeUs > 0 && sizeRead > 0) ? (double)sizeRead / ((double)readTimeUs / 1000000.0) : 0;
                double totalSpeed = (totalTime > 0 && sizeRead > 0) ? (double)sizeRead / ((double)totalTime / (double)1000) : 0;

                ParaErrorln("ReadThread Run", i,
                        "Session ID:", sessionId,
                        "Total Time:", totalTime, "ms",
                        "(Split:", std::fixed, std::setprecision(3), splitGetTimeMs, "ms",
                        "Stream:", std::fixed, std::setprecision(3), streamCreateTimeMs, "ms",
                        "Read:", std::fixed, std::setprecision(3), readTimeMs, "ms",
                        "Close:", std::fixed, std::setprecision(3), closeTimeMs, "ms)",
                        "Read-only Speed:", DoubleSizeUnitFormatter(readSpeed),
                        "Total Speed:", DoubleSizeUnitFormatter(totalSpeed),
                        "Rows:", rowsRead,
                        "Sz:", DoubleSizeUnitFormatter(sizeRead, ""));
            }

            if (!ENABLE_MAX_RUNS)
            {
                break;
            }
        }
        catch (const std::exception& e)
        {
            if (gMetrics)
            {
                gMetrics->RecordError();
            }
            ParaErrorln("ReadThread Run", i, "Error:", e.what());
            if (!ENABLE_MAX_RUNS)
            {
                break;
            }
        }
    }
}

// ============================================================================
// Realtime Monitor Thread
// ============================================================================
void RealtimeMonitorThread(int64_t intervalSec)
{
    int64_t lastOps = 0;
    int64_t lastRows = 0;
    int64_t lastSize = 0;
    int64_t lastTimestampMs = CurrentTimeMillis();

    while (!sStopFlag.load() && gMetrics != nullptr)
    {
        std::this_thread::sleep_for(std::chrono::seconds(intervalSec));

        if (sStopFlag.load() || gMetrics == nullptr) break;

        // Sample system resources if enabled
        if (gEnableSystemMetrics)
        {
            gSystemMetrics.Sample();
        }

        int64_t nowMs = CurrentTimeMillis();
        int64_t elapsedMs = nowMs - lastTimestampMs;
        if (elapsedMs <= 0) continue;

        int64_t currentOps = gMetrics->totalOps.load();
        int64_t currentRows = gMetrics->totalRows.load();
        int64_t currentSize = gMetrics->totalSize.load();

        int64_t deltaOps = currentOps - lastOps;
        int64_t deltaRows = currentRows - lastRows;
        int64_t deltaSize = currentSize - lastSize;

        double instantQPS = deltaOps * 1000.0 / elapsedMs;
        double instantRPS = deltaRows * 1000.0 / elapsedMs;
        double instantThroughput = deltaSize * 1000.0 / elapsedMs;

        int64_t testElapsedSec = (nowMs - gMetrics->startTimeMs) / 1000;

        Println("=== Realtime Monitor (", testElapsedSec, "s elapsed) ===");
        if (deltaOps == 0)
        {
            Println("  Instant QPS: 0.00 (no ops completed in this interval)");
            Println("  Instant RPS: 0.00");
            Println("  Instant Throughput: 0.00B/s");
        }
        else
        {
            Println("  Instant QPS:", std::fixed, std::setprecision(2), instantQPS);
            Println("  Instant RPS:", std::fixed, std::setprecision(2), instantRPS);
            Println("  Instant Throughput:", DoubleSizeUnitFormatter(instantThroughput, "/s"));
        }
        Println("  Total Ops:", currentOps, " | Errors:", gMetrics->errorCount.load());

        if (gEnableSystemMetrics)
        {
            Println("  Memory (RSS):", gSystemMetrics.memoryUsedKB.load(), "KB | CPU:",
                    std::fixed, std::setprecision(1), gSystemMetrics.cpuUsagePercent.load(), "%",
                    "(peak:", std::fixed, std::setprecision(1), gSystemMetrics.GetPeakCpuUsage(), "%)");
        }

        if (gEnableCoordinatedOmission && gMetrics->coordinatedOmissionMs.load() > 0)
        {
            Println("  Coordinated Omission:", gMetrics->coordinatedOmissionMs.load(), "ms total");
        }

        Println("==========================================");

        lastOps = currentOps;
        lastRows = currentRows;
        lastSize = currentSize;
        lastTimestampMs = nowMs;
    }
}

// ============================================================================
// Warmup Phase
// ============================================================================
void RunWarmup(int threadCount, int64_t warmupSeconds)
{
    if (warmupSeconds <= 0) return;

    Println("==========================================");
    Println("Warmup Phase Starting...");
    Println("Duration:", warmupSeconds, "seconds");
    Println("==========================================");

    Metrics warmupMetrics;
    warmupMetrics.Initialize(threadCount);
    gMetrics = &warmupMetrics;

    sRunsCounter = 0;
    gOpCounter = 0;
    sStopFlag = false;
    ENABLE_MAX_RUNS = true;  // Run continuously during warmup

    warmupMetrics.startTimeMs = CurrentTimeMillis();

    // Create global shared write session if session reuse is enabled
    if (gMode == "write" && gSessionReuse)
    {
        try
        {
            int64_t sessionStartUs = SteadyTimeMicros();
            auto builder = gMaxStorageApi->BuildTableWriteSession();
            (*builder).SetProject(gProjectName)
                      .SetSchema(gSchema)
                      .SetTable(gTableName);
            if (!gPartitionSpec.empty())
            {
                (*builder).SetPartitionSpec(gPartitionSpec);
            }
            gSharedWriteSession = (*builder).Build();
            gLastWriteSessionId = gSharedWriteSession->GetID();
            int64_t sessionEndUs = SteadyTimeMicros();
            gSharedSessionCreateTimeUs = sessionEndUs - sessionStartUs;
        }
        catch (const std::exception& e)
        {
            Errorln("Failed to create global shared session for warmup:", e.what());
            sStopFlag = true;
            return;
        }
    }

    std::vector<std::shared_ptr<std::thread>> threads;
    for(int i = 0; i < threadCount; i++)
    {
        int threadId = warmupMetrics.RegisterThread();
        if (gMode == "write")
        {
            threads.push_back(std::make_shared<std::thread>(WriteThread, threadId));
        }
        else
        {
            threads.push_back(std::make_shared<std::thread>(ReadThread, threadId));
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(warmupSeconds));
    sStopFlag = true;

    for(auto& t : threads)
    {
        t->join();
    }

    // Commit the global shared session after warmup
    if (gMode == "write" && gSessionReuse && gSharedWriteSession)
    {
        try
        {
            int64_t commitStartUs = SteadyTimeMicros();
            gSharedWriteSession->Commit({});
            int64_t commitEndUs = SteadyTimeMicros();
            gSharedSessionCommitTimeUs = commitEndUs - commitStartUs;
        }
        catch (const std::exception& e)
        {
            Errorln("Failed to commit global shared session after warmup:", e.what());
        }
        gSharedWriteSession = nullptr;
    }

    warmupMetrics.wallClockTimeMs = CurrentTimeMillis() - warmupMetrics.startTimeMs;

    Println("Warmup Phase Completed:");
    Println("  Warmup Ops:", warmupMetrics.totalOps.load());
    Println("  Warmup QPS:", std::fixed, std::setprecision(2), warmupMetrics.GetQPS());
    Println("  Warmup Throughput:", DoubleSizeUnitFormatter(warmupMetrics.GetThroughput(), "/s"));
    Println("==========================================\n");

    // Reset for actual test
    sRunsCounter = 0;
    gOpCounter = 0;
    sStopFlag = false;
    gMetrics = nullptr;
    ENABLE_MAX_RUNS = false;  // Reset flag for actual test
}

// ============================================================================
// Linear Scaling Test
// ============================================================================
std::vector<TestResult> RunLinearScalingTest(int minThreads, int maxThreads, int step, int64_t durationSeconds)
{
    std::vector<TestResult> results;

    Println("==========================================");
    Println("Linear Scaling Test");
    Println("Testing from", minThreads, "to", maxThreads, "threads, step:", step);
    Println("Duration per test:", durationSeconds, "seconds");
    Println("==========================================");

    for (int threads = minThreads; threads <= maxThreads; threads += step)
    {
        ENABLE_MAX_RUNS = true;
        sStopFlag = false;
        Metrics metrics;
        metrics.Initialize(threads);
        TestResult result;
        result.threadCount = threads;
        gThreadCount = threads;
        gMetrics = &metrics;

        sRunsCounter = 0;
        gOpCounter = 0;
        metrics.Reset();

        Println("Starting benchmark with", threads, "threads, duration:", durationSeconds, "seconds");

        // Create global shared write session if session reuse is enabled
        if (gMode == "write" && gSessionReuse)
        {
            try
            {
                int64_t sessionStartUs = SteadyTimeMicros();
                auto builder = gMaxStorageApi->BuildTableWriteSession();
                (*builder).SetProject(gProjectName)
                          .SetSchema(gSchema)
                          .SetTable(gTableName);
                if (!gPartitionSpec.empty())
                {
                    (*builder).SetPartitionSpec(gPartitionSpec);
                }
                gSharedWriteSession = (*builder).Build();
                gLastWriteSessionId = gSharedWriteSession->GetID();
                int64_t sessionEndUs = SteadyTimeMicros();
                gSharedSessionCreateTimeUs = sessionEndUs - sessionStartUs;
                Println("Global shared session created:", gLastWriteSessionId);
            }
            catch (const std::exception& e)
            {
                Errorln("Failed to create global shared session:", e.what());
                sStopFlag = true;
                return results;
            }
        }

        std::vector<std::shared_ptr<std::thread>> threadList;
        metrics.startTimeMs = CurrentTimeMillis();

        for(int i = 0; i < threads; i++)
        {
            int threadId = metrics.RegisterThread();
            if (gMode == "write")
            {
                threadList.push_back(std::make_shared<std::thread>(WriteThread, threadId));
            }
            else
            {
                threadList.push_back(std::make_shared<std::thread>(ReadThread, threadId));
            }
        }

        if (durationSeconds > 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
            sStopFlag = true;
        }

        for(auto& t : threadList)
        {
            t->join();
        }

        // Commit the global shared session after all threads finish
        if (gMode == "write" && gSessionReuse && gSharedWriteSession)
        {
            Println("Committing global shared session:", gSharedWriteSession->GetID());
            try
            {
                int64_t commitStartUs = SteadyTimeMicros();
                gSharedWriteSession->Commit({});
                int64_t commitEndUs = SteadyTimeMicros();
                gSharedSessionCommitTimeUs = commitEndUs - commitStartUs;
                Println("Global shared session committed successfully");
            }
            catch (const std::exception& e)
            {
                Errorln("Failed to commit global shared session:", e.what());
            }
            gSharedWriteSession = nullptr;
        }

        metrics.wallClockTimeMs = CurrentTimeMillis() - metrics.startTimeMs;
        gMetrics = nullptr;

        result = TestResult::FromMetrics(threads, metrics);
        results.push_back(result);

        Println("------------------------------------------");
        Println("Thread Count:", threads);
        Println("  Total Ops:", result.totalOps);
        Println("  Total Rows:", result.totalRows);
        Println("  Total Size:", DoubleSizeUnitFormatter(result.totalSize, ""));
        Println("  Wall Clock Time:", result.wallClockTimeMs, "ms");
        Println("  QPS:", std::fixed, std::setprecision(2), result.GetQPS());
        Println("  RPS:", std::fixed, std::setprecision(2), result.GetRPS());
        Println("  Throughput:", std::fixed, std::setprecision(2), DoubleSizeUnitFormatter(result.GetThroughput(), "/s"));
        Println("  Avg Latency:", std::fixed, std::setprecision(4), result.GetAvgLatency(), "s");
        Println("  P50 Latency:", std::fixed, std::setprecision(4), result.p50Latency, "s");
        Println("  P90 Latency:", std::fixed, std::setprecision(4), result.p90Latency, "s");
        Println("  P95 Latency:", std::fixed, std::setprecision(4), result.p95Latency, "s");
        Println("  P99 Latency:", std::fixed, std::setprecision(4), result.p99Latency, "s");
        Println("  P999 Latency:", std::fixed, std::setprecision(4), result.p999Latency, "s");
        Println("  Min Latency:", std::fixed, std::setprecision(4), result.minLatency, "s");
        Println("  Max Latency:", std::fixed, std::setprecision(4), result.maxLatency, "s");
        Println("  Errors:", result.errorCount);
        Println("------------------------------------------");

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    Println("==========================================");
    Println("Linear Scaling Analysis:");
    Println("Threads\tQPS\t\tRPS\t\tThroughput\tAvgLatency\tP99Latency");

    double baselineThroughput = 0;
    double baselineRPS = 0;
    int optimalThreads = minThreads;

    for (size_t i = 0; i < results.size(); i++)
    {
        const auto& r = results[i];
        Println(r.threadCount, "\t",
                std::fixed, std::setprecision(2), r.GetQPS(), "\t",
                std::fixed, std::setprecision(2), r.GetRPS(), "\t",
                std::fixed, std::setprecision(2), DoubleSizeUnitFormatter(r.GetThroughput(), "/s"), "\t",
                std::fixed, std::setprecision(4), r.GetAvgLatency(), "s\t",
                std::fixed, std::setprecision(4), r.p99Latency, "s");

        if (i == 0)
        {
            baselineThroughput = r.GetThroughput();
            baselineRPS = r.GetRPS();
        }
        else
        {
            double expectedThroughput = baselineThroughput * (double)r.threadCount / (double)results[0].threadCount;
            double expectedRPS = baselineRPS * (double)r.threadCount / (double)results[0].threadCount;

            double throughputRatio = r.GetThroughput() / expectedThroughput;
            double rpsRatio = r.GetRPS() / expectedRPS;

            if (throughputRatio < 0.9 || rpsRatio < 0.9)
            {
                Println("  Warning: Performance degradation detected at", r.threadCount, "threads");
                Println("    Expected Throughput:", DoubleSizeUnitFormatter(expectedThroughput, "/s"),
                        ", Actual:", DoubleSizeUnitFormatter(r.GetThroughput(), "/s"),
                        ", Ratio:", std::fixed, std::setprecision(2), throughputRatio);
                Println("    Expected RPS:", std::fixed, std::setprecision(2), expectedRPS,
                        ", Actual:", std::fixed, std::setprecision(2), r.GetRPS(),
                        ", Ratio:", std::fixed, std::setprecision(2), rpsRatio);

                if (optimalThreads == minThreads)
                {
                    optimalThreads = results[i-1].threadCount;
                }
            }
        }
    }

    Println("==========================================");
    Println("Optimal Thread Count (before degradation):", optimalThreads);
    Println("==========================================");

    return results;
}

// Helper function to build ReportConfig from global state
ReportConfig BuildReportConfig()
{
    ReportConfig config;
    config.projectName = gProjectName;
    config.tableName = gTableName;
    config.schema = gSchema;
    config.partitionSpec = gPartitionSpec;
    config.mode = gMode;
    config.threadCount = gThreadCount;
    config.rowsPerRun = gRowsPerRun;
    config.rateLimit = gRateLimit;
    config.warmupSeconds = gWarmupSeconds;
    config.useDataPool = gUseDataPool;
    config.enableCoordinatedOmission = gEnableCoordinatedOmission;
    config.enableSystemMetrics = gEnableSystemMetrics;
    config.lastWriteSessionId = gLastWriteSessionId;
    config.lastReadSessionId = gLastReadSessionId;
    config.tableSchema = gTableSchema;
    config.systemMetrics = &gSystemMetrics;
    return config;
}

// ============================================================================
// Run a single benchmark case with params from config or CLI
// ============================================================================
static int RunSingleBenchmark(const std::map<std::string, std::string>& params,
                              const std::string& suiteName,
                              std::string& outCaseJson,
                              std::string& outErrorMsg)
{
    ResetGlobalState();

    // Extract connection params
    std::string odpsEndpoint = GetParam(params, "odps-endpoint");
    std::string ep = GetParam(params, "tunnel-endpoint");
    std::string ak = GetParam(params, "access-key");
    std::string accessSecret = GetParam(params, "access-secret");
    std::string project = GetParam(params, "project");
    std::string table = GetParam(params, "table");
    std::string schema = GetParam(params, "schema", "default");
    std::string partitionSpec = GetParam(params, "partition");
    std::string mode = GetParam(params, "mode", "write");

    int THREAD_COUNT = GetParamInt(params, "thread", 1);
    int64_t EXHAUST_ROWS = GetParamInt64(params, "rows", 1000);
    int64_t maxRuns = GetParamInt64(params, "max-runs", 0);
    bool LINEAR_SCALING_TEST = GetParamBool(params, "linear-scaling-test", false);
    int MIN_THREADS = GetParamInt(params, "min-threads", 1);
    int MAX_THREADS = GetParamInt(params, "max-threads", 10);
    int THREAD_STEP = GetParamInt(params, "thread-step", 1);
    int64_t DURATION = GetParamInt64(params, "duration", 0);

    // Data gen params
    SetRandomStringLength(GetParamInt64(params, "random-string-length", 10));
    SetUseFixedString(GetParamBool(params, "use-fixed-string", false));
    std::string fixedStr = GetParam(params, "fixed-string", "not_set_yet");
    SetFixedString(fixedStr);

    // Performance params
    gVerbose = GetParamBool(params, "verbose", false);
    if (gVerbose) { gLogInterval = 100; }
    gWarmupSeconds = GetParamInt64(params, "warmup-seconds", 0);
    gRateLimit = GetParamDouble(params, "rate-limit", 0.0);
    gMaxRetries = GetParamInt(params, "max-retries", 3);
    gMonitorInterval = GetParamInt64(params, "monitor-interval", 5);
    gSessionReuse = GetParamBool(params, "session-reuse", true);
    bool truncateBeforeWrite = GetParamBool(params, "truncate-before-write", false);

    // Parameter priority: max-runs > duration > default (thread count)
    if (maxRuns > 0) {
        // Count mode: run max-runs total operations across all threads
        MAX_RUNS = maxRuns;
        ENABLE_MAX_RUNS = true;
    } else if (DURATION > 0) {
        // Duration mode: run for DURATION seconds
        MAX_RUNS = 1000000;
        ENABLE_MAX_RUNS = false;
    } else {
        // Default mode: each thread runs once
        MAX_RUNS = THREAD_COUNT;
        ENABLE_MAX_RUNS = true;
    }

    if (gRateLimit > 0)
    {
        gRateLimiter = std::make_unique<RateLimiter>(gRateLimit);
    }

    if (odpsEndpoint.empty() || ak.empty() || accessSecret.empty() || project.empty() || table.empty())
    {
        outErrorMsg = "Missing required params: odps-endpoint, access-key, access-secret, project, table";
        Errorln(outErrorMsg);
        return -1;
    }

    if (mode != "write" && mode != "read")
    {
        outErrorMsg = "mode must be 'write' or 'read'";
        Errorln(outErrorMsg);
        return -1;
    }

    gProjectName = project;
    gTableName = table;
    gSchema = schema;
    gPartitionSpec = partitionSpec;
    gMode = mode;
    gThreadCount = THREAD_COUNT;
    gRowsPerRun = EXHAUST_ROWS;

    Account account(ACCOUNT_ALIYUN, ak, accessSecret);
    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(ep);
    conf.SetEndpoint(odpsEndpoint);
    UserAgent userAgent("MAX_STORAGE_BENCHMARK", "1.0");
    conf.SetUserAgent(userAgent);

    MaxStorageApi maxStorageApi;
    try
    {
        maxStorageApi.Init(conf);
        gMaxStorageApi = &maxStorageApi;

        if (mode == "write")
        {
            // Truncate table before write if configured
            if (truncateBeforeWrite)
            {
                Println("Truncating table before write...");
                try
                {
                    IODPSPtr odps = IODPS::Create(conf, project);
                    ISQLTaskPtr sqlTask = ISQLTask::Create();
                    std::string truncateSql = "TRUNCATE TABLE " + table + ";";
                    Println("Executing SQL:", truncateSql);
                    IODPSInstancePtr instance = sqlTask->Run(odps, truncateSql);
                    instance->WaitForSuccess(60000);
                    Println("Table truncated successfully");
                }
                catch (const OdpsException& e)
                {
                    Errorln("Failed to truncate table:", e.GetErrorCode(), "-", e.GetErrorMsg());
                    // Continue anyway - table might be empty or not exist
                }
                catch (const std::exception& e)
                {
                    Errorln("Failed to truncate table:", e.what());
                    // Continue anyway
                }
            }

            Println("Try to get table schema...");
            auto builder = maxStorageApi.BuildTableWriteSession();
            (*builder).SetProject(project)
                      .SetSchema(schema)
                      .SetTable(table);

            if (!partitionSpec.empty())
            {
                (*builder).SetPartitionSpec(partitionSpec);
            }

            auto tempSession = (*builder).Build();
            std::string tempStreamId = "temp-stream-schema";
            auto tempStream = tempSession->BuildWriteStream()
                            ->SetStreamId(tempStreamId)
                            .SetStreamVersion(1)
                            .Build();
            gTableSchema = tempStream->GetArrowSchema();
            std::string tempSessionId = tempSession->GetID();
            tempStream->Close();
            tempSession->Abort();
            Println("Schema get done");
            Println("==========================================");
            Println("Table Schema Information:");
            Println(FormatSchemaInfo(gTableSchema));
            Println("==========================================");

            // Initialize data pool if enabled
            if (gUseDataPool && gTableSchema)
            {
                Println("Initializing data pool with", gDataPoolSize, "batches...");
                gDataPool.Initialize(
                    [](int64_t rows) { return GenerateTestData(gTableSchema, rows); },
                    EXHAUST_ROWS,
                    gDataPoolSize
                );
                Println("Data pool initialized");
            }
        }
        else
        {
            Println("Try to create read session...");
            SplitOptions splitOptions;
            splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

            auto builder = maxStorageApi.BuildTableReadSession();
            (*builder).SetProject(project)
                      .SetSchema(schema)
                      .SetTable(table)
                      .SetSplitOptions(splitOptions);

            if (!partitionSpec.empty())
            {
                FilterOptions filterOptions;
                filterOptions.mRequiredPartitions.push_back(partitionSpec);
                (*builder).SetFilterOptions(filterOptions);
            }

            gReadSession = (*builder).Build();
            std::string readSessionId = gReadSession->GetSessionId();
            gLastReadSessionId = readSessionId;
            auto splits = gReadSession->GetSplits();
            gTotalRecordCount = splits->GetRecordCount();

            if (gTotalRecordCount > 0)
            {
                auto testSplit = splits->GetSplit(0, 1);
                auto testStream = gReadSession->BuildTableReadStream()->SetSplit(testSplit).Build();
                auto testBatch = testStream->Read();
                if (testBatch != nullptr)
                {
                    gTableSchema = testBatch->schema();
                }
                testStream->Close();
            }

            Println("Read session create done");
            Println("Session ID:", readSessionId);
            Println("Total record count:", gTotalRecordCount);

            if (gTotalRecordCount == 0)
            {
                outErrorMsg = "No record in test table";
                Errorln(outErrorMsg);
                gMaxStorageApi = nullptr;
                return -1;
            }
        }

        if (LINEAR_SCALING_TEST)
        {
            // Run warmup before linear scaling test
            if (gWarmupSeconds > 0)
            {
                RunWarmup(MIN_THREADS, gWarmupSeconds);
            }

            std::vector<TestResult> results = RunLinearScalingTest(MIN_THREADS, MAX_THREADS, THREAD_STEP, DURATION);

            // Generate scaling case JSON for unified report
            ReportConfig scalingReportConfig = BuildReportConfig();
            outCaseJson = BuildScalingCaseJson(results, scalingReportConfig, suiteName);
        }
        else
        {
            // Run warmup phase
            if (gWarmupSeconds > 0)
            {
                RunWarmup(THREAD_COUNT, gWarmupSeconds);
            }

            Metrics metrics;
            metrics.Initialize(THREAD_COUNT);
            gMetrics = &metrics;
            sRunsCounter = 0;
            gOpCounter = 0;
            sStopFlag = false;
            metrics.startTimeMs = CurrentTimeMillis();

            // Initialize system metrics baseline for CPU calculation
            if (gEnableSystemMetrics)
            {
                gSystemMetrics.Sample();
            }

            // Start realtime monitor thread if configured
            std::shared_ptr<std::thread> monitorThread;
            if (gMonitorInterval > 0)
            {
                monitorThread = std::make_shared<std::thread>(RealtimeMonitorThread, gMonitorInterval);
            }

            // Create global shared write session if session reuse is enabled
            if (mode == "write" && gSessionReuse)
            {
                try
                {
                    int64_t sessionStartUs = SteadyTimeMicros();
                    auto builder = gMaxStorageApi->BuildTableWriteSession();
                    (*builder).SetProject(project)
                              .SetSchema(schema)
                              .SetTable(table);
                    if (!partitionSpec.empty())
                    {
                        (*builder).SetPartitionSpec(partitionSpec);
                    }
                    gSharedWriteSession = (*builder).Build();
                    gLastWriteSessionId = gSharedWriteSession->GetID();
                    int64_t sessionEndUs = SteadyTimeMicros();
                    gSharedSessionCreateTimeUs = sessionEndUs - sessionStartUs;
                    Println("Global shared session created:", gLastWriteSessionId);
                }
                catch (const std::exception& e)
                {
                    outErrorMsg = std::string("Failed to create global shared session: ") + e.what();
                    Errorln(outErrorMsg);
                    // Clean up monitor thread to prevent std::terminate on stack unwinding
                    sStopFlag = true;
                    if (monitorThread)
                    {
                        monitorThread->join();
                    }
                    return -1;
                }
            }
            std::vector<std::shared_ptr<std::thread>> threads;

            for(int i = 0; i < THREAD_COUNT; i++)
            {
                int threadId = metrics.RegisterThread();
                if (mode == "write")
                {
                    threads.push_back(std::make_shared<std::thread>(WriteThread, threadId));
                }
                else
                {
                    threads.push_back(std::make_shared<std::thread>(ReadThread, threadId));
                }
            }

            // If duration-based test (ENABLE_MAX_RUNS is false but DURATION > 0)
            if (!ENABLE_MAX_RUNS && DURATION > 0)
            {
                std::this_thread::sleep_for(std::chrono::seconds(DURATION));
                sStopFlag = true;
            }

            for(auto& t : threads)
            {
                t->join();
            }

            // Commit the global shared session after all threads finish
            if (mode == "write" && gSessionReuse && gSharedWriteSession)
            {
                Println("Committing global shared session:", gSharedWriteSession->GetID());
                try
                {
                    int64_t commitStartUs = SteadyTimeMicros();
                    gSharedWriteSession->Commit({});
                    int64_t commitEndUs = SteadyTimeMicros();
                    gSharedSessionCommitTimeUs = commitEndUs - commitStartUs;
                    Println("Global shared session committed successfully");
                }
                catch (const std::exception& e)
                {
                    Errorln("Failed to commit global shared session:", e.what());
                }
                gSharedWriteSession = nullptr;
            }

            // Stop monitor thread
            sStopFlag = true;
            if (monitorThread)
            {
                monitorThread->join();
            }

            metrics.wallClockTimeMs = CurrentTimeMillis() - metrics.startTimeMs;
            gMetrics = nullptr;

            // Final system resource sample before reporting
            if (gEnableSystemMetrics)
            {
                gSystemMetrics.Sample();
            }

            Println("Benchmark Finish");
            ReportConfig reportConfig = BuildReportConfig();
            PrintMetrics(metrics, reportConfig);

            // Generate benchmark case JSON for unified report
            outCaseJson = BuildBenchmarkCaseJson(metrics, reportConfig, suiteName);
        }
    }
    catch(OdpsException& e)
    {
        outErrorMsg = std::string("OdpsException: ") + e.what();
        std::cerr << outErrorMsg << std::endl;
        gMaxStorageApi = nullptr;
        return -1;
    }
    catch(std::exception& e)
    {
        outErrorMsg = std::string("Exception: ") + e.what();
        std::cerr << outErrorMsg << std::endl;
        gMaxStorageApi = nullptr;
        return -1;
    }

    gMaxStorageApi = nullptr;
    return 0;
}

struct MaxStorageBenchmarkTool: public Tool
{
    MaxStorageBenchmarkTool():
        mOptions("max storage api benchmark tool options")
    {
        mOptions.add_options()
            ("help", "print help message")
            ("odps-endpoint,H", po::value<std::string>(), "ODPS endpoint")
            ("tunnel-endpoint,E", po::value<std::string>()->default_value(""), "tunnel endpoint")
            ("access-key,K", po::value<std::string>(), "access key")
            ("access-secret,S", po::value<std::string>(), "access secret")
            ("project,P", po::value<std::string>(), "target project")
            ("table,T", po::value<std::string>(), "target table")
            ("schema", po::value<std::string>()->default_value("default"), "schema name")
            ("partition", po::value<std::string>()->default_value(""), "partition spec, e.g., p1=aaa/p2=bbb")
            ("mode,M", po::value<std::string>()->default_value("write"), "benchmark mode: write or read")
            ("thread,J", po::value<int>()->default_value(1), "concurrent threads")
            ("max-runs", po::value<int64_t>()->default_value(0), "run at most X pass, default disabled")
            ("rows,R", po::value<int64_t>()->default_value(1000), "rows per run")
            ("linear-scaling-test", po::value<bool>()->default_value(false), "enable linear scaling test")
            ("min-threads", po::value<int>()->default_value(1), "min threads for linear scaling test")
            ("max-threads", po::value<int>()->default_value(10), "max threads for linear scaling test")
            ("thread-step", po::value<int>()->default_value(1), "thread step for linear scaling test")
            ("duration,D", po::value<int64_t>()->default_value(30), "test duration in seconds for each thread count")
            ("random-string-length", po::value<int64_t>()->default_value(10), "length of random string for string columns")
            ("use-fixed-string", po::value<bool>()->default_value(false), "use fixed string instead of random string")
            ("fixed-string", po::value<std::string>()->default_value("not_set_yet"), "fixed string value when use-fixed-string is true")
            ("verbose,V", po::value<bool>()->default_value(false), "enable verbose per-operation logging (default: off)")
            ("warmup-seconds,W", po::value<int64_t>()->default_value(0), "warmup duration in seconds before actual test (0=disable)")
            ("rate-limit", po::value<double>()->default_value(0.0), "max operations per second per thread (0=unlimited)")
            ("monitor-interval", po::value<int64_t>()->default_value(5), "realtime monitor output interval in seconds (0=disable)")
            ("config,C", po::value<std::string>(), "config file path for benchmark suites (INI format)")
            ("suite-name", po::value<std::string>()->default_value(""), "comma-separated suite names to run (default: all)")
            ;
    }
    virtual ~MaxStorageBenchmarkTool() {}
    virtual std::string GetName() const override { return "MaxStorageBenchmarkTool"; }
    virtual int Run(int argc, char* argv[]) override;
    virtual std::string GetHelpMessage() const override { return MakeString(mOptions); }
    po::options_description mOptions;
};

TOOL_REGISTER(MaxStorageBenchmarkTool);

int MaxStorageBenchmarkTool::Run(int argc, char *argv[])
{
    po::variables_map argmap;
    po::store(po::parse_command_line(argc, argv, mOptions), argmap);
    po::notify(argmap);

    if (argmap.count("help"))
    {
        Println(mOptions);
        return 0;
    }

    // ========================================================================
    // Config file mode: run multiple suites/cases from INI config
    // ========================================================================
    if (argmap.count("config"))
    {
        std::string configPath = argmap["config"].as<std::string>();
        std::string suiteFilter = argmap["suite-name"].as<std::string>();

        // Parse suite name filter
        std::set<std::string> selectedSuites;
        if (!suiteFilter.empty())
        {
            auto parts = util::SplitString(suiteFilter, ',');
            for (const auto& p : parts)
            {
                selectedSuites.insert(p);
            }
        }

        Println("Loading config file:", configPath);
        BenchmarkFileConfig fileConfig = ParseBenchmarkConfig(configPath);

        Println("Found", fileConfig.suites.size(), "suite(s) in config file");
        if (!selectedSuites.empty())
        {
            Println("Filter: only running suite(s):", suiteFilter);
        }

        int totalSuites = 0;
        int successSuites = 0;
        int failedSuites = 0;
        std::vector<std::string> caseJsons;

        for (const auto& suite : fileConfig.suites)
        {
            if (!selectedSuites.empty() && selectedSuites.find(suite.name) == selectedSuites.end())
            {
                Println("Skipping suite:", suite.name);
                continue;
            }

            totalSuites++;

            Println("");
            Println("==========================================");
            Println("Running Suite:", suite.name);
            Println("==========================================");

            std::string caseJson;
            std::string errorMsg;
            int ret = RunSingleBenchmark(suite.params, suite.name, caseJson, errorMsg);
            if (ret == 0)
            {
                successSuites++;
                if (!caseJson.empty())
                {
                    caseJsons.push_back(caseJson);
                }
                Println("Suite", suite.name, "PASSED");
            }
            else
            {
                failedSuites++;
                Errorln("Suite", suite.name, "FAILED:", errorMsg);
            }
        }

        // Write unified report
        WriteUnifiedReport(caseJsons, "report.json");

        Println("");
        Println("==========================================");
        Println("Benchmark Complete");
        Println("  Total Suites:", totalSuites);
        Println("  Passed:", successSuites);
        Println("  Failed:", failedSuites);
        Println("  Report: report.json");
        Println("==========================================");

        return failedSuites > 0 ? -1 : 0;
    }

    // ========================================================================
    // CLI mode: build params map from command line args, run single benchmark
    // ========================================================================
    std::map<std::string, std::string> params;

    // Try to get default config from odpsctl if connection params not provided
    string odpsEndpoint = "";
    string ep = "";
    string ak = "";
    string accessSecret = "";
    string project = "";

    if (!argmap.count("odps-endpoint") || !argmap.count("access-key") || !argmap.count("access-secret"))
    {
        if (Utils::Initialize())
        {
            Configuration configObj = Utils::GetConfiguration();
            odpsEndpoint = configObj.GetEndpoint();
            ep = Utils::GetTunnelEndpoint();
            ak = configObj.GetAccount().GetId();
            accessSecret = configObj.GetAccount().GetKey();
            if (!argmap.count("project"))
            {
                project = Utils::GetProjectName();
            }
        }
    }

    if (argmap.count("odps-endpoint")) { odpsEndpoint = argmap["odps-endpoint"].as<std::string>(); }
    if (argmap.count("tunnel-endpoint")) { ep = argmap["tunnel-endpoint"].as<std::string>(); }
    if (argmap.count("access-key")) { ak = argmap["access-key"].as<std::string>(); }
    if (argmap.count("access-secret")) { accessSecret = argmap["access-secret"].as<std::string>(); }
    if (argmap.count("project")) { project = argmap["project"].as<std::string>(); }

    params["odps-endpoint"] = odpsEndpoint;
    params["tunnel-endpoint"] = ep;
    params["access-key"] = ak;
    params["access-secret"] = accessSecret;
    params["project"] = project;

    // Required in CLI mode
    if (!argmap.count("table"))
    {
        throw std::runtime_error(std::string("Required argument not found: ") + "table");
    }
    params["table"] = argmap["table"].as<std::string>();

    params["schema"] = argmap["schema"].as<std::string>();
    params["partition"] = argmap["partition"].as<std::string>();
    params["mode"] = argmap["mode"].as<std::string>();
    params["thread"] = std::to_string(argmap["thread"].as<int>());
    params["max-runs"] = std::to_string(argmap["max-runs"].as<int64_t>());
    params["rows"] = std::to_string(argmap["rows"].as<int64_t>());
    params["linear-scaling-test"] = argmap["linear-scaling-test"].as<bool>() ? "true" : "false";
    params["min-threads"] = std::to_string(argmap["min-threads"].as<int>());
    params["max-threads"] = std::to_string(argmap["max-threads"].as<int>());
    params["thread-step"] = std::to_string(argmap["thread-step"].as<int>());
    params["duration"] = std::to_string(argmap["duration"].as<int64_t>());
    params["random-string-length"] = std::to_string(argmap["random-string-length"].as<int64_t>());
    params["use-fixed-string"] = argmap["use-fixed-string"].as<bool>() ? "true" : "false";
    params["fixed-string"] = argmap["fixed-string"].as<std::string>();
    params["verbose"] = argmap["verbose"].as<bool>() ? "true" : "false";
    params["warmup-seconds"] = std::to_string(argmap["warmup-seconds"].as<int64_t>());
    params["rate-limit"] = std::to_string(argmap["rate-limit"].as<double>());
    params["monitor-interval"] = std::to_string(argmap["monitor-interval"].as<int64_t>());

    std::string caseJson;
    std::string errorMsg;
    std::string suiteName = params["mode"] + "_" + params["table"] + "_" + params["thread"] + "t";
    int ret = RunSingleBenchmark(params, suiteName, caseJson, errorMsg);

    if (ret != 0)
    {
        Errorln("Benchmark FAILED:", errorMsg);
    }

    if (ret == 0 && !caseJson.empty())
    {
        std::vector<std::string> cases = {caseJson};
        WriteUnifiedReport(cases, "report.json");
    }

    return ret;
}
