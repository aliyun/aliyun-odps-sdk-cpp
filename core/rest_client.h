#ifndef APSARA_ODPS_SDK_REST_CLIENT_H
#define APSARA_ODPS_SDK_REST_CLIENT_H

#include "configuration.h"
#include "rest_utils.h"
#include "util/utils.h"
#include <map>
#include <string>
#include <vector>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

class RestClient
{
public:
    RestClient(){};
    RestClient(const Configuration& conf) : conf(conf){};

    void SetConfiguration(const Configuration &conf)
    {
        this->conf = conf;
    }

    RestResponsePtr DoRequest(
        const std::string &resource,
        const std::string &method,
        const std::map<std::string, std::string> &headers,
        const std::map<std::string, std::string> &params,
        const std::string &body,
        const bool &can_parse_response);

private:
    Configuration conf;
};

typedef std::shared_ptr<RestClient> RestClientPtr;

} // namespace internal
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif