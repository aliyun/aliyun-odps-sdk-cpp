#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#include "arrow/api.h"
#include "gtest/gtest.h"
#include "include/configuration.h"
#include "include/odps_api.h"
#include "max_storage_api.h"
#include "test/common/test_util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::max_storage_api;

class TableReadBuilderTest : public ::testing::Test
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

TEST_F(TableReadBuilderTest, TestMaxStorageApiBuilderMethods)
{
    // 测试BuildTableReadSession方法是否能正常返回对象
    auto tableReadSessionBuilder = mMaxStorageApi.BuildTableReadSession();
    ASSERT_NE(nullptr, tableReadSessionBuilder);
}

class IPredicateTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// Test IPredicate::NO_PREDICATE initialization
TEST_F(IPredicateTest, NoPredicateInitialization) {
    EXPECT_NE(nullptr, IPredicate::NO_PREDICATE);
    EXPECT_EQ(IPredicate::PredicateType::RAW, IPredicate::NO_PREDICATE->GetType());
    EXPECT_EQ("", IPredicate::NO_PREDICATE->ToString());
}

// Test IAttribute interface
TEST_F(IPredicateTest, AttributeCreationAndValue) {
    auto attribute = IAttribute::Of("test_column");
    ASSERT_NE(nullptr, attribute);
    EXPECT_EQ(IPredicate::PredicateType::ATTRIBUTE, attribute->GetType());
    EXPECT_EQ("test_column", attribute->GetValue());
}

TEST_F(IPredicateTest, AttributeToStringWithBackticks) {
    auto attribute = IAttribute::Of("test_column");
    ASSERT_NE(nullptr, attribute);

    // Should add backticks around the value
    EXPECT_EQ("`test_column`", attribute->ToString());
}

TEST_F(IPredicateTest, AttributeToStringWithExistingBackticks) {
    auto attribute = IAttribute::Of("`test_column`");
    ASSERT_NE(nullptr, attribute);

    // Should not add extra backticks if already present
    EXPECT_EQ("`test_column`", attribute->ToString());
}

TEST_F(IPredicateTest, AttributeToStringWithEscapedBackticks) {
    auto attribute = IAttribute::Of("test_`column");
    ASSERT_NE(nullptr, attribute);

    // Should escape internal backticks
    EXPECT_EQ("`test_``column`", attribute->ToString());
}

// Test IConstant interface
TEST_F(IPredicateTest, ConstantCreationAndValue) {
    auto constant = IConstant::Of("123");
    ASSERT_NE(nullptr, constant);
    EXPECT_EQ(IPredicate::PredicateType::CONSTANT, constant->GetType());
    EXPECT_EQ("123", constant->GetValue());
}

TEST_F(IPredicateTest, ConstantToString) {
    auto constant = IConstant::Of("123");
    ASSERT_NE(nullptr, constant);
    EXPECT_EQ("123", constant->ToString());
}

// Test IRawPredicate interface
TEST_F(IPredicateTest, RawPredicateCreationAndValue) {
    auto rawPredicate = IRawPredicate::Of("column > 100");
    ASSERT_NE(nullptr, rawPredicate);
    EXPECT_EQ(IPredicate::PredicateType::RAW, rawPredicate->GetType());
    EXPECT_EQ("column > 100", rawPredicate->GetRawExpr());
}

TEST_F(IPredicateTest, RawPredicateToString) {
    auto rawPredicate = IRawPredicate::Of("column > 100");
    ASSERT_NE(nullptr, rawPredicate);
    EXPECT_EQ("column > 100", rawPredicate->ToString());
}

// Test IBinaryPredicate interface
TEST_F(IPredicateTest, BinaryPredicateEquals) {
    auto predicate = IBinaryPredicate::Equals("column1", "column2");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::BINARY, predicate->GetType());
    EXPECT_EQ(IBinaryPredicate::Operator::EQUALS, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetLeftOperand());
    EXPECT_EQ("column2", predicate->GetRightOperand());
    EXPECT_EQ("column1 = column2", predicate->ToString());
}

TEST_F(IPredicateTest, BinaryPredicateNotEquals) {
    auto predicate = IBinaryPredicate::NotEquals("column1", "100");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::BINARY, predicate->GetType());
    EXPECT_EQ(IBinaryPredicate::Operator::NOT_EQUALS, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetLeftOperand());
    EXPECT_EQ("100", predicate->GetRightOperand());
    EXPECT_EQ("column1 != 100", predicate->ToString());
}

TEST_F(IPredicateTest, BinaryPredicateGreaterThan) {
    auto predicate = IBinaryPredicate::GreaterThan("column1", "50");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::BINARY, predicate->GetType());
    EXPECT_EQ(IBinaryPredicate::Operator::GREATER_THAN, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetLeftOperand());
    EXPECT_EQ("50", predicate->GetRightOperand());
    EXPECT_EQ("column1 > 50", predicate->ToString());
}

TEST_F(IPredicateTest, BinaryPredicateLessThan) {
    auto predicate = IBinaryPredicate::LessThan("column1", "50");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::BINARY, predicate->GetType());
    EXPECT_EQ(IBinaryPredicate::Operator::LESS_THAN, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetLeftOperand());
    EXPECT_EQ("50", predicate->GetRightOperand());
    EXPECT_EQ("column1 < 50", predicate->ToString());
}

TEST_F(IPredicateTest, BinaryPredicateGreaterThanOrEqual) {
    auto predicate = IBinaryPredicate::GreaterThanOrEqual("column1", "50");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::BINARY, predicate->GetType());
    EXPECT_EQ(IBinaryPredicate::Operator::GREATER_THAN_OR_EQUAL, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetLeftOperand());
    EXPECT_EQ("50", predicate->GetRightOperand());
    EXPECT_EQ("column1 >= 50", predicate->ToString());
}

TEST_F(IPredicateTest, BinaryPredicateLessThanOrEqual) {
    auto predicate = IBinaryPredicate::LessThanOrEqual("column1", "50");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::BINARY, predicate->GetType());
    EXPECT_EQ(IBinaryPredicate::Operator::LESS_THAN_OR_EQUAL, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetLeftOperand());
    EXPECT_EQ("50", predicate->GetRightOperand());
    EXPECT_EQ("column1 <= 50", predicate->ToString());
}

// Test IUnaryPredicate interface
TEST_F(IPredicateTest, UnaryPredicateIsNull) {
    auto predicate = IUnaryPredicate::IsNull("column1");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::UNARY, predicate->GetType());
    EXPECT_EQ(IUnaryPredicate::Operator::IS_NULL, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetOperand());
    EXPECT_EQ("column1 is null", predicate->ToString());
}

TEST_F(IPredicateTest, UnaryPredicateNotNull) {
    auto predicate = IUnaryPredicate::NotNull("column1");
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::UNARY, predicate->GetType());
    EXPECT_EQ(IUnaryPredicate::Operator::NOT_NULL, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetOperand());
    EXPECT_EQ("column1 is not null", predicate->ToString());
}

// Test ICompoundPredicate interface
TEST_F(IPredicateTest, CompoundPredicateAnd) {
    std::vector<IPredicatePtr> predicates = {
        IBinaryPredicate::Equals("column1", "100"),
        IBinaryPredicate::GreaterThan("column2", "50")
    };

    auto predicate = ICompoundPredicate::And(predicates);
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::COMPOUND, predicate->GetType());
    EXPECT_EQ(ICompoundPredicate::Operator::AND, predicate->GetOperator());

    auto resultPredicates = predicate->GetPredicates();
    EXPECT_EQ(2u, resultPredicates.size());

    EXPECT_EQ("(column1 = 100) and (column2 > 50)", predicate->ToString());
}

TEST_F(IPredicateTest, CompoundPredicateOr) {
    std::vector<IPredicatePtr> predicates = {
        IBinaryPredicate::Equals("column1", "100"),
        IBinaryPredicate::GreaterThan("column2", "50")
    };

    auto predicate = ICompoundPredicate::Or(predicates);
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::COMPOUND, predicate->GetType());
    EXPECT_EQ(ICompoundPredicate::Operator::OR, predicate->GetOperator());

    auto resultPredicates = predicate->GetPredicates();
    EXPECT_EQ(2u, resultPredicates.size());

    EXPECT_EQ("(column1 = 100) or (column2 > 50)", predicate->ToString());
}

