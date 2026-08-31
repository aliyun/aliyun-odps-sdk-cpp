#include <algorithm>
#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "util/utils.h"
#include "common/http_connection.h"
#ifdef ENABLE_VIPSERVER
#include "vipclient_helper.hpp"
#endif

using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

std::string GetRouterServer(const Configuration& conf, const std::string& project)
{
    if (!conf.tunnelEndpoint.empty()) return conf.tunnelEndpoint;
    if (project.empty()) return std::string();

    RequestPtr req(new Request());
    req->SetMethod(HTTP_METHOD_GET);
    req->SetContentLength(0);
    req->SetEndpoint(conf.odpsEndpoint);
    req->SetResourcePath("projects/" + project + "/tunnel");
    req->SetParameter("service", "");
    if (!conf.GetTunnelQuotaName().empty())
    {
        req->SetParameter("quotaName", conf.GetTunnelQuotaName());
    }
    HttpConnectionPtr conn(new HttpConnection(conf));
    conn->SetRequest(req, true);
    conn->Open();
    ResponsePtr resp = conn->GetResponse();
    std::string content;
    resp->ReadBody(content);
    if (!resp->isSuccessful())
    {
        std::cerr<<"Trying to get tunnel route endpoint from:" + conf.odpsEndpoint + " failed."<<std::endl;
        resp->GetError(content, conf.odpsEndpoint);
    }
#ifdef ENABLE_VIPSERVER
    if (conf.IsUseVIPServer())
    {
        std::string ip;
        if (middleware::vipclient::helper::QueryValidIp(content.c_str(), &ip))
        {
            return "http://" + ip;
        }
    }
#endif
    return "http://" + content;
}

void RandomSleep(int min, int max)
{
    min *= 1000;
    max *= 1000;
    int dura = std::max(min, max) - std::min(min, max);
    usleep((uint32_t)(std::min(min, max) + (rand() % dura)));
}

bool SuccessWithRetry(HttpConnectionPtr& conn, ResponsePtr& resp)
{
    if (resp->isSuccessful()) return true;
    if (resp->GetStatusCode() == 429)
    {
        const int maxRetry = 6;
        int retryCount = 0;
        int sleepInterval = 1;
        while (retryCount < maxRetry)
        {
            usleep(sleepInterval * 1000 * 1000);
            conn->Close();
            conn->Open();
            resp = conn->GetResponse();
            if (resp->isSuccessful()) return true;
            ++retryCount;
            sleepInterval *= 2;
        }
        return false;
    }
    return false;
}

std::string GetTunnelMetrics(const Metrics& metrics)
{
    nlohmann::json tunnelMetrics;
    tunnelMetrics["PanguIOCost"] = metrics.PanguIOCost;
    tunnelMetrics["RateLimitCost"] = metrics.RateLimitCost;
    tunnelMetrics["NetworkCost"] = metrics.ClientIOCost - (metrics.ServerTotalCost - metrics.ServerIOCost);
    tunnelMetrics["TunnelProccessCost"] = metrics.ServerTotalCost - metrics.RateLimitCost - metrics.ServerIOCost - metrics.PanguIOCost;
    tunnelMetrics["ClientProcessCost"] = metrics.ClientProcessCost - metrics.ClientIOCost;
    return ToJsonString(tunnelMetrics);
}

}}}}}
