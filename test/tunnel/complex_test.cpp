#include <cstdlib>
#include <vector>
#include <string>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

class TunnelComplexTypeTest : public testing::Test
{
protected:
    static std::string sCleanUpTableName;
    static std::string sProjectName;
    static OdpsTunnel  sTunnel;
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        sProjectName = Utils::GetProjectName();
        sTunnel = Utils::GetTunnelInstance();
        sCleanUpTableName = "";
    }

    virtual void TearDown()
    {
        if (sCleanUpTableName.size() != 0)
        {
            Utils::DropTableIfExists(sCleanUpTableName);
        }
    }

    static shared_ptr<ODPSArray> BuildBigIntArray(size_t size)
    {
        string bigintArrTypeSpec = "ARRAY<BIGINT>";
        ODPSColumnTypeInfo bigintArrType = ODPSColumnTypeInfo::ParseTypeInfoString(bigintArrTypeSpec);
        // Create Array
        shared_ptr<ODPSArray> bigintArray = std::make_shared<ODPSArray>(bigintArrType);
        for (size_t i = 0; i < size; i++)
        {
            bigintArray->AppendBigIntValue(i);
        }
        return bigintArray;
    }

    static bool CheckBigIntArray(const ODPSArray& bigintArray)
    {
        if (bigintArray.GetElementType() != ODPS_BIGINT)
        {
            EXPECT_EQ(bigintArray.GetElementType(), ODPS_BIGINT);
            return false;
        }
        for (uint32_t i = 0; i < bigintArray.Size(); i++)
        {
            if (bigintArray.GetBigInt(i) != static_cast<int64_t>(i))
            {
                EXPECT_EQ(bigintArray.GetBigInt(i), static_cast<int64_t>(i));
                return false;
            }
        }
        return true;
    }
};

std::string TunnelComplexTypeTest::sCleanUpTableName = "";
std::string TunnelComplexTypeTest::sProjectName;
OdpsTunnel  TunnelComplexTypeTest::sTunnel;

TEST_F(TunnelComplexTypeTest, TestArrayBasicType)
{
    string bigintArrTypeSpec = "ARRAY<BIGINT>";
    ODPSColumnTypeInfo bigintArrType = ODPSColumnTypeInfo::ParseTypeInfoString(bigintArrTypeSpec);
    // Create Array
    ODPSArray bigintArray(bigintArrType);
    // element type assertion
    ASSERT_EQ(bigintArray.GetElementType(), ODPS_BIGINT);
    // append values
    for(int i = 0; i < 10; i++)
    {
        bigintArray.AppendBigIntValue(i);
    }
    // size assertion
    ASSERT_EQ(bigintArray.Size(), 10);
    // read values
    for(int i = 0; i < 10; i++)
    {
        ASSERT_EQ(bigintArray.GetBigInt(i), i);
    }
    // update values
    for(int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            bigintArray.SetBigIntValue(i, 99);
        }
    }
    // size assertion
    ASSERT_EQ(bigintArray.Size(), 10);
    // read values
    for(int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            ASSERT_EQ(bigintArray.GetBigInt(i), 99);
        }
        else
        {
            ASSERT_EQ(bigintArray.GetBigInt(i), i);
        }
    }
    // null values
    for (int i = 5; i < 10; i++)
    {
        bigintArray.SetNull(i);
    }
    // set back
    for (int i = 8; i < 10; i++)
    {
        bigintArray.SetBigIntValue(i, i);
    }
    // check nulls
    for (int i = 5; i < 10; i++)
    {
        if (i >= 8)
        {
            ASSERT_EQ(bigintArray.GetBigInt(i), i);
        }
        else
        {
            ASSERT_THROW(bigintArray.GetBigInt(i), OdpsException);
        }
    }
    // incompat type operations
    EXPECT_THROW(bigintArray.GetInteger(1), OdpsException);
    EXPECT_THROW(bigintArray.SetIntegerValue(2, 10), OdpsException);
    EXPECT_THROW(bigintArray.AppendIntegerValue(100), OdpsException);
    // print the size
    std::cerr << "ArraySize: " << bigintArray.GetInMemorySize() << std::endl;
}

