#include "odps_exception.h"
#include "table.h"
#include "util/utils.h"
#include "tinyxml2.h"
#include "common/marked_iterator.h"
#include "core/partition.h"
#include "common/json_serialize.h"
#include "util/string_util.h"
#include "curl/curl.h"
namespace apsara { namespace odps { namespace sdk { namespace internal {

ODPSTable::ODPSTable(const std::string &project, const std::string& schema, const std::string &name, RestClientPtr client)
    : projectName(project)
    , schemaName(schema)
    , tableName(name)
    , mRestClient(client)
{
    Reload();
}

ODPSTable::~ODPSTable()
{
}

const std::string& ODPSTable::GetName()
{
    return this->tableName;
}

IODPSTable& ODPSTable::SetOwner(const std::string &owner)
{
    this->owner = owner;
    return *this;
}

const std::string& ODPSTable::GetOwner()
{
    return this->owner;
}

const std::string& ODPSTable::GetProjectName()
{
    return this->projectName;
}

const std::string& ODPSTable::GetSchemaName()
{
    return schemaName;
}

const std::string& ODPSTable::GetComment()
{
    return this->comment;
}

const std::string& ODPSTable::GetTableId()
{
    return this->tableId;
}

long ODPSTable::GetRecordNum()
{
    return this->recordNum;
}

long ODPSTable::GetLife()
{
    return this->lifecycle;
}

long ODPSTable::GetHubLifecycle()
{
    return this->hubLifecycle;
}

long ODPSTable::GetSize()
{
    return this->size;
}

const std::string& ODPSTable::GetTableLabel()
{
    return this->tableLabel;
}

const std::vector<std::string>& ODPSTable::GetExtendedLabel()
{
    return this->extendedLabel;
}

bool ODPSTable::IsExternalTable()
{
    return this->isExternal;
}

bool ODPSTable::IsVirtualView()
{
    return this->isVirtualView;
}

IODPSTableSchemaPtr ODPSTable::GetSchema()
{
    return this->schema;
}

long ODPSTable::GetCreationTime()
{
    return this->createTime;
}

long ODPSTable::GetLastModifiedTime()
{
    return this->lastModifiedTime;
}

long ODPSTable::GetLastDDLTime()
{
    return this->lastDDLTime;
}

void to_json(nlohmann::json& j, const ODPSTable& t)
{
    j = nlohmann::json{
        {"tableName", t.tableName},
        {"projectName", t.projectName},
        {"owner", t.owner},
        {"tableId", t.tableId},
        {"comment", t.comment},
        {"createTime", t.createTime},
        {"extendedLabel", t.extendedLabel},
        {"hubLifecycle", t.hubLifecycle},
        {"isExternal", t.isExternal},
        {"isVirtualView", t.isVirtualView},
        {"lastDDLTime", t.lastDDLTime},
        {"lastModifiedTime", t.lastModifiedTime},
        {"lifecycle", t.lifecycle},
        {"recordNum", t.recordNum},
        {"shardExist", t.shardExist},
        {"size", t.size},
        {"tableLabel", t.tableLabel},
    };
}

void from_json(const nlohmann::json& j, ODPSTable& t)
{
    t.owner = j.value("owner", "");
    t.createTime = j.value("createTime", 0L);
    t.extendedLabel = j.value("extendedLabel", std::vector<std::string>());
    t.hubLifecycle = j.value("hubLifecycle", -1L);
    t.isExternal = j.value("isExternal", false);
    t.isVirtualView = j.value("isVirtualView", false);
    t.lastDDLTime = j.value("lastDDLTime", 0L);
    t.lastModifiedTime = j.value("lastModifiedTime", 0L);
    t.lifecycle = j.value("lifecycle", -1L);
    t.recordNum = j.value("recordNum", -1L);
    t.size = j.value("size", 0L);
    t.tableLabel = j.value("tableLabel", "");
    t.tempColumns = j.value("columns", std::vector<ODPSTableColumn>());
    t.tempPartitionKeys = j.value("partitionKeys", std::vector<ODPSTableColumn>());
}

void ODPSTable::LoadFromXml(tinyxml2::XMLElement *ele)
{
    if (ele->FirstChildElement("Comment") != nullptr)
    {
        const char *comment_c = ele->FirstChildElement("Comment")->GetText();
        if (comment_c)
        {
            this->comment = strcmp(comment_c, "") == 0 ? "" : std::string(comment_c);
        }
        else
        {
            this->comment = "";
        }
    }
    // table id
    if (ele->FirstChildElement("TableId") != nullptr)
    {
        std::string tableId(ele->FirstChildElement("TableId")->GetText());
        this->tableId = tableId;
    }

    std::string schemaRaw(ele->FirstChildElement("Schema")->GetText());

    util::FromJsonString(*this, schemaRaw);

    if (tempColumns.size() != 0)
    {
        this->schema = std::make_shared<ODPSTableSchema>();
        this->schema->AppendColumns(tempColumns);
        this->schema->AppendPartitions(tempPartitionKeys);
    }
}

void ODPSTable::Reload()
{
    std::string resource = RestResourceBuilder::BuildTableRest(this->projectName, this->tableName);
    map<string, string> headers;
    map<string, string> params;
    if (!schemaName.empty())
    {
        params["curr_schema"] = schemaName;
    }
    RestResponsePtr resp = std::dynamic_pointer_cast<RestResponse>(
        this->mRestClient->DoRequest(resource, HTTP_METHOD_GET, headers, params, "", true)
    );
    RestUtils::HandleOdpsFailures(resp);

    tinyxml2::XMLElement *ele = resp->xml_body.FirstChildElement("Table");
    LoadFromXml(ele);
}

ODPSTableExtendedInfo GetTableExtendedInfo(RestClientPtr restClient, const std::string& project, const std::string& schema, const std::string& table)
{
    std::string resource = RestResourceBuilder::BuildTableRest(project, table);
    map<string, string> headers;
    map<string, string> params;
    params["extended"] = "";
    if (!schema.empty())
    {
        params["curr_schema"] = schema;
    }
    RestResponsePtr resp = std::dynamic_pointer_cast<RestResponse>(
        restClient->DoRequest(resource, HTTP_METHOD_GET, headers, params, "", true)
    );
    RestUtils::HandleOdpsFailures(resp);

    ODPSTableExtendedInfo extInfo;
    tinyxml2::XMLHandle handle(resp->xml_body);
    tinyxml2::XMLElement *ele = handle.FirstChildElement("Table").FirstChildElement("Schema").ToElement();
    if (ele)
    {
        util::FromJsonString(extInfo, ele->GetText());
    }
    else
    {
        util::OdpsThrow(INTERNAL_ERROR, "Invalid Server XML Response:", resp->GetBody());
    }
    return extInfo;
}

ODPSTableExtendedInfo ODPSTable::GetExtendedInfo()
{
    return GetTableExtendedInfo(mRestClient, projectName, schemaName, tableName);
}

std::string ODPSTable::ToString()
{
    return util::ToJsonString(*this);
}

class PartitionIterator: public MarkedIterator<ODPSPartitionBasicInfo>
{
public:
    PartitionIterator(ODPSTable& table, RestClientPtr client, int64_t batchSize):
        mProjectName(table.GetProjectName()),
        mSchemaName(table.GetSchemaName()),
        mTableName(table.GetName()),
        mBatchSize(batchSize),
        mClient(client)
        {}