TEST_F(IPredicateTest, CompoundPredicateNot) {
    auto innerPredicate = IBinaryPredicate::Equals("column1", "100");
    auto predicate = ICompoundPredicate::Not(innerPredicate);
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::COMPOUND, predicate->GetType());
    EXPECT_EQ(ICompoundPredicate::Operator::NOT, predicate->GetOperator());

    auto resultPredicates = predicate->GetPredicates();
    EXPECT_EQ(1u, resultPredicates.size());

    EXPECT_EQ("not (column1 = 100)", predicate->ToString());
}

TEST_F(IPredicateTest, CompoundPredicateAddPredicate) {
    auto predicate = ICompoundPredicate::And({});
    ASSERT_NE(nullptr, predicate);

    predicate->AddPredicate(IBinaryPredicate::Equals("column1", "100"));
    predicate->AddPredicate(IBinaryPredicate::GreaterThan("column2", "50"));

    auto resultPredicates = predicate->GetPredicates();
    EXPECT_EQ(2u, resultPredicates.size());

    EXPECT_EQ("(column1 = 100) and (column2 > 50)", predicate->ToString());
}

// Test IInPredicate interface
TEST_F(IPredicateTest, InPredicateIn) {
    std::vector<std::string> values = {"1", "2", "3"};
    auto predicate = IInPredicate::In("column1", values);
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::IN, predicate->GetType());
    EXPECT_EQ(IInPredicate::Operator::IN, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetOperand());

    auto set = predicate->GetSet();
    EXPECT_EQ(3u, set.size());
    EXPECT_EQ("1", set[0]);
    EXPECT_EQ("2", set[1]);
    EXPECT_EQ("3", set[2]);

    EXPECT_EQ("column1 in (1, 2, 3)", predicate->ToString());
}

TEST_F(IPredicateTest, InPredicateNotIn) {
    std::vector<std::string> values = {"1", "2", "3"};
    auto predicate = IInPredicate::NotIn("column1", values);
    ASSERT_NE(nullptr, predicate);
    EXPECT_EQ(IPredicate::PredicateType::IN, predicate->GetType());
    EXPECT_EQ(IInPredicate::Operator::NOT_IN, predicate->GetOperator());
    EXPECT_EQ("column1", predicate->GetOperand());

    auto set = predicate->GetSet();
    EXPECT_EQ(3u, set.size());
    EXPECT_EQ("1", set[0]);
    EXPECT_EQ("2", set[1]);
    EXPECT_EQ("3", set[2]);

    EXPECT_EQ("column1 not in (1, 2, 3)", predicate->ToString());
}

// Test complex predicate combinations
TEST_F(IPredicateTest, ComplexPredicateCombination) {
    // Create a complex predicate: (column1 = 100 AND column2 > 50) OR column3 is null
    auto andPredicates = std::vector<IPredicatePtr>{
        IBinaryPredicate::Equals("column1", "100"),
        IBinaryPredicate::GreaterThan("column2", "50")
    };
    auto andPredicate = ICompoundPredicate::And(andPredicates);

    auto orPredicates = std::vector<IPredicatePtr>{
        andPredicate,
        IUnaryPredicate::IsNull("column3")
    };
    auto orPredicate = ICompoundPredicate::Or(orPredicates);

    EXPECT_EQ("((column1 = 100) and (column2 > 50)) or (column3 is null)", orPredicate->ToString());
}

// Test empty compound predicate
TEST_F(IPredicateTest, EmptyCompoundPredicate) {
    auto predicate = ICompoundPredicate::And({});
    EXPECT_EQ(IPredicate::NO_PREDICATE->ToString(), predicate->ToString());
}

class TableReadTest : public ::testing::Test
{
protected:
    static std::string sPredicateTestTable;
    static std::string sPredicateTestTableHashCluster;
    static std::string sPredicateTestTableRangeCluster;

    static void SetUpTestCase() {
        sPredicateTestTable = Utils::GetRandomTableName() + "_predicate_test_table";
        sPredicateTestTableHashCluster = sPredicateTestTable + "_hash_cluster";
        sPredicateTestTableRangeCluster = sPredicateTestTable + "_range_cluster";
        InitPredicateTestTable();
    }

    static void TearDownTestCase() {
        DropTableIfExists(sPredicateTestTable);
        DropTableIfExists(sPredicateTestTableHashCluster);
        DropTableIfExists(sPredicateTestTableRangeCluster);
    }

    static void InitPredicateTestTable() {
        DropTableIfExists(sPredicateTestTable);
        DropTableIfExists(sPredicateTestTableHashCluster);
        DropTableIfExists(sPredicateTestTableRangeCluster);

        // 创建测试表
        std::string createSql =
            "CREATE TABLE " + sPredicateTestTable +
            " (c1 BIGINT, c2 BIGINT, c3 DATETIME, c4 STRING);";
        std::cout << createSql << std::endl;
        ExecuteSql(createSql);

        // 使用TableWriteSession插入测试数据
        // 创建写入会话
        Configuration config = Utils::GetConfiguration();
        OdpsTunnel tunnel;
        tunnel.Init(config);

        // 构建数据批次
        int64_t totalRows = 100000;
        arrow::Int64Builder c1Builder;
        arrow::Int64Builder c2Builder;
        arrow::TimestampBuilder c3Builder(arrow::timestamp(arrow::TimeUnit::MILLI), arrow::default_memory_pool());
        arrow::StringBuilder c4Builder;

        // 创建Arrow RecordBatch
        auto now = std::chrono::system_clock::now();
        auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        for (int64_t row = 0; row < totalRows; ++row) {
            // 填充数据
            c1Builder.Append(row);
            c2Builder.Append(2 * row);
            // 根据Java代码修改：c3设置为当前时间减去i天的时间戳
            auto dateTime = now_seconds - 60 * 60 * 24 * row;
            c3Builder.Append(dateTime * 1000); // 转换为毫秒
            // 根据Java代码修改：c4设置为特定格式的字符串
            c4Builder.Append(std::to_string(row) + "test" + std::to_string(row + 1));
        }

        // 创建列数组
        std::shared_ptr<arrow::Array> c1Array;
        std::shared_ptr<arrow::Array> c2Array;
        std::shared_ptr<arrow::Array> c3Array;
        std::shared_ptr<arrow::Array> c4Array;

        c1Builder.Finish(&c1Array);
        c2Builder.Finish(&c2Array);
        c3Builder.Finish(&c3Array);
        c4Builder.Finish(&c4Array);

        // 创建schema
        auto schema = arrow::schema({
            arrow::field("c1", arrow::int64()),
            arrow::field("c2", arrow::int64()),
            arrow::field("c3", arrow::timestamp(arrow::TimeUnit::MICRO)),
            arrow::field("c4", arrow::utf8())
        });

        // 创建RecordBatch
        auto recordBatch = arrow::RecordBatch::Make(schema, totalRows, {c1Array, c2Array, c3Array, c4Array});
        // 写入数据，一个orc文件，每10000行一个block，以block粒度返回命中的数据
        auto upload = tunnel.CreateUpload(Utils::GetProjectName(), sPredicateTestTable);
        auto writer = upload->OpenArrowWriter(0);
        writer->Write(*recordBatch);
        writer->Close();
        upload->Commit();

        // 生成hash/range cluster表数据
        auto sql =
            "CREATE TABLE " + sPredicateTestTableHashCluster +
            " (c1 BIGINT, c2 BIGINT, c3 DATETIME, c4 STRING) CLUSTERED BY (c1) SORTED BY (c1) into 100 BUCKETS;";
        std::cout << sql << std::endl;
        ExecuteSql(sql);
        sql =
            "INSERT OVERWRITE TABLE " + sPredicateTestTableHashCluster +
            " SELECT * FROM " + sPredicateTestTable + ";";
        std::cout << sql << std::endl;
        ExecuteSql(sql);

        sql =
            "CREATE TABLE " + sPredicateTestTableRangeCluster +
            " (c1 BIGINT, c2 BIGINT, c3 DATETIME, c4 STRING) RANGE CLUSTERED BY (c1) SORTED BY (c1) into 100 BUCKETS;";
        std::cout << sql << std::endl;
        ExecuteSql(sql);
        sql =
            "INSERT OVERWRITE TABLE " + sPredicateTestTableRangeCluster +
            " SELECT * FROM " + sPredicateTestTable + ";";
        std::cout << sql << std::endl;
        ExecuteSql(sql);
    }

    static int64_t GetRowCountWithPredicate(
        const std::string& table,
        IPredicatePtr predicate) {
        // 使用SplitOptions替代SetCount和SetOffset
        SplitOptions splitOptions;
        splitOptions.mSplitMode = SplitMode::SIZE;
        splitOptions.mSplitSize = 10 * 1024 * 1024;

        FilterOptions filterOptions;
        filterOptions.mPredicate = predicate;
        filterOptions.mRequiredDataColumns = {"c1", "c2", "c3"};

        std::cout << "Predicate Filter:" << predicate->ToString() << std::endl;

        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
        MaxStorageApi api;
        api.Init(config);
        auto tableReadSession = (*api.BuildTableReadSession())
                                    .SetProject(Utils::GetProjectName())
                                    .SetSchema("default")
                                    .SetTable(table)
                                    .SetSplitOptions(splitOptions)
                                    .SetFilterOptions(filterOptions)
                                    .Build();

        std::cout << "Read Session:" << tableReadSession->GetSessionId() << std::endl;
        // 获取Splits
        auto splits = tableReadSession->GetSplits();
        EXPECT_GT(splits->GetSplitCount(), 0);

        int64_t totalRows = 0;
        for (int32_t i = 0; i < splits->GetSplitCount(); i++) {
            auto split = splits->GetSplit(i);
            auto readStream = tableReadSession->BuildTableReadStream()->SetSplit(split).Build();

            while (true)
            {
                auto batch = readStream->Read();
                if (batch == nullptr)
                {
                    break;
                }
                totalRows += batch->num_rows();
            }
        }
        std::cout << "Total rows: " << totalRows << std::endl;
        return totalRows;
    }

    static void ExecuteSql(const std::string& sql)
    {
        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
        auto project = Utils::GetProjectName();
        auto odps = IODPS::Create(config, project);
        ISQLTaskPtr task = ISQLTask::Create();
        IODPSInstancePtr instance = task->Run(odps, sql);
        try
        {
            instance->WaitForSuccess(60000);
        }
        catch (const OdpsException& e)
        {
            std::cerr << "SQL execution failed: " << e.GetErrorCode() << " - "
                      << e.GetErrorMsg() << std::endl;
            throw;
        }
    }

    static void DropTableIfExists(const std::string& tableName)
    {
        std::string dropSql = "drop table if exists " + tableName + ";";
        ExecuteSql(dropSql);
    }

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
        mTableName = Utils::GetRandomTableName() + "_table_read";
        mPartitionedTableName = Utils::GetRandomTableName() + "_partitioned_table";
        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
        mOdpsClient = IODPS::Create(config, mProjectName);
        mSqlTask = ISQLTask::Create();
        mMaxStorageApi.Init(config);
    }

    void TearDown() override {
        DropTableIfExists(mTableName);
        DropTableIfExists(mPartitionedTableName);
    }

    std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(ms);
        auto ms_part = ms - std::chrono::duration_cast<std::chrono::milliseconds>(sec);

        std::time_t t = sec.count();
        std::tm tm_buf;
        localtime_r(&t, &tm_buf);

        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
        std::string result(buffer);
        result += "." + std::to_string(ms_part.count());
        return result;
    }

    std::string GetCurrentTimestamp()
    {
        return FormatTimestamp(std::chrono::system_clock::now());
    }

    void ValidateBatch(
        std::shared_ptr<arrow::RecordBatch> batch,
        std::vector<std::string> expectedColumns)
    {
        ASSERT_NE(batch, nullptr);
        ASSERT_EQ(batch->num_columns(), (int)expectedColumns.size());

        std::shared_ptr<arrow::Int64Array> int64Array;
        std::shared_ptr<arrow::StringArray> stringArray;
        std::shared_ptr<arrow::DoubleArray> doubleArray;
        std::shared_ptr<arrow::BooleanArray> boolArray;

        for (const auto& column : expectedColumns)
        {
            auto field = batch->schema()->GetFieldByName(column);
            auto array = batch->GetColumnByName(column);
            ASSERT_NE(field, nullptr);
            ASSERT_NE(array, nullptr);
            if (column == "col1")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::INT64);
                int64Array = std::static_pointer_cast<arrow::Int64Array>(array);
            }
            else if (column == "col2")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::STRING);
                stringArray = std::static_pointer_cast<arrow::StringArray>(array);
            }
            else if (column == "col3")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::DOUBLE);
                doubleArray = std::static_pointer_cast<arrow::DoubleArray>(array);
            }
            else if (column == "col4")
            {
                ASSERT_EQ(field->type()->id(), arrow::Type::BOOL);
                boolArray = std::static_pointer_cast<arrow::BooleanArray>(array);
            }
            else if (column == "p1" || column == "p2")
            {
                // Partition columns
                ASSERT_EQ(field->type()->id(), arrow::Type::STRING);
            }
        }

        for (int64_t row = 0; row < batch->num_rows(); ++row)
        {
            if (int64Array) {
                ASSERT_EQ(int64Array->Value(row), 1);
            }
            if (stringArray) {
                ASSERT_EQ(stringArray->GetString(row), "test_string_2");
            }
            if (doubleArray) {
                ASSERT_DOUBLE_EQ(doubleArray->Value(row), 3.5);
            }
            if (boolArray) {
                ASSERT_TRUE(boolArray->Value(row));
            }
        }
    }

protected:
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

    void CreatePartitionedTestTable()
    {
        try
        {
            DropTableIfExists(mPartitionedTableName);

            std::string createSql =
                "create table " + mPartitionedTableName +
                " (col1 bigint, col2 string, col3 double, col4 boolean) "
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

    void InsertTestData(int numRows = 1000)
    {
        try
        {
            std::string insertSql = "insert into " + mTableName + " values (1, 'test_string_2', 3.5, true)";
            for (int i = 0; i < numRows - 1; ++i) {
                insertSql += ", (1, 'test_string_2', 3.5, true)";
            }
            insertSql += ";";

            ExecuteSql(insertSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to insert test data: " << e.what()
                      << std::endl;
            throw;
        }
    }

    void InsertPartitionedTestData()
    {
        try
        {
            // 使用动态分区插入数据
            std::string insertSql =
                "insert into " + mPartitionedTableName + " partition (p1, p2) "
                "values (1, 'test_string_2', 3.5, true, 'partition1', 'a'), "
                "(1, 'test_string_2', 3.5, true, 'partition1', 'b'), "
                "(1, 'test_string_2', 3.5, true, 'partition2', 'a'), "
                "(1, 'test_string_2', 3.5, true, 'partition2', 'b')";

            for (int i = 0; i < 249; ++i) {
                insertSql +=
                    ", (1, 'test_string_2', 3.5, true, 'partition1', 'a'), "
                    "(1, 'test_string_2', 3.5, true, 'partition1', 'b'), "
                    "(1, 'test_string_2', 3.5, true, 'partition2', 'a'), "
                    "(1, 'test_string_2', 3.5, true, 'partition2', 'b')";
            }
            insertSql += ";";

            ExecuteSql(insertSql);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to insert partitioned test data: " << e.what()
                      << std::endl;
            throw;
        }
    }
};

std::string TableReadTest::sPredicateTestTable;
std::string TableReadTest::sPredicateTestTableHashCluster;
std::string TableReadTest::sPredicateTestTableRangeCluster;

TEST_F(TableReadTest, TestTableRead)
{
    CreateTestTable();
    InsertTestData(1000);
    // 使用SplitOptions替代SetCount和SetOffset
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    // 获取Splits
    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    // 使用Stream Builder构建读取流
    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto split = splits->GetSplit(0, 1000);
    auto readStream = streamBuilder->SetSplit(split).Build();

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr)
        {
            break;
        }

        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});

        totalRows += batch->num_rows();
    }

    ASSERT_EQ(totalRows, 1000);
}

