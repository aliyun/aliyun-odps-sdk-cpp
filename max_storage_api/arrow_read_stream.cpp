#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/arrow_read_stream.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::util;

std::shared_ptr<arrow::RecordBatch> ArrowReadStreamImpl::Read()
{
    if (mClosed)
    {
        TunnelThrow(mAction + "Stream is closed");
    }

    if (mEof)
    {
        return nullptr;
    }

    if (!mReader)
    {
        // TODO: 短连接
        mReader = OpenReader();
    }

    auto batch = mReader->Read();

    if (!batch)
    {
        mEof = true;
        mCachedWireBytes = mReader->GetWireBytes();
        mReader.reset();
    }
    else
    {
        mCachedWireBytes = mReader->GetWireBytes();
    }

    return batch;
}
#endif  // ODPS_SDK_ENABLE_ARROW
