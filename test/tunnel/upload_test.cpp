#include <stdlib.h>
#include <iostream>
#include <functional>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "include/error_code.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::util;

class TunnelUploadTest : public testing::Test
{
protected:
    static std::string sProjectName;
    static std::string sTableName;
    static OdpsTunnel  sTunnel;
    static uint32_t    sBlockId;
    static uint64_t    sCount;

    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        sProjectName = Utils::GetProjectName();
        sTableName = Utils::GetRandomTableName();
        sTunnel = Utils::GetTunnelInstance();
        sBlockId = 0;
        sCount = 0;
        Utils::DropTableIfExists(sTableName);
        Utils::ExecSql("create table " + sTableName + " (col1 bigint)");
    }

    virtual void TearDown()
    {
        Utils::DropTableIfExists(sTableName);
        if (!mTableName.empty())
        {
            std::map<std::string, std::string> hints;
            hints["odps.namespace.schema"] = "true";
            hints["odps.sql.allow.namespace.schema"] = "true";
            Utils::ExecSql(sProjectName, "drop table if exists " + mTableName + ";", hints, 120 * 1000);
            Utils::DropTableIfExists(mTableName);
        }
        if (!mSchemaName.empty())
        {
            std::map<std::string, std::string> hints;
            hints["odps.namespace.schema"] = "true";
            hints["odps.sql.allow.namespace.schema"] = "true";
            Utils::ExecSql(sProjectName, "drop schema " + sProjectName + "." + mSchemaName + ";", hints, 120 * 1000);
        }
    }

    static uint32_t NewBlockId()
    {
        ++sBlockId;
        return sBlockId;
    }

    static IUploadPtr CreateUpload(const std::string &uploadId = "",
        bool overwrite = false)
    {
        IUploadPtr upload;

        if (uploadId.empty())
        {
            upload = sTunnel.CreateUpload(sProjectName, sTableName, "", "", overwrite);
            std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
        }
        else
        {
            upload = sTunnel.CreateUpload(sProjectName, sTableName, "", uploadId, overwrite);
            std::cerr << "create upload session from id=" << uploadId << std::endl;
        }

        return upload;
    }

    static IUploadPtr CreateUploadWithAppSignature(const std::string &uploadId = "",
        bool overwrite = false)
    {
        IUploadPtr upload;
        OdpsTunnel tmpTunnel = Utils::GetTunnelInstanceWithAppSignature();

        if (uploadId.empty())
        {
            upload = tmpTunnel.CreateUpload(sProjectName, sTableName, "", "", overwrite);
            std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
        }
        else
        {
            upload = tmpTunnel.CreateUpload(sProjectName, sTableName, "", uploadId, overwrite);
            std::cerr << "create upload session from id=" << uploadId << std::endl;
        }

        return upload;
    }

    static IUploadPtr CreateUploadWithV4Signature()
    {
        OdpsTunnel tmpTunnel = Utils::GetTunnelInstanceForV4Signature();
        return tmpTunnel.CreateUpload(sProjectName, sTableName, "", "", false);
    }

    static IDownloadPtr CreateDownload()
    {
        IDownloadPtr download = sTunnel.CreateDownload(sProjectName, sTableName);
        std::cerr << "create download session, id=" << download->GetDownloadId() << std::endl;
        return download;
    }

    static bool UploadData(const std::string &uploadId, const uint32_t blockId);
    static bool UploadStreamed(std::function<uint64_t()> data_generator, uint64_t count = 100, uint64_t packs = 1);
    static bool UploadStreamedZstd(std::function<uint64_t()> data_generator, uint64_t count = 100, uint64_t packs = 1);

public:
    std::string mTableName;
    std::string mSchemaName;
    IODPSPtr mODPS;
};

std::string TunnelUploadTest::sProjectName;
std::string TunnelUploadTest::sTableName;
OdpsTunnel  TunnelUploadTest::sTunnel;
uint32_t    TunnelUploadTest::sBlockId = 0;
uint64_t    TunnelUploadTest::sCount = 0;

bool TunnelUploadTest::UploadData(const std::string &uploadId, const uint32_t blockId)
{
    IUploadPtr upload = CreateUpload(uploadId);

    if (upload->GetUploadId() == "")
    {
        EXPECT_TRUE(upload->GetUploadId() != "");
        return false;
    }

    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    record.SetBigIntValue(0, blockId);

    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    if (writer.get() == NULL)
    {
        EXPECT_TRUE(writer.get() != NULL);
        return false;
    }

    if (!writer->Write(record))
    {
        EXPECT_STREQ("writer->Writer sucess", "");
        return false;
    }
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    return true;
}

bool TunnelUploadTest::UploadStreamed(
    std::function<uint64_t()> generator,
    uint64_t count, uint64_t packs)
{
    IStreamUploadPtr streamUpload = sTunnel.CreateStreamUpload(sProjectName, sTableName);
    IRecordPackPtr pack = streamUpload->CreateRecordPack(CompressOption::ZSTD_COMPRESS);
    ODPSTableRecordPtr r = streamUpload->CreateBufferRecord();
    for(uint64_t i = 0; i < packs; i++)
    {
        for (uint64_t j = 0; j < count; j++)
        {
            r->SetBigIntValue(0, generator());
            pack->Append(*r);
        }
        pack->Flush();
    }
    std::string metricsStr = pack->GetMetrics();
    std::cout<<" Metrics" << metricsStr << std::endl;
    TunnelTestMetrics tunnelMetrics;
    FromJsonString(tunnelMetrics, metricsStr);
    EXPECT_GE(tunnelMetrics.TunnelProccessCost, 0);
    EXPECT_GE(tunnelMetrics.ClientProcessCost, 0);
    EXPECT_GE(tunnelMetrics.NetworkCost, 0);
    EXPECT_GE(tunnelMetrics.PanguIOCost, 0);
    EXPECT_GE(tunnelMetrics.RateLimitCost, 0);
    return true;
}

bool TunnelUploadTest::UploadStreamedZstd(
    std::function<uint64_t()> generator,
    uint64_t count, uint64_t packs)
{
    IStreamUploadPtr streamUpload = sTunnel.CreateStreamUpload(sProjectName, sTableName, "", false, 0, CompressOption::ZSTD_COMPRESS, "");
    IRecordPackPtr pack = streamUpload->CreateRecordPack(CompressOption::ZSTD_COMPRESS);
    ODPSTableRecordPtr r = streamUpload->CreateBufferRecord();
    for(uint64_t i = 0; i < packs; i++)
    {
        for (uint64_t j = 0; j < count; j++)
        {
            r->SetBigIntValue(0, generator());
            pack->Append(*r);
        }
        std::cout << pack->Flush() << std::endl;
    }
    std::string metricsStr = pack->GetMetrics();
    std::cout<<" Metrics" << metricsStr << std::endl;
    TunnelTestMetrics tunnelMetrics;
    FromJsonString(tunnelMetrics, metricsStr);
    EXPECT_GE(tunnelMetrics.TunnelProccessCost, 0);
    EXPECT_GE(tunnelMetrics.ClientProcessCost, 0);
    EXPECT_GE(tunnelMetrics.NetworkCost, 0);
    EXPECT_GE(tunnelMetrics.PanguIOCost, 0);
    EXPECT_GE(tunnelMetrics.RateLimitCost, 0);
    return true;
}

// 指定的project参数不存在或为null，session创建失败
TEST_F(TunnelUploadTest, ProjectNotExistWhenCreateUpload)
{
    ASSERT_THROW(sTunnel.CreateUpload(sProjectName + "_not_exist", sTableName), OdpsException);
}

//  指定的table参数不存在或为null，session创建失败
TEST_F(TunnelUploadTest, TableNotExistWhenCreateUpload)
{
    ASSERT_THROW(sTunnel.CreateUpload(sProjectName, sTableName + "_not_exist"), OdpsException);
}

// 单线程写一个block，无压缩，正常close/complete, 数据导入成功
TEST_F(TunnelUploadTest, Normal)
{
    IUploadPtr upload = CreateUpload();

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << " quota " << upload->GetQuotaName() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);
    ASSERT_FALSE(upload->GetQuotaName().empty());

    uint32_t blockId = NewBlockId();
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& r1 = *_r;
    r1.SetBigIntValue(0, blockId);

    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Write(r1);
    writer->Close();
    std::string metricsStr = writer->GetMetrics();
    std::cout<<" Metrics" << metricsStr << std::endl;
    TunnelTestMetrics tunnelMetrics;
    FromJsonString(tunnelMetrics, metricsStr);
    EXPECT_GE(tunnelMetrics.TunnelProccessCost, 0);
    EXPECT_GE(tunnelMetrics.ClientProcessCost, 0);
    EXPECT_GE(tunnelMetrics.NetworkCost, 0);
    EXPECT_GE(tunnelMetrics.PanguIOCost, 0);
    EXPECT_GE(tunnelMetrics.RateLimitCost, 0);

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    ++sCount;

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(sCount-1, 1);
    ODPSTableRecordPtr _r2 = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r2;
    ASSERT_TRUE(reader->Read(r2));
    download->Complete();

    ASSERT_EQ(*r1.GetBigIntValue(0), *r2.GetBigIntValue(0));
}

TEST_F(TunnelUploadTest, GetBlockList)
{
    IUploadPtr upload = CreateUpload();

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << " quota " << upload->GetQuotaName() << std::endl;

    uint32_t blockId;
    std::vector<uint32_t> blocks;
    for(size_t i = 0; i < 3; ++i)
    {
        blockId = NewBlockId();
        ODPSTableRecordPtr _r = upload->CreateBufferRecord();
        ODPSTableRecord& r1 = *_r;
        r1.SetBigIntValue(0, blockId);

        IRecordWriterPtr writer = upload->OpenWriter(blockId);
        ASSERT_TRUE(writer.get() != NULL);
        writer->Write(r1);
        writer->Close();
        blocks.push_back(blockId);
    }
    blockId = NewBlockId();
    blocks.push_back(blockId);
    ASSERT_THROW(upload->Commit(blocks), OdpsException);
    blocks.pop_back();
    upload->Commit(blocks);

    IUploadPtr uploadWithBlockInfo = CreateUpload(upload->GetUploadId());
    int32_t blockListSize = uploadWithBlockInfo->GetBlockList().size();
    ASSERT_EQ(3, blockListSize);

}

TEST_F(TunnelUploadTest, ZstdCompress)
{
    OdpsTunnel t = Utils::GetTunnelInstance(false, CompressOption::ZSTD_COMPRESS);
    IUploadPtr upload = t.CreateUpload(sProjectName, sTableName, "", "");

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);

    uint32_t blockId = NewBlockId();
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& r1 = *_r;
    r1.SetBigIntValue(0, blockId);

    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Write(r1);
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    ++sCount;

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(sCount-1, 1);
    ODPSTableRecordPtr _r2 = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r2;
    ASSERT_TRUE(reader->Read(r2));
    download->Complete();
    ASSERT_EQ(*r1.GetBigIntValue(0), *r2.GetBigIntValue(0));
}

TEST_F(TunnelUploadTest, Lz4Compress)
{
    OdpsTunnel t = Utils::GetTunnelInstance(false, CompressOption::LZ4_COMPRESS);
    IUploadPtr upload = t.CreateUpload(sProjectName, sTableName, "", "");

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);

    uint32_t blockId = NewBlockId();
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& r1 = *_r;
    r1.SetBigIntValue(0, blockId);

    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Write(r1);
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    ++sCount;

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(sCount-1, 1);
    ODPSTableRecordPtr _r2 = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r2;
    ASSERT_TRUE(reader->Read(r2));
    download->Complete();
    ASSERT_EQ(*r1.GetBigIntValue(0), *r2.GetBigIntValue(0));
}

// 单线程多次空写一个block，无压缩，正常close/complete, 数据导入成功
TEST_F(TunnelUploadTest, EmptyWrite)
{
    IUploadPtr upload = CreateUpload();

    // only one empty write
    std::cerr << "only one empty write ..." << std::endl;
    std::vector<uint32_t> blocks;
    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Close();
    blocks.push_back(blockId);
    upload->Commit(blocks);
    blocks.clear();

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());

    // three empty write
    upload = CreateUpload();
    std::cerr << "three empty write ..." << std::endl;
    for (size_t i = 0; i < 3; ++i)
    {
        blockId = NewBlockId();
        writer = upload->OpenWriter(blockId);
        ASSERT_TRUE(writer.get() != NULL);
        writer->Close();
        blocks.push_back(blockId);
    }
    upload->Commit(blocks);
    blocks.clear();

    download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());

    // fix empty write
    upload = CreateUpload();
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    std::cerr << "fix empty write ..." << std::endl;
    size_t count = 5;
    for (size_t i = 1; i <= count; ++i)
    {
        blockId = NewBlockId();
        writer = upload->OpenWriter(blockId);
        ASSERT_TRUE(writer.get() != NULL);
        if (i % 2 == 0) {
            record.SetBigIntValue(0, blockId);
            writer->Write(record);
        }
        writer->Close();
        blocks.push_back(blockId);
    }
    upload->Commit(blocks);
    sCount += count / 2;
    download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
}

// 多次写同样的block, 以最后一次的结果为准
TEST_F(TunnelUploadTest, WriteOneBlockTimes)
{
    IUploadPtr upload = CreateUpload();

    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    record.SetBigIntValue(0, blockId);
    writer->Write(record);
    writer->Close();

    writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Close();

    for (size_t i = 0; i < 6; ++i)
    {
        writer = upload->OpenWriter(blockId);
        record.SetBigIntValue(0, blockId);
        writer->Write(record);
        writer->Close();
    }

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    ++sCount;

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
}

// upload数据结束后, 在writer close后, 数据不会导入表中
TEST_F(TunnelUploadTest, NotCommit)
{
    IUploadPtr upload = CreateUpload();

    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    record.SetBigIntValue(0, blockId);
    writer->Write(record);
    //++sCount;
    writer->Close();

    //std::vector<uint32_t> blocks;
    //blocks.push_back(blockId);
    //upload->Commit(blocks);

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
}

// upload数据结束后, complete此次上传, 再close/complete, 预期可重复complete, 但是数据不会导入到表中
TEST_F(TunnelUploadTest, CommitBeforeWriterClose)
{
    IUploadPtr upload = CreateUpload();

    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    record.SetBigIntValue(0, blockId);
    writer->Write(record);
    //writer->Close();

    std::cerr << "commit before writer close ..." << std::endl;
    std::vector<uint32_t> blocks;
    //blocks.push_back(blockId);
    upload->Commit(blocks);
    //++sCount;
    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());

    std::cerr << "commit after writer close ..." << std::endl;
    ASSERT_THROW(writer->Close(), OdpsException);
    //blocks.push_back(blockId);
    //ASSERT_THROW(upload->Commit(blocks), OdpsTunnelException);
    //++sCount;
    download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
}

// upload数据结束后, complete此次上传, 再重新open wrtier时失败
TEST_F(TunnelUploadTest, OpenWriterAfterCommit)
{
    IUploadPtr upload = CreateUpload();

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Close();

    std::cerr << "commit blocks ..." << std::endl;
    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);

    std::cerr << "open writer after commit ..." << std::endl;
    writer = upload->OpenWriter(blockId);
    ASSERT_THROW(writer->Close(), OdpsException);
}

TEST_F(TunnelUploadTest, TestOverwrite)
{
    IUploadPtr upload = CreateUpload();

    ODPSTableRecordPtr _r1 = upload->CreateBufferRecord();
    ODPSTableRecord& r1 = *_r1;

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    r1.SetBigIntValue(0, 1);
    writer->Write(r1);
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    ++sCount;

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(0, 1);
    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r;
    ASSERT_TRUE(reader->Read(r2));
    download->Complete();
    ASSERT_EQ(*r1.GetBigIntValue(0), *r2.GetBigIntValue(0));

    upload = CreateUpload("", true);

    ODPSTableRecordPtr _r3 = upload->CreateBufferRecord();
    ODPSTableRecord& r3 = *_r3;

    blockId = NewBlockId();
    writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    r3.SetBigIntValue(0, 2);
    writer->Write(r3);
    writer->Close();

    blocks.clear();
    blocks.push_back(blockId);
    upload->Commit(blocks);

    download = CreateDownload();
    ASSERT_EQ((uint64_t)1, download->GetRecordCount());
    reader = download->OpenReader(0, 1);
    ODPSTableRecordPtr _r4 = reader->CreateBufferRecord();
    ODPSTableRecord& r4 = *_r4;
    ASSERT_TRUE(reader->Read(r4));
    download->Complete();
    ASSERT_EQ(*r3.GetBigIntValue(0), *r4.GetBigIntValue(0));

}

/*
// upload数据结束后, complete此次上传, 再次使用之前未close的writer进行数据写入, 并complete,
// 预期第二次的数据写入成功
TEST_F(TunnelUploadTest, CommitAfterEmptyCommit)
{
    IUploadPtr upload = CreateUpload();

    ODPSTableRecord record(upload->GetSchema());

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    //writer->Writer(record);
    writer->Close();

    std::cerr << "empty commit ..." << std::endl;
    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    //++sCount;
    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());

    std::cerr << "commit after writer close ..." << std::endl;
    record.SetBigIntValue(0, blockId);
    writer->Write(record);
    writer->Close();
    upload->Commit(blocks);
    ++sCount;
    download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
}

// upload数据结束后, close此block, 再次使用该writer进行数据写入时失败
TEST_F(TunnelUploadTest, WriteRecordAfterWriterClose)
{
    IUploadPtr upload = CreateUpload();

    ODPSTableRecord record(upload->GetSchema());

    uint32_t blockId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Close();

    std::cerr << "write record after writer close ..." << std::endl;
    record.SetBigIntValue(0, blockId);
    writer->Write(record);
}
*/

TEST_F(TunnelUploadTest, NormalWithAppSignature)
{
    IUploadPtr upload = CreateUploadWithAppSignature();

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);

    uint32_t blockId = NewBlockId();
    ODPSTableRecordPtr _r1 = upload->CreateBufferRecord();
    ODPSTableRecord& r1 = *_r1;
    r1.SetBigIntValue(0, blockId);

    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Write(r1);
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
    ++sCount;

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(sCount, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(sCount-1, 1);
    ODPSTableRecordPtr _r2 = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r2;
    ASSERT_TRUE(reader->Read(r2));
    download->Complete();
    ASSERT_EQ(*r1.GetBigIntValue(0), *r2.GetBigIntValue(0));
}

TEST_F(TunnelUploadTest, UploadNullIntoNotNullColumn)
{
    std::string tableName = Utils::GetRandomTableName();
    Utils::DropTableIfExists(tableName);
    Utils::ExecSql("create table " + tableName + " (col1 string, col2 string not null)");
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, tableName, "", "", false);

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);

    uint32_t blockId = NewBlockId();
    ODPSTableRecordPtr _r1 = upload->CreateBufferRecord();
    ODPSTableRecord& r1 = *_r1;
    r1.SetStringValue(0, std::string("hello"));
    r1.SetNullValue(1);

    IRecordWriterPtr writer = upload->OpenWriter(blockId);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Write(r1);
    try
    {
        writer->Close();
        upload->Commit({blockId});
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        std::cerr << "Exception thrown: " << ex.GetErrorCode() << " " << ex.GetErrorMsg() << std::endl;
        ASSERT_EQ(DATA_STORE_ERROR, ex.GetErrorCode());
        ASSERT_TRUE(ex.GetErrorMsg().find("is not nullable") != std::string::npos);
    }
}

TEST_F(TunnelUploadTest, SchemaContainsDefaultValues)
{
    auto dvTableName = Utils::GetRandomTableName();
    Utils::DropTableIfExists(sTableName);
    Utils::DropTableIfExists(dvTableName);
    Utils::ExecSql("create table " + dvTableName + " (col1 bigint, col2 string default 'hello')");
    // Ensure to clear this table after test failed.
    sTableName = dvTableName;
    // Create Upload
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, dvTableName);
    ASSERT_NE(upload->GetUploadId(), "");
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_NE(upload->GetSchema(), nullptr);
    // inspect the schema, should contain default values..
    auto schema = upload->GetSchema();
    // cast to suppress type mismatch warning..
    ASSERT_EQ(schema->GetColumnCount(), static_cast<decltype(schema->GetColumnCount())>(2));
    // the first column dont have default value
    ASSERT_EQ(schema->GetTableColumn(0).GetDefaultValue(), "");
    // the second column should have default value "hello"
    ASSERT_EQ(schema->GetTableColumn(1).GetDefaultValue(), "hello");
}

TEST_F(TunnelUploadTest, Streamed)
{
    UploadStreamed([]{ return 666; }, 100, 2);

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(uint64_t(100 * 2), download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(0, 99999999);
    ODPSTableRecordPtr _r2 = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r2;
    for(int i = 0; i < 100 * 2; i++)
    {
        ASSERT_TRUE(reader->Read(r2));
        ASSERT_EQ(int64_t(666), *r2.GetBigIntValue(0));
    }
    download->Complete();
}

TEST_F(TunnelUploadTest, StreamedZstd)
{
    UploadStreamedZstd([]{ return 666; }, 100, 2);

    IDownloadPtr download = CreateDownload();
    ASSERT_EQ(uint64_t(100 * 2), download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(0, 99999999);
    ODPSTableRecordPtr _r2 = reader->CreateBufferRecord();
    ODPSTableRecord& r2 = *_r2;
    for(int i = 0; i < 100 * 2; i++)
    {
        ASSERT_TRUE(reader->Read(r2));
        ASSERT_EQ(int64_t(666), *r2.GetBigIntValue(0));
    }
    download->Complete();
}

TEST_F(TunnelUploadTest, PrimaryTypeCoverage)
{
    Utils::DropTableIfExists(sTableName);
    auto tunnel = Utils::GetTunnelInstance();
    uint64_t count = 100;
    std::vector<std::string> types = {"tinyint", "smallint", "int", "bigint",
        "boolean", "float", "double", "varchar(2)", "string",
        "timestamp", "timestamp_ntz", "binary", "datetime", "date", "decimal(38, 2)", "json"};
    std::string createTableSql = "set odps.sql.type.json.enable=true;set odps.sql.type.system.odps2=true;set odps.sql.decimal.odps2=true; create table " + sTableName + " (";
    for (size_t i = 0; i < types.size(); i++)
    {
        createTableSql += "c" + std::to_string(i) + " " + types[i];
        if (i != (types.size() - 1))
        {
            createTableSql += ", ";
        }
    }
    createTableSql += ") stored as aliorc";
    Utils::ExecSqlTogether(createTableSql);

    IUploadPtr upload = tunnel.CreateUpload(sProjectName, sTableName);
    ASSERT_NE(upload->GetUploadId(), "");
    std::cout << upload->GetUploadId() << std::endl;

    IRecordWriterPtr writer = upload->OpenWriter(0);
    {
        ODPSTableRecordPtr record = upload->CreateBufferRecord();
        for (uint64_t rc = 0; rc < count; rc++)
        {
            int i = 0;
            record->SetTinyIntValue(i++, rc);
            record->SetSmallIntValue(i++, rc);
            record->SetIntegerValue(i++, rc);
            record->SetBigIntValue(i++, rc);
            record->SetBoolValue(i++, rc % 2 == 0);
            record->SetFloatValue(i++, (float)rc);
            record->SetDoubleValue(i++, (double)rc);
            record->SetVarcharValue(i++, std::to_string(rc % 100).substr(0, 2));
            std::string src = std::to_string(rc);
            record->SetStringValue(i++, src);
            record->SetTimestampValue(i++, rc, rc * 1000000);
            record->SetTimestampNTZValue(i++, rc, rc * 1000000);
            record->SetBinaryValue(i++, src.c_str(), src.size());
            record->SetDatetimeValue(i++, rc);
            record->SetDateValue(i++, rc);
            record->SetDecimalValue(i++, src + ".98");
            record->SetJsonValue(i++, "{\"key\":\"val\"}");

            writer->Write(*record);
        }
    }

    writer->Close();
    upload->Commit({0});

    // DOWNLOAD
    IDownloadPtr download = tunnel.CreateDownload(sProjectName, sTableName, "", "");
    ASSERT_NE(download->GetDownloadId(), "");
    ASSERT_EQ(download->GetRecordCount(), count);
    std::cout << download->GetDownloadId() << std::endl;
    IRecordReaderPtr reader = download->OpenReader(0, count);
    ASSERT_NE(reader.get(), nullptr);
    ODPSTableRecordPtr record = reader->CreateBufferRecord();
    for (int64_t rc = 0; rc < (int64_t)count; rc++)
    {
        ASSERT_TRUE(reader->Read(*record));
        int i = 0;
        ASSERT_EQ(record->GetTinyInt(i++), rc);
        ASSERT_EQ(record->GetSmallInt(i++), rc);
        ASSERT_EQ(record->GetInteger(i++), rc);
        ASSERT_EQ(record->GetBigInt(i++), rc);
        ASSERT_EQ(record->GetBool(i++), rc % 2 == 0);
        ASSERT_EQ(record->GetFloat(i++), (float)rc);
        ASSERT_EQ(record->GetDouble(i++), (double)rc);
        ASSERT_EQ(record->GetVarchar(i++), std::to_string(rc % 100).substr(0, 2));
        std::string src = std::to_string(rc);
        ASSERT_EQ(record->GetString(i++), src);
        ASSERT_EQ(record->GetTimestamp(i++), TimeStamp(rc, rc * 1000000));
        ASSERT_EQ(record->GetTimestampNTZ(i++), TimeStamp(rc, rc * 1000000));
        ASSERT_EQ(record->GetBinary(i++), src);
        ASSERT_EQ(*(record->GetDatetimeValue(i++)), rc);
        ASSERT_EQ(*(record->GetDateValue(i++)), rc);
        ASSERT_EQ(record->GetDecimal(i++), src + ".98");
        ASSERT_EQ(record->GetJson(i++), "{\"key\":\"val\"}");
    }
    reader->Close();
    download->Complete();
}

TEST_F(TunnelUploadTest, ZstdCompressEmptyBodyBug)
{
    Utils::DropTableIfExists(sTableName);
    Utils::ExecSql("create table " + sTableName + " (a string, b string)");
    OdpsTunnel t = Utils::GetTunnelInstance(false, CompressOption::ZSTD_COMPRESS);
    IUploadPtr upload = t.CreateUpload(sProjectName, sTableName, "", "");

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);

    uint32_t blockId = 0;
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    IRecordWriterPtr writer = upload->OpenWriter(blockId, true);
    ASSERT_TRUE(writer.get() != NULL);
    ODPSTableRecord& r1 = *_r;

    std::string randstr = "iEEZO0VszRPGrEOXfvp5psbKO371X4FIZ04GawSpl9T17xIEWjLiB8COnSKgouB0mvwCgou7hPHLBNoqKLAzTszyup0RvanrGorxqKD54bRV8HvMDcmwT0cDU0WJJ92MTyhAV8C0PVlSn6sN0B2xk6DvxpIoxXMQus4eDJ9JVnwVUFlKuchl1YOu7Cs2kvYQtYfUP5SZMsDzFs6xHZ3gOWr76H0b4nTtSeeTImtFWttcYFhd6GkjIOOlBXfGVlFeviMhmaUJJSMPr4kZyNogVAcRp9Y3WdVwV7ovUEbh479HMjrBb7X7QU0ZcjRyLjkprXexUH1HCbmmROzG4GRMzHuK75TwO5NlBLl7ZgmEimcceAPtXGlNukdKP4v1p31SkcXDQk9BrhmedJJ6hZDtOK8AZMYcAxKepJ1g3hpbvmfF8wBIpcKAFYwQUy5Dkttu92y04VZTVoUIBKsNHRMHvNfocGa6rhfNEOeOspwdTZX74SYP0xbjQa1pCflsBcRUrntay2sPRsZjIZCEwlRP0lzCpAJvN8neS3WrahUyYcd7WeBmnFNPrcHhoKefLNk0ah0VDZx1L8ZAVibpguiscpeIXC0O51j8UsFLzOmfxeABo5FImwTwivQuG4W1EIbNUcqlN2336cVHIsbT7DsNpsIc2NSvHg0FIspdThQZJe2J0KeUmorBUz79qnVwESN5aSHzPGaTYYvLct7QmZPNOugxsuGJTHzB7LJuunbeFXPb2M3r0pOkeEQ2WMP5ockcdzOfGiiNmS36DfLMBfuOYI0GqsnbxUYz8q3BWWWhhnUjLMt53eBgdxszyxAAi0XYojmZPsV8hvwPR0nFdeIgn34OkkPI0k9lJ41okPnQX1xUj8U7rgDSC7J4juxCFs1I6YBGxeiZ7QpUpypGRv3ADb5r7BN3eyuOUIPHF7Smhwn6goK02pDrPCGW7hCjVSy1xKOQh3Wfg1RUooqwj3t9JKI1LCQBDYVw4Pf";

    r1.SetStringValue(0, randstr);
    r1.SetStringValue(1, "a");
    writer->Write(r1);
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(blockId);
    upload->Commit(blocks);
}

TEST_F(TunnelUploadTest, SchemaEnabledTableUploadDownload)
{
    sProjectName = Utils::GetSchemaEnabledProjectName();
    const std::string& tableName = Utils::GetRandomTableName();
    mSchemaName = Utils::GetRandomSchemaName();
    mTableName = sProjectName + "." + mSchemaName + "." + tableName;
    std::map<std::string, std::string> hints;
    hints["odps.namespace.schema"] = "true";
    hints["odps.sql.allow.namespace.schema"] = "true";
    Utils::ExecSql(sProjectName, "create schema " + sProjectName + "." + mSchemaName + ";", hints, 120 * 1000);
    Utils::ExecSql(sProjectName, "create table if not exists " + mTableName + " (col1 bigint);", hints, 120 * 1000);
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, tableName, "", "", false, mSchemaName);

    ASSERT_TRUE(upload->GetUploadId() != "");
    std::cout << "create upload session, id=" << upload->GetUploadId() << std::endl;
    ASSERT_STRCASEEQ("NORMAL", upload->GetStatus().c_str());
    ASSERT_TRUE(upload->GetSchema() != NULL);
    ASSERT_TRUE(upload->GetSchema()->GetColumnCount() > 0);

    ODPSTableRecordPtr r = upload->CreateBufferRecord();
    r->SetBigIntValue(0, 0);

    IRecordWriterPtr writer = upload->OpenWriter(0);
    ASSERT_TRUE(writer.get() != NULL);
    writer->Write(*r);
    writer->Close();

    std::vector<uint32_t> blocks;
    blocks.push_back(0);
    upload->Commit(blocks);

    IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName, "", "", mSchemaName);
    std::cout << "create download session, id=" << download->GetDownloadId() << std::endl;
    ASSERT_EQ(1u, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(0, 2);
    ODPSTableRecordPtr record = reader->CreateBufferRecord();
    ASSERT_TRUE(reader->Read(*record));
    download->Complete();
    ASSERT_EQ(0, *(record->GetBigIntValue(0)));
}

TEST_F(TunnelUploadTest, SchemaEnabledTableStreamUploadDownload)
{
    sProjectName = Utils::GetSchemaEnabledProjectName();
    const std::string& tableName = Utils::GetRandomTableName();
    mSchemaName = Utils::GetRandomSchemaName();
    mTableName = sProjectName + "." + mSchemaName + "." + tableName;
    std::map<std::string, std::string> hints;
    hints["odps.namespace.schema"] = "true";
    hints["odps.sql.allow.namespace.schema"] = "true";
    Utils::ExecSql(sProjectName, "create schema " + sProjectName + "." + mSchemaName + ";", hints, 120 * 1000);
    Utils::ExecSql(sProjectName, "create table if not exists " + mTableName + " (col1 bigint);", hints, 120 * 1000);

    IStreamUploadPtr streamUpload = sTunnel.CreateStreamUpload(sProjectName, tableName, 1, CompressOption::ZSTD_COMPRESS, mSchemaName);
    std::cout << "create stream upload session, id=" << streamUpload->GetUploadId() << std::endl;
    IRecordPackPtr pack = streamUpload->CreateRecordPack(CompressOption::ZSTD_COMPRESS);
    ASSERT_TRUE(streamUpload->GetSchema() != NULL);
    ASSERT_EQ(1u, streamUpload->GetSchema()->GetColumnCount());
    ODPSTableRecordPtr r = streamUpload->CreateBufferRecord();
    r->SetBigIntValue(0, 0);
    pack->Append(*r);
    pack->Flush();

    IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName, "", "", mSchemaName);
    std::cout << "create download session, id=" << download->GetDownloadId() << std::endl;
    ASSERT_EQ(1u, download->GetRecordCount());
    IRecordReaderPtr reader = download->OpenReader(0, 2);
    ODPSTableRecordPtr record = reader->CreateBufferRecord();
    ASSERT_TRUE(reader->Read(*record));
    download->Complete();
    ASSERT_EQ(0, *(record->GetBigIntValue(0)));
}


TEST_F(TunnelUploadTest, CreateTable300Columns)
{
    // Utils::DropTableIfExists(sTableName);
    std::string tableName = "test_async_big_table";
    auto tunnel = Utils::GetTunnelInstance();
    uint64_t count = 300;
    std::vector<std::string> types = {"tinyint", "smallint", "int", "bigint",
        "boolean", "float", "double", "varchar(2)", "varchar(28)", "varchar(12)", "string",
        "timestamp", "binary", "datetime", "date", "decimal(38, 2)", "decimal(45, 21)"};

    std::string createTableSql = "set odps.sql.type.json.enable=true;set odps.sql.type.system.odps2=true;set odps.sql.decimal.odps2=true; create table " + tableName + " (";

    for (size_t i = 0; i < count; i++)
    {
        int idx = int(rand() % types.size());
        createTableSql += "c" + std::to_string(i) + " " + types[idx];
        if (i != (count - 1))
        {
            createTableSql += ", ";
        }
    }
    createTableSql += ") stored as aliorc";
    Utils::ExecSqlTogether(createTableSql);
}

TEST_F(TunnelUploadTest, CreateUploadWithV4Signature)
{
    IUploadPtr upload = CreateUploadWithV4Signature();

    ASSERT_TRUE(upload->GetUploadId() != "");
}