TEST_F(TableReadTest, TestPartitionedTableRead)
{
    // 创建分区表并插入数据
    CreatePartitionedTestTable();
    InsertPartitionedTestData();

    // 测试读取分区表
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0, 1000);

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split).Build();

    ASSERT_NE(nullptr, readStream);

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) {
            break;
        }

        // 验证返回的数据包含分区列
        ValidateBatch(batch, {"col1", "col2", "col3", "col4", "p1", "p2"});

        totalRows += batch->num_rows();
    }

    // 验证总行数正确 (4条记录插入1000次)
    ASSERT_EQ(totalRows, 1000);
}

TEST_F(TableReadTest, TestSplitOptionsRowOffsetMode)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ROW_OFFSET模式下的SplitOptions
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    // 验证在ROW_OFFSET模式下调用GetSplitCount会抛出异常
    EXPECT_THROW(splits->GetSplitCount(), OdpsException);
    // 验证在ROW_OFFSET模式下通过调用GetSplit(index)会抛出异常
    EXPECT_THROW(splits->GetSplit(0), OdpsException);

    // 验证可以正常获取split
    for (int32_t i = 0; i < 10; i++) {
        auto split = splits->GetSplit(i, 100);
        ASSERT_NE(split, nullptr);
    }
}

TEST_F(TableReadTest, TestSplitOptionsSize)
{
    CreateTestTable();
    InsertTestData(1000);

    // 测试PARALLELISM模式下的SplitOptions
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::SIZE;
    splitOptions.mSplitSize = 1024 * 1024 * 1024;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    // 验证在PARALLELISM模式下能正确获取split数量
    EXPECT_GT(splits->GetSplitCount(), 0);
    EXPECT_EQ(splits->GetSplitMode(), SplitMode::SIZE);

    // 验证可以正常获取split
    for (int32_t i = 0; i < splits->GetSplitCount(); i++) {
        auto split = splits->GetSplit(i);
        ASSERT_NE(split, nullptr);
    }

    // 验证索引越界时抛出异常
    EXPECT_THROW(splits->GetSplit(splits->GetSplitCount()), OdpsException);
    EXPECT_THROW(splits->GetSplit(-1), OdpsException);
}

TEST_F(TableReadTest, TestReadOptionsMaxBatchRows)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试TableReadStreamBuilder的所有方法
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    // 测试SetSplit和SetReadOptions方法
    ReadOptions readOptions;
    readOptions.mMaxBatchRows = 50;

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();

    ASSERT_NE(nullptr, readStream);

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 1000);

    readOptions.mMaxBatchRows = 20001;
    readStream = streamBuilder->SetReadOptions(readOptions).Build();
    ASSERT_NE(nullptr, readStream);
    EXPECT_THROW(readStream->Read(), OdpsTunnelException);
}

TEST_F(TableReadTest, TestReadOptionsSkipRowNum)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ReadOptions的mSkipRowNum参数
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    // 设置跳过前100行
    ReadOptions readOptions;
    readOptions.mSkipRowNum = 100;

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();

    ASSERT_NE(nullptr, readStream);

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 900);
}

TEST_F(TableReadTest, TestReadOptionsMaxBatchRawSize)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ReadOptions的mMaxBatchRawSize参数
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    // 设置最大批次原始大小
    ReadOptions readOptions;
    readOptions.mMaxBatchRawSize = 1024;

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    streamBuilder->SetSplit(split);
    auto readStream = streamBuilder->SetReadOptions(readOptions).Build();
    ASSERT_NE(nullptr, readStream);
    EXPECT_THROW(readStream->Read(), OdpsTunnelException);
    readOptions.mMaxBatchRawSize = 256 * 1024 * 1024 + 1;
    readStream = streamBuilder->SetReadOptions(readOptions).Build();
    ASSERT_NE(nullptr, readStream);
    EXPECT_THROW(readStream->Read(), OdpsTunnelException);

    readOptions.mMaxBatchRawSize = 1024 * 1024;
    readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();

    ASSERT_NE(nullptr, readStream);

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 1000);
}

TEST_F(TableReadTest, TestReadOptionsDataColumns)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ReadOptions的mDataColumns参数
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    // 设置只读取特定列
    ReadOptions readOptions;
    readOptions.mDataColumns = {"col1", "col3"};

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();

    ASSERT_NE(nullptr, readStream);

    auto batch = readStream->Read();
    ASSERT_NE(nullptr, batch);

    // 验证只返回指定的列
    ASSERT_EQ(batch->num_columns(), 2);
    ASSERT_NE(batch->schema()->GetFieldByName("col1"), nullptr);
    ASSERT_NE(batch->schema()->GetFieldByName("col3"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col2"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col4"), nullptr);

    ValidateBatch(batch, {"col1", "col3"});
}

TEST_F(TableReadTest, TestReadOptionsDataColumnsUnordered)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ReadOptions的mDataColumnsUnordered参数
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    // 设置列并启用无序模式
    ReadOptions readOptions;
    readOptions.mDataColumns = {"col3", "col1"}; // 与表中列顺序不同
    readOptions.mDataColumnsUnordered = true;

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();

    ASSERT_NE(nullptr, readStream);
    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col3", "col1"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 1000);
}

TEST_F(TableReadTest, TestReadOptionsReadSplitByRowOffset)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ReadOptions的mCompressOption参数
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0, 200);

    // 设置压缩选项
    ReadOptions readOptions;
    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();
    ASSERT_NE(nullptr, readStream);
    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 200);
    split = splits->GetSplit(200, 300);
    totalRows = 0;
    readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();
    ASSERT_NE(nullptr, readStream);
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 300);
    split = splits->GetSplit(500, 500);
    totalRows = 0;
    readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();
    ASSERT_NE(nullptr, readStream);
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) break;
        ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);
        ValidateBatch(batch, {"col1", "col2", "col3", "col4"});
        totalRows += batch->num_rows();
    }
    ASSERT_EQ(totalRows, 500);
}

TEST_F(TableReadTest, TestReadOptionsAllParameters)
{
    CreateTestTable();
    InsertTestData(1000);
    // 测试ReadOptions的所有参数组合使用
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    // 设置所有ReadOptions参数
    ReadOptions readOptions;
    readOptions.mMaxBatchRows = 100;
    readOptions.mSkipRowNum = 50;
    readOptions.mMaxBatchRawSize = 1024 * 1024;
    readOptions.mDataColumns = {"col2", "col4"};
    readOptions.mDataColumnsUnordered = true;

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split)
                                 .SetReadOptions(readOptions)
                                 .Build();

    ASSERT_NE(nullptr, readStream);

    auto batch = readStream->Read();
    ASSERT_NE(nullptr, batch);

    // 验证参数组合效果
    ASSERT_LE(batch->num_rows(), readOptions.mMaxBatchRows);

    // 验证只返回指定的列且顺序正确
    ASSERT_EQ(batch->num_columns(), 2);
    ASSERT_EQ(batch->schema()->field(0)->name(), "col2");
    ASSERT_EQ(batch->schema()->field(1)->name(), "col4");

    std::shared_ptr<arrow::StringArray> stringArray =
        std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("col2"));
    std::shared_ptr<arrow::BooleanArray> boolArray =
        std::static_pointer_cast<arrow::BooleanArray>(batch->GetColumnByName("col4"));

    for (int64_t row = 0; row < batch->num_rows(); ++row)
    {
        ASSERT_EQ(stringArray->GetString(row), "test_string_2");
        ASSERT_TRUE(boolArray->Value(row));
    }
}

