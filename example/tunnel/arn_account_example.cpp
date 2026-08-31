#include <stdlib.h>
#include <iostream>
#include <memory>

#include "example/common/example_config.h"
#include "include/odps_tunnel.h"
#include "include/credentials_provider.h"

using namespace std;
using namespace apsara::odps::sdk;

// 用户自定义凭证提供者:可从任意来源(ARN/STS/元数据服务)返回凭证
class MyCredentialsProvider : public CredentialsProvider
{
public:
    MyCredentialsProvider(const std::string& accessId, const std::string& accessKey)
        : mAccessId(accessId), mAccessKey(accessKey) {}

    Credentials getCredentials() override
    {
        return Credentials(mAccessId, mAccessKey);
    }

private:
    std::string mAccessId;
    std::string mAccessKey;
};

int main(int argc, char *argv[])
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string ep = config.mTunnelEndpoint;

    if (argc > 1)
        ep = argv[1];

    try
    {
        // 凭证从 conf/testing.conf 读入;实际使用时可在 getCredentials()
        // 里从 ARN/STS 等来源动态获取
        CredentialsProviderPtr provider =
            std::make_shared<MyCredentialsProvider>(config.mAccessId,
                                                    config.mAccessKey);
        Account account(provider);           // ARN 账号(动态凭证)

        Configuration conf;
        conf.SetAccount(account);
        conf.SetTunnelEndpoint(ep);

        OdpsTunnel tunnel;
        tunnel.Init(conf);

        std::cout << "credentials provider example ok" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