TEST_F(TunnelComplexTypeTest, TestArrayStringType)
{
    string varcharArrTypeSpec = "ARRAY<VARCHAR(10)>";
    ODPSColumnTypeInfo varcharArrType = ODPSColumnTypeInfo::ParseTypeInfoString(varcharArrTypeSpec);
    ODPSArray varcharArray(varcharArrType);
    ASSERT_EQ(varcharArray.GetElementType(), ODPS_VARCHAR);
    for(int i = 0; i < 10; i++)
    {
        string tmpstr = to_string(i);
        varcharArray.AppendVarcharValue(tmpstr);
    }
    // size assertion
    ASSERT_EQ(varcharArray.Size(), 10);
    // read values
    for(int i = 0; i < 10; i++)
    {
        string tmpstr = to_string(i);
        ASSERT_EQ(varcharArray.GetVarchar(i), tmpstr);
    }
    // update values
    for(int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            varcharArray.SetVarcharValue(i, "99");
        }
    }
    // size assertion
    ASSERT_EQ(varcharArray.Size(), 10);
    // check values
    for(int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            ASSERT_EQ(varcharArray.GetVarchar(i), "99");
        }
        else
        {
            ASSERT_EQ(varcharArray.GetVarchar(i), std::to_string(i));
        }
    }
    // null values
    for (int i = 5; i < 10; i++)
    {
        varcharArray.SetNull(i);
    }
    // set back
    for (int i = 8; i < 10; i++)
    {
        varcharArray.SetVarcharValue(i, std::to_string(i));
    }
    // check nulls
    for (int i = 5; i < 10; i++)
    {
        if (i >= 8)
        {
            ASSERT_EQ(varcharArray.GetVarchar(i), std::to_string(i));
        }
        else
        {
            ASSERT_THROW(varcharArray.GetVarchar(i), OdpsTunnelException);
        }
    }
    std::cerr << "ArraySize: " << varcharArray.GetInMemorySize() << std::endl;
}

TEST_F(TunnelComplexTypeTest, TestArrayTimeStampType)
{
    string timestampArrTypeSpec = "ARRAY<TIMESTAMP>";
    ODPSColumnTypeInfo timestampArrType = ODPSColumnTypeInfo::ParseTypeInfoString(timestampArrTypeSpec);
    ODPSArray timestampArray(timestampArrType);
    ASSERT_EQ(timestampArray.GetElementType(), ODPS_TIMESTAMP);
    // append values
    for(int i = 0; i < 10; i++)
    {
        timestampArray.AppendTimestampValue(i, i*1000);
    }
    // size assertion
    ASSERT_EQ(timestampArray.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
    }
    // update values
    for (int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            timestampArray.SetTimestampValue(i, 99, 9999);
        }
    }
    // size assertion
    ASSERT_EQ(timestampArray.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
        TimeStamp ts = timestampArray.GetTimestamp(i);
        if (i % 2)
        {
            ASSERT_EQ(ts.GetSecond(), 99);
            ASSERT_EQ(ts.GetNano(), 9999);
        }
        else
        {
            ASSERT_EQ(ts.GetSecond(), i);
            ASSERT_EQ(ts.GetNano(), i * 1000);
        }
    }
    // null values
    for (int i = 5; i < 10; i++)
    {
        timestampArray.SetNull(i);
    }
    // set back
    for (int i = 8; i < 10; i++)
    {
        timestampArray.SetTimestampValue(i, i, i * 1000);
    }
    // check nulls
    for (int i = 5; i < 10; i++)
    {
        if (i >= 8)
        {
            ASSERT_EQ(timestampArray.GetTimestamp(i), TimeStamp(i, i * 1000));
        }
        else
        {
            ASSERT_THROW(timestampArray.GetTimestamp(i), OdpsTunnelException);
        }
    }
    std::cerr << "ArraySize: " << timestampArray.GetInMemorySize() << std::endl;
}

TEST_F(TunnelComplexTypeTest, TestArrayTimeStampNTZType)
{
    string timestampNTZArrTypeSpec = "ARRAY<TIMESTAMP_NTZ>";
    ODPSColumnTypeInfo timestampNTZArrType = ODPSColumnTypeInfo::ParseTypeInfoString(timestampNTZArrTypeSpec);
    ODPSArray timestampNTZArray(timestampNTZArrType);
    ASSERT_EQ(timestampNTZArray.GetElementType(), ODPS_TIMESTAMP_NTZ);
    // append values
    for(int i = 0; i < 10; i++)
    {
        timestampNTZArray.AppendTimestampNTZValue(i, i*1000);
    }
    // size assertion
    ASSERT_EQ(timestampNTZArray.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
    }
    // update values
    for (int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            timestampNTZArray.SetTimestampNTZValue(i, 99, 9999);
        }
    }
    // size assertion
    ASSERT_EQ(timestampNTZArray.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
        TimeStamp ts = timestampNTZArray.GetTimestampNTZ(i);
        if (i % 2)
        {
            ASSERT_EQ(ts.GetSecond(), 99);
            ASSERT_EQ(ts.GetNano(), 9999);
        }
        else
        {
            ASSERT_EQ(ts.GetSecond(), i);
            ASSERT_EQ(ts.GetNano(), i * 1000);
        }
    }
    // null values
    for (int i = 5; i < 10; i++)
    {
        timestampNTZArray.SetNull(i);
    }
    // set back
    for (int i = 8; i < 10; i++)
    {
        timestampNTZArray.SetTimestampNTZValue(i, i, i * 1000);
    }
    // check nulls
    for (int i = 5; i < 10; i++)
    {
        if (i >= 8)
        {
            ASSERT_EQ(timestampNTZArray.GetTimestampNTZ(i), TimeStamp(i, i * 1000));
        }
        else
        {
            ASSERT_THROW(timestampNTZArray.GetTimestampNTZ(i), OdpsTunnelException);
        }
    }
    std::cerr << "ArraySize: " << timestampNTZArray.GetInMemorySize() << std::endl;
}
TEST_F(TunnelComplexTypeTest, TestArrayNested)
{
    string nestedArrTypeSpec = "ARRAY<ARRAY<BIGINT>>";
    string subArrTypeSpec = "ARRAY<BIGINT>";
    ODPSColumnTypeInfo nestedArrType = ODPSColumnTypeInfo::ParseTypeInfoString(nestedArrTypeSpec);
    ODPSColumnTypeInfo subArrType = ODPSColumnTypeInfo::ParseTypeInfoString(subArrTypeSpec);
    ODPSArray nestedArray(nestedArrType);
    ASSERT_EQ(nestedArray.GetElementType(), ODPS_ARRAY);
    for (int i = 0; i < 10; i++)
    {
        shared_ptr<ODPSArray> array = BuildBigIntArray(10);
        nestedArray.AppendArrayValue(array);
    }
    // size assertion
    ASSERT_EQ(nestedArray.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
        shared_ptr<ODPSArray> array = dynamic_pointer_cast<ODPSArray>(nestedArray.GetArray(i));
        ASSERT_EQ(CheckBigIntArray(*array), true);
    }
    for (int i = 0; i < 10; i++)
    {
        if (i % 2)
        {
            shared_ptr<ODPSArray> array = BuildBigIntArray(20);
            nestedArray.SetArrayValue(i, array);
        }
    }
    ASSERT_EQ(nestedArray.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
        shared_ptr<ODPSArray> array = dynamic_pointer_cast<ODPSArray>(nestedArray.GetArray(i));
        ASSERT_EQ(CheckBigIntArray(*array), true);
        if (i % 2)
        {
            ASSERT_EQ(array->Size(), 20);
        }
    }
    std::cerr << "ArraySize: " << nestedArray.GetInMemorySize() << std::endl;
}

TEST_F(TunnelComplexTypeTest, TestMapSimple)
{
    string simpleMapTypeSpec = "MAP<BIGINT,STRING>";
    ODPSColumnTypeInfo simpleMapType = ODPSColumnTypeInfo::ParseTypeInfoString(simpleMapTypeSpec);
    ODPSMap simpleMap(simpleMapType);
    ASSERT_EQ(simpleMap.GetKeyTypeInfo().mType, ODPS_BIGINT);
    ASSERT_EQ(simpleMap.GetValueTypeInfo().mType, ODPS_STRING);
    // for string, we have two ways of inserting, test forwarding correctly
    // note the key type must correct, or will report type incompat.
    for (int64_t i = 0; i < 10; i++)
    {
        string tmpstr = to_string(i);
        simpleMap.SetStringValue(i, tmpstr);
    }
    ASSERT_EQ(simpleMap.Size(), 10);
    // get values
    for (int64_t i = 0; i < 10; i++)
    {
        string tmpstr = to_string(i);
        ASSERT_EQ(simpleMap.GetString(i), tmpstr);
    }
    // update values
    for (int64_t i = 5; i < 15; i++)
    {
        string tmpstr = to_string(i + 100);
        simpleMap.SetStringValue(i, tmpstr);
    }
    ASSERT_EQ(simpleMap.Size(), 15);
    for (int64_t i = 0; i < 10; i++)
    {
        string tmpstr = to_string(i);
        string tmpstr2 = to_string(i + 100);
        if (i >= 5)
        {
            ASSERT_EQ(simpleMap.GetString(i), tmpstr2);
        }
        else
        {
            ASSERT_EQ(simpleMap.GetString(i), tmpstr);
        }
    }
    // null values
    for (int64_t i = 10; i < 20; i++)
    {
        simpleMap.SetNullValue(i);
    }
    ASSERT_EQ(simpleMap.Size(), 20);
    for (int64_t i = 0; i < 20; i++)
    {
        string tmpstr = to_string(i);
        string tmpstr2 = to_string(i + 100);
        if (i >= 10)
        {
            ASSERT_EQ(simpleMap.Contains(i), true);
            ASSERT_EQ(simpleMap.ContainsAndNotNull(i), false);
            ASSERT_THROW(simpleMap.GetString(i), OdpsTunnelException);
        }
        else if (i >= 5)
        {
            ASSERT_EQ(simpleMap.GetString(i), tmpstr2);
        }
        else
        {
            ASSERT_EQ(simpleMap.GetString(i), tmpstr);
        }
    }
    std::cerr << "MapSize: " << simpleMap.GetInMemorySize() << std::endl;
}

// the bool for underlying key is naturally good, so we test TimeStamp only
TEST_F(TunnelComplexTypeTest, TestMapTimeStamp)
{
    string tsMapTypeSpec = "MAP<TIMESTAMP,BIGINT>";
    ODPSColumnTypeInfo tsMapType = ODPSColumnTypeInfo::ParseTypeInfoString(tsMapTypeSpec);
    ODPSMap tsMap(tsMapType);
    ASSERT_EQ(tsMap.GetKeyTypeInfo().mType, ODPS_TIMESTAMP);
    ASSERT_EQ(tsMap.GetValueTypeInfo().mType, ODPS_BIGINT);
    // key for this test is to ensure the comparability of TimeStamp, since
    // the underlying map is treemap(std::map)
    // so no need for very complex..
    for (int i = 0; i < 10; i++)
    {
        TimeStamp ts(i);
        tsMap.SetBigIntValue(ts, i);
    }
    ASSERT_EQ(tsMap.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
        TimeStamp ts(i);
        ASSERT_EQ(tsMap.GetBigInt(ts), static_cast<int64_t>(i));
    }
    std::cerr << "MapSize: " << tsMap.GetInMemorySize() << std::endl;
}

TEST_F(TunnelComplexTypeTest, TestMapTimeStampNTZ)
{
    string tsMapTypeSpec = "MAP<TIMESTAMP_NTZ,BIGINT>";
    ODPSColumnTypeInfo tsMapType = ODPSColumnTypeInfo::ParseTypeInfoString(tsMapTypeSpec);
    ODPSMap tsMap(tsMapType);
    ASSERT_EQ(tsMap.GetKeyTypeInfo().mType, ODPS_TIMESTAMP_NTZ);
    ASSERT_EQ(tsMap.GetValueTypeInfo().mType, ODPS_BIGINT);
    for (int i = 0; i < 10; i++)
    {
        TimeStamp ts(i);
        tsMap.SetBigIntValue(ts, i);
    }
    ASSERT_EQ(tsMap.Size(), 10);
    for (int i = 0; i < 10; i++)
    {
        TimeStamp ts(i);
        ASSERT_EQ(tsMap.GetBigInt(ts), static_cast<int64_t>(i));
    }
    std::cerr << "MapSize: " << tsMap.GetInMemorySize() << std::endl;
}

// note: we use ODPSArray for underlying storage of ODPSMap, so nested values should work good if nested array work good.
// TEST_F(TunnelComplexTypeTest, TestMapNested) omitted.

// for structs, we test everything in one go.
TEST_F(TunnelComplexTypeTest, TestStruct)
{
    string structTypeSpec = "STRUCT<a:BIGINT,b:STRING,c:TIMESTAMP,d:ARRAY<BIGINT>,e:TIMESTAMP_NTZ>";
    ODPSColumnTypeInfo structType = ODPSColumnTypeInfo::ParseTypeInfoString(structTypeSpec);
    ODPSStruct testStruct(structType);
    // accessing members.
    uint32_t bigIntIndex = testStruct.GetMemberIndex("a");
    uint32_t stringIndex = testStruct.GetMemberIndex("b");
    uint32_t timestampIndex = testStruct.GetMemberIndex("c");
    uint32_t arrayIndex = testStruct.GetMemberIndex("d");
    uint32_t timestampNTZIndex = testStruct.GetMemberIndex("e");
    ASSERT_THROW(testStruct.GetMemberIndex("not_exist_member_plz_dont_create"), OdpsTunnelException);
    // size assertion
    ASSERT_EQ(testStruct.Size(), 5);
    // set values for them
    shared_ptr<ODPSArray> tmpArray = BuildBigIntArray(10);
    testStruct.SetBigIntValue(bigIntIndex, 10);
    testStruct.SetStringValue(stringIndex, "teststring");
    testStruct.SetTimestampValue(timestampIndex, 10, 2000);
    testStruct.SetArrayValue(arrayIndex, tmpArray);
    testStruct.SetTimestampNTZValue(timestampNTZIndex, 10, 2000);
    // set values for incompat fields
    ASSERT_THROW(testStruct.SetArrayValue(bigIntIndex, tmpArray), OdpsTunnelException);
    ASSERT_THROW(testStruct.SetTimestampValue(stringIndex, 10, 2000), OdpsTunnelException);
    ASSERT_THROW(testStruct.SetStringValue(timestampIndex, "teststring"), OdpsTunnelException);
    ASSERT_THROW(testStruct.SetTimestampNTZValue(stringIndex, 10, 2000), OdpsTunnelException);
    ASSERT_THROW(testStruct.SetBigIntValue(arrayIndex, 10), OdpsTunnelException);
    // read out
    ASSERT_EQ(testStruct.GetBigInt(bigIntIndex), 10);
    ASSERT_EQ(testStruct.GetString(stringIndex), "teststring");
    ASSERT_EQ(testStruct.GetTimestamp(timestampIndex), TimeStamp(10, 2000));
    ASSERT_EQ(testStruct.GetTimestampNTZ(timestampNTZIndex), TimeStamp(10, 2000));
    shared_ptr<ODPSArray> readoutArray = testStruct.GetArray(arrayIndex);
    ASSERT_EQ(CheckBigIntArray(*readoutArray), true);
    ASSERT_EQ(readoutArray->Size(), 10);

    // nulling
    testStruct.SetNullValue(stringIndex);
    testStruct.SetNullValue(arrayIndex);
    ASSERT_EQ(testStruct.IsNull(stringIndex), true);
    ASSERT_EQ(testStruct.IsNull(arrayIndex), true);
    ASSERT_THROW(testStruct.GetString(stringIndex), OdpsTunnelException);
    ASSERT_THROW(testStruct.GetArray(arrayIndex), OdpsTunnelException);
    std::cerr << "StructSize: " << testStruct.GetInMemorySize() << std::endl;
}

TEST_F(TunnelComplexTypeTest, LegacyDecimalSupport)
{
    std::string tableName = Utils::GetRandomTableName();
    Utils::DropTableIfExists(tableName);
    Utils::ExecSql("create table " + tableName + " (d decimal, b bigint)");
    sCleanUpTableName = tableName;
    {
        IUploadPtr upload = sTunnel.CreateUpload(sProjectName, tableName);
        IRecordWriterPtr wr = upload->OpenWriter(0);
        ODPSTableRecordPtr r = upload->CreateBufferRecord();
        r->SetDecimalValue(0, "1.2");
        r->SetBigIntValue(1, 3);
        wr->Write(*r);
        wr->Close();
        upload->Commit({0});
    }
    {
        IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName);
        IRecordReaderPtr rd = download->OpenReader(0, 1);
        ODPSTableRecordPtr r = rd->CreateBufferRecord();
        ASSERT_TRUE(r->GetSchema()->GetTableColumn(0).GetTypeInfo().IsLegacyDecimal());
        ASSERT_TRUE(rd->Read(*r));
        ASSERT_EQ(r->GetDecimal(0), "1.2");
        ASSERT_EQ(r->GetBigInt(1), 3);
        rd->Close();
        download->Complete();
    }
}

TEST_F(TunnelComplexTypeTest, UploadDownload)
{
    std::string tableName = Utils::GetRandomTableName();
    Utils::DropTableIfExists(tableName);
    Utils::ExecSql("create table " + tableName + " (biarray array<bigint>, bimap map<string, bigint>, bistruct struct<a:string, b:bigint, c:double>)");
    sCleanUpTableName = tableName;
    {
        IUploadPtr upload = sTunnel.CreateUpload(sProjectName, tableName);
        ASSERT_TRUE(upload->GetUploadId() != "");
        ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
        ASSERT_TRUE(upload->GetSchema() != NULL);
        ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);
        IRecordWriterPtr wr = upload->OpenWriter(0);
        IODPSTableSchema* schema = upload->GetSchema();
        {
            ODPSTableRecordPtr _r = upload->CreateBufferRecord();
            ODPSTableRecord& buffer = *_r;
            shared_ptr<ODPSArray> biarray = make_shared<ODPSArray>(schema->GetTableColumn(0).GetTypeInfo());
            shared_ptr<ODPSMap> bimap = make_shared<ODPSMap>(schema->GetTableColumn(1).GetTypeInfo());
            shared_ptr<ODPSStruct> bistruct = make_shared<ODPSStruct>(schema->GetTableColumn(2).GetTypeInfo());
            biarray->AppendBigIntValue(4);
            biarray->AppendBigIntValue(5);
            biarray->AppendBigIntValue(6);
            bimap->SetBigIntValue<string>("e", 5);
            bimap->SetBigIntValue<string>("f", 6);
            bistruct->SetStringValue(bistruct->GetMemberIndex("a"), "sss");
            bistruct->SetBigIntValue(bistruct->GetMemberIndex("b"), 5);
            bistruct->SetDoubleValue(bistruct->GetMemberIndex("c"), 6.0);
            buffer.SetArrayValue(0, biarray);
            buffer.SetMapValue(1, bimap);
            buffer.SetStructValue(2, bistruct);
            std::cerr << "RecordSize: " << buffer.GetRecordSize() << std::endl;
            wr->Write(buffer);
        }
        {
            ODPSTableRecordPtr _r = upload->CreateBufferRecord();
            ODPSTableRecord& buffer = *_r;
            shared_ptr<ODPSArray> biarray = make_shared<ODPSArray>(schema->GetTableColumn(0).GetTypeInfo());
            shared_ptr<ODPSMap> bimap = make_shared<ODPSMap>(schema->GetTableColumn(1).GetTypeInfo());
            shared_ptr<ODPSStruct> bistruct = make_shared<ODPSStruct>(schema->GetTableColumn(2).GetTypeInfo());
            biarray->AppendBigIntValue(4);
            biarray->AppendBigIntValue(5);
            biarray->AppendNull();
            bimap->SetBigIntValue<string>("e", 5);
            bimap->SetNullValue(string("f"));
            bistruct->SetStringValue(bistruct->GetMemberIndex("a"), "sss");
            bistruct->SetNullValue(bistruct->GetMemberIndex("b"));
            bistruct->SetDoubleValue(bistruct->GetMemberIndex("c"), 6.0);
            buffer.SetArrayValue(0, biarray);
            buffer.SetMapValue(1, bimap);
            buffer.SetStructValue(2, bistruct);
            std::cerr << "RecordSize: " << buffer.GetRecordSize() << std::endl;
            wr->Write(buffer);
        }
        wr->Close();
        upload->Commit({0});
    }
    // start download to test correctness
    {
        IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName);
        ASSERT_NE("", download->GetDownloadId());
        ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());
        ASSERT_TRUE(NULL != download->GetSchema());
        IRecordReaderPtr rr = download->OpenReader(0, 100000);
        ODPSTableRecordPtr _r = rr->CreateBufferRecord();
        ODPSTableRecord& buffer = *_r;
        {
            ASSERT_TRUE(rr->Read(buffer));
            shared_ptr<ODPSArray> biarray = buffer.GetArrayValue(0);
            shared_ptr<ODPSMap> bimap = buffer.GetMapValue(1);
            shared_ptr<ODPSStruct> bistruct = buffer.GetStructValue(2);
            ASSERT_EQ(biarray->GetBigInt(0), 4);
            ASSERT_EQ(biarray->GetBigInt(1), 5);
            ASSERT_EQ(biarray->GetBigInt(2), 6);
            ASSERT_EQ(bimap->GetBigInt(string("e")), 5);
            ASSERT_EQ(bimap->GetBigInt(string("f")), 6);
            ASSERT_EQ(bistruct->GetString(bistruct->GetMemberIndex("a")), "sss");
            ASSERT_EQ(bistruct->GetBigInt(bistruct->GetMemberIndex("b")), 5);
            ASSERT_EQ(bistruct->GetDouble(bistruct->GetMemberIndex("c")), 6.0);
            std::cerr << "RecordSize: " << buffer.GetRecordSize() << std::endl;
        }
        {
            ASSERT_TRUE(rr->Read(buffer));
            shared_ptr<ODPSArray> biarray = buffer.GetArrayValue(0);
            shared_ptr<ODPSMap> bimap = buffer.GetMapValue(1);
            shared_ptr<ODPSStruct> bistruct = buffer.GetStructValue(2);
            ASSERT_EQ(biarray->GetBigInt(0), 4);
            ASSERT_EQ(biarray->GetBigInt(1), 5);
            ASSERT_TRUE(biarray->IsNull(2));
            ASSERT_EQ(bimap->GetBigInt(string("e")), 5);
            // CONTAINS and NULL
            ASSERT_TRUE(bimap->Contains(string("f")) && !(bimap->ContainsAndNotNull(string("f"))));
            ASSERT_EQ(bistruct->GetString(bistruct->GetMemberIndex("a")), "sss");
            ASSERT_TRUE(bistruct->IsNull(bistruct->GetMemberIndex("b")));
            ASSERT_EQ(bistruct->GetDouble(bistruct->GetMemberIndex("c")), 6.0);
            std::cerr << "RecordSize: " << buffer.GetRecordSize() << std::endl;
        }
        ASSERT_FALSE(rr->Read(buffer));
        rr->Close();
        download->Complete();
    }
}