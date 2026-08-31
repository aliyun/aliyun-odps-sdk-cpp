// MaxStorageApi - Instance Direct Read example (MCQA1)
//
// For MCQA1-style queries you read directly through the
// InstanceDirectReadStream, supplying instance id, task name, query id and
// optional offset/count. This bypasses the read-session step.
//
// Usage:
//   instance_direct_read <instance_id> <query_id>
//                        [task_name=console_sqlrt_task] [offset=0] [count=100]
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <arrow/api.h>
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
             << " <instance_id> <query_id>"
                " [task_name=console_sqlrt_task] [offset=0] [count=100]"
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
    string instanceId = argv[1];
    int64_t queryId = atoll(argv[2]);
    string taskName = argc > 3 ? argv[3] : "console_sqlrt_task";
    int64_t offset = argc > 4 ? atoll(argv[4]) : 0;
    int64_t count = argc > 5 ? atoll(argv[5]) : 100;

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    try
    {
        auto stream = (*api.BuildInstanceDirectReadStream())
                          .SetProject(project)
                          .SetInstance(instanceId)
                          .SetTaskName(taskName)
                          .SetQueryId(queryId)
                          .SetOffset(offset)
                          .SetCount(count)
                          .SetEnableLimit(true)
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
