#ifndef APSARA_ODPS_TUNNEL_INTERNAL_UPSERT_STREAM_H
#define APSARA_ODPS_TUNNEL_INTERNAL_UPSERT_STREAM_H

#include "odps_tunnel.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "tunnel/record_pack.h"
#include "tunnel/upsert_record.h"
#include "serialize.h"
#include "tunnel/stream_upload.h"

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

class Upsert;

class UpsertStream : public IUpsertStream
{
public:
    UpsertStream(const CompressOption& option, UpsertSession* upsert);
    ~UpsertStream();

public:
    virtual void Upsert(ODPSTableRecord& r) override;
    virtual void Upsert(ODPSTableRecord& r, std::vector<std::string> upsertCols) override;
    virtual void Delete(ODPSTableRecord& r) override;
    virtual void Flush(bool flushAll = true) override;
    virtual void Close() override;
    virtual void Reset() override;

    virtual int64_t getSlotBufferSize() override;
    virtual int64_t getMaxBufferSize() override;
    virtual void setSlotBufferSize(int64_t slotBufferSize) override;
    virtual void setMaxBufferSize(int64_t maxBufferSize) override;

    virtual void setListener(IListenerPtr listener) override;

private:
    IRecordPackPtr CreateRecordPack(int32_t slot);
    IRecordPackPtr CreateRecordPack(int32_t slot, const CompressOption& option);

    enum Operation
    {
        UPSERT,
        DELETE
    };

    enum Status
    {
        NORMAL,
        ERROR,
        CLOSED
    };
    CompressOption mCompressOption;
    UpsertSession* mUpsert;
    IListenerPtr mListener;

    std::mutex mLock;
    std::map<int32_t, IRecordPackPtr> mBucketBuffer;
    Status mStatus = Status::NORMAL;

    int64_t mSlotBufferSize = 1024 * 1024;
    int64_t mMaxBufferSize = 64 * 1024 * 1024;
    int64_t mTotalBufferSize = 0;

private:
    void Write(UpsertRecord& r, const Operation& op, std::vector<std::string> validColumns);
    Slot GetSlot(int32_t bucketNum);
    void CheckStatus();
    void UpdateSlot(int32_t id,
        const std::string& serverAddr,
        const std::string& strSlotNum);
    void UpdateSlot(int32_t id, const std::string& serverAddr);

};



}  // namespace tunnel
}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif