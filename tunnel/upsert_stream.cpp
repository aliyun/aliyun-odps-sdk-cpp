#include "tunnel/upsert.h"
#include "tunnel/upsert_stream.h"
#include "util.h"
#include "tunnel/hash_helper.h"
#include "common/http_connection.h"


using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

namespace apsara { namespace odps { namespace sdk { namespace internal { namespace tunnel {

UpsertStream::UpsertStream(const CompressOption& option, UpsertSession* upsert)
     : mCompressOption(option), mUpsert(upsert)
{
    bool compress = CompressOption::ODPS_RAW != mCompressOption.algorithm;

    for(auto bucket : upsert->GetBuckets())
    {
        int32_t slot = bucket.first;
        if(!compress)
        {
            mBucketBuffer[slot] = CreateRecordPack(slot);
        }
        else
        {
            mBucketBuffer[slot] = CreateRecordPack(slot, option);
        }
    }
}

UpsertStream::~UpsertStream()
{
}

void UpsertStream::Upsert(ODPSTableRecord& r)
{
    UpsertRecord& upsertRecord = dynamic_cast<UpsertRecord&>(r);
    Write(upsertRecord, Operation::UPSERT, std::vector<std::string>());
}

void UpsertStream::Upsert(ODPSTableRecord& r, std::vector<std::string> upsertCols)
{
    UpsertRecord& upsertRecord = dynamic_cast<UpsertRecord&>(r);
    if (upsertCols.size() != 0 && !mUpsert->supportPartialUpdate())
    {
        throw OdpsTunnelException("Table " + mUpsert->GetTable() 
        + " do not support partial update, consider set table properties 'acid.partial.fields.update.enable=true'");
    }

    if (upsertCols.size() != 0)
    {
        for(uint32_t i = 0; i < upsertCols.size(); i++)
        {
            if(upsertRecord.mColName2Id.find(upsertCols[i]) == upsertRecord.mColName2Id.end())
            {
                throw OdpsTunnelException("Invalid column name: " + upsertCols[i]);
            }
        }
    }
    Write(upsertRecord, Operation::UPSERT, upsertCols);
}

void UpsertStream::Delete(ODPSTableRecord& r)
{
    UpsertRecord& upsertRecord = dynamic_cast<UpsertRecord&>(r);
    Write(upsertRecord, Operation::DELETE, std::vector<std::string>());
}

void UpsertStream::Close()
{
    if (mStatus == Status::NORMAL)
    {
        Flush();
        mStatus = Status::CLOSED;
    }
}

void UpsertStream::Reset()
{
    for(auto i : mUpsert->GetBuckets())
    {
        IRecordPackPtr pack = mBucketBuffer[i.first];
        if(pack != nullptr)
        {
            pack->Reset();
        }
    }
    mTotalBufferSize = 0;
    mStatus = Status::NORMAL;
}

int64_t UpsertStream::getSlotBufferSize()
{
    return mSlotBufferSize;
}

int64_t UpsertStream::getMaxBufferSize()
{
    return mMaxBufferSize;
}

void UpsertStream::setSlotBufferSize(int64_t slotBufferSize)
{
    mSlotBufferSize = slotBufferSize;
}

void UpsertStream::setMaxBufferSize(int64_t maxBufferSize)
{
    mMaxBufferSize = maxBufferSize;
}

void UpsertStream::setListener(IListenerPtr listener)
{
    mListener = listener;
}

IRecordPackPtr UpsertStream::CreateRecordPack(int32_t slot)
{
    return CreateRecordPack(slot, mCompressOption);
}

IRecordPackPtr UpsertStream::CreateRecordPack(int32_t slot, const CompressOption& option)
{
    if (!CompressOptionCompatWithStreaming(option))
    {
        throw OdpsTunnelException("Compress algorithm " + CompressOptionToEncoding(option) + " is not compatible with streaming tunnel.");
    }

    RecordPackPtr recordPackPtr = std::make_shared<RecordPack>(
        mUpsert->GetODPSSchema(),
        mCompressOption,
        mUpsert->GetUpsertRecordPackWriter());
    recordPackPtr->SetBucketId(slot);
    IRecordPackPtr iRecordPackPtr = std::dynamic_pointer_cast<IRecordPack>(recordPackPtr);
    return iRecordPackPtr;
}

void UpsertStream::Write(UpsertRecord& r, const Operation& op, std::vector<std::string> valueColumns)
{
    CheckStatus();
    int32_t hashVal = HashHelper::GetHasher(r, mUpsert->GetHashKeys());
    int32_t bucket = hashVal % mUpsert->GetBuckets().size();
    IRecordPackPtr pack = mBucketBuffer[bucket];
    r.SetOperation(op == Operation::UPSERT ? 'U' : 'D');

    std::string bigintArrTypeSpec = "ARRAY<BIGINT>";
    ODPSColumnTypeInfo bigintArrType = ODPSColumnTypeInfo::ParseTypeInfoString(bigintArrTypeSpec);
    std::shared_ptr<ODPSArray> valueCols = std::make_shared<ODPSArray>(bigintArrType);

    for(uint32_t i = 0; i < valueColumns.size(); i++)
    {
        valueCols->AppendBigIntValue(mUpsert->GetName2Id()[valueColumns[i]]);
    }
    r.SetValueCols(valueCols);
    int64_t bytes = pack->GetDataSize();
    if(pack != nullptr)
    {
        pack->Append(r);
    }
    bytes = pack->GetDataSize() - bytes;
    mTotalBufferSize += bytes;
    if(pack->GetDataSize() > mSlotBufferSize)
    {
        Flush(false);
    }
    else if(mTotalBufferSize > mMaxBufferSize)
    {
        Flush(true);
    }
}

void UpsertStream::Flush(bool flushAll)
{
    int64_t totalBufferSize = 0;
    for(auto i : mUpsert->GetBuckets())
    {
        IRecordPackPtr pack = mBucketBuffer[i.first];
        if(pack != nullptr && pack->GetRecordCount() > 0)
        {
            if(pack->GetRecordCount() > 0)
            {
                if(pack->GetDataSize() > mSlotBufferSize || flushAll)
                {
                    int32_t retry = 0;
                    bool success = true;
                    do{
                        try
                        {
                            FlushResult result = pack->Flush(FlushOption());
                            if(mListener != nullptr)
                            {
                                mListener->OnFlush(result);
                            }
                            success = true;
                        }
                        catch(const OdpsException& e)
                        {
                            success = false;
                            if(mListener != nullptr)
                            {
                                if(mListener->OnFlushFail(e.GetErrorMsg(), retry))
                                {
                                    ++retry;
                                }
                                else
                                {
                                    mStatus = Status::ERROR;
                                    throw;
                                }
                            }
                            else
                            {
                                throw;
                            }
                        }
                    } while (!success);
                }
                totalBufferSize += pack->GetDataSize();
            }
        }
    }

    if(flushAll)
    {
        mTotalBufferSize = 0;
    }
    else
    {
        mTotalBufferSize = totalBufferSize;
    }
}

void UpsertStream::CheckStatus()
{
    if (mStatus == Status::CLOSED)
    {
        throw OdpsTunnelException(UPSERT_STREAM_CLOSED, "Upsert Stream Is Closed.");
    }
    else if (mStatus == Status::ERROR)
    {
        throw OdpsTunnelException(UPSERT_STREAM_ERROR, "Upsert Stream Has Errors.");
    }
}

void UpsertStream::UpdateSlot(int32_t id, const std::string& serverAddr)
{
    std::lock_guard<std::mutex> _lock(mLock);
    try
    {
        auto buckets = mUpsert->GetBuckets();
        buckets.at(id) = Slot(id, serverAddr);
    }
    catch(std::out_of_range& ingore) {}
}


}}}}}