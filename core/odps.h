#ifndef APSARA_ODPS_SDK_ODPS_H
#define APSARA_ODPS_SDK_ODPS_H

#include "configuration.h"
#include "odps_api.h"
#include "rest_client.h"

namespace apsara { namespace odps { namespace sdk {

namespace internal {

class Odps: public IODPS
{
public:
    Odps(const Configuration& conf, const std::string& project = "");

    virtual ~Odps();

    virtual void SetProject(const std::string& project) override;

    virtual const std::string& GetProject() const override;

    virtual void SetEndpoint(const std::string& endpoint) override;

    virtual const std::string& GetEndpoint() const override;

    virtual const Account& GetAccount() const override;

    virtual const Configuration& GetConfiguration() const override;

    virtual IODPSTablesPtr GetTables() const override;

    virtual IODPSResourcePtr CreateResource(
        const std::string& name,
        const IODPSResource::ODPSResourceType& type,
        std::stringstream& content
    ) override;

    virtual IODPSResourcePtr CreateResource(
        const std::string& name,
        std::stringstream& content
    ) override;

    virtual IODPSInstancePtr RecoverInstance(const std::string& instanceId, const std::string& project) override;

    virtual IODPSInstancePtr CreateInstance(const std::string& jobdesc, const std::string& project) override;

    RestClientPtr GetRestClient() const;

private:
    Account account;
    std::string project;
    std::string endpoint;
    Configuration config;
    RestClientPtr restClientPtr;
};

typedef std::shared_ptr<Odps> OdpsPtr;

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif