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

class TunnelQuotaTest : public testing::Test
{
protected:

    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        mProjectName = Utils::GetProjectName();
        mTableName = Utils::GetRandomTableName();
        mTunnel = Utils::GetTunnelInstance();
        Utils::DropTableIfExists(mTableName);
        Utils::ExecSql("create table " + mTableName + " (col1 bigint)");
    }

    virtual void TearDown()
    {
        Utils::DropTableIfExists(mTableName);
        if (!mSchemaName.empty())
        {
            if (!mTableName.empty())
            {
                std::map<std::string, std::string> hints;
                hints["odps.namespace.schema"] = "true";
                hints["odps.sql.allow.namespace.schema"] = "true";
                Utils::ExecSql(mProjectName, "drop table if exists " + mTableName + ";", hints, 120 * 1000);
            }
            std::map<std::string, std::string> hints;
            hints["odps.namespace.schema"] = "true";
            hints["odps.sql.allow.namespace.schema"] = "true";
            Utils::ExecSql(mProjectName, "drop schema " + mProjectName + "." + mSchemaName + ";", hints, 120 * 1000);
        }
    }

public:
    std::string mProjectName;
    std::string mTableName;
    std::string mSchemaName;
    IODPSPtr mODPS;
    OdpsTunnel mTunnel;
};

TEST_F(TunnelQuotaTest, TestCreateUpload)
{
    auto conf = Utils::GetConfiguration();
    conf.SetTunnelQuotaName("not_exist_quota_name");
    OdpsTunnel tunnel;
    tunnel.Init(conf);
    try
    {
        tunnel.CreateUpload(mProjectName, mTableName);
        ASSERT_FALSE(true) << "should throw quota not found";
    }
    catch (const OdpsException& ex)
    {
        ASSERT_NE(ex.GetErrorMsg().find("not_exist_quota_name"), std::string::npos) << ex.GetErrorMsg();
    }
}

TEST_F(TunnelQuotaTest, TestCreateDownload)
{
    auto conf = Utils::GetConfiguration();
    conf.SetTunnelQuotaName("not_exist_quota_name");
    OdpsTunnel tunnel;
    tunnel.Init(conf);
    try
    {
        tunnel.CreateDownload(mProjectName, mTableName);
        ASSERT_FALSE(true) << "should throw quota not found";
    }
    catch (const OdpsException& ex)
    {
        ASSERT_NE(ex.GetErrorMsg().find("not_exist_quota_name"), std::string::npos) << ex.GetErrorMsg();
    }
}

TEST_F(TunnelQuotaTest, TestUploadDefault)
{
    auto conf = Utils::GetConfiguration();
    conf.SetTunnelQuotaName("default");
    OdpsTunnel tunnel;
    tunnel.Init(conf);
    auto upload = tunnel.CreateUpload(mProjectName, mTableName);
    auto record = upload->CreateBufferRecord();
    record->SetBigIntValue(0, 1);
    auto writer = upload->OpenWriter(0);
    writer->Write(*record);
    writer->Close();
    upload->Commit();
}

TEST_F(TunnelQuotaTest, TestDownloadDefault)
{
    auto conf = Utils::GetConfiguration();
    conf.SetTunnelQuotaName("default");
    OdpsTunnel tunnel;
    tunnel.Init(conf);
    auto upload = tunnel.CreateUpload(mProjectName, mTableName);
    auto record = upload->CreateBufferRecord();
    record->SetBigIntValue(0, 1);
    auto writer = upload->OpenWriter(0);
    writer->Write(*record);
    writer->Close();
    upload->Commit();
    auto download = tunnel.CreateDownload(mProjectName, mTableName);
    auto reader = download->OpenReader(0, 1);
    record = reader->CreateBufferRecord();
    while(reader->Read(*record));
    reader->Close();
}

TEST_F(TunnelQuotaTest, TestStreamUploadDefault)
{
    auto conf = Utils::GetConfiguration();
    conf.SetTunnelQuotaName("default");
    OdpsTunnel tunnel;
    tunnel.Init(conf);
    auto upload = tunnel.CreateStreamUpload(mProjectName, mTableName);
    auto record = upload->CreateBufferRecord();
    auto pack = upload->CreateRecordPack();
    record->SetBigIntValue(0, 1);
    pack->Append(*record);
    pack->Flush();
}