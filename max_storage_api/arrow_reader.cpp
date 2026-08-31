#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/arrow_reader.h"
#include "tunnel/arrow_stream.h"
#include "tunnel/serialize.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;
using namespace apsara::odps::tunnel::serialize;

ArrowBatchReader::ArrowBatchReader(const RequestPtr& req, const Configuration& conf)
    : mConf(conf)
{
    mConn = std::make_shared<HttpConnection>(conf, true);
    mConn->SetRequest(req);
    mConn->Open();

    ResponsePtr resp = mConn->GetResponse();

    if (!resp->isSuccessful())
    {
        TunnelThrow(*resp, conf.tunnelEndpoint);
    }

    mRequestId = resp->GetHeader(HEADER_ODPS_REQUEST_ID);

    static const int DEFAULT_BUFFER_SIZE = 64 * 1024;
    auto httpStream = std::make_shared<HttpInputStream>(resp, DEFAULT_BUFFER_SIZE);
    auto zeroCopyReader = std::make_shared<ZeroCopyStreamReader>(httpStream);
    auto arrowStream = std::make_shared<ArrowInputStream>(zeroCopyReader);
    auto result = arrow::ipc::RecordBatchStreamReader::Open(arrowStream);

    if (!result.ok())
    {
        TunnelThrow(ARROW_ERROR,
                    result.status().ToString(),
                    mRequestId,
                    conf.tunnelEndpoint);
    }

    mReader = result.ValueUnsafe();
}

std::shared_ptr<arrow::RecordBatch> ArrowBatchReader::Read()
{
    auto result = mReader->Next();
    if (!result.ok())
    {
        TunnelThrow(ARROW_ERROR,
                    result.status().ToString(),
                    mRequestId,
                    mConf.tunnelEndpoint);
    }
    return result.ValueUnsafe();
}
#endif  // ODPS_SDK_ENABLE_ARROW
