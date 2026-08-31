#include "resource.h"
#include "util/utils.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {

bool Resource::Exists()
{
    std::string rest_path = RestResourceBuilder::BuildResourceRest(
        this->GetProject(), this->GetName());
    map<std::string, std::string> headers;
    map<std::string, std::string> params;
    params["meta"] = "";
    RestResponsePtr resp = this->odps->GetRestClient()->DoRequest(rest_path, HTTP_METHOD_GET, headers, params, "", false);
    return resp->status_code == 200;
}

bool Resource::Create(bool overwrite)
{
    std::string rest_path;
    std::string method;
    if (overwrite)
    {
        rest_path = RestResourceBuilder::BuildResourceRest(this->project, this->name);
        method = HTTP_METHOD_PUT;
    }
    else
    {
        rest_path = RestResourceBuilder::BuildResourcesRest(this->project);
        method = HTTP_METHOD_POST;
    }

    // 设置http头
    long size = 0L;
    std::string md5_content = util::GenerateMd5Signature(this->content, size);
    std::map<std::string, std::string> headers;
    headers[CONTENT_TYPE] = "application/octet-stream";
    headers[CONTENT_LENGTH] = std::to_string(size);
    headers[CONTENT_MD5] = md5_content;
    headers["Content-Disposition"] = "attachment;filename=" + this->name;
    headers["x-odps-resource-type"] = "file";
    headers["x-odps-resource-name"] = this->name;
    if (this->comment != "")
    {
        headers["x-odps-comment"] = this->comment;
    }
    if (this->is_temp_resource)
    {
        headers["x-odps-resource-istemp"] = "true";
    }

    // 空的params
    map<std::string, std::string> params;

    std::string body = this->content.str();
    RestResponsePtr resp = this->odps->GetRestClient()->DoRequest(rest_path, method, headers, params, body, false);
    return resp->IsSuccessful();
}

bool Resource::Delete()
{
    std::string rest = RestResourceBuilder::BuildResourceRest(this->project, this->name);
    map<std::string, std::string> headers;
    RestResponsePtr resp = odps->GetRestClient()->DoRequest(rest, "DELETE", headers, {}, "", false);
    return resp->IsSuccessful();
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
