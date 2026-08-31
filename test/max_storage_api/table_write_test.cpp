#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "arrow/api.h"
#include "gtest/gtest.h"
#include "include/configuration.h"
#include "include/odps_api.h"
#include "max_storage_api.h"
#include "test/common/test_util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;

class TableWriteTest : public ::testing::Test
{
protected:
    std::string mProjectName;
    std::string mTableName;
    std::string mPartitionedTableName;
    MaxStorageApi mMaxStorageApi;
    IODPSPtr mOdpsClient;
    ISQLTaskPtr mSqlTask;

    void SetUp() override
    {
        mProjectName = Utils::GetProjectName();
        mTableName = Utils::GetRandomTableName() + "_table_write";
        mPartitionedTableName = Utils::GetRandomTableName() + "_table_write_partitioned_table";
        Configuration config = Utils::GetConfiguration();
        AliyunAccount account(config.GetAccount().GetId(), config.GetAccount().GetKey(), "test_region");
        config.SetAccount(account);
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
        UserAgent userAgent("TableWriteTest", "1.0");
        userAgent.properties["tag"] = "default";
        config.SetUserAgent(userAgent);
        mOdpsClient = IODPS::Create(config, mProjectName);
        mSqlTask = ISQLTask::Create();
        mMaxStorageApi.Init(config);
    }

    void TearDown() override {
        DropTableIfExists(mTableName);
        DropTableIfExists(mPartitionedTableName);
    }

    void ExecuteSql(const std::string& sql)
    {
        IODPSInstancePtr instancePtr = mSqlTask->Run(mOdpsClient, sql, {
            {"odps.table.append2.enable", "true"},
            {"odps.sql.type.system.odps2", "true"},
        });
        try
        {
            instancePtr->WaitForSuccess(60000);
        }
        catch (const OdpsException& e)
        {
            std::cerr << "SQL execution failed: " << e.GetErrorCode() << " - "
                      << e.GetErrorMsg() << std::endl;
            throw;
        }
    }

    std::shared_ptr<arrow::RecordBatch> ExecuteSelect(const std::string& sql)
    {
        IODPSInstancePtr instancePtr = mSqlTask->Run(mOdpsClient, sql);
        try
        {
            instancePtr->WaitForSuccess(60000);
        }
        catch (const OdpsException& e)
        {
            std::cerr << "SQL execution failed: " << e.GetErrorCode() << " - "
                      << e.GetErrorMsg() << std::endl;
            throw;
        }

        auto is = mMaxStorageApi.BuildInstanceReadSession()->SetInstance(instancePtr->GetInstanceId())
            .SetProject(mOdpsClient->GetProject()).SetEnableLimit(false).Build();
        auto ir = is->BuildInstanceReadStream()->SetSplit(is->GetSplits()->GetSplit(0, 9999999)).Build();
        return ir->Read();
    }

    void DropTableIfExists(const std::string& tableName)
    {
        std::string dropSql = "drop table if exists " + tableName + ";";
        ExecuteSql(dropSql);
    }

    std::shared_ptr<arrow::RecordBatch> GenerateDataSimple(size_t count)
    {
        auto schema = arrow::schema({
            arrow::field("col1", arrow::int64()),
            arrow::field("col2", arrow::utf8()),
        });
        auto builder1 = std::make_shared<arrow::Int64Builder>(arrow::system_memory_pool());
        auto builder2 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());

        for (size_t i = 0; i < count; i++)
        {
            builder1->Append(i).ok();
            builder2->Append("string_" + std::to_string(i)).ok();
        }

        std::shared_ptr<arrow::Array> a1, a2;
        builder1->Finish(&a1).ok();
        builder2->Finish(&a2).ok();

        return arrow::RecordBatch::Make(schema, a1->length(), {a1, a2});
    }

protected:
    void CreateTestTable(std::string secondaryType = "string")
    {
        try
        {
            DropTableIfExists(mTableName);

            std::string createSql =
                "create table " + mTableName +
                " (col1 bigint, col2 " + secondaryType + ");";

            ExecuteSql(createSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create test table: " << e.what()
                      << std::endl;
            throw;
        }
    }

    void CreateTestTableAP2()
    {
        try
        {
            DropTableIfExists(mTableName);

            std::string createSql =
                "create table " + mTableName +
                " (col1 bigint, col2 string) tblproperties (\"table.format.version\"=\"2\");";

            ExecuteSql(createSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create test table: " << e.what()
                      << std::endl;
            throw;
        }
    }

    void CreatePartitionedTestTable(std::string secondaryType = "string")
    {
        try
        {
            DropTableIfExists(mPartitionedTableName);

            std::string createSql =
                "create table " + mPartitionedTableName +
                " (col1 bigint, col2 " + secondaryType + ") "
                "partitioned by (p1 string, p2 string);";

            ExecuteSql(createSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create partitioned test table: " << e.what()
                      << std::endl;
            throw;
        }
    }

};

TEST_F(TableWriteTest, TestUpsert)
{
    const std::string& sql = "create table " + mTableName +
                " (key bigint not null, value bigint, primary key(key)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\", \"acid.partial.fields.update.enable\"=\"true\");";
    ExecuteSql(sql);
    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .Build();
    auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();

    auto schema = stream->GetArrowSchema();
    auto keyBuilder = std::make_shared<arrow::Int64Builder>(arrow::system_memory_pool());
    auto valBuilder = std::make_shared<arrow::Int64Builder>(arrow::system_memory_pool());
    auto opBuilder = std::make_shared<arrow::Int8Builder>(arrow::system_memory_pool());
    auto valsBuilder = std::make_shared<arrow::ListBuilder>(arrow::system_memory_pool(), std::make_shared<arrow::Int64Builder>());

    for (size_t i = 0; i < 1024; i++)
    {
        keyBuilder->Append(i % 128).ok();
        valBuilder->Append(i).ok();
        opBuilder->Append(1).ok();
        valsBuilder->Append().ok();
    }

    std::shared_ptr<arrow::Array> keyArray, valArray, opArray, valsArray;
    keyBuilder->Finish(&keyArray).ok();
    valBuilder->Finish(&valArray).ok();
    opBuilder->Finish(&opArray).ok();
    valsBuilder->Finish(&valsArray).ok();

    auto records = arrow::RecordBatch::Make(schema, 1024, {keyArray, valArray, opArray, valsArray});

    stream->Write(*records);
    stream->Close();
    tableWriteSession->Commit({});

    auto verify = ExecuteSelect("select * from " + mProjectName + "." + mTableName + ";");

    EXPECT_EQ(128, verify->num_rows());
    EXPECT_EQ(2, verify->num_columns());
    for (int i = 0; i < verify->num_rows(); i++)
    {
        EXPECT_EQ(verify->column_name(0), "key");
        EXPECT_EQ(verify->column_name(1), "value");

        // 验证value值是否符合预期公式: value == 1024 - 128 + key
        auto key_array = std::static_pointer_cast<arrow::Int64Array>(verify->column(0));
        auto value_array = std::static_pointer_cast<arrow::Int64Array>(verify->column(1));

        int64_t key = key_array->Value(i);
        int64_t value = value_array->Value(i);

        EXPECT_EQ(value, 1024 - 128 + key);
    }
}

TEST_F(TableWriteTest, TestTableWriteSimple)
{
    CreateTestTable();
    auto recordBatch = GenerateDataSimple(10);

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .Build();

    auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();
    stream->Write(*recordBatch);
    stream->Close();
    tableWriteSession->Commit({});
    // verify it
    auto verify = ExecuteSelect("select * from " + mProjectName + "." + mTableName + ";");
    ASSERT_TRUE(recordBatch->Equals(*verify)) << "expect: " << recordBatch->ToString() << " got " << verify->ToString();
}

TEST_F(TableWriteTest, TestTableWriteFullPartitioned)
{
    CreatePartitionedTestTable();
    auto recordBatch = GenerateDataSimple(10);

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetPartitionSpec("p1=aaa/p2=bbb")
                                .Build();

    auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();
    stream->Write(*recordBatch);
    stream->Close();
    tableWriteSession->Commit({});
    // verify it
    auto verify = ExecuteSelect("select col1, col2 from " + mProjectName + "." + mPartitionedTableName + " where p1='aaa' and p2='bbb';");
    ASSERT_TRUE(recordBatch->Equals(*verify)) << "expect: " << recordBatch->ToString() << " got " << verify->ToString();
}

TEST_F(TableWriteTest, TestTableWritePartiallyPartitioned)
{
    CreatePartitionedTestTable();

    auto builder1 = std::make_shared<arrow::Int64Builder>(arrow::system_memory_pool());
    auto builder2 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());
    auto builder3 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());

    for (size_t i = 0; i < 5; i++)
    {
        builder1->Append(i).ok();
        builder2->Append("string_" + std::to_string(i)).ok();
        builder3->Append("bbb").ok();
    }

    std::shared_ptr<arrow::Array> a1, a2, a3;
    builder1->Finish(&a1).ok();
    builder2->Finish(&a2).ok();
    builder3->Finish(&a3).ok();

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetPartitionSpec("p1=aaa")
                                .Build();

    auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();

    auto recordBatch = arrow::RecordBatch::Make(stream->GetArrowSchema(), a1->length(), {a1, a2, a3});

    stream->Write(*recordBatch);
    stream->Close();
    tableWriteSession->Commit({});
    // verify it
    auto verify = ExecuteSelect("select col1, col2, p2 from " + mProjectName + "." + mPartitionedTableName + " where p1='aaa';");
    ASSERT_TRUE(recordBatch->Equals(*verify)) << "expect: " << recordBatch->ToString() << " got " << verify->ToString();
}

TEST_F(TableWriteTest, TestTableWriteDP)
{
    CreatePartitionedTestTable();

    auto builder1 = std::make_shared<arrow::Int64Builder>(arrow::system_memory_pool());
    auto builder2 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());
    auto builder3 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());
    auto builder4 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());

    for (size_t i = 0; i < 5; i++)
    {
        builder1->Append(i).ok();
        builder2->Append("string_" + std::to_string(i)).ok();
        builder3->Append("aaa").ok();
        builder4->Append("bbb").ok();
    }
    for (size_t i = 0; i < 5; i++)
    {
        builder1->Append(i).ok();
        builder2->Append("string_" + std::to_string(i)).ok();
        builder3->Append("ccc").ok();
        builder4->Append("ddd").ok();
    }

    std::shared_ptr<arrow::Array> a1, a2, a3, a4;
    builder1->Finish(&a1).ok();
    builder2->Finish(&a2).ok();
    builder3->Finish(&a3).ok();
    builder4->Finish(&a4).ok();

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetPartitionSpec("")
                                .Build();

    auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();

    auto recordBatch = arrow::RecordBatch::Make(stream->GetArrowSchema(), a1->length(), {a1, a2, a3, a4});

    stream->Write(*recordBatch);
    stream->Close();
    tableWriteSession->Commit({});
    // verify it
    auto verify = ExecuteSelect("select col1, col2, p1, p2 from " + mProjectName + "." + mPartitionedTableName + " sort by p1, p2;");
    ASSERT_TRUE(recordBatch->Equals(*verify)) << "expect: " << recordBatch->ToString() << " got " << verify->ToString();
}

TEST_F(TableWriteTest, TestTableWriteMultiStreamsWithSpecialState)
{
    CreateTestTable();
    auto recordBatch = GenerateDataSimple(1);

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .Build();

    {
        auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();
        stream->Write(*recordBatch);
        stream->Close();
    }
    {
        auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-2").SetStreamVersion(1).Build();
        stream->Write(*recordBatch);
        stream->Write(*recordBatch);
        stream->Write(*recordBatch);
    }
    {
        auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(2).Build();
        stream->Write(*recordBatch);
        stream->Close();
    }
    {
        auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-3").SetStreamVersion(2).Build();
        stream->Write(*recordBatch);
        stream->Close();
    }
    {
        auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-3").SetStreamVersion(3).Build();
        stream->Write(*recordBatch);
    }
    std::map<std::string, int64_t> expectSs{{"test-stream-1", 2}, {"test-stream-3", 2}};
    ASSERT_EQ(tableWriteSession->ListStream(), expectSs);
    EXPECT_THROW(tableWriteSession->Commit({{"test-stream-1", 3}}), OdpsTunnelException);
    tableWriteSession->Commit(expectSs);
    // verify it
    auto verify = ExecuteSelect("select * from " + mProjectName + "." + mTableName + ";");
    ASSERT_EQ(verify->num_rows(), 2);
}

TEST_F(TableWriteTest, TestTableWriteDPOverwrite)
{
    CreatePartitionedTestTable();

    auto builder1 = std::make_shared<arrow::Int64Builder>(arrow::system_memory_pool());
    auto builder2 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());
    auto builder3 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());
    auto builder4 = std::make_shared<arrow::StringBuilder>(arrow::system_memory_pool());

    for (size_t i = 0; i < 5; i++)
    {
        builder1->Append(i).ok();
        builder2->Append("string_" + std::to_string(i)).ok();
        builder3->Append("aaa").ok();
        builder4->Append("bbb").ok();
    }
    for (size_t i = 0; i < 5; i++)
    {
        builder1->Append(i).ok();
        builder2->Append("string_" + std::to_string(i)).ok();
        builder3->Append("ccc").ok();
        builder4->Append("ddd").ok();
    }

    std::shared_ptr<arrow::Array> a1, a2, a3, a4;
    builder1->Finish(&a1).ok();
    builder2->Finish(&a2).ok();
    builder3->Finish(&a3).ok();
    builder4->Finish(&a4).ok();

    {
        for (int i = 0; i < 2; i++)
        {
            auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                        .SetProject(mProjectName)
                                        .SetSchema("default")
                                        .SetTable(mPartitionedTableName)
                                        .SetPartitionSpec("")
                                        .Build();

            auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();
            auto recordBatch = arrow::RecordBatch::Make(stream->GetArrowSchema(), a1->length(), {a1, a2, a3, a4});
            stream->Write(*recordBatch);
            stream->Close();
            tableWriteSession->Commit({});
        }
        // verify it
        auto verify = ExecuteSelect("select col1, col2, p1, p2 from " + mProjectName + "." + mPartitionedTableName + " sort by p1, p2;");
        ASSERT_EQ(verify->num_rows(), 20);
    }
    {
        auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetPartitionSpec("")
                                    .SetOverwrite(true)
                                    .Build();

        auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();
        auto recordBatch = arrow::RecordBatch::Make(stream->GetArrowSchema(), a1->length(), {a1, a2, a3, a4});
        stream->Write(*recordBatch);
        stream->Close();
        tableWriteSession->Commit({});
    }
    // verify it
    auto verify = ExecuteSelect("select col1, col2, p1, p2 from " + mProjectName + "." + mPartitionedTableName + " sort by p1, p2;");
    ASSERT_EQ(verify->num_rows(), 10);
}

TEST_F(TableWriteTest, TestTableNotFound)
{
    ASSERT_THROW((*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetPartitionSpec("p1=aaa/p2=bbb")
                                .Build(), OdpsException);
}

TEST_F(TableWriteTest, TestAbortWrite)
{
    CreateTestTable();
    auto recordBatch = GenerateDataSimple(1);

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .Build();

    tableWriteSession->Abort();
    ASSERT_THROW(tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-3").SetStreamVersion(3).Build(), OdpsException);
}

TEST_F(TableWriteTest, TestTableWriteAP2)
{
    CreateTestTableAP2();
    auto recordBatch = GenerateDataSimple(10);

    auto tableWriteSession = (*mMaxStorageApi.BuildTableWriteSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .Build();

    auto stream = tableWriteSession->BuildWriteStream()->SetStreamId("test-stream-1").SetStreamVersion(1).Build();
    stream->Write(*recordBatch);
    stream->Close();
    tableWriteSession->Commit({});
    // verify it
    auto verify = ExecuteSelect("select * from " + mProjectName + "." + mTableName + ";");
    ASSERT_TRUE(recordBatch->Equals(*verify)) << "expect: " << recordBatch->ToString() << " got " << verify->ToString();
}