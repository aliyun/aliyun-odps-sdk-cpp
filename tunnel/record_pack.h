#ifndef APSARA_ODPS_TUNNEL_INTERNAL_RECORD_PACK_H
#define APSARA_ODPS_TUNNEL_INTERNAL_RECORD_PACK_H

#include "odps_tunnel.h"
#include "tunnel/stream_upload.h"
#include "tunnel/upsert.h"
#include "tunnel/upsert_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "record_pack_impl.h"
#include "serialize.h"
#include "util.h"

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

using google::protobuf::io::StringOutputStream;

class RecordPack : public RecordPackImpl
{
public:
    RecordPack(ODPSTableSchemaPtr schema,
        const CompressOption& option,
        const IRecordPackWriterPtr& packWriter,
        size_t reserveSize = 1024 * 1024);
    virtual ~RecordPack();

public:
    virtual bool Append(const ODPSTableRecord& record);
    virtual int64_t GetDataSize() const
    {
        return mBuffer.size();
    }
    virtual int64_t GetRecordCount() const
    {
        return mTotalRecords;
    }
    virtual std::string Flush();
    virtual FlushResult Flush(const FlushOption& option);
    virtual void Reset();
    void UpdateServerMetrics(const std::string& metrics);
    virtual std::string GetMetrics();

public:
    virtual const CompressOption& GetCompressOption() const
    {
        return mCompressOption;
    }
    virtual const char* GetData() const
    {
        return mBuffer.c_str();
    }
    virtual const FlushOption* GetFlushOption() const
    {
        return mFlushOption;
    }

    virtual void SetBucketId (uint32_t bucketId)
    {
        mBucketId = bucketId;
    }

    virtual uint32_t GetBucketId() const
    {
        return mBucketId;
    }

    virtual void UpdateClientIOCost(int64_t writeTime)
    {
        mMetrics.ClientIOCost += writeTime;
    }

private:
    void Complete();

private:
    std::string mBuffer;
    int64_t mTotalRecords;
    std::shared_ptr<StringOutputStream> mOutput;
    CompressOption mCompressOption;
    IRecordPackWriterPtr mPackWriter;
    ProtoSerializerPtr mSerializer;
    ODPSTableSchemaPtr mSchema;
    bool mCompleted;
    const FlushOption* mFlushOption;
    uint32_t mBucketId = 0;
    Metrics mMetrics;
};

typedef std::shared_ptr<RecordPack> RecordPackPtr;

}
}
}
}
}

#endif

