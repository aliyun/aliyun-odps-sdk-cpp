#include "rest_utils.h"
#include "util/utils.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {

RestResponsePtr RestUtils::ToRestResponse(
    Response &resp, HttpConnection &conn, bool can_parse)
{
    RestResponsePtr detail = std::make_shared<RestResponse>();

    // response header
    std::map<std::string, std::string> headers;
    resp.GetHeaders(headers);
    detail->headers = headers;

    // response status code
    int resp_code = resp.GetStatusCode();
    detail->status_code = resp_code;

    // response body
    std::ostringstream oss;
    char buff[512];
    int64_t bytes;
    while (0 != (bytes = resp.ReadBody(buff, 512)))
    {
        oss << std::string(buff, bytes);
    }
    const std::string &msg = oss.str().empty() ? conn.TraceError() : oss.str();
    detail->body = msg;
    if (resp.isSuccessful())
    {
        detail->is_successful = true;
    }
    else
    {
        detail->is_successful = false;
    }

    if (can_parse)
    {
        char *ret = (char *)msg.c_str();
        detail->xml_body.Parse(ret);
    }
    return detail;
}

RestResponsePtr RestUtils::ToRestResponse(
    Response &resp, HttpConnection &conn)
{
    return ToRestResponse(resp, conn, false);
}

void RestUtils::HandleOdpsFailures(RestResponsePtr resp)
{
    if (!resp->IsSuccessful())
    {
        tinyxml2::XMLHandle handle(&(resp->xml_body));
        tinyxml2::XMLElement *err_code = handle.FirstChildElement("Error").FirstChildElement("Code").ToElement();
        tinyxml2::XMLElement *err_message = handle.FirstChildElement("Error").FirstChildElement("Message").ToElement();
        tinyxml2::XMLElement *request_id = handle.FirstChildElement("Error").FirstChildElement("RequestId").ToElement();

        if (err_code && err_message && request_id)
        {
            string e_code(err_code->GetText());
            string e_message(err_message->GetText());
            string e_req(request_id->GetText());

            util::OdpsThrowWithRequestId(e_code, e_req, e_message);
        }
        else
        {
            util::OdpsThrow("UnknownError", resp->body);
        }
    }
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
