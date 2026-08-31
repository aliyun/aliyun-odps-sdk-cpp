#include <stdlib.h>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::util;

class TunnelDownloadTest : public testing::Test
{
protected:
    static std::string sProjectName;
    static std::string sTableName;
    static OdpsTunnel  sTunnel;
    static uint64_t    sCount;

    static void SetUpTestCase()
    {
        sProjectName = Utils::GetProjectName();
        sTableName = Utils::GetRandomTableName();
        sTunnel = Utils::GetTunnelInstance();
        sCount = 1024;
        Utils::DropTableIfExists(sTableName);
        Utils::ExecSqlTogether("set odps.sql.type.json.enable=true; create table " + sTableName + "(c1 bigint, c2 json, c3 string)");

        IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
        IRecordWriterPtr writer = upload->OpenWriter(0);
        ODPSTableRecordPtr _r = upload->CreateBufferRecord();
        ODPSTableRecord& record = *_r;
        for (uint64_t i = 0; i < sCount; ++i)
        {
            const std::string str = "string_" + std::to_string(i);
            const std::string jsonStr = "{\"key\":\"val\"}";
            record.SetBigIntValue(0, i);
            record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
            record.SetStringValue(2, str.c_str(), str.length());
            writer->Write(record);
        }
        writer->Close();
        std::vector<uint32_t> blocks;
        blocks.push_back(0);
        upload->Commit(blocks);
    }

    static void TearDownTestCase()
    {
        Utils::DropTableIfExists(sTableName);
    }

    virtual void SetUp()
    {

    }

    virtual void TearDown()
    {

    }

    IDownloadPtr CreateDownload(
            const std::string &downloadId = "")
    {
        IDownloadPtr download;
        if (downloadId.empty())
        {
            download = sTunnel.CreateDownload(sProjectName, sTableName, "", "");
            std::cerr << "create download session, id=" << download->GetDownloadId() << std::endl;
        }
        else
        {
            download = sTunnel.CreateDownload(sProjectName, sTableName, "", downloadId);
            std::cerr << "create download session from id=" << downloadId << std::endl;
        }

        return download;
    }

    IDownloadPtr CreateDownloadWithAppSignature(
            const std::string &downloadId = "")
    {
        IDownloadPtr download;
        OdpsTunnel tmpTunnel = Utils::GetTunnelInstanceWithAppSignature();
        if (downloadId.empty())
        {
            download = tmpTunnel.CreateDownload(sProjectName, sTableName, "", "");
            std::cerr << "create download session, id=" << download->GetDownloadId() << std::endl;
        }
        else
        {
            download = tmpTunnel.CreateDownload(sProjectName, sTableName, "", downloadId);
            std::cerr << "create download session from id=" << downloadId << std::endl;
        }

        return download;
    }

    bool DownloadData(uint64_t start, uint64_t count, const std::string &downloadId = "");
    bool DownloadDataByBuffer(uint64_t start, uint64_t count, uint64_t bufferRecordCount, const std::string &downloadId = "");
    void UploadData();

};

std::string TunnelDownloadTest::sProjectName;
std::string TunnelDownloadTest::sTableName;
OdpsTunnel  TunnelDownloadTest::sTunnel;
uint64_t    TunnelDownloadTest::sCount = 0;

void TunnelDownloadTest::UploadData()
{
    Utils::DropTableIfExists(sTableName);
    Utils::ExecSqlTogether("set odps.sql.type.json.enable=true; create table " + sTableName + "(c1 bigint, c2 json, c3 string)");

    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    IRecordWriterPtr writer = upload->OpenWriter(0);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < sCount; ++i)
    {
        const std::string str = "string_" + std::to_string(i);
        const std::string jsonStr = "{\"key\":\"val\"}";
        record.SetBigIntValue(0, i);
        record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
        record.SetStringValue(2, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    std::vector<uint32_t> blocks;
    blocks.push_back(0);
    upload->Commit(blocks);
}


bool TunnelDownloadTest::DownloadData(uint64_t start, uint64_t count, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);
    IRecordReaderPtr reader = download->OpenReader(start, count);
    if (reader.get() == NULL) return false;

    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;

    uint64_t i = start;
    while (reader->Read(record))
    {
        if (static_cast<int64_t>(i) != *record.GetBigIntValue(0))
        {
            EXPECT_EQ(static_cast<int64_t>(i), *record.GetBigIntValue(0));
            return false;
        }

        const std::string str = "string_" + std::to_string(i);
        const std::string jsonStr = "{\"key\":\"val\"}";

        uint32_t len = 0;
        const char *jsonVal = record.GetJsonValue(1, len);
        if (jsonStr.length() != len || jsonStr != jsonVal)
        {
            EXPECT_STREQ(jsonStr.c_str(), jsonVal);
            return false;
        }

        const char *strVal = record.GetStringValue(2, len);
        if (str.length() != len || str != strVal)
        {
            EXPECT_STREQ(str.c_str(), strVal);
            return false;
        }
        ++i;
    }
    reader->Close();
    download->Complete();
    if (start + count != i)
    {
        EXPECT_EQ(start + count, i);
        return false;
    }
    return true;
}

