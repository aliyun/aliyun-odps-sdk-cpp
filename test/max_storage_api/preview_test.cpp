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

class PreviewStreamTest : public testing::Test
{
protected:
    static std::string sProjectName;
    static std::string sTableName;
    static MaxStorageApi sMaxStorageApi;
    static IODPSPtr sOdpsClient;
    static ISQLTaskPtr sSqlTask;

    static void SetUpTestCase()
    {
        sProjectName = Utils::GetProjectName();
        sTableName = Utils::GetRandomTableName() + "_partition";

        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());

        sOdpsClient = IODPS::Create(config, sProjectName);
        sSqlTask = ISQLTask::Create();
        sMaxStorageApi.Init(config);

        CreateTestTable();
        InsertTestData();
    }

    static void TearDownTestCase() { DropTableIfExists(sTableName); }

    // 执行SQL的辅助方法
    static void ExecuteSql(const std::string& sql)
    {
        IODPSInstancePtr instancePtr = sSqlTask->Run(sOdpsClient, sql);
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

    // 删除表的辅助方法
    static void DropTableIfExists(const std::string& tableName)
    {
        std::string dropSql = "drop table if exists " + tableName + ";";
        ExecuteSql(dropSql);
    }

    virtual void SetUp() {}
    virtual void TearDown() {}
    // 验证单个batch的数据结构和数据内容
    static void ValidateBatch(std::shared_ptr<arrow::RecordBatch> batch,
                              const std::vector<std::string>& expectedColumns,
                              int64_t globalRowIndex,
                              const std::string& expectedRegion = "",
                              const std::string& expectedDt = "")
    {
        ASSERT_NE(batch, nullptr);
        bool is_all = expectedColumns.size() == 0 && expectedRegion.empty() &&
                      expectedDt.empty();

        // 1. 验证列结构
        ValidateBatchStructure(batch, expectedColumns, is_all);

        // 2. 验证数据内容
        ValidateBatchData(batch,
                          expectedColumns,
                          globalRowIndex,
                          expectedRegion,
                          expectedDt,
                          is_all);
    }

    // 验证列结构
    static void ValidateBatchStructure(
        std::shared_ptr<arrow::RecordBatch> batch,
        const std::vector<std::string>& expectedColumns,
        bool is_all)
    {
        // 验证列数
        int64_t expectedColumnCount = is_all ? 6 : int64_t(expectedColumns.size());  // 数据列 + 分区列(region, dt)
        ASSERT_EQ(batch->num_columns(), expectedColumnCount);
        // 验证数据列类型
        for (int64_t i = 0; i < int64_t(expectedColumns.size()); ++i)
        {
            const auto& colName = expectedColumns[i];
            auto field = batch->schema()->field(i);

            if (colName == "col1")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::INT64);
            }
            else if (colName == "col2")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::STRING);
            }
            else if (colName == "col3")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::DOUBLE);
            }
            else if (colName == "col4")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::BOOL);
            }
        }
    }

    // 验证数据内容
    static void ValidateBatchData(
        std::shared_ptr<arrow::RecordBatch> batch,
        const std::vector<std::string>& expectedColumns,
        int64_t globalRowIndex,
        const std::string& expectedRegion = "",
        const std::string& expectedDt = "",
        bool is_all = false)
    {
        for (int64_t rowIdx = 0; rowIdx < batch->num_rows(); ++rowIdx)
        {
            // 验证数据列
            ValidateDataColumns(batch, rowIdx, globalRowIndex, expectedColumns);

            // 验证分区列
            if (is_all)
            {
                // 如果是全量数据，分区列在第4、5列
                ValidatePartitionColumns(batch,
                                         rowIdx,
                                         globalRowIndex,
                                         4,
                                         expectedRegion,
                                         expectedDt);
            }
            else if (batch->num_columns() >= int64_t(expectedColumns.size()) + 2)
            {
                // 如果指定了列，分区列在数据列之后
                ValidatePartitionColumns(batch,
                                         rowIdx,
                                         globalRowIndex,
                                         int64_t(expectedColumns.size()),
                                         expectedRegion,
                                         expectedDt);
            }

            globalRowIndex++;
        }
    }

    // 验证数据列
    static void ValidateDataColumns(std::shared_ptr<arrow::RecordBatch> batch,
                                    int64_t rowIdx,
                                    int64_t globalRowIndex,
                                    const std::vector<std::string>& columns)
    {
        // 根据我们插入的测试数据验证每一行
        // 数据格式：col1=i, col2="test_string_i", col3=i*1.5, col4=(i%2==0)
        int64_t localIndex = globalRowIndex % 3;  // 0, 1, 2 循环

        // 如果columns为空，则验证所有数据列（前4列）
        std::vector<std::string> columnsToValidate = columns;
        if (columns.empty())
        {
            columnsToValidate = {"col1", "col2", "col3", "col4"};
        }

        for (size_t i = 0; i < columnsToValidate.size(); ++i)
        {
            const auto& colName = columnsToValidate[i];
            auto colArray = batch->column(i);

            if (colName == "col1")
            {
                auto intArray =
                    std::static_pointer_cast<arrow::Int64Array>(colArray);
                int64_t expectedValue = localIndex;
                int64_t actualValue = intArray->Value(rowIdx);
                ASSERT_EQ(actualValue, expectedValue);
            }
            else if (colName == "col2")
            {
                auto strArray =
                    std::static_pointer_cast<arrow::StringArray>(colArray);
                std::string expectedValue =
                    "test_string_" + std::to_string(localIndex);
                std::string actualValue = strArray->GetString(rowIdx);
                ASSERT_EQ(actualValue, expectedValue);
            }
            else if (colName == "col3")
            {
                auto doubleArray =
                    std::static_pointer_cast<arrow::DoubleArray>(colArray);
                double expectedValue = localIndex * 1.5;
                double actualValue = doubleArray->Value(rowIdx);
                ASSERT_DOUBLE_EQ(actualValue, expectedValue);
            }
            else if (colName == "col4")
            {
                auto boolArray =
                    std::static_pointer_cast<arrow::BooleanArray>(colArray);
                bool expectedValue = (localIndex % 2 == 0);
                bool actualValue = boolArray->Value(rowIdx);
                ASSERT_EQ(actualValue, expectedValue);
            }
        }
    }

    // 验证分区列
    static void ValidatePartitionColumns(
        std::shared_ptr<arrow::RecordBatch> batch,
        int64_t rowIdx,
        int64_t globalRowIndex,
        size_t dataColumnCount,
        const std::string& expectedRegion = "",
        const std::string& expectedDt = "")
    {
        // 确定当前行属于哪个分区
        std::string actualRegion, actualDt;
        if (globalRowIndex < 3)
        {
            actualRegion = "east";
            actualDt = "20250701";
        }
        else if (globalRowIndex < 6)
        {
            actualRegion = "west";
            actualDt = "20250701";
        }
        else
        {
            actualRegion = "east";
            actualDt = "20250702";
        }

        // 读取分区列
        auto regionArray = std::static_pointer_cast<arrow::StringArray>(
            batch->column(dataColumnCount));
        auto dtArray = std::static_pointer_cast<arrow::StringArray>(
            batch->column(dataColumnCount + 1));

        std::string returnedRegion = regionArray->GetString(rowIdx);
        std::string returnedDt = dtArray->GetString(rowIdx);

        // 如果指定了期望的分区，则额外验证
        if (!expectedRegion.empty())
        {
            ASSERT_EQ(returnedRegion, expectedRegion);
        }
        if (!expectedDt.empty())
        {
            ASSERT_EQ(returnedDt, expectedDt);
        }
    }

private:
    static void CreateTestTable()
    {
        try
        {
            DropTableIfExists(sTableName);

            // 创建带分区的表
            std::string createSql =
                "create table " + sTableName +
                " (col1 bigint, col2 string, col3 double, col4 boolean) "
                "partitioned by (region string, dt string);";

            ExecuteSql(createSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create test table: " << e.what()
                      << std::endl;
            throw;
        }
    }

    static void InsertTestData()
    {
        try
        {
            // 插入多个分区的测试数据
            std::vector<std::pair<std::string, std::string>> partitions = {
                {"east", "20250701"},
                {"west", "20250701"},
                {"east", "20250702"}};

            for (const auto& partition : partitions)
            {
                for (int i = 0; i < 3; ++i)
                {
                    std::string insertSql =
                        "insert into " + sTableName + " partition(region='" +
                        partition.first + "', dt='" + partition.second + "') " +
                        "values (" + std::to_string(i) + ", " +
                        "'test_string_" + std::to_string(i) + "', " +
                        std::to_string(i * 1.5) + ", " +
                        (i % 2 == 0 ? "true" : "false") + ");";

                    ExecuteSql(insertSql);
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to insert test data: " << e.what()
                      << std::endl;
            throw;
        }
    }
};

std::string PreviewStreamTest::sProjectName;
std::string PreviewStreamTest::sTableName;
MaxStorageApi PreviewStreamTest::sMaxStorageApi;
IODPSPtr PreviewStreamTest::sOdpsClient;
ISQLTaskPtr PreviewStreamTest::sSqlTask;

// 测试1：不指定分区，获取所有数据
TEST_F(PreviewStreamTest, TestAllPartitions)
{
    auto previewStream = (*sMaxStorageApi.BuildTablePreviewStream())
                             .SetProject(sProjectName)
                             .SetSchema("default")
                             .SetTable(sTableName)
                             .Build();

    int64_t globalRowIndex = 0;
    int batchCount = 0;
    int64_t totalRows = 0;

    while (true)
    {
        auto batch = previewStream->Read();
        if (batch == nullptr)
        {
            break;
        }

        ValidateBatch(batch, {}, globalRowIndex, "", "");

        totalRows += batch->num_rows();
        globalRowIndex += batch->num_rows();
        batchCount++;
    }
    ASSERT_EQ(totalRows, 9);

    previewStream->Close();
}

// 测试2：指定存在的分区
TEST_F(PreviewStreamTest, TestSpecificPartition)
{
    auto previewStream = (*sMaxStorageApi.BuildTablePreviewStream())
                             .SetProject(sProjectName)
                             .SetSchema("default")
                             .SetTable(sTableName)
                             .SetColumns({"col1", "col2"})
                             .SetPartition("region=east,dt=20250701")
                             .SetLimit(2)
                             .Build();

    int64_t globalRowIndex = 0;
    int batchCount = 0;
    int64_t totalRows = 0;

    while (true)
    {
        auto batch = previewStream->Read();
        if (batch == nullptr)
        {
            break;
        }

        ValidateBatch(
            batch, {"col1", "col2"}, globalRowIndex, "east", "20250701");

        totalRows += batch->num_rows();
        globalRowIndex += batch->num_rows();
        batchCount++;
    }
    ASSERT_EQ(totalRows, 2);

    previewStream->Close();
}

// 测试3：指定不存在的分区
TEST_F(PreviewStreamTest, TestNonExistentPartition)
{
    auto previewStream = (*sMaxStorageApi.BuildTablePreviewStream())
                             .SetProject(sProjectName)
                             .SetSchema("default")
                             .SetTable(sTableName)
                             .SetPartition("region=nonexistent,dt=20250701")
                             .SetLimit(10)
                             .Build();

    auto batch = previewStream->Read();
    ASSERT_EQ(batch, nullptr);
    previewStream->Close();
}

// 测试4：部分分区条件
TEST_F(PreviewStreamTest, TestPartialPartition)
{
    std::vector<std::string> columns = {"col1", "col2", "col3", "col4"};
    auto previewStream = (*sMaxStorageApi.BuildTablePreviewStream())
                             .SetProject(sProjectName)
                             .SetSchema("default")
                             .SetTable(sTableName)
                             .SetPartition("region=east")
                             .SetColumns(columns)
                             .SetLimit(1)
                             .Build();

    int64_t globalRowIndex = 0;
    int batchCount = 0;

    while (true)
    {
        auto batch = previewStream->Read();
        if (batch == nullptr)
        {
            break;
        }

        ValidateBatch(batch, columns, globalRowIndex, "east", "");

        globalRowIndex += batch->num_rows();
        batchCount++;
    }
    ASSERT_EQ(globalRowIndex, 1);
    previewStream->Close();
}
