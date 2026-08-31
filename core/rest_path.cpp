#include "rest_path.h"
#include <sstream>

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal {
const static std::string PROJECTS = "projects";
const static std::string RESOURCES = "resources";
const static std::string INSTANCES = "instances";
const static std::string AUTH = "authorization";
const static std::string TABLES = "tables";

std::string RestResourceBuilder::BuildResourceRest(const std::string& project, const std::string& resourceName)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << RESOURCES << "/" << resourceName;
    std::string resource = oss.str();
    return resource;
}

std::string RestResourceBuilder::BuildResourcesRest(const std::string& project)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << RESOURCES;
    std::string resource = oss.str();
    return resource;
}

std::string RestResourceBuilder::BuildInstancesRest(const std::string& project)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << INSTANCES;
    std::string resource = oss.str();
    return resource;
}

std::string RestResourceBuilder::BuildInstanceRest(const std::string& project, const std::string& instance)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << INSTANCES << "/" << instance;
    std::string resource = oss.str();
    return resource;
}

std::string RestResourceBuilder::BuildAuthorizationRest(const std::string& project)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << AUTH;
    std::string resource = oss.str();
    return resource;
}

std::string RestResourceBuilder::BuildTablesRest(const std::string& project)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << TABLES;
    std::string resource = oss.str();
    return resource;
}

std::string RestResourceBuilder::BuildTableRest(const std::string& project, const std::string& tableName)
{
    std::ostringstream oss;
    oss << PROJECTS << "/" << project;
    oss << "/" << TABLES << "/" << tableName;
    std::string resource = oss.str();
    return resource;
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara