#include "odps.h"
#include "instance.h"
#include "tables.h"
#include "resource.h"

namespace apsara { namespace odps { namespace sdk {

std::shared_ptr<IODPS> IODPS::Create(const Configuration& conf, const std::string& project)
{
    return std::make_shared<internal::Odps>(conf, project);
}

namespace internal {

Odps::Odps(const Configuration& conf,
        const std::string& _project):
    account(conf.GetAccount()),
    project(_project),
    endpoint(conf.GetEndpoint()),
    config(conf)
{

    if (project != "")
    {
        this->config.SetDefaultProject(project);
    }

    this->restClientPtr = std::make_shared<RestClient>(this->config);
}

Odps::~Odps()
{
}

const Account& Odps::GetAccount() const
{
    return this->account;
}

const Configuration& Odps::GetConfiguration() const
{
    return this->config;
}

void Odps::SetProject(const std::string& project)
{
    this->project = project;
    this->config.SetDefaultProject(project);
}

void Odps::SetEndpoint(const std::string& endpoint)
{
    this->endpoint = endpoint;
    this->config.SetEndpoint(endpoint);
}

RestClientPtr Odps::GetRestClient() const
{
    return this->restClientPtr;
}

const std::string& Odps::GetProject() const
{
    return this->project;
}

const std::string& Odps::GetEndpoint() const
{
    return this->endpoint;
}

IODPSInstancePtr Odps::RecoverInstance(const std::string &instanceId, const std::string& project)
{
    std::string tmpProject = project;
    if (tmpProject.empty())
    {
        tmpProject = GetProject();
    }
    auto result = std::make_shared<Instance>(GetConfiguration(), tmpProject);
    result->SetInstanceId(instanceId);
    return result;
}

IODPSInstancePtr Odps::CreateInstance(const std::string& jobdesc, const std::string& project)
{
    std::string tmpProject = project;
    if (tmpProject.empty())
    {
        tmpProject = GetProject();
    }
    return std::make_shared<Instance>(GetConfiguration(), tmpProject, jobdesc);
}

IODPSTablesPtr Odps::GetTables() const
{
    return std::make_shared<ODPSTables>(config, project);
}


IODPSResourcePtr Odps::CreateResource(
        const std::string& name,
        const IODPSResource::ODPSResourceType& type,
        std::stringstream& content)
{
    return std::make_shared<Resource>(
        std::make_shared<Odps>(this->config, this->project),
        name,
        type,
        content
    );
}

IODPSResourcePtr Odps::CreateResource(
        const std::string& name,
        std::stringstream& content)
{
    return CreateResource(name, IODPSResource::ODPSResourceType::FILE, content);
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
