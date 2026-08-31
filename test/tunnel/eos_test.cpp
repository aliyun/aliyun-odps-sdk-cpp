#ifdef ODPS_SDK_ENABLE_ARROW
#include <cassert>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <stdlib.h>
#include <iostream>
#include <functional>
#include <thread>

#include "util/timer.h"
#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "include/error_code.h"
#include "gtest/gtest.h"
#include <string>
#include "test/common/test_util.h"
#include "arrow/api.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;

class TunnelEOSUploadTest : public testing::Test
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

    static EOSInfo UploadStreamed(IEOSStreamPtr clientStream, std::shared_ptr<arrow::Schema> schema, int64_t rows, EOSInfo offset)
    {
        try
        {
            arrow::MemoryPool* pool = arrow::default_memory_pool();
            arrow::Int64Builder firstColumnBuilder(pool);
            shared_ptr<arrow::Array> firstColumnArray;
            for (int64_t i = 0; i < rows; i++)
            {
                firstColumnBuilder.AppendValues({static_cast<int64_t>(i)}).ok();
            }
            firstColumnBuilder.Finish(&firstColumnArray).ok();
            shared_ptr<arrow::RecordBatch> batch = arrow::RecordBatch::Make(schema, rows, {firstColumnArray});
            clientStream->Write(*batch, offset);
            return offset;
        }
        catch (const ExactlyOnceAlreadyWrittenException& ex)
        {
            return EOSInfo{ex.GetSequenceId(), ex.GetSequenceOffset()};
        }
    }

    static void AssertTableRows(OdpsTunnel& tunnel, std::string projectName, std::string tableName, int64_t rows)
    {
        auto download = tunnel.CreateDownload(projectName, tableName);
        ASSERT_EQ(download->GetRecordCount(), uint64_t(rows));
    }

public:
    std::string mTableName;
    std::string mSchemaName;
    IODPSPtr mODPS;
};

std::string TunnelEOSUploadTest::sProjectName;
std::string TunnelEOSUploadTest::sTableName;
OdpsTunnel  TunnelEOSUploadTest::sTunnel;
uint32_t    TunnelEOSUploadTest::sBlockId = 0;
uint64_t    TunnelEOSUploadTest::sCount = 0;

TEST_F(TunnelEOSUploadTest, TestNormal)
{
    IStreamUploadPtr streamUpload = sTunnel.CreateStreamUpload(sProjectName, sTableName);
    auto clientStream = streamUpload->CreateEOSStream("c1", CompressOption::ZSTD_COMPRESS);
    EOSInfo tmp{0, 0};
    ASSERT_EQ(clientStream->Tell(), tmp);
    // written
    std::cout << "written" << std::endl;
    tmp.mSequenceId = 1;
    tmp.mSequenceOffset = 28;
    ASSERT_EQ(UploadStreamed(clientStream, streamUpload->GetArrowSchema(), 100, {1, 28}), tmp);
    ASSERT_EQ(clientStream->Tell(), tmp);
    AssertTableRows(sTunnel, sProjectName, sTableName, 100);
    // not written!
    std::cout << "not written" << std::endl;
    ASSERT_EQ(UploadStreamed(clientStream, streamUpload->GetArrowSchema(), 100, {1, 27}), tmp);
    std::cout << "try tell" << std::endl;
    ASSERT_EQ(clientStream->Tell(), tmp);
    AssertTableRows(sTunnel, sProjectName, sTableName, 100);
    // written 2
    std::cout << "written 2" << std::endl;
    tmp.mSequenceId = 1;
    tmp.mSequenceOffset = 29;
    ASSERT_EQ(UploadStreamed(clientStream, streamUpload->GetArrowSchema(), 100, {1, 29}), tmp);
    ASSERT_EQ(clientStream->Tell(), tmp);
    AssertTableRows(sTunnel, sProjectName, sTableName, 200);
    // {
    //     // recreate another instance, should persist
    //     std::cout << "another tell" << std::endl;
    //     OdpsTunnel dt;
    //     Configuration conf = Utils::GetConfiguration();
    //     conf.SetTunnelEndpoint("http://localhost:18091");
    //     dt.Init(conf);
    //     auto streamUpload = dt.CreateStreamUpload(sProjectName, sTableName);
    //     auto clientStream = streamUpload->CreateEOSStream("c1", CompressOption::ZSTD_COMPRESS);
    //     ASSERT_EQ(clientStream->Tell(), tmp);
    // }
    std::cout << "release" << std::endl;
    // release, back to 0
    auto startTime = util::timing::GetCurrentTimeInSeconds();
    while(true)
    {
        try
        {
            clientStream->Release();
            clientStream.reset();
            break;
        }
        catch (const OdpsException& ex)
        {
            std::cout << "DEBUG: " << ex.ToString() << std::endl;
            ASSERT_TRUE(util::timing::GetCurrentTimeInSeconds() - startTime < 30);
        }
    }
    std::cout << "release time: " << util::timing::GetCurrentTimeInSeconds() - startTime << std::endl;
    clientStream = streamUpload->CreateEOSStream("c1", CompressOption::ZSTD_COMPRESS);
    tmp.mSequenceId = 0;
    tmp.mSequenceOffset = 0;
    ASSERT_EQ(clientStream->Tell(), tmp);
    // written again
    std::cout << "written again" << std::endl;
    tmp.mSequenceId = 1;
    tmp.mSequenceOffset = 28;
    ASSERT_EQ(UploadStreamed(clientStream, streamUpload->GetArrowSchema(), 100, {1, 28}), tmp);
    ASSERT_EQ(clientStream->Tell(), tmp);
    AssertTableRows(sTunnel, sProjectName, sTableName, 300);
}

TEST_F(TunnelEOSUploadTest, TestConcurrentWriteSeal)
{
    IStreamUploadPtr streamUpload = sTunnel.CreateStreamUpload(sProjectName, sTableName);
    auto clientStream = streamUpload->CreateEOSStream("c1", CompressOption::ZSTD_COMPRESS);
    EOSInfo tmp{0, 0};
    ASSERT_EQ(clientStream->Tell(), tmp);
    std::atomic_bool fence{false};
    std::thread writerT{[&fence, &clientStream, &streamUpload]{
        try
        {
            arrow::MemoryPool* pool = arrow::default_memory_pool();
            arrow::Int64Builder firstColumnBuilder(pool);
            shared_ptr<arrow::Array> firstColumnArray;
            int64_t ROWS = 10000000;
            for (int64_t i = 0; i < ROWS; i++)
            {
                firstColumnBuilder.AppendValues({static_cast<int64_t>(i)}).ok();
            }
            firstColumnBuilder.Finish(&firstColumnArray).ok();
            shared_ptr<arrow::RecordBatch> batch = arrow::RecordBatch::Make(streamUpload->GetArrowSchema(), ROWS, {firstColumnArray});
            std::cout << "WR start!" << std::endl;
            fence.store(true);
            auto startTm = util::timing::GetCurrentTimeInSeconds();
            clientStream->Write(*batch, {1, 1});
            std::cout << "Write time: " << util::timing::GetCurrentTimeInSeconds() - startTm << std::endl;
        }
        catch (const std::exception& ex)
        {
            std::cout << "Writer Thread EX: " << ex.what() << std::endl;
        }
    }};
    std::thread releaserT{[&fence, &clientStream]{
        try
        {
            while(fence.load() != true) {}
            std::cout << "Wait 1 sec to seal!" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            clientStream->Release();
        }
        catch (const std::exception& ex)
        {
            std::cout << "Releaser Thread EX: " << ex.what() << std::endl;
        }
    }};
    writerT.join();
    releaserT.join();
}

#endif
