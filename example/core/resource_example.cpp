#include "odps_api.h"
#include "example/common/example_config.h"
#include <iostream>
#include <sstream>

using namespace std;
using namespace apsara::odps::sdk;

int main(void)
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, config.mOdpsEndpoint);
    IODPSPtr odps = IODPS::Create(conf, config.mProjectName);
    std::cout << odps->GetProject() << std::endl;

    stringstream ss;
    ss << "This is a test content!";
    std::string name("test_resource_for_sdk");

    IODPSResourcePtr resource = odps->CreateResource(
        name,
        ss
    );
    if (!resource->Exists())
    {
        std::cout << "Resource " + name + " does not exist! Now create!" << std::endl;
        if (resource->Create(false))
        {
            std::cout << "Resource " + name + " create successfully!" << std::endl;
        }
        else
        {
            std::cout << "Resource " + name + " create failed!" << std::endl;
        }

    }
    else
    {
        std::cout << "Resource " + name + " exists! Now update!" << std::endl;
        if (resource->Create(true))
        {
            std::cout << "Resource " + name + " update successfully!" << std::endl;
        }
        else
        {
            std::cout << "Resource " + name + " update failed!" << std::endl;
        }
    }
    return 0;
}
