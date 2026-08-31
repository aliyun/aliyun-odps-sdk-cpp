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

class InstanceReadBuilderTest : public ::testing::Test
{
protected:
    MaxStorageApi mMaxStorageApi;

public:
    void SetUp() override
    {
        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
        mMaxStorageApi.Init(config);
    }

    void TearDown() override {}
};

TEST_F(InstanceReadBuilderTest, TestMaxStorageApiBuilderMethods)
{
    // 测试BuildInstanceReadSession方法是否能正常返回对象
    auto instanceReadSessionBuilder = mMaxStorageApi.BuildInstanceReadSession();
    ASSERT_NE(nullptr, instanceReadSessionBuilder);
    auto instanceDirectReadStreamBuilder = mMaxStorageApi.BuildInstanceDirectReadStream();
    ASSERT_NE(nullptr, instanceDirectReadStreamBuilder);
}

class InstanceTest : public testing::Test
{
protected:
    std::string mProjectName;
    std::string mTableName;
    MaxStorageApi mMaxStorageApi;
    IODPSPtr mOdpsClient;
    ISQLTaskPtr mSqlTask;
    std::string mInstanceId;
    std::set<int64_t> mData;

    void SetUp() override
    {
        mProjectName = Utils::GetProjectName();
        mTableName = Utils::GetRandomTableName();

        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());

        mOdpsClient = IODPS::Create(config, mProjectName);
        mSqlTask = ISQLTask::Create();
        mMaxStorageApi.Init(config);

        CreateTestTable();
        InsertTestData();
        mInstanceId = EnquiryTestTable();
    }

    void TearDown() override { DropTableIfExists(mTableName); }

    void ExecuteSql(const std::string& sql)
    {
        IODPSInstancePtr instancePtr = mSqlTask->Run(mOdpsClient, sql);
        try
        {
            instancePtr->WaitForSuccess(30000);
        }
        catch (const OdpsException& e)
        {
            std::cerr << "SQL execution failed: " << e.GetErrorCode() << " - "
                      << e.GetErrorMsg() << std::endl;
            throw;
        }
    }

    void DropTableIfExists(const std::string& tableName)
    {
        std::string dropSql = "drop table if exists " + tableName + ";";
        ExecuteSql(dropSql);
    }

    void ValidateBatch(
        const std::shared_ptr<arrow::RecordBatch>& batch,
        std::set<int64_t>& data)
    {
        std::shared_ptr<arrow::Int64Array> int64Array;
        std::shared_ptr<arrow::StringArray> stringArray;
        std::shared_ptr<arrow::DoubleArray> doubleArray;
        std::shared_ptr<arrow::BooleanArray> boolArray;
        ASSERT_EQ(batch->num_columns(), 4);
        for (int64_t column = 0; column < 4; ++column)
        {
            auto field = batch->schema()->field(column);
            ASSERT_EQ(field->name(), "col" + std::to_string(column+1));
            if (field->name() == "col1")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::INT64);
                int64Array = std::static_pointer_cast<arrow::Int64Array>(batch->column(column));
            }
            else if (field->name() == "col2")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::STRING);
                stringArray = std::static_pointer_cast<arrow::StringArray>(batch->column(column));
            }
            else if (field->name() == "col3")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::DOUBLE);
                doubleArray = std::static_pointer_cast<arrow::DoubleArray>(batch->column(column));
            }
            else if (field->name() == "col4")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::BOOL);
                boolArray = std::static_pointer_cast<arrow::BooleanArray>(batch->column(column));
            }
        }

        for (int row = 0; row < batch->num_rows(); ++row)
        {
            int64_t value = int64Array->Value(row);
            data.insert(value);
            ASSERT_EQ("test_string_" + std::to_string(value), stringArray->GetString(row));
            ASSERT_EQ((double)value * 1.5, doubleArray->Value(row));
            ASSERT_EQ(value % 2 == 0, boolArray->Value(row));
        }
    }

private:
    void CreateTestTable()
    {
        try
        {
            DropTableIfExists(mTableName);
            std::string createSql =
                "create table " + mTableName +
                " (col1 bigint, col2 string, col3 double, col4 boolean);";

            ExecuteSql(createSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create test table: " << e.what()
                      << std::endl;
            throw;
        }
    }

    std::string EnquiryTestTable()
    {
        try
        {
            std::string enquirySql = "select * from " + mTableName + ";";
            IODPSInstancePtr instancePtr =
                mSqlTask->Run(mOdpsClient, enquirySql);
            instancePtr->WaitForSuccess(30000);
            std::cout << enquirySql << " success." << std::endl;
            std::cout << instancePtr->GenerateLogView(24) << std::endl;
            return instancePtr->GetInstanceId();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to enquiry test table: " << e.what()
                      << std::endl;
            throw;
        }
    }

    void InsertTestData()
    {
        try
        {
            // Use a single SQL statement to insert all 10 rows of test data
            std::string insertSql = "insert into " + mTableName + " values ";
            for (int i = 0; i < 10; ++i) {
                if (i > 0) {
                    insertSql += ",";
                }
                insertSql += " (" + std::to_string(i) + ", " +
                             "'test_string_" + std::to_string(i) +
                             "', " + std::to_string(i * 1.5) + ", " +
                             (i % 2 == 0 ? "true" : "false") + ")";
                mData.insert(i);
            }
            insertSql += ";";

            ExecuteSql(insertSql);
            std::cout << insertSql << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to insert test data: " << e.what()
                      << std::endl;
            throw;
        }
    }
};

TEST_F(InstanceTest, TestNormal)
{
    auto session = (*(mMaxStorageApi.BuildInstanceReadSession()))
                               .SetProject(mProjectName)
                               .SetInstance(mInstanceId)
                               .Build();

    auto splits = session->GetSplits();
    ASSERT_NE(nullptr, splits);
    ASSERT_EQ((int64_t)mData.size(), splits->GetRecordCount());
    auto stream = session->BuildInstanceReadStream()->SetSplit(splits->GetSplit(0, mData.size())).Build();

    std::set<int64_t> data;
    while (true)
    {
        auto batch = stream->Read();
        if (batch == nullptr)
        {
            break;
        }

        ValidateBatch(batch, data);
    }
    ASSERT_EQ(data, mData);
    stream->Close();
}

TEST_F(InstanceTest, TestCreateInstanceReadStreamWithColumns)
{
    auto session = (*(mMaxStorageApi.BuildInstanceReadSession()))
                               .SetProject(mProjectName)
                               .SetInstance(mInstanceId)
                               .Build();

    auto splits = session->GetSplits();
    ASSERT_NE(nullptr, splits);
    ASSERT_EQ((int64_t)mData.size(), splits->GetRecordCount());
    // Test reading with specific columns
    std::vector<std::string> columns = {"col1", "col2"};
    auto stream = session->BuildInstanceReadStream()
                         ->SetSplit(splits->GetSplit(0, splits->GetRecordCount()))
                          .SetColumns(columns)
                          .Build();

    std::set<int64_t> data;
    while (true)
    {
        auto batch = stream->Read();
        if (batch == nullptr)
        {
            break;
        }

        // Validate only selected columns are returned
        ASSERT_EQ(batch->num_columns(), (int64_t)columns.size());

        for (int64_t column = 0; column < batch->num_columns(); ++column)
        {
            auto field = batch->schema()->field(column);
            ASSERT_TRUE(field->name() == "col1" || field->name() == "col2");
        }
    }
    stream->Close();
}

TEST_F(InstanceTest, TestCreateInstanceReadStreamWithOffset)
{
    auto session = (*(mMaxStorageApi.BuildInstanceReadSession()))
                               .SetProject(mProjectName)
                               .SetInstance(mInstanceId)
                               .Build();

    auto splits = session->GetSplits();
    ASSERT_NE(nullptr, splits);
    ASSERT_EQ((int64_t)mData.size(), splits->GetRecordCount());
    // Test reading with offset (read second half of data)
    int64_t offset = 0;
    int64_t count = mData.size() / 2;
    auto stream = session->BuildInstanceReadStream()
                         ->SetSplit(splits->GetSplit(offset, count))
                          .Build();

    int64_t rowCount = 0;
    std::set<int64_t> data;
    while (true)
    {
        auto batch = stream->Read();
        if (batch == nullptr)
        {
            break;
        }

        rowCount += batch->num_rows();
        offset += batch->num_rows();
        ValidateBatch(batch, data);
    }
    stream->Close();
    count = mData.size() - offset;
    stream = session->BuildInstanceReadStream()
                         ->SetSplit(splits->GetSplit(offset, count))
                          .Build();
    while (true)
    {
        auto batch = stream->Read();
        if (batch == nullptr)
        {
            break;
        }

        rowCount += batch->num_rows();
        ValidateBatch(batch, data);
    }
    ASSERT_EQ((int64_t)mData.size(), rowCount);
    stream->Close();
}

TEST_F(InstanceTest, TestRebuildSessionWithSessionId)
{
    // Create initial session
    auto session1 = (*(mMaxStorageApi.BuildInstanceReadSession()))
                                .SetProject(mProjectName)
                                .SetInstance(mInstanceId)
                                .Build();

    std::string sessionId = session1->GetSessionId();
    ASSERT_FALSE(sessionId.empty());

    // Rebuild session with the same sessionId
    auto session2 = (*(mMaxStorageApi.BuildInstanceReadSession()))
                                .SetProject(mProjectName)
                                .SetInstance(mInstanceId)
                                .SetSessionId(sessionId)
                                .Build();

    // Both sessions should have the same sessionId
    ASSERT_EQ(sessionId, session2->GetSessionId());

    // Test that rebuilt session works correctly
    auto splits = session2->GetSplits();
    ASSERT_NE(nullptr, splits);
    ASSERT_EQ((int64_t)mData.size(), splits->GetRecordCount());

    auto stream = session2->BuildInstanceReadStream()->SetSplit(splits->GetSplit(0, mData.size())).Build();
    std::set<int64_t> data;
    while (true)
    {
        auto batch = stream->Read();
        if (batch == nullptr)
        {
            break;
        }
        ValidateBatch(batch, data);
    }
    ASSERT_EQ(data, mData);
    stream->Close();
}

TEST_F(InstanceTest, TestInstanceDirectReadStream)
{
    std::string instanceId;
    int64_t queryId = -1;
    std::tie(instanceId, queryId) = Utils::ExecSqlByMCQA1("select * from " + mTableName + ";");
    if (instanceId.empty() || queryId < 0)
    {
        FAIL() << "Failed to execute sql query";
    }

    auto instanceStreamBuilder = mMaxStorageApi.BuildInstanceDirectReadStream();
    auto instanceStream = (*instanceStreamBuilder)
        .SetProject(mProjectName)
        .SetInstance(instanceId)
        .SetCount(100)
        .SetOffset(0)
        .SetQueryId(queryId)
        .SetTaskName("console_sqlrt_task")
        .Build();

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = instanceStream->Read();
        if (batch == nullptr)
        {
            break;
        }
        ValidateBatch(batch, mData);
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 10);
    instanceStream->Close();
}

TEST_F(InstanceTest, TestInstanceDirectReadStreamBuilderAllMethods)
{
    std::string instanceId;
    int64_t queryId = -1;
    std::tie(instanceId, queryId) = Utils::ExecSqlByMCQA1("select * from " + mTableName + ";");
    if (instanceId.empty() || queryId < 0)
    {
        FAIL() << "Failed to execute sql query";
    }

    // 测试所有Builder方法的链式调用
    auto instanceStreamBuilder = mMaxStorageApi.BuildInstanceDirectReadStream();
    ASSERT_NE(nullptr, instanceStreamBuilder);

    auto& builderRef = instanceStreamBuilder->SetProject(mProjectName)
                                            .SetInstance(instanceId)
                                            .SetTaskName("console_sqlrt_task")
                                            .SetQueryId(queryId)
                                            .SetOffset(0)
                                            .SetCount(5)
                                            .SetEnableLimit(true);

    // 验证返回的是同一个builder实例（链式调用）
    ASSERT_EQ(&builderRef, instanceStreamBuilder.get());

    // 构建流
    auto instanceStream = instanceStreamBuilder->Build();
    ASSERT_NE(nullptr, instanceStream);

    // 验证可以读取数据
    int64_t totalRows = 0;
    std::set<int64_t> data;
    while (true)
    {
        auto batch = instanceStream->Read();
        if (batch == nullptr)
        {
            break;
        }
        ValidateBatch(batch, data);
        totalRows += batch->num_rows();
    }

    // 因为设置了count为5，但实际读取可能因为分批而不同，这里主要验证能读取到数据
    ASSERT_GT(totalRows, 0);
    instanceStream->Close();
}

// 新增测试用例：测试默认参数和边界条件
TEST_F(InstanceTest, TestInstanceDirectReadStreamWithDefaultParams)
{
    std::string instanceId;
    int64_t queryId = -1;
    std::tie(instanceId, queryId) = Utils::ExecSqlByMCQA1("select * from " + mTableName + ";");
    if (instanceId.empty() || queryId < 0)
    {
        FAIL() << "Failed to execute sql query";
    }

    // 测试使用最小参数集构建
    auto instanceStreamBuilder = mMaxStorageApi.BuildInstanceDirectReadStream();
    auto instanceStream = (*instanceStreamBuilder)
        .SetProject(mProjectName)
        .SetInstance(instanceId)
        .SetQueryId(queryId)
        .SetTaskName("console_sqlrt_task")
        .Build();

    ASSERT_NE(nullptr, instanceStream);

    // 验证可以读取数据
    int64_t totalRows = 0;
    while (true)
    {
        auto batch = instanceStream->Read();
        if (batch == nullptr)
        {
            break;
        }
        totalRows += batch->num_rows();
    }

    ASSERT_GT(totalRows, 0);
    instanceStream->Close();
}
