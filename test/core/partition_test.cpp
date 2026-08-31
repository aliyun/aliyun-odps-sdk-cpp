#include "common/logging.h"
#include "include/odps_api.h"
#include "test/common/test_util.h"
#include "gtest/gtest.h"
#include <iostream>
#include <stdlib.h>

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

class PartitionTest : public testing::Test
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
        mConf = Utils::GetConfiguration();
        mProjectName = Utils::GetProjectName();
        mOnePart = "table_with_one_partition";
        mTwoPart = "table_with_two_partition";

        std::string sql =
            "drop table if exists " + mOnePart + ";" +
            "create table if not exists " + mOnePart + "(col1 string, col2 bigint) partitioned by (pt1 string);" +
            "alter table " + mOnePart + " add if not exists partition(pt1='hello') partition(pt1='world') partition(pt1=':- $#.@!');";
        Utils::ExecSqlTogether(sql);
        sql =
            "drop table if exists " + mTwoPart + ";" +
            "create table if not exists " + mTwoPart + "(col1 string, col2 bigint) partitioned by (pt1 string, pt2 bigint);" +
            "alter table " + mTwoPart + " add if not exists " +
            "partition(pt1='hello world', pt2='122345') " +
            "partition(pt1=':- $#.@! fff', pt2=444) " +
            "partition(pt1='hi',pt2=1);";
        Utils::ExecSqlTogether(sql);
    }

    virtual void TearDown()
    {
    }

    Configuration mConf;
    std::string mProjectName;
    std::string mOnePart;
    std::string mTwoPart;
    std::vector<std::string> partNames1 = {
        "pt1=':- 0.@!'",
        "pt1='hello'",
        "pt1='world'"
    };
    std::vector<std::string> partNames2 = {
        "pt1=':- 0.@! fff',pt2='444'",
        "pt1='hello world',pt2='122345'",
        "pt1='hi',pt2='1'"
    };

    void GetPartitionNames(const std::string tableName, std::vector<std::string>& expectPartNames)
    {
        IODPSPtr odps = IODPS::Create(mConf, mProjectName);
        IODPSTablePtr table = odps->GetTables()->Get(mProjectName, tableName);
        std::vector<std::string> partNames;
        table->GetPartitionNames(partNames);

        EXPECT_EQ(partNames.size(), expectPartNames.size());

        for (size_t i = 0; i < partNames.size(); i++)
        {
            EXPECT_EQ(partNames[i], expectPartNames[i]);

            // there should not throw any exception
            auto partition = table->GetPartition(partNames[i]);
            ODPSPartitionExtendedInfo extInfo = partition->GetExtendedInfo();
            std::cout << "Partition " << partNames[i] << " "
                << "SZ " << partition->GetPartitionSize() << " "
                << "RC " << partition->GetPartitionRecordNum() << " "
                << "CT " << partition->GetCreateTime() << " "
                << "MT " << partition->GetLastModifiedTime() << " "
                << "AT " << partition->GetLastAccessTime() << " "
                << "DT " << partition->GetLastDDLTime() << " "
                << "FN " << extInfo.mFileNum << " "
                << "PS " << extInfo.mPhysicalSize << " "
                << "LC " << extInfo.mLifeCycle << " "
                << "CL " << (extInfo.mReserved.mClusterType.empty() ? std::string("N/A") : extInfo.mReserved.mClusterType) << " "
                << "BN " << extInfo.mReserved.mBucketNum << " "
                << "ATTR "
                << (extInfo.mIsExstore ? "X" : "-")
                << (extInfo.mIsArchived ? "Z" : "-")
                << std::endl;
        }
    }
};

TEST_F(PartitionTest, TestGetPartitionNames)
{
    GetPartitionNames(mOnePart, partNames1);
    GetPartitionNames(mTwoPart, partNames2);
}
