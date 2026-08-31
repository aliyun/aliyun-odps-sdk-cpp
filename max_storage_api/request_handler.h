#ifndef APSARA_ODPS_MAX_STORAGE_REQUEST_HANDLER_H
#define APSARA_ODPS_MAX_STORAGE_REQUEST_HANDLER_H

#include <memory>

#include "common/http_message.h"
#include "include/configuration.h"
#include "util/utils.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

class RequestHandler {
public:
    RequestHandler() = delete;
    virtual ~RequestHandler() = default;

public:
    template <typename RequestT, typename ResponseT>
    void HandleRequest(const RequestT& request, ResponseT& response)
    {
        mRequest->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
        util::FromJsonString(response, HandleRequest(util::ToJsonCompactString(request)));
    }

    template <typename ResponseT>
    void HandleRequest(const char* payload, uint32_t len, ResponseT& response)
    {
        mRequest->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_OCTET_STREAM);
        util::FromJsonString(response, HandleRequest(std::string(payload, len)));
    }

    void HandleRequest()
    {
        HandleRequest("");
    }

public:
    void SetRequestParameter(const std::string& name, const std::string& value) { mRequest->SetParameter(name, value); }
    void SetRequestHeader(const std::string& key, const std::string& value) { mRequest->SetHeader(key, value); }
    std::string GetRequestId() const { return mRequestId; }
    std::string GetToken() const { return mToken; }

    std::string GetResponseHeader(const std::string& key) const
    {
        const auto& it = mResponseHeaders.find(key);
        if (it != mResponseHeaders.end())
        {
            return it->second;
        }
        else
        {
            return "";
        }
    }

protected:
    RequestHandler(
        const Configuration& conf,
        const internal::RequestPtr& request)
    : mConf(conf)
    , mRequest(request) {}

    std::string HandleRequest(const std::string& reqBody);

protected:
    Configuration mConf;
    internal::RequestPtr mRequest;
    std::string mRequestId;
    std::string mToken;
    std::map<std::string, std::string> mResponseHeaders;
};

class TableRequestHandler : public RequestHandler
{
public:
    TableRequestHandler(
        const Configuration& conf,
        const std::string& action,
        const std::string& project,
        const std::string& schema,
        const std::string& table)
    : RequestHandler(
        conf,
        util::CreateMaxStorageApiTableRequest(conf, action, project, schema, table))
    {
    }
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif // APSARA_ODPS_MAX_STORAGE_REQUEST_HANDLER_H
