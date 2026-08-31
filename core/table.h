#ifndef APSARA_ODPS_SDK_TABLE_H
#define APSARA_ODPS_SDK_TABLE_H

#include "odps_table.h"
#include "rest_client.h"
#include "rest_path.h"
#include "common/marked_iterator.h"
#include "common/odps_table_schema.h"
#include <nlohmann/json.hpp>


namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal
{

class ODPSTable : public IODPSTable
{
public:
    friend void to_json(nlohmann::json& j, const ODPSTable& t);
    friend void from_json(const nlohmann::json& j, ODPSTable& t);

    ODPSTable(const std::string &project, const std::string& schema, const std::string &name, RestClientPtr client);
    virtual ~ODPSTable();

    virtual void Reload();

    // 表名
    virtual const std::string& GetName() override;
    // 所属project名
    virtual const std::string& GetProjectName() override;
    // 表的拥有者
    virtual const std::string& GetSchemaName() override;
    virtual const std::string& GetOwner() override;
    virtual IODPSTable& SetOwner(const std::string &owner) override;
    // 表的comment
    virtual const std::string& GetComment() override;
    // 表的id
    virtual const std::string& GetTableId() override;
    // 表的Record数, 若无准确数据，则返回-1
    virtual long GetRecordNum() override;
    // 表的生命周期值，单位:天
    virtual long GetLife() override;
    // 表的datahub生命周期值，单位:天
    virtual long GetHubLifecycle() override;
    // 内部存储大小，单位:Byte
    virtual long GetSize() override;
    // label
    virtual const std::string& GetTableLabel() override;
    // extended label
    virtual const std::vector<std::string>& GetExtendedLabel() override;
    // 判断表是否为外部表
    virtual bool IsExternalTable() override;
    // 判断表是否为虚拟视图
    virtual bool IsVirtualView() override;
    // 创建时间，单位:秒
    virtual long GetCreationTime() override;
    // 最后修改时间，单位:秒
    virtual long GetLastModifiedTime() override;
    // 最后一次DDL时间，单位:秒
    virtual long GetLastDDLTime() override;

    // 获取表Schema
    virtual IODPSTableSchemaPtr GetSchema() override;

    virtual std::shared_ptr<Iterator<ODPSPartitionBasicInfo>> ListPartitions(int64_t batchSize = 1000) override;

    virtual bool GetPartitionNames(std::vector<std::string>& partNames) override;

    virtual IODPSPartitionPtr GetPartition(const std::vector<std::pair<std::string, std::string>>& partitionValues) override;
    virtual IODPSPartitionPtr GetPartition(const std::string& partitionSpec) override;

    virtual ODPSTableExtendedInfo GetExtendedInfo() override;

    // for debug
    virtual std::string ToString() override;

    void LoadFromXml(tinyxml2::XMLElement* tableElement);

private:
    // format pt1=<>/pt2=<> to pt1='<>',pt2='<>', part names returned is url escaped and we need un-escape it
    std::string FormatPartName(const std::string partName);

protected:
    // TODO: rename these member variables to conform with tunnel code style
    // 构造时指定
    std::string projectName;
    std::string schemaName;
    std::string tableName;

    // 单独获取
    std::string owner;
    std::string tableId;
    std::string comment;
    long createTime = 0;
    std::vector<std::string> extendedLabel;
    long hubLifecycle = -1L;
    bool isExternal = false;
    bool isVirtualView = false;
    long lastDDLTime = 0;
    long lastModifiedTime = 0;
    long lifecycle = -1L;
    long recordNum = -1L;
    bool shardExist = false;
    long size = 0;
    std::string tableLabel;

    std::vector<ODPSTableColumn> tempColumns;
    std::vector<ODPSTableColumn> tempPartitionKeys;
    // 表结构
    ODPSTableSchemaPtr schema;

    RestClientPtr mRestClient;
};

typedef std::shared_ptr<ODPSTable> ODPSTablePtr;

ODPSTableExtendedInfo GetTableExtendedInfo(RestClientPtr restClient, const std::string& project, const std::string& schema, const std::string& table);

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif