#include "record_pack.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;

RecordPack::RecordPack(ODPSTableSchemaPtr schema,
    const CompressOption& option,
    const IRecordPackWriterPtr& packWriter, size_t reserveSize)
    : mTotalRecords(0)
    , mCompressOption(option)
    , mPackWriter(packWriter)
    , mSchema(schema)
    , mCompleted(false)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    mBuffer.reserve(reserveSize);
    mOutput = std::make_shared<StringOutputStream>(&mBuffer);
    bool compress = CompressOption::ODPS_RAW != mCompressOption.algorithm;
    mSerializer.reset(new ProtoSerializer(mOutput.get(),
                                          mSchema.get(),
                                          compress,
                                          false,
                                          DEFAULT_TOTAL_BYTES_LIMIT,
                                          &mCompressOption));
}

RecordPack::~RecordPack()
{
    mSerializer.reset();
}

bool RecordPack::Append(const ODPSTableRecord& record)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    if (!mCompleted)
    {
        bool success = mSerializer->Serialize(record);
        ++mTotalRecords;
        return success;
    }
    else
    {
        throw OdpsTunnelException(PACK_NOT_FLUSHED, "Pack is not successfully flushed, retry flush or reset to release buffer.");
    }
}

std::string RecordPack::Flush()
{
    return Flush(FlushOption()).mTraceId;
}

FlushResult RecordPack::Flush(const FlushOption& option)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    Complete();
    mFlushOption = &option;
    FlushResult result;
    result.mTraceId = mPackWriter->WriteRecordPack(*this);
    result.mFlushSize = mOutput->ByteCount();
    result.mRecordCount = mTotalRecords;
    Reset();

    return result;
}

void RecordPack::Reset()
{
    mSerializer.reset();
    mBuffer.clear();
    mOutput.reset(new StringOutputStream(&mBuffer));
    bool compress = CompressOption::ODPS_RAW != mCompressOption.algorithm;
    mSerializer.reset(new ProtoSerializer(mOutput.get(),
                                          mSchema.get(),
                                          compress,
                                          false,
                                          DEFAULT_TOTAL_BYTES_LIMIT,
                                          &mCompressOption));
    mTotalRecords = 0;
    mCompleted = false;
}

void RecordPack::Complete()
{
    if (!mCompleted)
    {
        mCompleted = true;
        mSerializer->Complete();
    }
}

void RecordPack::UpdateServerMetrics(const std::string& metrics)
{
    if(metrics.empty()) return;
    Metrics newMetrics;
    FromJsonString(newMetrics, metrics);
    mMetrics.PanguIOCost += newMetrics.PanguIOCost;
    mMetrics.ServerTotalCost += newMetrics.ServerTotalCost;
    mMetrics.RateLimitCost += newMetrics.RateLimitCost;
    mMetrics.ServerIOCost += newMetrics.ServerIOCost;
}

std::string RecordPack::GetMetrics()
{
    return GetTunnelMetrics(mMetrics);
}