// MaxStorageApi - Volume FS Read example (new-style volume, path based)
//
// Read a path inside a NEW-type Volume. Optionally seek with offset/count.
//
// Usage:
//   volume_fs_read <volume> <path> [offset=0] [count=0 (=all)]
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
             << " <volume> <path> [offset=0] [count=0 (=all)]" << endl;
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
    int64_t offset = argc > 3 ? atoll(argv[3]) : 0;
    int64_t count = argc > 4 ? atoll(argv[4]) : 0;

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    try
    {
        auto builder = api.BuildVolumeFSReadStream();
        builder->SetProject(project).SetVolume(volume).SetPath(path);
        if (offset > 0) builder->SetOffset(offset);
        if (count > 0) builder->SetCount(count);
        auto stream = builder->Build();

        char buffer[8192];
        int64_t totalBytes = 0;
        string content;
        while (true)
        {
            int64_t n = stream->Read(buffer, sizeof(buffer));
            if (n <= 0) break;
            totalBytes += n;
            content.append(buffer, n);
        }
        stream->Close();
        cout << "read " << totalBytes << " bytes" << endl;
        cout << "content: " << content << endl;
    }
    catch (const OdpsException& e)
    {
        cerr << "OdpsException: " << e.GetErrorCode() << " - " << e.GetErrorMsg()
             << endl;
        return 2;
    }
    return 0;
}
