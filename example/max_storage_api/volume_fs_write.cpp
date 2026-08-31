// MaxStorageApi - Volume FS Write example (new-style volume, path based)
//
// Write a small payload to a path inside a NEW-type Volume. No session needed.
//
// Usage:
//   volume_fs_write <volume> <path> [replica_count=0]
//   path example: "/myvolume/dir/file.txt"
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <cstdlib>
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
    if (argc < 3)
    {
        cerr << "Usage: " << argv[0]
             << " <volume> <path> [replica_count=0]" << endl;
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
    string path = argv[2];
    int64_t replicaCount = argc > 3 ? atoll(argv[3]) : 0;

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    const string payload = "hello,max_storage_api,volume_fs_write!";

    try
    {
        auto builder = api.BuildVolumeFSWriteStream();
        builder->SetProject(project).SetVolume(volume).SetPath(path);
        if (replicaCount > 0) builder->SetReplicaCount(replicaCount);
        auto stream = builder->Build();

        stream->Write(payload.data(), payload.size());
        stream->Close();
        cout << "wrote " << payload.size() << " bytes to " << path << endl;
    }
    catch (const OdpsException& e)
    {
        cerr << "OdpsException: " << e.GetErrorCode() << " - " << e.GetErrorMsg()
             << endl;
        return 2;
    }
    return 0;
}
