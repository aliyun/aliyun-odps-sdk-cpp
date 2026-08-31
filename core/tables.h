#ifndef APSARA_ODPS_SDKmTables_H
#define APSARA_ODPS_SDKmTables_H

#include <vector>
#include "odps_api.h"
#include "core/odps.h"
#include "core/rest_path.h"
#include "table.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {

class ODPSTables: public IODPSTables
{
private:
    std::vector<apsara::odps::sdk::IODPSTablePtr> mTables;
    std::string mProjectName;
    Configuration mConf;
    RestClientPtr mRestClient;

public:
    ODPSTables(const Configuration& conf, const std::string& project);
    virtual ~ODPSTables();

    /**
     *  @brief 获得指定表信息
     *
     *  @param table_name   表名
     *
     *  @return 对应表的指针
     */
    virtual IODPSTablePtr Get(const std::string &table_name) override;

    /**
     *  @brief 获得指定表信息
     *
     *  @param project_name 表所在project名
     *  @param table_name   表名
     *
     *  @return 对应表的指针
     */
    virtual IODPSTablePtr Get(const std::string &project_name, const std::string &table_name) override;

    /**
     *  @brief 获得指定表信息
     *
     *  @param project_name 表所在project名
     *  @param schema_name schema名
     *  @param table_name   表名
     *
     *  @return 对应表的指针
     */
    virtual IODPSTablePtr Get(const std::string &project_name, const std::string& schema_name, const std::string &table_name) override;

    /**
     *  @brief 判断指定表是否存在
     *
     *  @param project_name 表所在project名
     *  @param table_name   表名
     *
     *  @return 表存在与否
     */
    virtual bool Exists(const std::string &project_name, const std::string &table_name) override;

    /**
     *  @brief 判断指定表是否存在
     *
     *  @param table_name   表名
     *
     *  @return 表存在与否
     */
    virtual bool Exists(const std::string &table_name) override;

    virtual std::shared_ptr<Iterator<ODPSTableBasicInfo>> ListTables(const std::string& schemaName = "") override;

    virtual ODPSTableExtendedInfo GetTableExtendedInfo(const std::string& project, const std::string& schema, const std::string& table) override;
};

typedef std::shared_ptr<ODPSTables> ODPSTablesPtr;

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif