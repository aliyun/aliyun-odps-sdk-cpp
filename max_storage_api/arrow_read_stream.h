#ifndef APSARA_ODPS_MAX_STORAGE_ARROW_READ_STREAM_H
#define APSARA_ODPS_MAX_STORAGE_ARROW_READ_STREAM_H
#ifdef ODPS_SDK_ENABLE_ARROW

#include "include/max_storage_api.h"
#include "max_storage_api/arrow_reader.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

class ArrowReadStreamImpl : public IArrowReadStream
{
public:
    ArrowReadStreamImpl(const std::string& action) : mAction(action) {}
    virtual ~ArrowReadStreamImpl() {}

public:
    virtual std::shared_ptr<arrow::RecordBatch> Read();
    virtual int64_t GetWireBytes() const override { return mCachedWireBytes; }

protected:
    virtual std::shared_ptr<ArrowBatchReader> OpenReader() = 0;

protected:
    std::string mAction;
    std::shared_ptr<ArrowBatchReader> mReader;
    bool mClosed = false;
    bool mEof = false;
    int64_t mCachedWireBytes = 0;
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // ODPS_SDK_ENABLE_ARROW
#endif
