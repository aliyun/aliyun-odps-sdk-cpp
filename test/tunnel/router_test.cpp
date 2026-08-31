#include <stdlib.h>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"

#include "tunnel/util.h"
//#include "http_connection.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

class TunnelRouterTest : public testing::Test
{
protected:
    static std::string sTableName;
    static void SetUpTestCase()
    {
        sTableName = Utils::GetRandomTableName();
        Utils::DropTableIfExists(sTableName);
        Utils::ExecSql("create table " + sTableName + "(c1 bigint, c2 string)");
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
};

std::string TunnelRouterTest::sTableName;

// 指定的project参数为empty，返回空地址
TEST_F(TunnelRouterTest, ProjectIsEmpty)
{
    std::string EmptyProjectName;
    ASSERT_EQ(GetRouterServer(Utils::GetConfiguration(), EmptyProjectName), std::string());
}

// override
TEST_F(TunnelRouterTest, TunnelOverridesOdpsEndpoint)
{
    Configuration conf = Utils::GetConfiguration();
    conf.SetTunnelEndpoint("test");
    ASSERT_EQ(GetRouterServer(conf, Utils::GetProjectName()), "test");
}

// 指定的ODPS endpoint错误，抛异常
TEST_F(TunnelRouterTest, OdpsEndpointIsIncorrect)
{
    Configuration conf = Utils::GetConfiguration();
    conf.SetEndpoint(Utils::GetTunnelEndpoint());
    // because this is detected by the http client, not tunnel logic. so odpsexception thrown
    ASSERT_THROW(GetRouterServer(conf, Utils::GetProjectName()), OdpsException);
}

//准确得到路由信息
TEST_F(TunnelRouterTest, Normal)
{
    Configuration conf = Utils::GetConfiguration();
    conf.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
    ASSERT_EQ(GetRouterServer(conf, Utils::GetProjectName()), Utils::GetTunnelEndpoint());
}

// 单线程下载, 无压缩, 准确download数据
TEST_F(TunnelRouterTest, NormalUploadDownloadData)
{
    std::string projectName = Utils::GetProjectName();
    OdpsTunnel tunnel = Utils::GetTunnelInstance(true);
    uint64_t count = 1024;

    // upload data
    IUploadPtr upload = tunnel.CreateUpload(projectName, sTableName);
    IRecordWriterPtr writer = upload->OpenWriter(0);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < count; ++i)
    {
        const std::string str = "string_" + std::to_string(i);
        record.SetBigIntValue(0, i);
        record.SetStringValue(1, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    std::vector<uint32_t> blocks;
    blocks.push_back(0);
    upload->Commit(blocks);

    // download data
    IDownloadPtr download = tunnel.CreateDownload(projectName, sTableName);

    ASSERT_NE("", download->GetDownloadId());
    ASSERT_STRCASEEQ("NORMAL", download->GetStatus().c_str());
    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(2), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(count, download->GetRecordCount());

    uint64_t number = 0;
    IRecordReaderPtr reader = download->OpenReader(0, count);
    while (reader->Read(record))
    {
        ASSERT_EQ(static_cast<int64_t>(number), *record.GetBigIntValue(0));

        const std::string str = "string_" + std::to_string(number);
        uint32_t len = 0;
        const char *val = record.GetStringValue(1, len);
        ASSERT_STREQ(str.c_str(), val);
        ++number;
    }
    reader->Close();
    download->Complete();

    ASSERT_EQ(count, number);
}
