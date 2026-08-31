#pragma once

#include "configuration.h"
#include "odps_table.h"
#include <map>

using namespace std;

namespace apsara
{
namespace odps
{
namespace sdk
{

enum class InstanceStatus {
    RUNNING = 0,
    SUSPENDED = 1,
    TERMINATED =  2
};

enum class TaskStatus {
    WAITING = 0,
    RUNNING = 1,
    SUCCESS = 2,
    FAILED = 3,
    SUSPENDED = 4,
    CANCELLED = 5
};

struct ODPSTableBasicInfo
{
    std::string mTableName;
    std::string mOwner;
};

class IODPSTables
{
public:
    virtual ~IODPSTables() {}

    // /**
    //  *  @brief 列出project下所有table
    //  */
    // virtual std::vector<std::string> ListAll() = 0;

    /**
     *  @brief 获得指定表信息
     *
     *  @param table_name   表名
     *
     *  @return 对应表的指针
     */
    virtual IODPSTablePtr Get(const std::string &table_name) = 0;

    /**
     *  @brief 获得指定表信息
     *
     *  @param project_name 表所在project名
     *  @param table_name   表名
     *
     *  @return 对应表的指针
     */
    virtual IODPSTablePtr Get(const std::string &project_name, const std::string &table_name) = 0;

    /**
     *  @brief 获得指定表信息
     *
     *  @param project_name 表所在project名
     *  @param schema_name schema名
     *  @param table_name   表名
     *
     *  @return 对应表的指针
     */
    virtual IODPSTablePtr Get(const std::string &project_name, const std::string& schema_name, const std::string &table_name) = 0;

    /**
     *  @brief 判断指定表是否存在
     *
     *  @param project_name 表所在project名
     *  @param table_name   表名
     *
     *  @return 表存在与否
     */
    virtual bool Exists(const std::string &project_name, const std::string &table_name) = 0;

    /**
     *  @brief 判断指定表是否存在
     *
     *  @param table_name   表名
     *
     *  @return 表存在与否
     */
    virtual bool Exists(const std::string &table_name) = 0;

    virtual std::shared_ptr<Iterator<ODPSTableBasicInfo>> ListTables(const std::string& schemaName = "") = 0;

    virtual ODPSTableExtendedInfo GetTableExtendedInfo(const std::string& project, const std::string& schema, const std::string& table) = 0;
};

using IODPSTablesPtr = std::shared_ptr<IODPSTables>;

class IODPSInstance
{
public:
    virtual ~IODPSInstance() {}
    virtual const std::string& GetInstanceId() = 0;

    virtual bool Create() = 0;

    virtual void Stop() = 0;

    //等待时间单位 毫秒
    virtual void WaitForSuccess(uint32_t sleepTimeoutMs, uint16_t sleepIntervalMs = 1000) = 0;

    virtual InstanceStatus AcquireStatus(bool isBlock) = 0;

    virtual const std::unordered_map<std::string, TaskStatus>& AcquireTaskStatus() = 0;

    virtual const std::unordered_map<std::string, std::string>&  GetTaskResults() = 0;

    virtual std::string GetTaskDetailJson(const std::string& task_name) = 0;

    virtual std::string GenerateLogView(long hours) = 0;

    static InstanceStatus ParseStatus(const std::string& status);

    static TaskStatus ParseTaskStatus(const std::string& status);

    static std::string GetInstanceStatusName(InstanceStatus status);

    static std::string GetTaskStatusName(TaskStatus status);
};

using IODPSInstancePtr = std::shared_ptr<IODPSInstance>;

class IODPSResource
{
public:
    enum ODPSResourceType
    {
        FILE
    };

    virtual ~IODPSResource() {}

    virtual void SetProject(const std::string &project) = 0;

    virtual const std::string& GetProject() const = 0;

    virtual void SetName(const std::string &name) = 0;

    virtual const std::string& GetName() const = 0;

    virtual void SetType(const ODPSResourceType &type) = 0;

    virtual const ODPSResourceType& GetType() const = 0;

    virtual void SetIsTempResource(const bool &is_temp) = 0;

    virtual bool IsTempResource() = 0;

    virtual void SetComment(const std::string &comment) = 0;

    virtual const std::string& GetComment() const = 0;

    virtual bool Exists() = 0;

    virtual bool Create(bool overwrite) = 0;

    virtual bool Delete() = 0;
};

using IODPSResourcePtr = std::shared_ptr<IODPSResource>;

class IODPS
{
public:
    virtual ~IODPS() {}

    virtual void SetProject(const std::string& project) = 0;

    virtual const std::string& GetProject() const = 0;

    virtual void SetEndpoint(const std::string& endpoint) = 0;

    virtual const std::string& GetEndpoint() const = 0;

    virtual const Account& GetAccount() const = 0;

    virtual const Configuration& GetConfiguration() const = 0;

    virtual IODPSTablesPtr GetTables() const = 0;

    virtual IODPSResourcePtr CreateResource(
        const std::string &name,
        const IODPSResource::ODPSResourceType &type,
        std::stringstream &content) = 0;

    virtual IODPSResourcePtr CreateResource(
        const std::string &name,
        std::stringstream &content) = 0;

    virtual IODPSInstancePtr RecoverInstance(
        const std::string &instanceId,
        const std::string &project = "") = 0;

    virtual IODPSInstancePtr CreateInstance(const std::string &jobdesc, const std::string &project = "") = 0;

    static std::shared_ptr<IODPS> Create(const Configuration& conf, const std::string &project);
};
using IODPSPtr = std::shared_ptr<IODPS>;

class ISQLTask{
public:
    virtual ~ISQLTask() {}

    virtual void SetName(const std::string& name) = 0;

    virtual const std::string& GetName() const = 0;

    virtual void SetQuery(const std::string& query) = 0;

    virtual const std::string& GetQuery() const = 0;

    virtual void SetProperty(const std::string& key, const std::string& value) = 0;

    virtual std::string GetProperty(const std::string& key) const = 0;

    virtual void SetDefaultHints(const std::map<std::string, std::string>& hints) = 0;

    virtual void RemoveDefaultHints() = 0;

    virtual IODPSInstancePtr Run(IODPSPtr odps, const std::string& sql) = 0;

    virtual IODPSInstancePtr Run(IODPSPtr odps, const std::string& sql, const std::map<string, string>& hints) = 0;

    virtual IODPSInstancePtr Run(IODPSPtr odps, const std::string& project, const std::string& sql, const std::string& taskName,
        std::map<string, string> hints, const std::map<string, string>& aliases, int priority, const std::string& type) = 0;

    static std::shared_ptr<ISQLTask> Create(const std::map<std::string,std::string>& props = std::map<std::string,std::string>());
};
using ISQLTaskPtr = std::shared_ptr<ISQLTask>;

} // namespace sdk
} // namespace odps
} // namespace apsara
