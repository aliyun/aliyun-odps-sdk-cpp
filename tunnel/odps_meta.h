#ifndef APSARA_ODPS_TUNNEL_INTERNAL_META_H
#define APSARA_ODPS_TUNNEL_INTERNAL_META_H

#include <stdint.h>
#include <memory>
#include <vector>
#include <string>
#include "common/odps_column.h"

#include "util/string_util.h"
#include "odps_tunnel.h"
#include "error_code.h"

#include "common/odps_table_schema.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal { namespace tunnel {

struct OdpsPartition
{
    std::string name;
    std::string value;
    OdpsPartition()
    {
        name = "";
        value = "";
    }
    OdpsPartition(std::string partName,std::string partValue)
    {
        name = partName;
        value = partValue;
    }
};

inline void to_json(nlohmann::json& j, const OdpsPartition& p)
{
    j = nlohmann::json{{"name", p.name}, {"value", p.value}};
}

inline void from_json(const nlohmann::json& j, OdpsPartition& p)
{
    p.name = j.at("name").get<std::string>();
    p.value = j.at("value").get<std::string>();
}

typedef std::vector<OdpsPartition> OdpsPartitions;

struct TunnelTableSchema
{
    TunnelTableSchema(const std::string &schemaJson);

    TunnelTableSchema()
    {
        isVirtualView = false;
    };

    bool isVirtualView;
    std::vector<ODPSTunnelTableColumn> fields;
    std::vector<ODPSTunnelTableColumn> partitionKeys;

    ODPSTableSchemaPtr ToODPSTableSchema(const std::vector<std::string> &colNames = std::vector<std::string>())
    {

        ODPSTableSchemaPtr tableSchemaPtr(new ODPSTableSchema());
        std::vector<uint32_t> validIdxs;
        std::vector<ODPSTunnelTableColumn> validFlds;

        if(!colNames.empty()){

            for(size_t i = 0; i < colNames.size(); ++i){
                size_t j = 0;
                for(; j < fields.size(); ++j){
                    if(util::ToLowerCaseString(fields[j].GetName()) == util::ToLowerCaseString(colNames[i]))
                    {
                        validFlds.push_back(fields[j]);
                        validIdxs.push_back(j);
                        break;
                    }
                }
                if(j == fields.size())
                {
                    throw OdpsException(INTERNAL_ERROR, "Column " + colNames[i] +  " does't exist.");
                }
            }

        }else{
            validFlds = this->fields;
        }

        for(size_t i = 0; i < fields.size(); ++i)
        {
            tableSchemaPtr->AppendColumn(fields[i]);
        }

        return tableSchemaPtr;

    }

    bool ValidatePartition(const OdpsPartitions& partitions)
    {
        if(partitionKeys.size() > 0 && partitions.size() == 0)
        {
           throw OdpsException(INTERNAL_ERROR, util::ToJsonString(partitions));
        }
        else if (partitionKeys.size() != partitions.size())
        {
            throw OdpsException(INTERNAL_ERROR, util::ToJsonString(partitions));
        }

        // partititon keys are strictly ordered
        for(size_t i=0; i<partitionKeys.size(); ++i)
        {
            if(partitionKeys[i].GetName() != partitions[i].name)
            {
                util::OdpsThrow(INTERNAL_ERROR,
                        "Partition information",
                        util::ToJsonString(partitions),
                        "is incompatible with provided partition.");
            }
        }

        return true;
    }

    void addField(ODPSTunnelTableColumn col)
    {
        fields.push_back(col);
    }

};

inline void to_json(nlohmann::json& j, const TunnelTableSchema& s)
{
    j = nlohmann::json{
        {"IsVirtualView", s.isVirtualView},
        {"columns", s.fields},
        {"partitionKeys", s.partitionKeys},
    };
}

inline void from_json(const nlohmann::json& j, TunnelTableSchema& s)
{
    s.isVirtualView = j.value("IsVirtualView", false);
    j.at("columns").get_to(s.fields);
    j.at("partitionKeys").get_to(s.partitionKeys);
}

// 定义放在 from_json 声明之后,保证构造函数里 get_to 的 POI 能找到 ADL 重载
inline TunnelTableSchema::TunnelTableSchema(const std::string &schemaJson)
{
    isVirtualView = false;
    nlohmann::json::parse(schemaJson).get_to(*this);
}

typedef std::shared_ptr<TunnelTableSchema> TunnelTableSchemaPtr;

}}}}}
#endif
