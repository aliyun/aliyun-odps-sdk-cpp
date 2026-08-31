// MaxStorageApi - Table Preview example
//
// Lightweight preview that does not need a session. Optionally restrict to a
// specific partition, project a subset of columns, and cap the row count.
//
// Usage:
//   table_preview <table> [partition] [columns_csv] [limit=10] [schema=default]
//   partition example: "region=east,dt=20250701"
//   columns_csv example: "col1,col2"
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <arrow/api.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "example/common/example_config.h"
#include "include/configuration.h"
#include "include/max_storage_api.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;

static vector<string> SplitCsv(const string& s)
{
    vector<string> out;
    if (s.empty()) return out;
    stringstream ss(s);
    string item;
    while (getline(ss, item, ',')) out.push_back(item);
    return out;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0]
             << " <table> [partition] [columns_csv] [limit=10] [schema=default]"
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
    string table = argv[1];
    string partition = argc > 2 ? argv[2] : "";
    vector<string> columns = SplitCsv(argc > 3 ? argv[3] : "");
    int64_t limit = argc > 4 ? atoll(argv[4]) : 10;
    string schema = argc > 5 ? argv[5] : "default";

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    try
    {
        auto builder = api.BuildTablePreviewStream();
        builder->SetProject(project).SetSchema(schema).SetTable(table).SetLimit(limit);
        if (!partition.empty()) builder->SetPartition(partition);
        if (!columns.empty()) builder->SetColumns(columns);
        auto stream = builder->Build();

        int64_t totalRows = 0;
        while (auto batch = stream->Read())
        {
            totalRows += batch->num_rows();
            cout << "batch rows=" << batch->num_rows()
                 << " cols=" << batch->num_columns() << endl;
            cout << batch->ToString() << endl;
        }
        stream->Close();
        cout << "preview total rows: " << totalRows << endl;
    }
    catch (const OdpsException& e)
    {
        cerr << "OdpsException: " << e.GetErrorCode() << " - " << e.GetErrorMsg()
             << endl;
        return 2;
    }
    return 0;
}
