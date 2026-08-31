#ifndef APSARA_ODPS_SDK_INSTANCE_H
#define APSARA_ODPS_SDK_INSTANCE_H

#include "util/utils.h"
#include "odps_api.h"
#include "rest_path.h"
#include "security_manager.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {


class Instance: public IODPSInstance
{
public:
    Instance(
        const Configuration& conf,
        const std::string& project,
        const std::string& job = "");

    virtual ~Instance() {}

    virtual const std::string& GetInstanceId() override;

    // For test
    void SetInstanceId(const std::string& id);

    virtual bool Create() override;

    virtual void Stop() override;

    virtual std::string GetTaskDetailJson(const std::string& task_name) override;

    virtual void WaitForSuccess(uint32_t sleepTimeoutMs, uint16_t sleepIntervalMs = 1000) override;

    virtual InstanceStatus AcquireStatus(bool isBlock) override;

    virtual const std::unordered_map<std::string, TaskStatus>& AcquireTaskStatus() override;

    bool IsSuccessful();

    virtual const std::unordered_map<std::string, std::string>&  GetTaskResults() override;

    std::string GenerateLogViewHost();
    std::string GeneratePolicy(long hours);
    std::string GenerateToken(long hours);
    virtual std::string GenerateLogView(long hours) override;

private:
    std::string id;
    std::string job;
    std::string project;
    Configuration mConf;
    RestClientPtr mRestClient;
    SecurityManagerPtr sm;
    std::string mLogViewHost;
    std::unordered_map<std::string, TaskStatus> mTaskStatus;
    std::unordered_map<std::string, std::string> mTaskResults;
};

typedef std::shared_ptr<Instance> InstancePtr;

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif