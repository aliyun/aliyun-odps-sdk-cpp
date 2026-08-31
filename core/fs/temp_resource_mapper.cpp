#include "temp_resource_mapper.h"


namespace apsara { namespace odps { namespace sdk { namespace internal {

std::map<std::string, std::string> TempResourceMapper::path_mapping;

std::string TempResourceMapper::GetPathMappingInfo(const std::string& appId)
{
    std::map<std::string, std::string> filterd_path;
    if (appId != "")
    {
        std::map<std::string, std::string>::iterator it;
        for (it = path_mapping.begin(); it != path_mapping.end(); it++)
        {
            filterd_path[it->first] = it->second;
        }
    }
    else
    {
        return "";
    }

    std::stringstream ss;
    std::map<std::string, std::string>::iterator it;
    for (it = filterd_path.begin(); it != filterd_path.end(); it++)
    {
        ss << it->first << "=" << it->second << "\n";
        path_mapping.erase(it->first);
    }

    return util::Base64Encode(ss.str());
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
