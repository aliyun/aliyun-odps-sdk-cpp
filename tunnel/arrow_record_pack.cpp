#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_pack.h"
#include "arrow_util.h"
#include <arrow/ipc/writer.h>
#include "lz4_stream.h"
#include <memory>

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

ArrowRecordPack::ArrowRecordPack(const CompressOption& option,
    size_t reserveSize):
    mCompressOption(option),
    mReserveSize(reserveSize)
{
    Reset();
}

void ArrowRecordPack::Reset()
{
    mTotalRecords = 0;
    mCompleted = false;
    mBuffer.clear();
    mBuffer.reserve(mReserveSize);
    mOutput = std::make_shared<google::protobuf::io::StringOutputStream>(&mBuffer);
    if(mCompressOption.algorithm != CompressOption::CompressAlgorithm::ODPS_LZ4)
    {
        mArrowOutput.reset(new ArrowOutputStream<ZeroCopyStreamWriter>(new ZeroCopyStreamWriter(mOutput), 64 * 1024));
    }
    else
    {
        std::shared_ptr<google::protobuf::io::Lz4OutputStream> lz4Stream = std::make_shared<google::protobuf::io::Lz4OutputStream>(mOutput.get());
        mArrowOutput.reset(new ArrowOutputStream<ZeroCopyStreamWriter>(new ZeroCopyStreamWriter(lz4Stream), 64 * 1024));
    }
}

void ArrowRecordPack::Complete()
{
    mCompleted = true;
    mArrowOutput->Flush().ok();
    mArrowOutput->Close().ok();
    mArrowOutput.reset();
    mOutput.reset();
}

bool ArrowRecordPack::Append(const arrow::RecordBatch& rbatch)
{
    if (mCompleted)
    {
        throw OdpsTunnelException("Cannot append to a completed pack.");
    }

    arrow::Status status;
    const auto& options = GetWriteOptions(mCompressOption);
    status = arrow::ipc::SerializeRecordBatch(rbatch, options, mArrowOutput.get());
    if (status.ok())
    {
        mTotalRecords += rbatch.num_rows();
        return true;
    }
    else
    {
        throw OdpsTunnelException("ArrowHttpOutputStream Serialize Exception");
    }
}

}}}}}
#endif
