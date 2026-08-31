#ifndef APSARA_ODPS_TUNNEL_INTERNAL_ARROW_RECORD_PACK_H
#define APSARA_ODPS_TUNNEL_INTERNAL_ARROW_RECORD_PACK_H

#ifdef ODPS_SDK_ENABLE_ARROW
#include "odps_tunnel.h"
#include "tunnel/arrow_http_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "serialize.h"
#include "util.h"
#include <memory>

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

using google::protobuf::io::StringOutputStream;

class ArrowRecordPack
{
public:
    ArrowRecordPack(const CompressOption& option,
        size_t reserveSize = 1024 * 1024);
    virtual ~ArrowRecordPack() {};

public:
    bool Append(const arrow::RecordBatch& rbatch);
    int64_t GetDataSize() const
    {
        return mBuffer.size();
    }
    int64_t GetRecordCount() const
    {
        return mTotalRecords;
    }
    void Complete();
    const std::string& Data() const { return mBuffer; };
    void Reset();

    const CompressOption& GetCompressOption() const { return mCompressOption; }

private:
    std::string mBuffer;
    int64_t mTotalRecords = 0;
    std::shared_ptr<StringOutputStream> mOutput;
    std::shared_ptr<IArrowOutputStream> mArrowOutput;
    CompressOption mCompressOption;
    bool mCompleted = false;
    int64_t mReserveSize = 0;
};

typedef std::shared_ptr<ArrowRecordPack> ArrowRecordPackPtr;

}
}
}
}
}

#endif
#endif