bool TunnelDownloadTest::DownloadDataByBuffer(uint64_t start, uint64_t count, uint64_t bufferRecordCount, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);

    std::vector<std::string> colNames;
    CompressOption option = CompressOption::NO_COMPRESS;
    IBufferRecordReaderPtr reader = download->OpenBufferReader(start, count, bufferRecordCount, colNames, option);
    if (reader.get() == NULL) return false;

    ODPSTableRecordPtr record;
    uint64_t i = start;
    while((record = reader->ReadWithRetry(5)) != nullptr)
    {
        if (static_cast<int64_t>(i) != *record->GetBigIntValue(0))
        {
            EXPECT_EQ(static_cast<int64_t>(i), *record->GetBigIntValue(0));
            return false;
        }

        const std::string str = "string_" + std::to_string(i);
        const std::string jsonStr = "{\"key\":\"val\"}";

        uint32_t len = 0;
        const char *jsonVal = record->GetJsonValue(1, len);
        if (jsonStr.length() != len || jsonStr != jsonVal)
        {
            EXPECT_STREQ(jsonStr.c_str(), jsonVal);
            return false;
        }

        const char *strVal = record->GetStringValue(2, len);
        if (str.length() != len || str != strVal)
        {
            EXPECT_STREQ(str.c_str(), strVal);
            return false;
        }
        ++i;
    }
    reader->Close();
    download->Complete();
    uint64_t end = std::min(sCount, start + count);
    if (end != i)
    {
        EXPECT_EQ(end, i);
        return false;
    }
    return true;
}

// 指定的project参数不存在或为null，session创建失败
TEST_F(TunnelDownloadTest, ProjectNotExistWhenCreateDownload)
{
    ASSERT_THROW(sTunnel.CreateDownload(sProjectName + "_not_exist", sTableName), OdpsException);
}

//  指定的table参数不存在或为null，session创建失败
TEST_F(TunnelDownloadTest, TableNotExistWhenCreateDownload)
{
    ASSERT_THROW(sTunnel.CreateDownload(sProjectName, sTableName + "_not_exist"), OdpsException);
}

// 单线程下载, 无压缩, 准确download数据
TEST_F(TunnelDownloadTest, Normal)
{
    IDownloadPtr download = CreateDownload();

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());
    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    uint64_t count = 0;
    IRecordReaderPtr reader = download->OpenReader(0, sCount);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    while (reader->Read(record))
    {
        ASSERT_EQ(static_cast<int64_t>(count), *record.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(count);
        const std::string jsonStr = "{\"key\":\"val\"}";

        uint32_t len = 0;
        const char *jsonVal = record.GetJsonValue(1, len);
        ASSERT_STREQ(jsonStr.c_str(), jsonVal);

        const char *strVal = record.GetStringValue(2, len);
        ASSERT_STREQ(str.c_str(), strVal);

        ++count;
    }
    ASSERT_FALSE(download->GetQuotaName().empty());
    reader->Close();
    std::string metricsStr = reader->GetMetrics();
    std::cout<<" Metrics" << metricsStr << std::endl;
    TunnelTestMetrics tunnelMetrics;
    FromJsonString(tunnelMetrics, metricsStr);
    EXPECT_GE(tunnelMetrics.TunnelProccessCost, 0);
    EXPECT_GE(tunnelMetrics.ClientProcessCost, 0);
    EXPECT_GE(tunnelMetrics.NetworkCost, 0);
    EXPECT_GE(tunnelMetrics.PanguIOCost, 0);
    EXPECT_GE(tunnelMetrics.RateLimitCost, 0);
    download->Complete();

    ASSERT_EQ(sCount, count);
}

// 从0下载数据，数据量小于,等于,大于总数量; buffer size是否被数据量整除
TEST_F(TunnelDownloadTest, BufferDownload)
{
    // 下载数据量等于数据表总行数
    ASSERT_TRUE(DownloadDataByBuffer(0, sCount, sCount/2));
    ASSERT_TRUE(DownloadDataByBuffer(0, sCount, 300));

    // 下载数据量大于数据表总行数
    ASSERT_TRUE(DownloadDataByBuffer(0, 2000, sCount/2));
    ASSERT_TRUE(DownloadDataByBuffer(0, 2000, 300));

    // 下载数据量小于数据表总行数, buffer设为0时会重置为1
    ASSERT_TRUE(DownloadDataByBuffer(0, sCount/100, 0));
    ASSERT_TRUE(DownloadDataByBuffer(0, sCount/100, 4));
}

// 从指定位置下载数据，数据量小于,等于,大于总数量; buffer size是否被数据量整除
TEST_F(TunnelDownloadTest, BufferDownloadPart)
{
    ASSERT_TRUE(DownloadDataByBuffer(100, sCount, sCount/2));
    ASSERT_TRUE(DownloadDataByBuffer(100, sCount, 300));

    ASSERT_TRUE(DownloadDataByBuffer(100, 2000, sCount/2));
    ASSERT_TRUE(DownloadDataByBuffer(100, 2000, 300));

    ASSERT_TRUE(DownloadDataByBuffer(100, sCount/100, 0));
    ASSERT_TRUE(DownloadDataByBuffer(100, sCount/100, 4));
}

TEST_F(TunnelDownloadTest, ZstdCompress)
{
    OdpsTunnel t = Utils::GetTunnelInstance(false, CompressOption::ZSTD_COMPRESS);
    IDownloadPtr download = t.CreateDownload(sProjectName, sTableName, "", "");

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());
    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    uint64_t count = 0;
    IRecordReaderPtr reader = download->OpenReader(0, sCount, true);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    while (reader->Read(record))
    {
        ASSERT_EQ(static_cast<int64_t>(count), *record.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(count);
        const std::string jsonStr = "{\"key\":\"val\"}";
        uint32_t len = 0;

        const char *jsonVal = record.GetJsonValue(1, len);
        ASSERT_STREQ(jsonStr.c_str(), jsonVal);

        const char *strVal = record.GetStringValue(2, len);
        ASSERT_STREQ(str.c_str(), strVal);

        ++count;
    }
    reader->Close();
    download->Complete();

    ASSERT_EQ(sCount, count);
}

TEST_F(TunnelDownloadTest, Lz4Compress)
{
    OdpsTunnel t = Utils::GetTunnelInstance(false, CompressOption::LZ4_COMPRESS);
    IDownloadPtr download = t.CreateDownload(sProjectName, sTableName, "", "");

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());
    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    uint64_t count = 0;
    IRecordReaderPtr reader = download->OpenReader(0, sCount, true);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    while (reader->Read(record))
    {
        ASSERT_EQ(static_cast<int64_t>(count), *record.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(count);
        const std::string jsonStr = "{\"key\":\"val\"}";
        uint32_t len = 0;

        const char *jsonVal = record.GetJsonValue(1, len);
        ASSERT_STREQ(jsonStr.c_str(), jsonVal);

        const char *strVal = record.GetStringValue(2, len);
        ASSERT_STREQ(str.c_str(), strVal);

        ++count;
    }
    reader->Close();
    download->Complete();

    ASSERT_EQ(sCount, count);
}

// 单线程下载部分, 无压缩, 准确download数据
TEST_F(TunnelDownloadTest, PartOfData)
{
    ASSERT_TRUE(DownloadData(0, sCount/2));
}

// 单线程下载部分, start小于0, download数据异常
TEST_F(TunnelDownloadTest, StartOffsetLessThanZero)
{
    bool expected = false;
    try
    {
        DownloadData(-1, sCount/2);
    }
    catch (OdpsException &e)
    {
        ASSERT_NE(e.GetErrorMsg().find("row range is not valid"), std::string::npos);
        expected = true;
    }
    ASSERT_TRUE(expected);
}

// 单线程下载部分, count等于0
TEST_F(TunnelDownloadTest, CountEqualZero)
{
    bool expected = false;
    try
    {
        DownloadData(0, 0);
    }
    catch (OdpsException &e)
    {
        ASSERT_NE(e.GetErrorMsg().find("row range is not valid"), std::string::npos);
        expected = true;
    }
    ASSERT_TRUE(expected);
}

// 单线程下载部分, count小于0
TEST_F(TunnelDownloadTest, CountLessThanZero)
{
    bool expected = false;
    try
    {
        DownloadData(0, -1);
    }
    catch (OdpsException &e)
    {
        ASSERT_NE(e.GetErrorMsg().find("row range is not valid"), std::string::npos);
        expected = true;
    }
    ASSERT_TRUE(expected);
}

// 单线程下载部分, start大于总记录数, download数据异常
TEST_F(TunnelDownloadTest, StartOffsetGreatThanTotalCount)
{
    IDownloadPtr download = CreateDownload();

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());

    IRecordReaderPtr reader = download->OpenReader(sCount * 2, 1);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    bool excpected = reader->Read(record);
    reader->Close();
    download->Complete();
    ASSERT_FALSE(excpected);
}

// 单线程下载部分, start在合法值内, start + count大于文件总大小, 无压缩
TEST_F(TunnelDownloadTest, RangeGreatThanTotalCount)
{
    IDownloadPtr download = CreateDownload();

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());

    uint64_t count = 0;
    IRecordReaderPtr reader = download->OpenReader(0, sCount * 2);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    while (reader->Read(record))
    {
        ASSERT_EQ(static_cast<int64_t>(count), *record.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(count);
        const std::string jsonStr = "{\"key\":\"val\"}";
        uint32_t len = 0;

        const char *jsonVal = record.GetJsonValue(1, len);
        ASSERT_STREQ(jsonStr.c_str(), jsonVal);

        const char *strVal = record.GetStringValue(2, len);
        ASSERT_STREQ(str.c_str(), strVal);

        ++count;
    }
    reader->Close();
    download->Complete();

    ASSERT_EQ(sCount, count);
}

// 单线程下载，无压缩，指定下载的某些列
TEST_F(TunnelDownloadTest, WithColumns)
{
    IDownloadPtr download = CreateDownload();

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());

    uint32_t totalColumns = download->GetSchema()->GetColumnCount();
    for (uint32_t i = 0; i < totalColumns; ++i)
    {
        std::vector<std::string> columns;
        columns.push_back(download->GetSchema()->GetTableColumn(i).GetName());
        uint64_t count = 0;
        IRecordReaderPtr reader = download->OpenReader(0, sCount, columns);
        ODPSTableRecordPtr _r = reader->CreateBufferRecord();
        ODPSTableRecord& record = *_r;
        while (reader->Read(record))
        {
            switch (reader->GetSchema()->GetTableColumn(0).GetType())
            {
                case ODPS_BIGINT:
                {
                    ASSERT_EQ(static_cast<int64_t>(count), *record.GetBigIntValue(0));
                    break;
                }
                case ODPS_STRING:
                {
                    const std::string str = "string_" + std::to_string(count);
                    uint32_t len = 0;
                    const char *val = record.GetStringValue(0, len);
                    ASSERT_STREQ(str.c_str(), val);
                    break;
                }
                case ODPS_JSON:
                {
                    const std::string str = "{\"key\":\"val\"}";
                    uint32_t len = 0;
                    const char *val = record.GetJsonValue(0, len);
                    ASSERT_STREQ(str.c_str(), val);
                    break;
                }
                default:
                {
                    ASSERT_STREQ("excpect column type bigint/string.", "");
                    break;
                }
            }
            ++count;
        }
        reader->Close();
        ASSERT_EQ(sCount, count);
    }
    download->Complete();
}

TEST_F(TunnelDownloadTest, NormalWithApplicationSignature)
{
    IDownloadPtr download = CreateDownloadWithAppSignature();

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());
    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    uint64_t count = 0;
    IRecordReaderPtr reader = download->OpenReader(0, sCount);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    while (reader->Read(record))
    {
        ASSERT_EQ(static_cast<int64_t>(count), *record.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(count);
        const std::string jsonStr = "{\"key\":\"val\"}";
        uint32_t len = 0;

        const char *jsonVal = record.GetJsonValue(1, len);
        ASSERT_STREQ(jsonStr.c_str(), jsonVal);

        const char *strVal = record.GetStringValue(2, len);
        ASSERT_STREQ(str.c_str(), strVal);

        ++count;
    }
    reader->Close();
    download->Complete();

    ASSERT_EQ(sCount, count);
}

TEST_F(TunnelDownloadTest, DownloadModified)
{
    UploadData();
    IDownloadPtr download = CreateDownload();

    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    IRecordWriterPtr writer = upload->OpenWriter(0);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < sCount; ++i)
    {
        const std::string str = "string_" + std::to_string(i);
        const std::string jsonStr = "{\"key\":\"val\"}";
        record.SetBigIntValue(0, i);
        record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
        record.SetStringValue(2, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    std::vector<uint32_t> blocks;
    blocks.push_back(0);
    upload->Commit(blocks);

    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    try
    {
        download->OpenReader(0, 99999999);
        ASSERT_EQ("Should Not Reach Here", "Should throw modified exception");
    }
    catch(const OdpsException& e)
    {
        ASSERT_EQ(e.GetErrorCode(), "TableModified");
    }
    std::vector<std::string> colNames;
    IRecordReaderPtr reader = download->OpenReader(0, 99999999, colNames, CompressOption::NO_COMPRESS, true);
    ASSERT_NE(reader.get(), nullptr);

    ODPSTableRecordPtr _r1 = upload->CreateBufferRecord();
    ODPSTableRecord& record1 = *_r1;
    uint64_t count = 0;
    while (reader->Read(record1))
    {
        ASSERT_EQ(static_cast<int64_t>(count), *record1.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(count);
        const std::string jsonStr = "{\"key\":\"val\"}";
        uint32_t len = 0;

        const char *jsonVal = record1.GetJsonValue(1, len);
        ASSERT_STREQ(jsonStr.c_str(), jsonVal);

        const char *strVal = record1.GetStringValue(2, len);
        ASSERT_STREQ(str.c_str(), strVal);

        ++count;
    }
    ASSERT_EQ(sCount, count);
    reader->Close();
    download->Complete();
}

TEST_F(TunnelDownloadTest, BufferDownloadModified)
{
    UploadData();
    IDownloadPtr download = CreateDownload();

    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    IRecordWriterPtr writer = upload->OpenWriter(0);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < sCount; ++i)
    {
        const std::string str = "string_" + std::to_string(i);
        const std::string jsonStr = "{\"key\":\"val\"}";
        record.SetBigIntValue(0, i);
        record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
        record.SetStringValue(2, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    std::vector<uint32_t> blocks;
    blocks.push_back(0);
    upload->Commit(blocks);

    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    try
    {
        download->OpenReader(0, 99999999);
        ASSERT_EQ("Should Not Reach Here", "Should throw modified exception");
    }
    catch(const OdpsException& e)
    {
        ASSERT_EQ(e.GetErrorCode(), "TableModified");
    }
    std::vector<std::string> colNames;
    IBufferRecordReaderPtr reader = download->OpenBufferReader(0, 99999999, sCount, colNames, CompressOption::NO_COMPRESS, true);
    ASSERT_NE(reader.get(), nullptr);

    uint64_t i = 0;
    while((_r = reader->ReadWithRetry(5)) != nullptr)
    {
        EXPECT_EQ(static_cast<int64_t>(i), *_r->GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(i);
        const std::string jsonStr = "{\"key\":\"val\"}";

        uint32_t len = 0;
        const char *jsonVal = _r->GetJsonValue(1, len);
        EXPECT_STREQ(jsonStr.c_str(), jsonVal);
        const char *strVal = _r->GetStringValue(2, len);
        EXPECT_STREQ(str.c_str(), strVal);
        ++i;
    }
    ASSERT_EQ(sCount, i);
    reader->Close();
    download->Complete();
}

TEST_F(TunnelDownloadTest, OldDecimalDefaultPrecisionScale)
{
    Utils::DropTableIfExists(sTableName);
    Utils::ExecSql("create table " + sTableName + "(c1 decimal)");

    IDownloadPtr download = CreateDownload();
    IODPSTableSchema* downloadSchema = download->GetSchema();
    ODPSColumnTypeInfo typeInfo = downloadSchema->GetTableColumn(0).GetTypeInfo();
    ASSERT_EQ(static_cast<int32_t>(54), typeInfo.mPrecision);
    ASSERT_EQ(static_cast<int32_t>(18), typeInfo.mScale);

    download->Complete();
    Utils::DropTableIfExists(sTableName);
}