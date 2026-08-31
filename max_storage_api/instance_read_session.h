#ifndef APSARA_ODPS_MAX_STORAGE_API_INSTANCE_READ_SESSION_H
#define APSARA_ODPS_MAX_STORAGE_API_INSTANCE_READ_SESSION_H

#include "common/odps_table_schema.h"
#include "include/max_storage_api.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/split.h"
#include "max_storage_api/table_schema.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

IMPLEMENT_BUILDER(InstanceReadSessionBuilderImpl, IInstanceReadSessionBuilder, IInstanceReadSessionPtr,
    DEFINE_BUILDER_PARAM(InstanceReadSessionBuilderImpl, Project, std::string);
    DEFINE_BUILDER_PARAM(InstanceReadSessionBuilderImpl, Instance, std::string);
    DEFINE_BUILDER_PARAM(InstanceReadSessionBuilderImpl, EnableLimit, bool);
    DEFINE_BUILDER_PARAM(InstanceReadSessionBuilderImpl, SessionId, std::string);
);

class InstanceReadSessionImpl : public IInstanceReadSession
{
public:
    InstanceReadSessionImpl(const InstanceReadSessionBuilderImpl& builder);
    virtual ~InstanceReadSessionImpl() {};

public:
    IInstanceReadStreamBuilderPtr BuildInstanceReadStream() override;
    std::string GetSessionId() override;
    ISplitsPtr GetSplits() override;

private:
    void CreateReadSession();
    void GetReadSession(const std::string& sessionId);
    void LoadFromJson(const std::string& json);

    Configuration mConf;
    std::string mProject;
    std::string mInstance;
    std::string mSessionId;
    int64_t mRecordCount = 0;
    std::string mStatus;
    std::string mOwner;
    std::string mInitiated;
    std::string mQuotaName;
    TableSchema mTableSchema;
    std::string mRequestId;
    bool mEnableLimit;
};
typedef std::shared_ptr<InstanceReadSessionImpl> InstanceReadSessionImplPtr;

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
