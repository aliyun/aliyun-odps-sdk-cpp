// MaxStorageApi - Instance Read example
//
// Run a SQL via ISQLTask, then read its results through InstanceReadSession.
// This is the typical "read SELECT result" path.
//
// Usage:
//   instance_read "<sql>"
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <arrow/api.h>
#include <iostream>
#include <string>

#include "example/common/example_config.h"
#include "include/configuration.h"
#include "include/max_storage_api.h"
#include "include/odps_api.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " \"<sql>\"" << endl;
        cerr << "endpoint / credentials / project are read from conf/testing.conf" << endl;
        return 1;
    }

    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string odpsEndpoint = config.mOdpsEndpoint;
    string tunnelEndpoint = config.mTunnelEndpoint;
    string project = config.mProjectName;
    string sql = argv[1];

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    try
    {
        auto odps = IODPS::Create(conf, project);
        auto sqlTask = ISQLTask::Create();
        auto instance = sqlTask->Run(odps, sql);
        instance->WaitForSuccess(60000);

        cout << "instance id: " << instance->GetInstanceId() << endl;

        auto session = (*api.BuildInstanceReadSession())
                           .SetProject(project)
                           .SetInstance(instance->GetInstanceId())
                           .SetEnableLimit(false)
                           .Build();

        auto splits = session->GetSplits();
        cout << "session id: " << session->GetSessionId()
             << ", record count: " << splits->GetRecordCount() << endl;

        // Read the entire range as a single split.
        auto stream = session->BuildInstanceReadStream()
                          ->SetSplit(splits->GetSplit(0, splits->GetRecordCount()))
                          .Build();

        int64_t totalRows = 0;
        while (auto batch = stream->Read())
        {
            totalRows += batch->num_rows();
            cout << batch->ToString() << endl;
        }
        stream->Close();
        cout << "total rows: " << totalRows << endl;
    }
    catch (const OdpsException& e)
    {
        cerr << "OdpsException: " << e.GetErrorCode() << " - " << e.GetErrorMsg()
             << endl;
        return 2;
    }
    return 0;
}
