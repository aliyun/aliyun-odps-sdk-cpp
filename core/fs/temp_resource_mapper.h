#ifndef APSARA_ODPS_SDK_TEMP_RESOURCE_MAPPER_H
#define APSARA_ODPS_SDK_TEMP_RESOURCE_MAPPER_H

#include <string>
#include "util/utils.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal
{

class TempResourceMapper
{
public:
    static std::string GetPathMappingInfo(const std::string& appId);

private:
    static std::map<std::string, std::string> path_mapping;
};

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif