#include <stdlib.h>
#include <iostream>
#include <time.h>
#include "example/common/example_config.h"
#include "include/odps_tunnel.h"
#include <arrow/api.h>

using namespace std;
using namespace arrow;
using namespace apsara::odps::sdk;

void GenericPrintArrowBatch(shared_ptr<arrow::RecordBatch> batch)
{
    int64_t rowcount = batch->num_rows();
    int columncount = batch->num_columns();
    for (auto fname: batch->schema()->field_names())
    {
        cout << fname << " ";
    }
    cout << endl;
    for (int64_t i = 0; i < rowcount; i++)
    {
        // colnames
        for(int c = 0; c < columncount; c++)
        {
            if (c != 0)
            {
                cout << " ";
            }
            arrow::Type::type currentType = batch->schema()->field(c)->type()->id();
            if (currentType == Type::INT8)
            {
                cout << static_pointer_cast<Int8Array>(batch->column(c))->Value(i);
            }
            else if (currentType == Type::INT16)
            {
                cout << static_pointer_cast<Int16Array>(batch->column(c))->Value(i);
            }
            else if (currentType == Type::INT32)
            {
                cout << static_pointer_cast<Int32Array>(batch->column(c))->Value(i);
            }
            else if (currentType == Type::INT64)
            {
                cout << static_pointer_cast<Int64Array>(batch->column(c))->Value(i);
            }
            else if (currentType == Type::FLOAT)
            {
                cout << static_pointer_cast<FloatArray>(batch->column(c))->Value(i);
            }
            else if (currentType == Type::DOUBLE)
            {
                cout << static_pointer_cast<DoubleArray>(batch->column(c))->Value(i);
            }
            else if (currentType == Type::STRING)
            {
                cout << static_pointer_cast<StringArray>(batch->column(c))->GetString(i);
            }
            else if (currentType == Type::BOOL)
            {
                cout << static_pointer_cast<BooleanArray>(batch->column(c))->Value(i);
            }
            else
            {
                cout << "<unprintable>";
            }
        }
        cout << endl;
    }
}

int main(int argc, char *argv[])
{
    struct timespec task_time_start, task_time_end;
    unsigned long task_time_diff_ns;
    clock_gettime(CLOCK_MONOTONIC, &task_time_start);


    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string ep = config.mTunnelEndpoint;
    string odpsEndpoint = config.mOdpsEndpoint;
    string project  = config.mProjectName;
    string table  = "<your_table>";
    uint64_t start = 0;
    uint64_t count = 1000000;
    string partition_spec = "";

    if (argc > 1)
        table = argv[1];

    if (argc > 2)
        partition_spec = argv[2];

    if (argc > 3)
        start = atoi(argv[3]);

    if (argc > 4)
        count = atoi(argv[4]);

    Account account(ACCOUNT_ALIYUN, config.mAccessId, config.mAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(ep);
    conf.SetEndpoint(odpsEndpoint);

    OdpsTunnel dt;

    try
    {
        dt.Init(conf);

        IDownloadPtr download = dt.CreateDownload(project, table, partition_spec);

        std::string downloadId = download->GetDownloadId();

        std::cout << "downloadId:" << downloadId << std::endl;
        std::cout << "create session status: " << download->GetStatus() << std::endl;

        IArrowRecordReaderPtr rr = download->OpenArrowReader(start, count, {}, CompressOption(CompressOption::ODPS_ZSTD, 1, 1));

        std::shared_ptr<arrow::RecordBatch> r;

        int64_t count = 0;
        while(rr->Read(r))
        {
            int rows = r->num_rows();
            GenericPrintArrowBatch(r);
            count += rows;
        }
        rr->Close();
        download->Complete();
        std::cout << count << endl;
    }
    catch(OdpsTunnelException& e)
    {
        std::cerr << "OdpsTunnelException:\n" << e.what() << std::endl;
    }

    clock_gettime(CLOCK_MONOTONIC, &task_time_end);
    task_time_diff_ns = ((task_time_end.tv_sec * 1000000000) + task_time_end.tv_nsec) -((task_time_start.tv_sec * 1000000000) + task_time_start.tv_nsec);

    std::cout << "Totle Time : " <<  (double)task_time_diff_ns/1000000000 << "s" << endl;
}