TEST_F(TableReadTest, TestFilterOptionsRequiredDataColumns)
{
    CreateTestTable();
    InsertTestData(100);

    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    // 设置过滤选项，只读取特定数据列
    FilterOptions filterOptions;
    filterOptions.mRequiredDataColumns = {"col1", "col3"};

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mTableName)
                                .SetSplitOptions(splitOptions)
                                .SetFilterOptions(filterOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 100);

    auto split = splits->GetSplit(0,  100);

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split).Build();

    ASSERT_NE(nullptr, readStream);

    auto batch = readStream->Read();
    ASSERT_NE(nullptr, batch);

    // 验证只返回指定的列
    ASSERT_EQ(batch->num_columns(), 2);
    ASSERT_NE(batch->schema()->GetFieldByName("col1"), nullptr);
    ASSERT_NE(batch->schema()->GetFieldByName("col3"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col2"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col4"), nullptr);

    ValidateBatch(batch, {"col1", "col3"});
}

TEST_F(TableReadTest, TestFilterOptionsRequiredPartitionColumns)
{
    CreatePartitionedTestTable();
    InsertPartitionedTestData();

    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    // 设置过滤选项，只读取特定分区列
    FilterOptions filterOptions;
    filterOptions.mRequiredPartitionColumns = {"p1"};

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetSplitOptions(splitOptions)
                                .SetFilterOptions(filterOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 1000);

    auto split = splits->GetSplit(0,  1000);

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split).Build();

    ASSERT_NE(nullptr, readStream);

    auto batch = readStream->Read();
    ASSERT_NE(nullptr, batch);

    // 验证返回所有数据列和指定的分区列
    ASSERT_EQ(batch->num_columns(), 1); // p1
    ASSERT_EQ(batch->schema()->GetFieldByName("col1"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col2"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col3"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("col4"), nullptr);
    ASSERT_NE(batch->schema()->GetFieldByName("p1"), nullptr);
    ASSERT_EQ(batch->schema()->GetFieldByName("p2"), nullptr); // p2不应该出现

    ValidateBatch(batch, {"p1"});
}

TEST_F(TableReadTest, TestFilterOptionsRequiredPartitions)
{
    // 创建分区表并插入数据
    CreatePartitionedTestTable();
    InsertPartitionedTestData();

    // 测试使用FilterOptions读取分区表
    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    // 设置过滤选项，只读取特定分区
    FilterOptions filterOptions;
    filterOptions.mRequiredPartitions = {"p1=partition1/p2=a"};

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetSplitOptions(splitOptions)
                                .SetFilterOptions(filterOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 250);

    auto split = splits->GetSplit(0,  250);

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split).Build();

    ASSERT_NE(nullptr, readStream);

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) {
            break;
        }

        // 验证返回的数据包含分区列
        ValidateBatch(batch, {"col1", "col2", "col3", "col4", "p1", "p2"});

        // 验证分区列的值符合过滤条件
        auto p1Array = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("p1"));
        for (int64_t row = 0; row < batch->num_rows(); ++row) {
            ASSERT_EQ(p1Array->GetString(row), "partition1");
        }

        totalRows += batch->num_rows();
    }

    // 验证只读取一个分区的数据
    ASSERT_EQ(totalRows, 250);
}

TEST_F(TableReadTest, TestFilterOptionsCombinedAllFilters)
{
    CreatePartitionedTestTable();
    InsertPartitionedTestData();

    SplitOptions splitOptions;
    splitOptions.mSplitMode = SplitMode::ROW_OFFSET;

    // 组合使用所有过滤选项
    FilterOptions filterOptions;
    filterOptions.mRequiredDataColumns = {"col2", "col4"};
    filterOptions.mRequiredPartitionColumns = {"p2"};
    filterOptions.mRequiredPartitions = {"p1=partition2/p2=a"};

    auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                .SetProject(mProjectName)
                                .SetSchema("default")
                                .SetTable(mPartitionedTableName)
                                .SetSplitOptions(splitOptions)
                                .SetFilterOptions(filterOptions)
                                .Build();

    auto splits = tableReadSession->GetSplits();
    ASSERT_EQ(splits->GetRecordCount(), 250);

    auto split = splits->GetSplit(0,  250);

    auto streamBuilder = tableReadSession->BuildTableReadStream();
    auto readStream = streamBuilder->SetSplit(split).Build();

    ASSERT_NE(nullptr, readStream);

    int64_t totalRows = 0;
    while (true)
    {
        auto batch = readStream->Read();
        if (batch == nullptr) {
            break;
        }

        // 验证返回指定的数据列和分区列
        ASSERT_EQ(batch->num_columns(), 3); // col2, col4, p2
        ASSERT_EQ(batch->schema()->GetFieldByName("col1"), nullptr);
        ASSERT_NE(batch->schema()->GetFieldByName("col2"), nullptr);
        ASSERT_EQ(batch->schema()->GetFieldByName("col3"), nullptr);
        ASSERT_NE(batch->schema()->GetFieldByName("col4"), nullptr);
        ASSERT_EQ(batch->schema()->GetFieldByName("p1"), nullptr);
        ASSERT_NE(batch->schema()->GetFieldByName("p2"), nullptr);

        // 验证分区列的值符合过滤条件
        auto p2Array = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("p2"));
        for (int64_t row = 0; row < batch->num_rows(); ++row) {
            ASSERT_EQ(p2Array->GetString(row), "a");
        }

        ValidateBatch(batch, {"col2", "col4", "p2"});

        totalRows += batch->num_rows();
    }

    // 验证只读取符合所有过滤条件的数据
    ASSERT_EQ(totalRows, 250);
}

TEST_F(TableReadTest, TestIncrementalReadOptionsCDCMode)
{
    try {
        std::string dropSql = "DROP TABLE IF EXISTS " + mTableName + ";";
        std::cout << "Executing SQL: " << dropSql << std::endl;
        ExecuteSql(dropSql);

        std::string createSql =
            "CREATE TABLE " + mTableName +
            " (id BIGINT PRIMARY KEY NOT NULL, val STRING) "
            "TBLPROPERTIES (\"transactional\"=\"true\", \"acid.cdc.mode.enable\"=\"true\", "
            "\"acid.cdc.build.async\"=\"false\", \"cdc.insert.into.passthrough.enable\"=\"true\", "
            "\"write.bucket.num\" = \"16\");";
        std::cout << "Executing SQL: " << createSql << std::endl;
        ExecuteSql(createSql);

        // 设置增量读取选项 - CDC模式，从版本0开始
        IncrementalReadOptions incrementalOptions;
        incrementalOptions.mEnableIncrementalRead = true;
        incrementalOptions.mStartVersion = 0;
        incrementalOptions.mMode = IncrementalReadMode::CDC;

        SplitOptions splitOptions;
        splitOptions.mSplitMode = SplitMode::BUCKET;

        auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        auto splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 0);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 1);

        // 插入初始数据
        std::string insertSql = "INSERT INTO TABLE " + mTableName + " VALUES (1, '1');";
        std::cout << "Executing SQL: " << insertSql << std::endl;
        ExecuteSql(insertSql);

        // 设置增量读取选项 - CDC模式，从版本1开始
        incrementalOptions.mStartVersion = 1;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 2);

        auto split = splits->GetSplit(0);
        auto streamBuilder = tableReadSession->BuildTableReadStream();
        auto readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        int64_t count = 0;
        auto batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 1);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "1");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2); // insert操作
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);
        readStream->Close();

        // 插入第二行数据
        insertSql = "INSERT INTO TABLE " + mTableName + " VALUES (2, '2');";
        std::cout << "Executing SQL: " << insertSql << std::endl;
        ExecuteSql(insertSql);

        // 设置增量读取选项 - CDC模式，从版本2开始
        incrementalOptions.mStartVersion = 2;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 3);

        split = splits->GetSplit(0);
        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 2);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "2");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2); // insert操作
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);
        readStream->Close();

        // 删除一行数据
        auto deleteSql = "DELETE FROM " + mTableName + " WHERE id = 2;";
        std::cout << "Executing SQL: " << deleteSql << std::endl;
        ExecuteSql(deleteSql);

        // 设置增量读取选项 - CDC模式，从版本3开始
        incrementalOptions.mStartVersion = 3;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 4);

        split = splits->GetSplit(0);
        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 2);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "2");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 4); // delete操作
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);
        readStream->Close();

        // 更新一行数据
        auto updateSql = "UPDATE " + mTableName + " SET val = '3' WHERE id = 1;";
        std::cout << "Executing SQL: " << updateSql << std::endl;
        ExecuteSql(updateSql);

        // 设置增量读取选项 - CDC模式，从版本4开始
        incrementalOptions.mStartVersion = 4;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        split = splits->GetSplit(0);
        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 2);

            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));

            // 第一行：update_before
            ASSERT_EQ(idArray->Value(0), 1);
            ASSERT_EQ(valArray->GetString(0), "1");
            ASSERT_EQ(metaOpArray->Value(0), 16); // update_before操作

            // 第二行：update_after
            ASSERT_EQ(idArray->Value(1), 1);
            ASSERT_EQ(valArray->GetString(1), "3");
            ASSERT_EQ(metaOpArray->Value(1), 32); // update_after操作

            count += batch->num_rows();
        }
        ASSERT_EQ(count, 2);
        readStream->Close();

        // 从版本0读取所有增量数据
        incrementalOptions.mStartVersion = 0;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 2);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        count = 0;
        int splitCount = 0;
        for (int i = 0; i < splits->GetSplitCount(); i++) {
            split = splits->GetSplit(i);
            streamBuilder = tableReadSession->BuildTableReadStream();
            readStream = streamBuilder->SetSplit(split).Build();
            ASSERT_NE(nullptr, readStream);

            // 读取数据
            do {
                batch = readStream->Read();
                if (batch != nullptr) {
                    count += batch->num_rows();
                }
            } while (batch != nullptr);
            readStream->Close();
            splitCount++;
        }
        ASSERT_EQ(count, 5);
        ASSERT_EQ(splitCount, 2);

    } catch (const std::exception& e) {
        std::cerr << "Failed to test incremental read with CDC mode: " << e.what() << std::endl;
        throw;
    }
}

TEST_F(TableReadTest, TestIncrementalReadOptionsCDCModeWithPartition)
{
    try {
        std::string dropSql = "DROP TABLE IF EXISTS " + mPartitionedTableName + ";";
        std::cout << "Executing SQL: " << dropSql << std::endl;
        ExecuteSql(dropSql);

        std::string createSql =
            "CREATE TABLE " + mPartitionedTableName +
            " (id BIGINT PRIMARY KEY NOT NULL, val STRING) PARTITIONED BY (pt STRING) "
            "TBLPROPERTIES (\"transactional\"=\"true\", \"acid.cdc.mode.enable\"=\"true\", "
            "\"acid.cdc.build.async\"=\"false\", \"cdc.insert.into.passthrough.enable\"=\"true\", "
            "\"write.bucket.num\" = \"16\");";
        std::cout << "Executing SQL: " << createSql << std::endl;
        ExecuteSql(createSql);

        // 设置增量读取选项 - CDC模式，从版本0开始
        IncrementalReadOptions incrementalOptions;
        incrementalOptions.mEnableIncrementalRead = true;
        incrementalOptions.mStartVersion = 0;
        incrementalOptions.mMode = IncrementalReadMode::CDC;

        SplitOptions splitOptions;
        splitOptions.mSplitMode = SplitMode::BUCKET;

        auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        auto splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 0);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 1);

        // 插入pt=pt1分区的数据
        std::string insertSql = "INSERT INTO TABLE " + mPartitionedTableName + " PARTITION (pt = 'pt1') VALUES (1, '1');";
        std::cout << "Executing SQL: " << insertSql << std::endl;
        ExecuteSql(insertSql);

        // 设置增量读取选项 - CDC模式，从版本1开始，只读取pt=pt1分区
        incrementalOptions.mStartVersion = 1;
        FilterOptions singlePartitionFilter;
        singlePartitionFilter.mRequiredPartitions.push_back("pt=pt1");

        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetFilterOptions(singlePartitionFilter)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 2);

        auto split = splits->GetSplit(0);
        auto streamBuilder = tableReadSession->BuildTableReadStream();
        auto readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        int64_t count = 0;
        auto batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 1);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "1");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2); // insert操作

            // 检查分区列值
            auto ptArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("pt"));
            ASSERT_EQ(ptArray->GetString(0), "pt1");
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);
        readStream->Close();

        // 插入pt=pt2分区的数据
        insertSql = "INSERT INTO TABLE " + mPartitionedTableName + " PARTITION (pt = 'pt2') VALUES (2, '2');";
        std::cout << "Executing SQL: " << insertSql << std::endl;
        ExecuteSql(insertSql);

        // 设置过滤选项和分区列表
        FilterOptions filterOptions;
        filterOptions.mRequiredPartitions.push_back("pt=pt1");
        filterOptions.mRequiredPartitions.push_back("pt=pt2");

        // 设置增量读取选项 - CDC模式，从版本2开始，读取所有指定分区
        incrementalOptions.mStartVersion = 2;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetFilterOptions(filterOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 3);

        split = splits->GetSplit(0);
        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 2);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "2");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2); // insert操作

            // 检查分区列值
            auto ptArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("pt"));
            ASSERT_EQ(ptArray->GetString(0), "pt2");
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);
        readStream->Close();

        // 删除pt=pt2分区中id=2的数据
        auto deleteSql = "DELETE FROM " + mPartitionedTableName + " WHERE id = 2;";
        std::cout << "Executing SQL: " << deleteSql << std::endl;
        ExecuteSql(deleteSql);

        // 设置增量读取选项 - CDC模式，从版本3开始
        incrementalOptions.mStartVersion = 3;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetFilterOptions(filterOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 4);

        split = splits->GetSplit(0);
        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 2);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "2");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 4); // delete操作

            // 检查分区列值
            auto ptArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("pt"));
            ASSERT_EQ(ptArray->GetString(0), "pt2");
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);
        readStream->Close();

        // 更新pt=pt1分区中id=1的数据
        auto updateSql = "UPDATE " + mPartitionedTableName + " SET val = '3' WHERE id = 1;";
        std::cout << "Executing SQL: " << updateSql << std::endl;
        ExecuteSql(updateSql);

        // 设置增量读取选项 - CDC模式，从版本4开始
        incrementalOptions.mStartVersion = 4;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetFilterOptions(filterOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        split = splits->GetSplit(0);
        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // CDC模式下应该包含系统列
            ASSERT_GE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 2);

            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));

            // 第一行：update_before
            ASSERT_EQ(idArray->Value(0), 1);
            ASSERT_EQ(valArray->GetString(0), "1");
            ASSERT_EQ(metaOpArray->Value(0), 16); // update_before操作

            // 第二行：update_after
            ASSERT_EQ(idArray->Value(1), 1);
            ASSERT_EQ(valArray->GetString(1), "3");
            ASSERT_EQ(metaOpArray->Value(1), 32); // update_after操作

            // 检查分区列值
            auto ptArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("pt"));
            ASSERT_EQ(ptArray->GetString(0), "pt1");
            ASSERT_EQ(ptArray->GetString(1), "pt1");

            count += batch->num_rows();
        }
        ASSERT_EQ(count, 2);
        readStream->Close();

        // 从版本0读取所有增量数据
        incrementalOptions.mStartVersion = 0;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mPartitionedTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetFilterOptions(filterOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;

        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 2);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        count = 0;
        int splitCount = 0;
        std::vector<std::shared_ptr<arrow::RecordBatch>> batchList;

        for (int i = 0; i < splits->GetSplitCount(); i++) {
            split = splits->GetSplit(i);
            streamBuilder = tableReadSession->BuildTableReadStream();
            readStream = streamBuilder->SetSplit(split).Build();
            ASSERT_NE(nullptr, readStream);

            // 读取数据
            do {
                batch = readStream->Read();
                if (batch != nullptr) {
                    batchList.push_back(batch);
                    count += batch->num_rows();
                }
            } while (batch != nullptr);
            readStream->Close();
            splitCount++;
        }

        ASSERT_EQ(count, 5);
        ASSERT_EQ(splitCount, 2);

        // 清理测试表
        dropSql = "DROP TABLE IF EXISTS " + mPartitionedTableName + ";";
        std::cout << "Executing SQL: " << dropSql << std::endl;
        ExecuteSql(dropSql);

    } catch (const std::exception& e) {
        std::cerr << "Failed to test incremental read with CDC mode on partitioned table: " << e.what() << std::endl;
        throw;
    }
}

TEST_F(TableReadTest, TestIncrementalReadOptionsAppendMode)
{
    // 创建支持事务的表
    try {
        std::string dropSql = "DROP TABLE IF EXISTS " + mTableName + ";";
        ExecuteSql(dropSql);

        std::string createSql =
            "CREATE TABLE " + mTableName +
            " (id BIGINT PRIMARY KEY NOT NULL, val STRING) "
            "TBLPROPERTIES (\"transactional\"=\"true\", \"acid.cdc.mode.enable\"=\"true\", "
            "\"acid.cdc.build.async\"=\"false\", \"cdc.insert.into.passthrough.enable\"=\"true\", "
            "\"write.bucket.num\" = \"16\");";
        std::cout << createSql << std::endl;
        ExecuteSql(createSql);

        // 设置增量读取选项 - Append模式
        IncrementalReadOptions incrementalOptions;
        incrementalOptions.mEnableIncrementalRead = true;
        incrementalOptions.mStartVersion = 0;
        incrementalOptions.mMode = IncrementalReadMode::APPEND;

        SplitOptions splitOptions;
        splitOptions.mSplitMode = SplitMode::BUCKET;

        auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << tableReadSession->GetSessionId() << std::endl;
        auto splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 0);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 1);

        // 插入初始数据
        std::string insertSql = "INSERT INTO TABLE " + mTableName + " VALUES (1, '1');";
        std::cout << insertSql << std::endl;
        ExecuteSql(insertSql);

        incrementalOptions.mStartVersion = 1;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 2);

        auto split = splits->GetSplit(0);

        auto streamBuilder = tableReadSession->BuildTableReadStream();
        auto readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        int64_t count = 0;
        auto batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_NE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 1);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "1");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2);
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);

        readStream->Close();

        // 插入第二行数据
        insertSql = "INSERT INTO TABLE " + mTableName + " VALUES (2, '2');";
        std::cout << insertSql << std::endl;
        ExecuteSql(insertSql);

        incrementalOptions.mStartVersion = 2;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 3);

        split = splits->GetSplit(0);

        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_NE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 2);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "2");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2);
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);

        readStream->Close();

        // 删除一行数据
        auto deleteSql = "DELETE FROM " + mTableName + " WHERE id = 2;";
        std::cout << deleteSql << std::endl;
        ExecuteSql(deleteSql);

        incrementalOptions.mStartVersion = 3;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 4);

        split = splits->GetSplit(0);

        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_EQ(batch->num_rows(), 0);
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 0);

        readStream->Close();

        // 更新一行数据
        auto updateSql = "UPDATE " + mTableName + " SET val = '3' WHERE id = 1;";
        std::cout << updateSql << std::endl;
        ExecuteSql(updateSql);

        incrementalOptions.mStartVersion = 4;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        split = splits->GetSplit(0);

        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_NE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 1);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "3");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 8);
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);

        readStream->Close();

        // 从verion0读取全量数据
        incrementalOptions.mStartVersion = 0;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();
        std::cout << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_GE(splits->GetSplitCount(), 1);
        ASSERT_LE(splits->GetSplitCount(), 2);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        count = 0;
        for (int i = 0; i < splits->GetSplitCount(); i++) {
            split = splits->GetSplit(i);

            streamBuilder = tableReadSession->BuildTableReadStream();
            readStream = streamBuilder->SetSplit(split).Build();
            ASSERT_NE(nullptr, readStream);

            // 读取数据
            batch = readStream->Read();
            if (batch != nullptr && batch->num_rows() > 0) {
                // Append模式下应该包含系统列
                ASSERT_NE(batch->num_columns(), 3);
                ASSERT_EQ(batch->num_rows(), 1);
                auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
                ASSERT_EQ(idArray->Value(0), 1);
                auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
                ASSERT_EQ(valArray->GetString(0), "3");
                auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
                ASSERT_EQ(metaOpArray->Value(0), 8);
                count += batch->num_rows();
            }
        }

        ASSERT_EQ(count, 1);

        readStream->Close();

    } catch (const std::exception& e) {
        std::cerr << "Failed to test incremental read with Append mode: " << e.what() << std::endl;
        throw;
    }
}

TEST_F(TableReadTest, TestIncrementalReadOptionsWithTimestamp)
{
    try {
        std::string dropSql = "DROP TABLE IF EXISTS " + mTableName + ";";
        std::cout << "Executing SQL: " << dropSql << std::endl;
        ExecuteSql(dropSql);

        std::string createSql =
            "CREATE TABLE " + mTableName +
            " (id BIGINT PRIMARY KEY NOT NULL, val STRING) "
            "TBLPROPERTIES (\"transactional\"=\"true\", \"acid.cdc.mode.enable\"=\"true\", "
            "\"acid.cdc.build.async\"=\"false\", \"cdc.insert.into.passthrough.enable\"=\"true\", "
            "\"write.bucket.num\" = \"16\");";
        std::cout << "Executing SQL: " << createSql << std::endl;
        ExecuteSql(createSql);

        // 获取初始时间戳
        auto initTime = GetCurrentTimestamp();
        std::cout << "initTime: " << initTime << std::endl;

        // 等待一小段时间确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto startTimestamp = GetCurrentTimestamp();
        std::cout << "startTimestamp: " << startTimestamp << std::endl;

        // 设置增量读取选项 - 基于时间戳的Append模式
        IncrementalReadOptions incrementalOptions;
        incrementalOptions.mEnableIncrementalRead = true;
        incrementalOptions.mStartTimeStamp = startTimestamp;
        incrementalOptions.mMode = IncrementalReadMode::APPEND;

        SplitOptions splitOptions;
        splitOptions.mSplitMode = SplitMode::BUCKET;

        auto tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();

        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;
        auto splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 0);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 1);

        // 等待一小段时间确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        startTimestamp = GetCurrentTimestamp();
        std::cout << "startTimestamp: " << startTimestamp << std::endl;

        // 插入初始数据
        std::string insertSql = "INSERT INTO TABLE " + mTableName + " VALUES (1, '1');";
        std::cout << "Executing SQL: " << insertSql << std::endl;
        ExecuteSql(insertSql);

        // 设置增量读取选项 - Append模式，基于时间戳
        incrementalOptions.mStartTimeStamp = startTimestamp;
        incrementalOptions.mMode = IncrementalReadMode::APPEND;

        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();

        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 2);

        auto split = splits->GetSplit(0);
        auto streamBuilder = tableReadSession->BuildTableReadStream();
        auto readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        int64_t count = 0;
        auto batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_NE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 1);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "1");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2); // insert操作
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);

        readStream->Close();

        // 等待一小段时间确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        startTimestamp = GetCurrentTimestamp();
        std::cout << "startTimestamp: " << startTimestamp << std::endl;

        // 插入第二行数据
        insertSql = "INSERT INTO TABLE " + mTableName + " VALUES (2, '2');";
        std::cout << "Executing SQL: " << insertSql << std::endl;
        ExecuteSql(insertSql);

        incrementalOptions.mStartTimeStamp = startTimestamp;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();

        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 3);

        split = splits->GetSplit(0);

        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_NE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 2);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "2");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 2); // insert操作
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);

        readStream->Close();

        // 等待一小段时间确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        startTimestamp = GetCurrentTimestamp();
        std::cout << "startTimestamp: " << startTimestamp << std::endl;

        // 删除一行数据
        auto deleteSql = "DELETE FROM " + mTableName + " WHERE id = 2;";
        std::cout << "Executing SQL: " << deleteSql << std::endl;
        ExecuteSql(deleteSql);

        incrementalOptions.mStartTimeStamp = startTimestamp;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();

        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 4);

        split = splits->GetSplit(0);

        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下删除操作不返回数据行
            ASSERT_EQ(batch->num_rows(), 0);
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 0);

        readStream->Close();

        // 等待一小段时间确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        startTimestamp = GetCurrentTimestamp();
        std::cout << "startTimestamp: " << startTimestamp << std::endl;

        // 更新一行数据
        auto updateSql = "UPDATE " + mTableName + " SET val = '3' WHERE id = 1;";
        std::cout << "Executing SQL: " << updateSql << std::endl;
        ExecuteSql(updateSql);

        incrementalOptions.mStartTimeStamp = startTimestamp;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();

        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_EQ(splits->GetSplitCount(), 1);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        split = splits->GetSplit(0);

        streamBuilder = tableReadSession->BuildTableReadStream();
        readStream = streamBuilder->SetSplit(split).Build();
        ASSERT_NE(nullptr, readStream);

        // 读取数据
        count = 0;
        batch = readStream->Read();
        if (batch != nullptr) {
            // Append模式下应该包含系统列
            ASSERT_NE(batch->num_columns(), 3);
            ASSERT_EQ(batch->num_rows(), 1);
            auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
            ASSERT_EQ(idArray->Value(0), 1);
            auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
            ASSERT_EQ(valArray->GetString(0), "3");
            auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
            ASSERT_EQ(metaOpArray->Value(0), 8); // update操作
            count += batch->num_rows();
        }
        ASSERT_EQ(count, 1);

        readStream->Close();

        // 从初始时间读取全量数据
        incrementalOptions.mStartTimeStamp = initTime;
        tableReadSession = (*mMaxStorageApi.BuildTableReadSession())
                                    .SetProject(mProjectName)
                                    .SetSchema("default")
                                    .SetTable(mTableName)
                                    .SetSplitOptions(splitOptions)
                                    .SetIncrementalReadOptions(incrementalOptions)
                                    .Build();

        std::cout << "Session ID: " << tableReadSession->GetSessionId() << std::endl;
        splits = tableReadSession->GetSplits();
        ASSERT_NE(nullptr, splits);
        ASSERT_GE(splits->GetSplitCount(), 1);
        ASSERT_LE(splits->GetSplitCount(), 2);
        ASSERT_EQ(tableReadSession->GetIncrementalInfo().mLatestVersion, 5);

        count = 0;
        for (int i = 0; i < splits->GetSplitCount(); i++) {
            split = splits->GetSplit(i);

            streamBuilder = tableReadSession->BuildTableReadStream();
            readStream = streamBuilder->SetSplit(split).Build();
            ASSERT_NE(nullptr, readStream);

            // 读取数据
            batch = readStream->Read();
            if (batch != nullptr && batch->num_rows() > 0) {
                // Append模式下应该包含系统列
                ASSERT_NE(batch->num_columns(), 3);
                ASSERT_EQ(batch->num_rows(), 1);
                auto idArray = std::static_pointer_cast<arrow::Int64Array>(batch->GetColumnByName("id"));
                ASSERT_EQ(idArray->Value(0), 1);
                auto valArray = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("val"));
                ASSERT_EQ(valArray->GetString(0), "3");
                auto metaOpArray = std::static_pointer_cast<arrow::Int8Array>(batch->GetColumnByName("__meta_op_type"));
                ASSERT_EQ(metaOpArray->Value(0), 8); // update操作
                count += batch->num_rows();
            }
        }

        ASSERT_EQ(count, 1);

        readStream->Close();

    } catch (const std::exception& e) {
        std::cerr << "Failed to test incremental read with timestamp: " << e.what() << std::endl;
        throw;
    }
}

