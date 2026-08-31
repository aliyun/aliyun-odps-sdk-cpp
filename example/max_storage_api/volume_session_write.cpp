// MaxStorageApi - Volume Session Write example (old-style volume)
//
// For OLD-type Volumes you address files by (volume, partition, file). The
// write goes through a session that must be Commit()ed (or Abort()ed) at the
// end.
//
// Usage:
//   volume_session_write <volume> <partition> <file>
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <iostream>
#include <string>

#include "example/common/example_config.h"
#include "include/configuration.h"
#include "include/max_storage_api.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0]
             << " <volume> <partition> <file>" << endl;
        cerr << "endpoint / credentials / project are read from conf/testing.conf" << endl;
        return 1;
    }

    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string odpsEndpoint = config.mOdpsEndpoint;
    string tunnelEndpoint = config.mTunnelEndpoint;
    string project = config.mProjectName;
    string volume = argv[1];
    string partition = argv[2];
    string file = argv[3];

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    const string payload = "hello,volume_session_write!";

    try
    {
        auto session = (*api.BuildVolumeWriteSession())
                           .SetProject(project)
                           .SetVolume(volume)
                           .SetPartition(partition)
                           .Build();
        cout << "session id: " << session->GetSessionId()
             << ", status: " << session->GetStatus() << endl;

        auto stream = session->BuildWriteStream()->SetFile(file).Build();
        stream->Write(payload.data(), payload.size());
        stream->Close();

        session->Commit();
        cout << "commit ok, wrote " << payload.size() << " bytes to " << file
             << endl;
    }
    catch (const OdpsException& e)
    {
        cerr << "OdpsException: " << e.GetErrorCode() << " - " << e.GetErrorMsg()
             << endl;
        return 2;
    }
    return 0;
}
