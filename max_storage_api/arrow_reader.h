#ifndef APSARA_ODPS_MAX_STORAGE_ARROW_READER_H
#define APSARA_ODPS_MAX_STORAGE_ARROW_READER_H
#ifdef ODPS_SDK_ENABLE_ARROW

#include <arrow/api.h>
#include <arrow/ipc/reader.h>
#include <memory>

#include "common/http_message.h"
#include "common/http_connection.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

class ArrowBatchReader {
public:
    ArrowBatchReader(const internal::RequestPtr& req, const Configuration& conf);

    std::shared_ptr<arrow::RecordBatch> Read();

    int64_t GetWireBytes() const { return mConn ? (int64_t)mConn->GetDownstreamBytes() : 0; }

private:
    Configuration mConf;
    std::shared_ptr<arrow::RecordBatchReader> mReader;
    internal::HttpConnectionPtr mConn;
    std::string mRequestId;
};


}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // ODPS_SDK_ENABLE_ARROW
#endif