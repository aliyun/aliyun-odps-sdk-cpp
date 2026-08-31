#ifndef APSARA_ODPS_SDK_PARTITION_H
#define APSARA_ODPS_SDK_PARTITION_H

#include <nlohmann/json.hpp>
#include "odps_table.h"
#include "core/rest_client.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{


class ODPSPartition: public IODPSPartition
{
public:
    friend void from_json(const nlohmann::json& j, ODPSPartition& p);

    ODPSPartition(IODPSTable& table,
        const std::vector<std::pair<std::string, std::string>>& partitionValues,
        RestClientPtr client):
        mProjectName(table.GetProjectName()),
        mSchemaName(table.GetSchemaName()),
        mTableName(table.GetName()),
        mPartitionValues(partitionValues),
        mClient(client)
    {
        Reload();
    }

    ODPSPartition(IODPSTable& table,
        const std::string& partitionSpec,
        RestClientPtr client):
        mProjectName(table.GetProjectName()),
        mSchemaName(table.GetSchemaName()),
        mTableName(table.GetName()),
        mPartitionSpec(partitionSpec),
        mClient(client)
    {
        Reload();
    }


    virtual ~ODPSPartition() {}
    virtual std::string GetProjectName() const override { return mProjectName; }
    virtual std::string GetTableName() const override { return mTableName; }

    virtual const std::vector<std::pair<std::string, std::string>>& GetPartitionValues() const override { return mPartitionValues; }
    virtual std::string ToPartitionSpec() const override;
    virtual ODPSPartitionExtendedInfo GetExtendedInfo() override;
    virtual int64_t GetCreateTime() const override { return mCreateTime; };
    virtual int64_t GetLastDDLTime() const override { return mLastDDLTime; };
    virtual int64_t GetLastModifiedTime() const override { return mLastModifiedTime; };
    virtual int64_t GetLastAccessTime() const override { return mLastAccessTime; };
    virtual int64_t GetPartitionSize() const override { return mPartitionSize; };
    virtual int64_t GetPartitionRecordNum() const override { return mPartitionRecordNum; };

    virtual void Reload() override;

private:
    std::string mProjectName;
    std::string mSchemaName;
    std::string mTableName;
    std::vector<std::pair<std::string, std::string>> mPartitionValues;
    std::string mPartitionSpec;
    int64_t mCreateTime = -1;
    int64_t mLastDDLTime = -1;
    int64_t mLastModifiedTime = -1;
    int64_t mLastAccessTime = -1;
    int64_t mPartitionSize = -1;
    int64_t mPartitionRecordNum = -1;

    RestClientPtr mClient;
};

// 只用于解析服务端返回的分区元信息,该类型从不被序列化
inline void from_json(const nlohmann::json& j, ODPSPartition& p)
{
    p.mCreateTime = j.value("createTime", static_cast<int64_t>(-1));
    p.mLastAccessTime = j.value("lastAccessTime", static_cast<int64_t>(-1));
    p.mLastDDLTime = j.value("lastDDLTime", static_cast<int64_t>(-1));
    p.mLastModifiedTime = j.value("lastModifiedTime", static_cast<int64_t>(-1));
    p.mPartitionSize = j.value("partitionSize", static_cast<int64_t>(-1));
    p.mPartitionRecordNum = j.value("partitionRecordNum", static_cast<int64_t>(-1));
}


}}}}

#endif