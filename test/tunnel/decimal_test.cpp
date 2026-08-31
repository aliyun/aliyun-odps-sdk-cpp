#include <stdlib.h>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "include/odps_api.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"
#include "include/odps_types.h"
#include "tunnel/arrow_meta_helper.h"

#include "tunnel/util.h"

// Decimal256 test data: for decimal(76, 38)
// precision=76, scale=38, so max 38 integer digits and 38 decimal digits
static const std::vector<std::string> DECIMAL_76_TEST_VALUES = {
    "12345678901234567890123456789012345678.56789012345678901234567890123456789012",
    "-98765432109876543210987654321098765432.54321098765432109876543210987654321098",
    "0.00000000000000000000000000000000000001",
    "99999999999999999999999999999999999999.99999999999999999999999999999999999999",
    "0.00000000000000000000000000000000000000",
    "1.23456789012345678901234567890123456789",
    "-0.99999999999999999999999999999999999999",
};

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

class DecimalTest : public testing::Test
{
protected:
    static std::string sProjectName;
    static std::string sTableName;
    static OdpsTunnel  sTunnel;
    static IODPSPtr    sODPS;
    static std::map<std::string, std::string> sHints;

    static void SetUpTestCase()
    {
        sHints = {
            {"odps.sql.type.system.odps2", "true"},
            {"odps.sql.decimal.odps2", "true"},
            {"odps.sql.decimal256.enable", "true"}
        };
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        sProjectName = Utils::GetProjectName();
        sTableName = Utils::GetRandomTableName();
        sTunnel = Utils::GetTunnelInstance();
        sODPS = Utils::GetODPS(sProjectName);
    }

    virtual void TearDown()
    {
        ExecSql("drop table if exists " + sTableName);
    }

    // Execute SQL via SDK API (no odpscmd dependency)
    void ExecSql(const std::string& sql)
    {
        std::string sqlWithSemicolon = sql;
        if (!sqlWithSemicolon.empty() && sqlWithSemicolon.back() != ';')
        {
            sqlWithSemicolon += ";";
        }
        ISQLTaskPtr sqlTask = ISQLTask::Create();
        IODPSInstancePtr instance = sqlTask->Run(sODPS, sqlWithSemicolon, sHints);
        instance->WaitForSuccess(120000);  // 2 minutes timeout
    }
};

std::string DecimalTest::sProjectName;
std::string DecimalTest::sTableName;
OdpsTunnel  DecimalTest::sTunnel;
IODPSPtr    DecimalTest::sODPS;
std::map<std::string, std::string> DecimalTest::sHints;

#ifdef ODPS_SDK_ENABLE_ARROW
TEST_F(DecimalTest, TestParseTypeInfo)
{
    string decimalTypeSpec = "decimal";
    ODPSColumnTypeInfo colTypeInfo = ODPSColumnTypeInfo::ParseTypeInfoString(decimalTypeSpec);
    EXPECT_EQ(colTypeInfo.mPrecision, 54);
    EXPECT_EQ(colTypeInfo.mScale, 18);
    EXPECT_TRUE(colTypeInfo.IsLegacyDecimal());

    auto dataTypePtr = TransformToArrowType(colTypeInfo);
    auto decimalTypePtr = dynamic_pointer_cast<arrow::Decimal128Type>(dataTypePtr);
    EXPECT_EQ(decimalTypePtr->precision(), 38);
    EXPECT_EQ(decimalTypePtr->scale(), 18);
}
#endif

TEST_F(DecimalTest, NewDecimalOverflow)
{
    ExecSql("create table " + sTableName + "(a decimal(3, 2)) stored as aliorc");
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    ODPSTableRecordPtr record = upload->CreateBufferRecord();
    record->SetDecimalValue(0, "12.34"); // this should properly overflow
    try
    {
        IRecordWriterPtr writer = upload->OpenWriter(0);
        writer->Write(*record);
        writer->Close();
        ASSERT_TRUE(false) << "an exception should be thrown due to data overflow.";
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ(DATA_STORE_ERROR, ex.GetErrorCode());
        ASSERT_TRUE(ex.GetErrorMsg().find("Data overflow") != std::string::npos);
    }
}

// Test Decimal256: precision=76, scale=38 (precision > 38 requires Decimal256 support)
TEST_F(DecimalTest, Decimal256ReadWrite)
{
    // Create table with decimal(76, 38) - high precision decimal requiring Decimal256
    ExecSql("create table " + sTableName + " (id bigint, dec_value decimal(76, 38)) stored as aliorc");

    // Upload test data
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    IRecordWriterPtr writer = upload->OpenWriter(0);
    ODPSTableRecordPtr record = upload->CreateBufferRecord();

    for (size_t i = 0; i < DECIMAL_76_TEST_VALUES.size(); ++i)
    {
        record->SetBigIntValue(0, static_cast<int64_t>(i));
        record->SetDecimalValue(1, DECIMAL_76_TEST_VALUES[i]);
        writer->Write(*record);
    }
    writer->Close();
    upload->Commit({0});

    // Download and verify
    IDownloadPtr download = sTunnel.CreateDownload(sProjectName, sTableName);
    ASSERT_EQ(download->GetRecordCount(), DECIMAL_76_TEST_VALUES.size());

    IRecordReaderPtr reader = download->OpenReader(0, DECIMAL_76_TEST_VALUES.size());
    ODPSTableRecordPtr readRecord = reader->CreateBufferRecord();

    for (size_t i = 0; i < DECIMAL_76_TEST_VALUES.size(); ++i)
    {
        ASSERT_TRUE(reader->Read(*readRecord));
        ASSERT_EQ(static_cast<int64_t>(i), *readRecord->GetBigIntValue(0));

        std::string expectedValue = DECIMAL_76_TEST_VALUES[i];
        std::string actualValue = readRecord->GetDecimal(1);
        ASSERT_EQ(expectedValue, actualValue)
            << "Decimal256 value mismatch at index " << i
            << ", expected: " << expectedValue
            << ", actual: " << actualValue;
    }
    reader->Close();
    download->Complete();
}

// Test Decimal256 null values
TEST_F(DecimalTest, Decimal256NullValueReadWrite)
{
    ExecSql("create table " + sTableName + " (id bigint, dec_value decimal(76, 38)) stored as aliorc");

    // Upload data with null values
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    IRecordWriterPtr writer = upload->OpenWriter(0);
    ODPSTableRecordPtr record = upload->CreateBufferRecord();

    // Write some non-null values
    record->SetBigIntValue(0, 0);
    record->SetDecimalValue(1, DECIMAL_76_TEST_VALUES[0]);
    writer->Write(*record);

    // Write null value
    record->SetBigIntValue(0, 1);
    record->SetNullValue(1);
    writer->Write(*record);

    // Write another non-null value
    record->SetBigIntValue(0, 2);
    record->SetDecimalValue(1, DECIMAL_76_TEST_VALUES[1]);
    writer->Write(*record);

    writer->Close();
    upload->Commit({0});

    // Download and verify
    IDownloadPtr download = sTunnel.CreateDownload(sProjectName, sTableName);
    ASSERT_EQ(download->GetRecordCount(), static_cast<uint64_t>(3));

    IRecordReaderPtr reader = download->OpenReader(0, 3);
    ODPSTableRecordPtr readRecord = reader->CreateBufferRecord();

    // First record: non-null
    ASSERT_TRUE(reader->Read(*readRecord));
    ASSERT_EQ(0, *readRecord->GetBigIntValue(0));
    ASSERT_FALSE(readRecord->IsNullValue(1));
    ASSERT_EQ(DECIMAL_76_TEST_VALUES[0], readRecord->GetDecimal(1));

    // Second record: null
    ASSERT_TRUE(reader->Read(*readRecord));
    ASSERT_EQ(1, *readRecord->GetBigIntValue(0));
    ASSERT_TRUE(readRecord->IsNullValue(1));

    // Third record: non-null
    ASSERT_TRUE(reader->Read(*readRecord));
    ASSERT_EQ(2, *readRecord->GetBigIntValue(0));
    ASSERT_FALSE(readRecord->IsNullValue(1));
    ASSERT_EQ(DECIMAL_76_TEST_VALUES[1], readRecord->GetDecimal(1));

    reader->Close();
    download->Complete();
}
