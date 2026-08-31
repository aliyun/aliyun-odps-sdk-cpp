#include "partition.h"
#include "common/json_serialize.h"
#include "core/rest_path.h"

namespace apsara {

namespace odps { namespace sdk { namespace internal {

std::string ODPSPartition::ToPartitionSpec() const
{
    std::ostringstream oss;
    for (size_t i = 0; i < mPartitionValues.size(); i++)
    {
        oss << mPartitionValues.at(i).first << "='" << mPartitionValues.at(i).second << "'";
        if (i != mPartitionValues.size() - 1)
        {
            oss << ",";
        }
    }
    return oss.str();
}

void ODPSPartition::Reload()
{
    std::map<std::string, std::string> params;
    if (mPartitionSpec.empty()) {
        params["partition"] = ToPartitionSpec();
    } else {
        params["partition"] = mPartitionSpec;
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

    tinyxml2::XMLHandle handle(resp->xml_body);
    tinyxml2::XMLElement *ele = handle.FirstChildElement("Partition").FirstChildElement("Schema").ToElement();
    if (!ele)
    {
        util::OdpsThrow(INTERNAL_ERROR, "Invalid Server XML Response:", resp->GetBody());
    }
    util::FromJsonString(*this, ele->GetText());
}

ODPSPartitionExtendedInfo ODPSPartition::GetExtendedInfo()
{
    std::map<std::string, std::string> params;
    if (mPartitionSpec.empty()) {
        params["partition"] = ToPartitionSpec();
    } else {
        params["partition"] = mPartitionSpec;
    }
    params["extended"] = "";
    if (!mSchemaName.empty())
    {
        params["curr_schema"] = mSchemaName;
    }
    std::string resource = RestResourceBuilder::BuildTableRest(mProjectName, mTableName);

    RestResponsePtr resp = std::dynamic_pointer_cast<RestResponse>(
        mClient->DoRequest(resource, HTTP_METHOD_GET, {}, params, "", true)
    );
    RestUtils::HandleOdpsFailures(resp);

    ODPSPartitionExtendedInfo extInfo;
    tinyxml2::XMLHandle handle(resp->xml_body);
    tinyxml2::XMLElement *ele = handle.FirstChildElement("Partition").FirstChildElement("Schema").ToElement();
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


}}}}