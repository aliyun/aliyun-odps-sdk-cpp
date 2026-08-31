#include "common/http_message.h"

#include <nlohmann/json.hpp>
#include "error_code.h"
#include "common/http_connection.h"
#include <map>
#include <string>

namespace apsara { namespace odps { namespace sdk { namespace internal {

#define RESPONSE_CODES 57
static const char* const status_lines[RESPONSE_CODES] =
{
    "Continue",
    "Switching Protocols",
    "Processing",
#define LEVEL_200  3
    "OK",
    "Created",
    "Accepted",
    "Non-Authoritative Information",
    "No Content",
    "Reset Content",
    "Partial Content",
    "Multi-Status",
#define LEVEL_300 11
    "Multiple Choices",
    "Moved Permanently",
    "Found",
    "See Other",
    "Not Modified",
    "Use Proxy",
    "unused",
    "Temporary Redirect",
#define LEVEL_400 19
    "Bad Request",
    "Authorization Required",
    "Payment Required",
    "Forbidden",
    "Not Found",
    "Method Not Allowed",
    "Not Acceptable",
    "Proxy Authentication Required",
    "Request Time-out",
    "Conflict",
    "Gone",
    "Length Required",
    "Precondition Failed",
    "Request Entity Too Large",
    "Request-URI Too Large",
    "Unsupported Media Type",
    "Requested Range Not Satisfiable",
    "Expectation Failed",
    "unused",
    "unused",
    "unused",
    "unused",
    "Unprocessable Entity",
    "Locked",
    "Failed Dependency",
    /* This is a hack, but it is required for ap_index_of_response
     * to work with 426.
     */
    "No code",
    "Upgrade Required",
#define LEVEL_500 46
    "Internal Server Error",
    "Method Not Implemented",
    "Bad Gateway",
    "Service Temporarily Unavailable",
    "Gateway Time-out",
    "HTTP Version Not Supported",
    "Variant Also Negotiates",
    "Insufficient Storage",
    "unused",
    "unused",
    "Not Extended"
};

void HttpMessage::GetError(const std::string& json, const std::string& endpoint)
{
    std::string errorCode;
    std::string errorMsg = json;
    std::map<std::string, std::string> extraInfo;
    try
    {
        nlohmann::json errMap = nlohmann::json::parse(json);
        errorCode = errMap.at("Code").get<std::string>();
        errorMsg = errMap.at("Message").get<std::string>();
        for (nlohmann::json::const_iterator it = errMap.begin(); it != errMap.end(); ++it)
        {
            if (it.value().is_string())
            {
                extraInfo[it.key()] = it.value().get<std::string>();
            }
            else
            {
                extraInfo[it.key()] = it.value().dump(4);
            }
        }
    }
    catch (const std::exception& e)
    {
        errorMsg = json;
    }

#ifdef ODPS_SDK_ENABLE_ARROW
    if (errorCode == ALREADY_WRITTEN)
    {
        throw ExactlyOnceAlreadyWrittenException(errorCode, errorMsg, GetHeader(HEADER_ODPS_REQUEST_ID), extraInfo);
    }
#endif

    throw OdpsException(errorCode, errorMsg + " from:" + endpoint, GetHeader(HEADER_ODPS_REQUEST_ID), extraInfo);
}

int64_t Request::WriteBody(const char* buffer, uint64_t len)
{
    return mConn->Write(buffer, len);
}

bool Response::isSuccessful()
{
    if (!statusCode && conn)
    {
        try {
            conn->CloseUpstream();
            conn->WaitResponse();
        }
        catch (OdpsException& e) {
            if (conn->IsTimeout())
            {
                throw OdpsException("Connection Timeout");
            }
            return false;
        }
    }
    return GetStatusCode() / 100 == HTTP_SUCCESS_STATUS_CODE / 100;
}

int64_t Response::ReadBody(char* buffer, uint64_t len)
{
    conn->CloseUpstream();
    return conn->Read(buffer, len);
}

void Response::ReadBody(std::string& content)
{
    char buff[64];
    int64_t bytes;
    int64_t totalBytes = 0;
    while (totalBytes != GetContentLength() && (0 != (bytes = ReadBody(buff, 64))))
    {
        totalBytes += bytes;
        content.append(buff, bytes);
    }
}

}}}}
