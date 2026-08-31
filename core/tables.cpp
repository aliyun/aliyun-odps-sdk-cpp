#include "tables.h"
#include "tinyxml2.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {

ODPSTables::ODPSTables(
    const Configuration& conf,
    const std::string& project):
    mProjectName(project),
    mConf(conf),
    mRestClient(std::make_shared<RestClient>(conf))
{
}

ODPSTables::~ODPSTables()
{
}

class TableIterator: public MarkedIterator<ODPSTableBasicInfo>
{
public:
    TableIterator(const std::string& projectName,
                  const std::string& schemaName,
                  RestClientPtr client):
        mProjectName(projectName),
        mSchemaName(schemaName),
        mClient(client)
        {}

    virtual ~TableIterator() {}

protected:
    virtual std::string Refill(
        boost::optional<std::string> lastMarker,
        std::vector<std::shared_ptr<ODPSTableBasicInfo>>& container) override
    {
        std::map<std::string, std::string> params;
        params["expectmarker"] = "true";
        params["maxitems"] = "1000";
        if (lastMarker)
        {
            params["marker"] = lastMarker.value();
        }
        if (!mSchemaName.empty())
        {
            params["curr_schema"] = mSchemaName;
        }
        std::string resource = RestResourceBuilder::BuildTablesRest(mProjectName);

        RestResponsePtr resp = std::dynamic_pointer_cast<RestResponse>(
            mClient->DoRequest(resource, HTTP_METHOD_GET, {}, params, "", true)
        );
        RestUtils::HandleOdpsFailures(resp);

        tinyxml2::XMLElement *ele = resp->xml_body.FirstChildElement("Tables");
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
            else if (it->Name() == std::string("Table"))
            {
                std::shared_ptr<ODPSTableBasicInfo> info = std::make_shared<ODPSTableBasicInfo>();
                info->mTableName = it->FirstChildElement("Name")->GetText();
                info->mOwner = it->FirstChildElement("Owner")->GetText();
                container.push_back(info);
            }
        }
        return marker;
    }

private:
    std::string mProjectName;
    std::string mSchemaName;

    RestClientPtr mClient;
};

IODPSTablePtr ODPSTables::Get(const string &table_name)
{
    return Get(mProjectName, "", table_name);
}

IODPSTablePtr ODPSTables::Get(const string &project_name, const string &table_name)
{
    return Get(project_name, "", table_name);
}

IODPSTablePtr ODPSTables::Get(const std::string &project_name, const std::string& schema_name, const std::string &table_name)
{
    return std::make_shared<ODPSTable>(project_name, schema_name, table_name, std::make_shared<RestClient>(mConf));
}

bool ODPSTables::Exists(const string &project_name, const string &table_name)
{
    IODPSTablePtr tb = Get(project_name, table_name);
    try
    {
        // access lazied member to force reload
        tb->GetTableId();
        return true;
    }
    catch (const OdpsException &e)
    {
        if (e.GetErrorCode() == NO_SUCH_OBJECT || e.GetErrorCode() == NO_SUCH_TABLE)
        {
            return false;
        }
        throw e;
    }
}

bool ODPSTables::Exists(const string &table_name)
{
    return Exists(mProjectName, table_name);
}

std::shared_ptr<Iterator<ODPSTableBasicInfo>> ODPSTables::ListTables(const std::string& schemaName)
{
    return std::make_shared<TableIterator>(mProjectName, schemaName, mRestClient);
}

ODPSTableExtendedInfo ODPSTables::GetTableExtendedInfo(const std::string& project, const std::string& schema, const std::string& table)
{
    return internal::GetTableExtendedInfo(mRestClient, project, schema, table);
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara