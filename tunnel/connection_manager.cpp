#include "connection_manager.h"
#include "common/http_connection.h"
#include "util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

ConnectionManager::ConnectionManager(const RequestPtr& req, const Configuration& conf)
    : mRequestPtr(req),
      mConf(conf)
{
}

ConnectionManager::~ConnectionManager()
{}

HttpConnectionPtr ConnectionManager::OpenReaderConnection()
{
    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(mRequestPtr);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();
    if (SuccessWithRetry(conn, resp))
    {
        return conn;
    }
    else
    {
        ostringstream oss;
        char buff[512];
        int64_t bytes;
        while(0 != (bytes = resp->ReadBody(buff, 512)))
        {
            oss << string(buff,bytes);
        }
        const std::string& errorMsg = oss.str().empty()?conn->TraceError():oss.str();
        resp->GetError(errorMsg, mConf.tunnelEndpoint);
    }
}


HttpConnectionPtr ConnectionManager::OpenWriterConnection()
{
    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(mRequestPtr);
    conn->Open();
    return conn;
}