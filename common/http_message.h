#ifndef APSARA_ODPS_SDK_HTTP_MESSAGE_H
#define APSARA_ODPS_SDK_HTTP_MESSAGE_H

#include <map>
#include <string>
#include <memory>
#include "odps_exception.h"
#include "common/http_flags.h"

namespace apsara{ namespace odps{ namespace sdk{
namespace internal
{

class HttpMessage
{
public:
    HttpMessage() : contentLength(-1)
    {
    }

    void GetHeaders(std::map<std::string, std::string>& h) const {
        h = headers;
    }

    void SetHeaders(const std::map<std::string, std::string>& headers) {
        this->headers = headers;
    }

    void SetHeader(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    std::string GetHeader(const std::string& key) const {
        auto it = headers.find(key);
        return (it == headers.end()) ? "" : it->second;
    }

    bool ContainsHeader(const std::string& key)
    {
        return headers.find(key) != headers.end();
    }

    long GetContentLength() {
        return contentLength;
    }

    void SetContentLength(long contentLength) {
        this->contentLength = contentLength;
    }

    [[noreturn]] void GetError(const std::string& json, const std::string& endpoint = "");
private:
    std::map<std::string, std::string> headers;
    long contentLength;
};

class HttpConnection;
class Request : public HttpMessage
{
public:
    Request() : mConn(NULL) {
        SetHeader(HEADER_ODPS_TUNNEL_VERSION, VERSION_6);
    }

    virtual ~Request() {}

    const std::string& GetMethod() const {
        return method;
    }

    void SetMethod(const std::string& method) {
        this->method = method;
    }

    const std::string& GetEndpoint() const {
        return endpoint;
    }

    void SetEndpoint(const std::string& ep) {
        endpoint = ep;
    }

    const std::string& GetResourcePath() const {
        return resourcePath;
    }

    void SetResourcePath(const std::string& resourcePath) {
        this->resourcePath = resourcePath;
    }

    void GetParameters(std::map<std::string, std::string>& param) const {
        param = parameters;
    }

    void SetParameters(const std::map<std::string, std::string>& param) {
        parameters = param;
    }

    void SetParameter(const std::string& name, const std::string& value) {
        parameters[name] = value;
    }

    void SetHttpConnection(HttpConnection* conn){
        mConn = conn;
    }

    std::string GetParameter(const std::string& name) const {
        typeof(parameters.begin()) it = parameters.find(name);
        return (it == parameters.end()) ? "" : it->second;
    }

    int64_t WriteBody(const char* buffer, uint64_t len);

    void SetBody(const std::string& body) {
        this->body = body;
    }
    const std::string& GetBody() const {
        return body;
    }

private:
    std::string method;
    std::string endpoint;
    std::string resourcePath;
    HttpConnection* mConn;
    std::map<std::string, std::string> parameters;
    std::string body;
};

typedef std::shared_ptr<Request> RequestPtr;

class HttpConnection;
class Response : public HttpMessage
{
private:
    static const int HTTP_SUCCESS_STATUS_CODE = 200;

    HttpConnection* conn;
    int statusCode;

public:
    Response(HttpConnection* conn) : conn(conn),statusCode(0) {}
    virtual ~Response() {}

    void SetStatusCode(int sc) {
        statusCode = sc;
    }

    int GetStatusCode() {
        return statusCode;
    }

    bool isSuccessful();

    virtual int64_t ReadBody(char* buffer, uint64_t len);
    void ReadBody(std::string& content);
};

typedef std::shared_ptr<Response> ResponsePtr;

}}}}
#endif
