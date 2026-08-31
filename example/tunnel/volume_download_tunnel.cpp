#include <iostream>
#include <string>

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
    config.SetUserAgent(UserAgent("VOLUME_DOWNLOAD_EXAMPLE", "1.0.0.0"));

    OdpsTunnel dt;

    dt.Init(config);

    IVolumeDownloadPtr vd = dt.CreateVolumeDownload(project, volume, partition, fileName);
    std::string downloadId = vd->GetDownloadId();
    std::cout<<"DownloadId : "<<vd->GetDownloadId()<<std::endl;
    std::cout<<"Status : "<<vd->GetStatus()<<std::endl;
    std::cout<<"FileLength : "<<vd->GetFileLength()<<std::endl;
    char buffer[1024];
    int n;
    IVolumeInputStreamPtr vr = vd->OpenInputStream();
    while ( (n = vr->Read(buffer, 1024)) > 0)
    {
        buffer[n] = '\0';
        std::cout<<buffer;
    }

    vd->Complete();
    return 0;
}
