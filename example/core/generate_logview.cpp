#include "odps_api.h"
#include "example/common/example_config.h"
#include <iostream>

using namespace std;
using namespace apsara::odps::sdk;

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: generate_logview <instance_id>" << std::endl;
        std::cout << "endpoint / credentials / project are read from conf/testing.conf"
                  << std::endl;
        return 0;
    }

    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string instance_id(argv[1]);

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, config.mOdpsEndpoint);
    IODPSPtr odps = IODPS::Create(conf, config.mProjectName);

    IODPSInstancePtr instance = odps->RecoverInstance(instance_id);
    std::string logView = instance->GenerateLogView(24);
    std::cout << logView << std::endl;

    return 0;
}
