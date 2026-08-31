#include "max_storage_api/request_handler.h"
#include "common/http_connection.h"
#include "common/http_flags.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::max_storage_api;

std::string RequestHandler::HandleRequest(const std::string& reqBody)
{
    mRequest->SetHeader(CONTENT_LENGTH, std::to_string(reqBody.size()));
    mRequest->SetBody(reqBody);
    internal::HttpConnectionPtr conn = std::make_shared<internal::HttpConnection>(mConf);
    conn->SetRequest(mRequest);
    conn->Open();
    internal::ResponsePtr resp = conn->GetResponse();
    if (!resp->isSuccessful())
    {
        util::TunnelThrow(*resp, mConf.tunnelEndpoint);
    }

    std::string responseBody;
    resp->ReadBody(responseBody);
    mRequestId = resp->GetHeader(HEADER_ODPS_REQUEST_ID);
    mToken = resp->GetHeader(HEADER_ODPS_MAX_STORAGE_ROUTE_TOKEN);

    resp->GetHeaders(mResponseHeaders);
    conn->Close();
    return responseBody;
}
