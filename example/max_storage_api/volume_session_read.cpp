// MaxStorageApi - Volume Session Read example (old-style volume)
//
// Read a (volume, partition, file) tuple from an OLD-type Volume. The read
// goes through a session; offset/count is supported on the read stream.
//
// Usage:
//   volume_session_read <volume> <partition> <file> [offset=0] [count=0 (=all)]
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
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0]
             << " <volume> <partition> <file> [offset=0] [count=0 (=all)]"
             << endl;
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
    int64_t offset = argc > 4 ? atoll(argv[4]) : 0;
    int64_t count = argc > 5 ? atoll(argv[5]) : 0;

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    try
    {
        auto session = (*api.BuildVolumeReadSession())
                           .SetProject(project)
                           .SetVolume(volume)
                           .SetPartition(partition)
                           .SetFile(file)
                           .Build();
        cout << "session id: " << session->GetSessionId()
             << ", status: " << session->GetStatus() << endl;

        auto streamBuilder = session->BuildVolumeReadStream();
        if (offset > 0) streamBuilder->SetOffset(offset);
        if (count > 0) streamBuilder->SetCount(count);
        auto stream = streamBuilder->Build();

        char buffer[8192];
        string content;
        int64_t totalBytes = 0;
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