    virtual ~PartitionIterator() {}

protected:
    virtual std::string Refill(
        boost::optional<std::string> lastMarker,
        std::vector<std::shared_ptr<ODPSPartitionBasicInfo>>& container) override
    {
        std::map<std::string, std::string> params;
        params["partitions"] = "";
        params["expectmarker"] = "true";
        params["maxitems"] = std::to_string(mBatchSize);
        if (lastMarker)
        {
            params["marker"] = lastMarker.value();
        }
        if (!mSchemaName.empty())
        {
            params["curr_schema"] = mSchemaName;
        }
        std::string resource = RestResourceBuilder::BuildTableRest(mProjectName, mTableName);

        RestResponsePtr resp = std::dynamic_pointer_cast<RestResponse>(
            mClient->DoRequest(resource, HTTP_METHOD_GET, {}, params, "", true)
        );
        RestUtils::HandleOdpsFailures(resp);

        tinyxml2::XMLElement *ele = resp->xml_body.FirstChildElement("Partitions");
        std::string marker;
        for (auto it = ele->FirstChildElement(); it != nullptr; it = it->NextSiblingElement())
        {
            if (it->Name() == std::string("Marker"))
            {
                if (it->GetText())
                {
                    marker = it->GetText();
                }
            }
            else if (it->Name() == std::string("Partition"))
            {
                int64_t createTime = 0, lastDDLTime = 0, lastModifiedTime = 0, lastAccessTime = 0;
                std::vector<std::pair<std::string, std::string>> partitionValues;
                for (auto pit = it->FirstChildElement(); pit != nullptr; pit = pit->NextSiblingElement())
                {
                    if (pit->Name() == std::string("CreationTime"))
                    {
                        pit->QueryInt64Text(&createTime);
                    }
                    else if (pit->Name() == std::string("LastDDLTime"))
                    {
                        pit->QueryInt64Text(&lastDDLTime);
                    }
                    else if (pit->Name() == std::string("LastModifiedTime"))
                    {
                        pit->QueryInt64Text(&lastModifiedTime);
                    }
                    else if (pit->Name() == std::string("LastAccessTime"))
                    {
                        pit->QueryInt64Text(&lastAccessTime);
                    }
                    else if (pit->Name() == std::string("Column"))
                    {
                        partitionValues.push_back(std::make_pair(
                            std::string(pit->Attribute("Name")),
                            std::string(pit->Attribute("Value"))
                        ));
                    }
                }
                std::shared_ptr<ODPSPartitionBasicInfo> tmp = std::make_shared<ODPSPartitionBasicInfo>();
                tmp->mPartitionValues = partitionValues;
                tmp->mCreateTime = createTime;
                tmp->mLastDDLTime = lastDDLTime;
                tmp->mLastModifiedTime = lastModifiedTime;
                tmp->mLastAccessTime = lastAccessTime;
                container.push_back(tmp);
            }
        }
        return marker;
    }

private:
    std::string mProjectName;
    std::string mSchemaName;
    std::string mTableName;
    int64_t mBatchSize;

