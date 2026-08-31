#ifndef APSARA_ODPS_SDK_REST_RESPONSE_H
#define APSARA_ODPS_SDK_REST_RESPONSE_H

#include "odps_api.h"
#include "tinyxml2.h"
#include <map>
#include <memory>
#include <string>

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal
{
class RestResponse
{
public:
    std::map<std::string, std::string> headers;
    int status_code;
    std::string body;
    bool is_successful;
    
    tinyxml2::XMLDocument xml_body;

    std::string GetHeader(const std::string &key);

    void GetHeaders(map<string, string> &result);

    int GetStatusCode();

    std::string GetBody();

    bool IsSuccessful();
};

typedef std::shared_ptr<RestResponse> RestResponsePtr;

} // namespace internal
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif