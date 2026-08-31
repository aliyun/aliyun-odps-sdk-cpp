#ifndef APSARA_ODPS_SDK_REST_PATH_H
#define APSARA_ODPS_SDK_REST_PATH_H

#include <string>

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal
{

class RestResourceBuilder
{
public:
    static std::string BuildResourceRest(const std::string& project, const std::string& resourceName);

    static std::string BuildResourcesRest(const std::string& project);

    static std::string BuildInstancesRest(const std::string& project);

    static std::string BuildInstanceRest(const std::string& project, const std::string& instance);

    static std::string BuildAuthorizationRest(const std::string& project);

    static std::string BuildTablesRest(const std::string& project);

    static std::string BuildTableRest(const std::string& project, const std::string& tableName);

};

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif