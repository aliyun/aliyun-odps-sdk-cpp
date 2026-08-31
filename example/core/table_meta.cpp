#include "odps_api.h"
#include "example/common/example_config.h"
#include <iostream>

using namespace std;
using namespace apsara::odps::sdk;



int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: table_meta <table_name> [schema]" << std::endl;
        std::cout << "endpoint / credentials / project are read from conf/testing.conf"
                  << std::endl;
        return 0;
    }

    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string project = config.mProjectName;
    string table_name(argv[1]);
    string schema;
    if (argc > 2) {
        schema = argv[2];
    }

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, config.mOdpsEndpoint);

    try
    {

        IODPSPtr odps = IODPS::Create(conf, project);

        IODPSTablePtr table = odps->GetTables()->Get(project, schema, table_name);
        odps->GetTables()->GetTableExtendedInfo(project, schema, table_name);
        ODPSTableExtendedInfo tableInfo = table->GetExtendedInfo();

        std::cout << "Table " << table_name << " "
            << "CC \"" << table->GetComment() << "\" "
            << "SZ " << table->GetSize() << " "
            << "RC " << table->GetRecordNum() << " "
            << "CT " << table->GetCreationTime() << " "
            << "MT " << table->GetLastModifiedTime() << " "
            << "DT " << table->GetLastDDLTime() << " "
            << "LC " << table->GetLife() << " "
            << "FN " << tableInfo.mFileNum << " "
            << "PS " << tableInfo.mPhysicalSize << " "
            << "CL " << (tableInfo.mReserved.mClusterType.empty() ? std::string("N/A") : tableInfo.mReserved.mClusterType) << " "
            << "BN " << tableInfo.mReserved.mBucketNum << " "
            << "ATTR "
                << (tableInfo.mIsArchived ? "Z" : "-")
                << (tableInfo.mReserved.mIsTransactional ? "T" : "-")
                << (tableInfo.mReserved.mHasRowAccessPolicy ? "R" : "-")
            << std::endl;

        std::vector<std::string> partNames;
        table->GetPartitionNames(partNames);
        for (auto it = partNames.begin(); it != partNames.end(); it++)
        {
            auto partition = table->GetPartition(*it);
            ODPSPartitionExtendedInfo extInfo = partition->GetExtendedInfo();
            std::cout << "Partition " << *it << " "
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
    catch (OdpsException& ex)
    {
        std::cout << ex.what() << std::endl;
    }

    return 0;
}