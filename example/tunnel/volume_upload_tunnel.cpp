#include <iostream>
#include <string>
#include <sstream>

#include "example/common/example_config.h"
#include "include/odps_tunnel.h"
#include "include/configuration.h"

using namespace std;
using namespace apsara::odps::sdk;

int main(int argc, char *argv[])
{
    ExampleConfig exampleConfig;
    if (!LoadExampleConfig(exampleConfig))
        return 1;

    string endpoint = exampleConfig.mTunnelEndpoint;
    string project  = exampleConfig.mProjectName;
    string volume = "<volume name>";
    string partition = "<partition name>";
    string fileName = "<file name>";
    string odpsEndpoint = exampleConfig.mOdpsEndpoint;

    if (argc > 1)
        volume = argv[1];

    if (argc > 2)
        partition = argv[2];

    if (argc > 3)
        fileName = argv[3];

    Account account(ACCOUNT_ALIYUN, exampleConfig.mAccessId, exampleConfig.mAccessKey);

    Configuration config(account, odpsEndpoint);
    config.SetTunnelEndpoint(endpoint);
    config.SetUserAgent(UserAgent("VOLUME_UPLOAD_EXAMPLE", "1.0.0.0"));

    OdpsTunnel dt;
    dt.Init(config);

    IVolumeUploadPtr vu = dt.CreateVolumeUpload(project, volume, partition);
    std::string uploadId = vu->GetUploadId();
    std::cout<<"UploadId : "<<vu->GetUploadId()<<std::endl;
    std::cout<<"Status : "<<vu->GetStatus()<<std::endl;

    stringstream ss;
    for (int i = 0; i < 100; i++)
    {
        ss<<i<<endl;
    }

    IVolumeOutputStreamPtr vw = vu->OpenOutputStream(fileName);
    vw->Write(ss.str().data(), ss.str().size());
    vw->Close();

    std::vector<std::string> fileList(1, fileName);
    vu->Commit(fileList);

    return 0;
}
