#include "security_manager.h"
#include "util/string_util.h"
#include "tinyxml2.h"
#include "util/utils.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {

SecurityManager::~SecurityManager() {}

SecurityManager::SecurityManager(const std::string& project, RestClientPtr client) : client(client), project(project)
{
}

std::string SecurityManager::GenerateAuthorizationToken(const std::string &policy, const std::string &type)
{
    if (util::ToLowerCaseString(type) == "bearer")
    {
        std::string rest_path = RestResourceBuilder::BuildAuthorizationRest(this->project);
        std::map<std::string, std::string> headers;
        headers[CONTENT_TYPE] = "application/json";
        std::map<std::string, std::string> params;
        params["sign_bearer_token"] = "";
        std::string body(policy);
        RestResponsePtr response = std::dynamic_pointer_cast<RestResponse>(
            this->client->DoRequest(rest_path, HTTP_METHOD_POST, headers, params, body, true)
        );
        if (response->is_successful)
        {
            tinyxml2::XMLElement *ele = response->xml_body.FirstChildElement("Authorization")->FirstChildElement("Result");
            if (ele->GetText() != nullptr)
            {
                return std::string(ele->GetText());
            }
            return "";
        }
        else
        {
            util::OdpsThrow(INTERNAL_ERROR, "Failed to sign bearer token:", response->status_code, response->body);
        }
    }
    else
    {
        util::OdpsThrow(INVALID_ARGUMENT, "Unknown token type:", type);
    }
    return "";
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
