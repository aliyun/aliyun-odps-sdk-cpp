#include <stdlib.h>
#include <iostream>
#include <time.h>

#include "example/common/example_config.h"
#include "include/odps_tunnel.h"
#include "arrow/api.h"

using namespace std;
using namespace apsara::odps;
using namespace apsara::odps::sdk;

using arrow::DoubleBuilder;
using arrow::FloatBuilder;
using arrow::Int64Builder;
using arrow::Int32Builder;
using arrow::ListBuilder;
using arrow::StringBuilder;
using arrow::BooleanBuilder;


std::string GetRandStr(const int len)
{
    std::string str;
    for (int i = 0; i < len; ++i)
    {
        str.push_back( 'A' + rand()%26 );
    }
    return str;
}

const std::string auto_varyer[] = {
    "6vhEE1n3m55sgaFgy",
    "JWzjl3ZM2nqVmVDek",
    "GcSyznTAQInCcqkyi",
    "fq9vc3jifcQJi5ekq",
    "ive9vVZLOF5qV7JoX",
    "ZwZ3UAyat4oYIsBDL",
    "Q91U7JGEMcF0s8TRf",
    "J0ilO3COScfJtExmn",
    "AVUtzWwmYoQ5BVnYZ",
    "P37FsuIIYAvQedsQ0"
};


int main(int argc, char *argv[])
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string ep = config.mTunnelEndpoint;
    string odpsEndpoint = config.mOdpsEndpoint;
    string project  = config.mProjectName;
    string table  = "<your_table>";
    string partition_spec = "";
    uint32_t blockId = 0;
    int64_t count = 10;

    if (argc > 1)
        table = argv[1];

    if (argc > 2)
        blockId = atoi(argv[2]);

    if (argc > 3)
        count = atoi(argv[3]);

    Account account(ACCOUNT_ALIYUN, config.mAccessId, config.mAccessKey);

    cout << "write count: " << count << endl;

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(ep);
    conf.SetEndpoint(odpsEndpoint);

    OdpsTunnel dt;

    try
    {
        dt.Init(conf);

        IUploadPtr upload = dt.CreateUpload(project, table, partition_spec);

        std::cout << "create session status : " << upload->GetStatus() << std::endl;

        std::shared_ptr<arrow::Schema> schema = upload->GetArrowSchema();

        cout << "creating arrow writer.." << endl;

        IArrowRecordWriterPtr wr = upload->OpenArrowWriter(blockId, CompressOption(CompressOption::ODPS_ZSTD, 1, 1));

        arrow::MemoryPool* pool = arrow::default_memory_pool();

        Int64Builder i64_builder(pool);
        Int32Builder i32_builder(pool);
        FloatBuilder f32_builder(pool);
        DoubleBuilder f64_builder(pool);
        StringBuilder str_builder(pool);
        BooleanBuilder bool_builder(pool);

        std::shared_ptr<arrow::Array> array1;
        std::shared_ptr<arrow::Array> array2;
        std::shared_ptr<arrow::Array> array3;
        std::shared_ptr<arrow::Array> array4;
        std::shared_ptr<arrow::Array> array5;
        std::shared_ptr<arrow::Array> array6;


        for (int64_t j = 0; j < count; j++)
        {
            i64_builder.AppendValues({j});
            i32_builder.AppendValues({(int)j * 2});
            f32_builder.AppendValues({ ((float)j) + 0.5f });
            f64_builder.AppendValues({ ((double)j) + 0.7 });
            str_builder.AppendValues({ auto_varyer[j % 10] });
            bool_builder.AppendValues(std::vector<bool>{ (j % 10) < 5});
        }

        i64_builder.Finish(&array1);
        i32_builder.Finish(&array2);
        f32_builder.Finish(&array3);
        f64_builder.Finish(&array4);
        str_builder.Finish(&array5);
        bool_builder.Finish(&array6);
        std::shared_ptr<arrow::RecordBatch> batch;
        cout << "making batch.." << endl;
        batch = arrow::RecordBatch::Make(schema, array1->length(), {array1, array2, array3, array4, array5, array6});       //create a record batch

        clock_t startTime,endTime;
        cout << "starting write.." << endl;
        startTime = clock();
        wr->Write(*batch);

        wr->Close();
        std::cout << "upload status :" << upload->GetStatus() << std::endl;
        std::vector<uint32_t> blocks;
        blocks.push_back(blockId);
        upload->Commit(blocks);
        std::cout << "commit status: " << upload->GetStatus() << std::endl;
        endTime = clock();
        std::cout << "STAT: UPLOAD TIME " <<(double)(endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
    }
    catch(OdpsTunnelException& e)
    {
        std::cerr << "OdpsTunnelException:\n" << e.what() << std::endl;
    }
}