    RestClientPtr mClient;

};

std::shared_ptr<Iterator<ODPSPartitionBasicInfo>> ODPSTable::ListPartitions(int64_t batchSize)
{
    return std::make_shared<PartitionIterator>(*this, mRestClient, batchSize);
}

std::string ODPSTable::FormatPartName(const std::string partName)
{
    std::string tmp;
    std::vector<std::string> parts = util::SplitString(partName, "/");
    bool first = true;
    for (auto part: parts)
    {
        if (!first)
        {
            tmp += ",";
        }
        else
        {
            first = false;
        }
        std::vector<std::string> kv = util::SplitString(part, "=");
        if (kv.size() != 2)
        {
            throw OdpsTunnelException("InvalidArgument", "PartitionSpec invalid: " + part);
        }
        std::string k = util::TrimString(kv[0]);
        std::string v = util::TrimString(kv[1]);
        if (k.size() == 0 || v.size() == 0)
        {
            throw OdpsTunnelException("InvalidArgument",
                    "PartitionSpec invalid: " + part +
                    " Found PartitionKeySize:" + std::to_string(k.size()) +
                    " PartitionValueSize:" + std::to_string(v.size()));
        }

        auto v2 = curl_unescape(v.c_str(), v.size());
        // reformat it.
        tmp += k;
        tmp += "='";
        tmp += v2;
        tmp += "'";
        curl_free(v2);
    }
    return tmp;
}

bool ODPSTable::GetPartitionNames(std::vector<std::string>& partNames)
{
    std::map<std::string, std::string> params;
    params["partitions"] = "";
    params["name"] = "";
    if (!schemaName.empty())
    {
        params["curr_schema"] = schemaName;
    }

    std::string resource = RestResourceBuilder::BuildTableRest(projectName, tableName);

    RestResponsePtr resp = std::dynamic_pointer_cast<RestResponse>(
        mRestClient->DoRequest(resource, HTTP_METHOD_GET, {}, params, "", true)
    );
    RestUtils::HandleOdpsFailures(resp);

    tinyxml2::XMLElement *ele = resp->xml_body.FirstChildElement("Partitions");
    std::string marker;
    for (auto it = ele->FirstChildElement(); it != nullptr; it = it->NextSiblingElement())
    {
        if (it->Name() != std::string("Partition"))
        {
            continue;
        }

        for (auto pit = it->FirstChildElement(); pit != nullptr; pit = pit->NextSiblingElement())
        {
            if (pit->Name() != std::string("Name"))
            {
                continue;
            }
            auto partName = std::string(pit->GetText());
            partNames.push_back(FormatPartName(partName));
        }
    }
    return true;
}

IODPSPartitionPtr ODPSTable::GetPartition(const std::vector<std::pair<std::string, std::string>> &partitionValues)
{
    return std::make_shared<ODPSPartition>(*this, partitionValues, mRestClient);
}

IODPSPartitionPtr ODPSTable::GetPartition(const std::string& partitionSpec)
{
    return std::make_shared<ODPSPartition>(*this, partitionSpec, mRestClient);
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
