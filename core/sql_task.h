#ifndef APSARA_ODPS_SDK_SQL_TASK_H
#define APSARA_ODPS_SDK_SQL_TASK_H

#include "odps_api.h"

namespace apsara { namespace odps { namespace sdk {

namespace internal {

class SQLTask : public ISQLTask{
public:
    SQLTask(const map<string, string>& props);

    virtual ~SQLTask();

    virtual void SetName(const std::string& name) override;

    virtual const std::string& GetName() const override;

    virtual void SetQuery(const std::string& query) override;

    virtual const std::string& GetQuery() const override;

    virtual void SetProperty(const string& key, const string& value) override;

    virtual std::string GetProperty(const std::string& key) const override;

    virtual void SetDefaultHints(const std::map<std::string, std::string>& hints) override;

    virtual void RemoveDefaultHints() override;

    virtual IODPSInstancePtr Run(IODPSPtr odps, const std::string& sql) override;

    virtual IODPSInstancePtr Run(IODPSPtr odps, const std::string& sql, const std::map<string, string>& hints) override;

    virtual IODPSInstancePtr Run(IODPSPtr odps, const std::string& project, const std::string& sql, const std::string& taskName,
        std::map<string, string> hints, const std::map<string, string>& aliases, int priority, const string& type) override;

private:
    std::string name;
    std::string query;
    std::map<std::string, std::string> properties;
    std::map<std::string, std::string> defaultHints;
};

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
