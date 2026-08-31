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

    string ep = config.mTunnelEndpoint;
    string odpsEndpoint = config.mOdpsEndpoint;
    string project  = config.mProjectName;
    string table  = "<your_table>";
    uint32_t blockId = 0;

    if (argc > 1)
        table = argv[1];

    if (argc > 2)
        blockId = atoi(argv[2]);

    Account account(ACCOUNT_ALIYUN, config.mAccessId, config.mAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(ep);
    conf.SetEndpoint(odpsEndpoint);
    conf.SetUserAgent(UserAgent("UPLOAD_EXAMPLE", "1.0.0.0"));

    OdpsTunnel dt;

    try
    {
        dt.Init(conf);
        IUploadPtr upload = dt.CreateUpload(project, table);
        std::cout << upload->GetStatus() << std::endl;
        IRecordWriterPtr wr = upload->OpenWriter(blockId);
        ODPSTableRecordPtr _r = upload->CreateBufferRecord();
        ODPSTableRecord& r = *_r;

        for (size_t i = 0; i < 100; i++)
        {
            if (i % 10 == 0)
            {
                r.SetNullValue(0);
            }
            else
            {
                r.SetBigIntValue(0, i);
            }
            wr->Write(r);
        }
        wr->Close();
        std::cout << upload->GetStatus() << std::endl;
        std::vector<uint32_t> blocks;
        blocks.push_back(blockId);
        upload->Commit(blocks);
        std::cout << upload->GetStatus() << std::endl;
    }
    catch(OdpsException& e)
    {
        std::cerr << "OdpsTunnelException:\n" << e.what() << std::endl;
    }
}