// 测试无谓词情况
TEST_F(TableReadTest, TestNoPredicatePushdown) {
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, IPredicate::NO_PREDICATE);
    EXPECT_EQ(100000, readerRowCount);
}

// 测试简单谓词下推
TEST_F(TableReadTest, TestPredicatePushdown) {
    auto predicate = IBinaryPredicate::GreaterThan("c1", "20000");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(80000, readerRowCount); // c1 > 20000
}

// 测试复合谓词下推
TEST_F(TableReadTest, TestPredicatePushdown2) {
    auto c1 = IBinaryPredicate::GreaterThan("c1", "20000");
    auto c2 = IBinaryPredicate::LessThan("c2", "100000");
    std::vector<IPredicatePtr> predicates = {c1, c2};
    auto predicate = ICompoundPredicate::And(predicates);

    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(30000, readerRowCount); // c1 > 20000 and c2 < 1000000
}

// 测试矛盾谓词
TEST_F(TableReadTest, TestPredicatePushdown3) {
    auto c1 = IBinaryPredicate::GreaterThan("c1", "20000");
    auto c2 = IBinaryPredicate::LessThan("c1", "15000");
    std::vector<IPredicatePtr> predicates = {c1, c2};
    auto predicate = ICompoundPredicate::And(predicates);

    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(0, readerRowCount); // c1 > 20000 and c1 < 15000
}

// 测试多层复合谓词
TEST_F(TableReadTest, TestPredicatePushdownWithMultiCompoundPredicate) {
    auto c1 = IBinaryPredicate::GreaterThan("c1", "20000");
    auto c2 = IBinaryPredicate::LessThan("c1", "15000");
    std::vector<IPredicatePtr> innerPredicates = {c1, c2};
    auto innerPredicate = ICompoundPredicate::And(innerPredicates);

    auto outerPredicate = ICompoundPredicate::Or({innerPredicate});

    auto predicate = ICompoundPredicate::And({outerPredicate});

    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(0, readerRowCount); // (((c1 > 20000) and (c1 < 15000)))
}

// 测试总是为真的谓词
TEST_F(TableReadTest, TestPredicatePushdownAlwaysTrue) {
    auto c1 = IBinaryPredicate::GreaterThan("c1", "20000");
    auto c2 = IPredicate::NO_PREDICATE;
    auto orPredicate = ICompoundPredicate::Or({c1, c2});

    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, orPredicate);
    EXPECT_EQ(100000, readerRowCount); // NO_PREDICATE

    auto andPredicate = ICompoundPredicate::And({c1, c2});

    readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, andPredicate);
    EXPECT_EQ(80000, readerRowCount); // (c1 > 20000)
}

// 测试等值谓词
TEST_F(TableReadTest, TestPredicatePushdown4) {
    auto predicate = IBinaryPredicate::Equals("c1", "20000");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(10000, readerRowCount); // c1 = 20000
}

// 测试一元谓词
TEST_F(TableReadTest, TestPredicatePushdown5) {
    auto predicate = IUnaryPredicate::NotNull("c1");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(100000, readerRowCount); // c1 is not null
}

// 测试IN谓词
TEST_F(TableReadTest, TestPredicatePushdown6) {
    std::vector<std::string> values = {"1", "10001"};
    auto predicate = IInPredicate::In("c1", values);
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_EQ(20000, readerRowCount); // c1 in (1, 10001)
}

// 测试Datetime
TEST_F(TableReadTest, TestPredicatePushdownDatetime) {
    auto predicate = IBinaryPredicate::GreaterThanOrEqual("c3", "'2024-01-01 00:00:00'");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTable, predicate);
    EXPECT_LT(readerRowCount, 100000); // c3 >= '2024-01-01 00:00:00'
}

// 测试hash clustered table
TEST_F(TableReadTest, TestPredicatePushdownHashClusterTable) {
    auto predicate = IBinaryPredicate::Equals("c1", "20000");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTableHashCluster, predicate);
    EXPECT_LT(readerRowCount, 2000); // c1 = 20000
}

TEST_F(TableReadTest, TestPredicatePushdownHashClusterTable2) {
    auto predicate = IBinaryPredicate::Equals("c2", "20000");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTableHashCluster, predicate);
    EXPECT_LE(readerRowCount, 100000); // c2 = 20000
}

// 测试range clustered table
TEST_F(TableReadTest, TestPredicatePushdownRangeClusterTable) {
    auto predicate = IBinaryPredicate::Equals("c1", "20000");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTableRangeCluster, predicate);
    EXPECT_LT(readerRowCount, 2000); // c1 = 20000
}

TEST_F(TableReadTest, TestPredicatePushdownRangeClusterTable2) {
    auto predicate = IBinaryPredicate::Equals("c2", "20000");
    long readerRowCount = GetRowCountWithPredicate(sPredicateTestTableRangeCluster, predicate);
    EXPECT_LT(readerRowCount, 2000); // c2 = 20000
}
