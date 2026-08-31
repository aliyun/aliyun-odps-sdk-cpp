// MaxStorageApi - Table Write example
//
// Build a TableWriteSession, write one Arrow RecordBatch through a
// (streamId, streamVersion) write stream, then commit the session.
//
// The target table is expected to have schema compatible with:
//     col1 BIGINT, col2 STRING
//
// Usage:
//   table_write <table> [partition_spec] [schema=default]
//   partition_spec example: "p1=a/p2=b" (use "" for non-partitioned tables)
//   (endpoint / credentials / project are read from conf/testing.conf)

#include <arrow/api.h>
#include <iostream>
#include <memory>
#include <string>

#include "example/common/example_config.h"
#include "include/configuration.h"
#include "include/max_storage_api.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;

static shared_ptr<arrow::RecordBatch> BuildSampleBatch(int64_t count)
{
    auto pool = arrow::default_memory_pool();
    arrow::Int64Builder col1(pool);
    arrow::StringBuilder col2(pool);
    for (int64_t i = 0; i < count; ++i)
    {
        col1.Append(i).ok();
        col2.Append("row_" + to_string(i)).ok();
    }
    shared_ptr<arrow::Array> a1, a2;
    col1.Finish(&a1).ok();
    col2.Finish(&a2).ok();

    auto schema = arrow::schema({
        arrow::field("col1", arrow::int64()),
        arrow::field("col2", arrow::utf8()),
    });
    return arrow::RecordBatch::Make(schema, count, {a1, a2});
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0]
             << " <table> [partition_spec] [schema=default]" << endl;
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
    string partitionSpec = argc > 2 ? argv[2] : "";
    string schema = argc > 3 ? argv[3] : "default";

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, odpsEndpoint);
    conf.SetTunnelEndpoint(tunnelEndpoint);

    MaxStorageApi api;
    api.Init(conf);

    try
    {
        auto session = (*api.BuildTableWriteSession())
                           .SetProject(project)
                           .SetSchema(schema)
                           .SetTable(table)
                           .SetPartitionSpec(partitionSpec)
                           .Build();
        cout << "write session id: " << session->GetID() << endl;

        auto stream = session->BuildWriteStream()
                          ->SetStreamId("example-stream-1")
                          .SetStreamVersion(1)
                          .Build();

        auto batch = BuildSampleBatch(100);
        stream->Write(*batch);
        stream->Close();
        cout << "wire bytes: " << stream->GetWireBytes() << endl;

        session->Commit({});
        cout << "commit ok" << endl;
    }
    catch (const OdpsException& e)
    {
        cerr << "OdpsException: " << e.GetErrorCode() << " - " << e.GetErrorMsg()
             << endl;
        return 2;
    }
    return 0;
}
