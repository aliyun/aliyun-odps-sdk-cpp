#include "rest_response.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

std::string RestResponse::GetHeader(const std::string &key)
{
    map<std::string, std::string>::iterator it = headers.find(key);
    if (it != headers.end())
    {
        return it->second;
    }
    return "";
}

void RestResponse::GetHeaders(map<std::string, std::string> &result)
{
    result = this->headers;
}

int RestResponse::GetStatusCode()
{
    return this->status_code;
}

std::string RestResponse::GetBody()
{
    return this->body;
}

bool RestResponse::IsSuccessful()
{
    return this->is_successful;
}

} // namespace internal
} // namespace sdk
} // namespace odps
} // namespace apsara
