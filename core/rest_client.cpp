#include "rest_client.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

RestResponsePtr RestClient::DoRequest(
    const std::string &resource,
    const std::string &method,
    const map<std::string, std::string> &headers,
    const map<std::string, std::string> &params,
    const std::string &body,
    const bool &can_parse_response)
{
    RequestPtr req(new Request());
    req->SetMethod(method);
    req->SetEndpoint(this->conf.GetEndpoint());
    req->SetResourcePath(resource);

    if (headers.size() > 0)
    {
        req->SetHeaders(headers);
        if (body != "")
        {
            if (!req->ContainsHeader(CONTENT_LENGTH))
            {
                req->SetHeader(CONTENT_LENGTH, to_string(body.length()));
            }
            if (!req->ContainsHeader(CONTENT_MD5))
            {
                req->SetHeader(CONTENT_MD5, util::GenerateMd5Signature(body));
            }
        }
    }

    if (params.size() > 0)
    {
        req->SetParameters(params);
    }

    // 设置Http连接
    HttpConnectionPtr conn = std::make_shared<HttpConnection>(this->conf);
    // 发送请求
    conn->SetRequest(req);
    conn->Open();

    if (body != "")
    {
        req->SetHttpConnection(conn.get());
        req->WriteBody(body.c_str(), body.length());
    }

    conn->CloseUpstream();
    conn->WaitResponse();
    ResponsePtr resp = conn->GetResponse();
    RestResponsePtr rest_resp = RestUtils::ToRestResponse(*(resp.get()), *(conn.get()), can_parse_response);
    conn->Close();
    return rest_resp;
}

} // namespace internal
} // namespace sdk
} // namespace odps
} // namespace apsara
