#include <stdlib.h>
#include <iostream>

#include "example/common/example_config.h"
#include "include/odps_tunnel.h"

using namespace std;
using namespace apsara::odps;
using namespace apsara::odps::sdk;


int main(int argc, char *argv[])
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string tunnelEp = config.mTunnelEndpoint;
    string project = config.mProjectName;
    string table = "<your_table>";

    if (argc > 1)
        table = argv[1];

    Account account(ACCOUNT_ALIYUN, config.mAccessId, config.mAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(tunnelEp);
    conf.SetUserAgent(UserAgent("UPSERT_EXAMPLE", "1.0.0.0"));

    OdpsTunnel dt;

    try
    {
        dt.Init(conf);
        IUpsertPtr upsert = dt.CreateUpsert(project, table);
        std::cout << upsert->GetStatus() << std::endl;
        IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

        ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
        r->SetStringValue(0, "0");
        r->SetStringValue(1, "v1");
        upsertStream->Upsert(*r);

        r->SetStringValue(0, "1");
        r->SetStringValue(1, "v1");
        upsertStream->Upsert(*r);

        r->SetStringValue(0, "2");
        r->SetStringValue(1, "v1");
        upsertStream->Upsert(*r);
        upsertStream->Delete(*r);

        upsertStream->Flush();
        upsertStream->Close();
        upsert->Commit(false);

        std::cout << upsert->GetStatus() << std::endl;
    }
    catch(OdpsException& e)
    {
        std::cerr << "OdpsTunnelException:\n" << e.what() << std::endl;
    }
}
