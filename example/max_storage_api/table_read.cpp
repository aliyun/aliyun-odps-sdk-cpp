// MaxStorageApi - Table Read example
//
// Build a TableReadSession, split the table by SIZE, then iterate every split
// and read Arrow RecordBatches via TableReadStream.
//
// Usage:
//   table_read <table> [schema=default]
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <arrow/api.h>
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
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <table> [schema=default]" << endl;
        cerr << "endpoint / credentials / project are read from conf/testing.conf" << endl;
        return 1;
    }

    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string odpsEndpoint = config.mOdpsEndpoint;
    string tunnelEndpoint = config.mTunnelEndpoint;
    string project = config.mProjectName;
    string table = argv[1];
    string schema = argc > 2 ? argv[2] : "default";

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::SIZE;
    splitOptions.mSplitSize = 256LL * 1024 * 1024;

    ReadOptions readOptions;
    readOptions.mMaxBatchRows = 4096;

    try
    {
        auto session = (*api.BuildTableReadSession())
                           .SetProject(project)
                           .SetSchema(schema)
                           .SetTable(table)
                           .SetSplitOptions(splitOptions)
                           .Build();

        cout << "session id: " << session->GetSessionId() << endl;

        auto splits = session->GetSplits();
        cout << "split count: " << splits->GetSplitCount()
             << ", record count: " << splits->GetRecordCount() << endl;

        int64_t totalRows = 0;
        for (int32_t i = 0; i < splits->GetSplitCount(); ++i)
        {
            auto stream = session->BuildTableReadStream()
                              ->SetSplit(splits->GetSplit(i))
                              .SetReadOptions(readOptions)
                              .Build();

            while (auto batch = stream->Read())
            {
                totalRows += batch->num_rows();
            }
            stream->Close();
            cout << "split " << i << " done, rows so far: " << totalRows << endl;
        }
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
