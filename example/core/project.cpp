#include "odps_api.h"
#include "example/common/example_config.h"
#include <iostream>

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
    return 0;
}
