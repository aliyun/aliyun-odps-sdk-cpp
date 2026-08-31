#ifndef APSARA_ODPS_SDK_SECURITY_MANAGER_H
#define APSARA_ODPS_SDK_SECURITY_MANAGER_H

#include <string>
#include "core/rest_client.h"
#include "core/rest_path.h"



namespace apsara { namespace odps { namespace sdk { namespace internal {

class SecurityManager
{
public:
    virtual ~SecurityManager();
    SecurityManager(const std::string& project, RestClientPtr client);

    std::string GenerateAuthorizationToken(const std::string &policy, const std::string &type);

private:
    RestClientPtr client;
    std::string project;
};

typedef std::shared_ptr<SecurityManager> SecurityManagerPtr;

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif