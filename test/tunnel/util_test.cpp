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

class TunnelUtilTest : public testing::Test
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
        mBlockId = 0;
        Utils::DropTableIfExists(mTableName);
    }

    virtual void TearDown()
    {
        Utils::DropTableIfExists(mTableName);
    }
    std::string mProjectName;
    std::string mTableName;
    OdpsTunnel mTunnel;
    int mBlockId;

    void UploadIntoPartition(const std::string& partitionspec)
    {
        IUploadPtr upload = mTunnel.CreateUpload(mProjectName, mTableName, partitionspec);
        ASSERT_NE(upload->GetUploadId(), "");
        unsigned blkId = mBlockId++;
        IRecordWriterPtr wr = upload->OpenWriter(blkId);
        ASSERT_NE(wr.get(), nullptr);
        IODPSTableSchema* schema = upload->GetSchema();
        ASSERT_NE(schema, nullptr);
        ODPSTableRecordPtr _r = upload->CreateBufferRecord();
        ODPSTableRecord& r = *_r;
        for (size_t i = 0; i < 100; i++)
        {
            if (i % 10 == 0)
            {
                r.SetNullValue(0);
            }
            else
            {
                r.SetBigIntValue(0, i);
            }
            ASSERT_EQ(wr->Write(r), true);
        }
        wr->Close();
        upload->Commit({blkId});
    }
};

TEST_F(TunnelUtilTest, TestPartitionSpecReformat)
{
    ASSERT_EQ(ReformatPartitionSpec(""), "");
    ASSERT_EQ(ReformatPartitionSpec("pt='a'"), "pt=a");
    ASSERT_EQ(ReformatPartitionSpec("pt=\"b\""), "pt=b");
    ASSERT_EQ(ReformatPartitionSpec("pt = \"b\""), "pt=b");
    ASSERT_EQ(ReformatPartitionSpec("pt =a, pd=b"), "pt=a,pd=b");
    ASSERT_EQ(ReformatPartitionSpec("pt ='a', pd= \"b\""), "pt=a,pd=b");
}

TEST_F(TunnelUtilTest, TestPartitionSpecReformatToUpload)
{
    Utils::ExecSql("create table " + mTableName + " (i bigint) partitioned by (dt string, pt bigint)");
    // create partition
    Utils::ExecSql("alter table " + mTableName + " add partition (dt=12345,pt=2)");
    // upload
    UploadIntoPartition("dt='12345',pt=2");
    UploadIntoPartition("dt=\"12345\",pt=2");
    UploadIntoPartition("dt= '12345' , pt = 2");
    UploadIntoPartition("dt= '12345' , pt = '2'");
}