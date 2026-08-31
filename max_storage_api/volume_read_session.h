#ifndef APSARA_ODPS_MAX_STORAGE_API_VOLUME_READ_SESSION_H
#define APSARA_ODPS_MAX_STORAGE_API_VOLUME_READ_SESSION_H

#include "include/max_storage_api.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/volume_read_stream.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace max_storage_api
{

IMPLEMENT_BUILDER(VolumeReadSessionBuilderImpl, IVolumeReadSessionBuilder, IVolumeReadSessionPtr,
    DEFINE_BUILDER_PARAM(IVolumeReadSessionBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(IVolumeReadSessionBuilder, Volume, std::string);
    DEFINE_BUILDER_PARAM(IVolumeReadSessionBuilder, Partition, std::string);
    DEFINE_BUILDER_PARAM(IVolumeReadSessionBuilder, File, std::string);
    DEFINE_BUILDER_PARAM(IVolumeReadSessionBuilder, SessionId, std::string);
);

class VolumeReadSessionImpl : public IVolumeReadSession
{
public:
    VolumeReadSessionImpl(const VolumeReadSessionBuilderImpl& builder);

    IVolumeReadStreamBuilderPtr BuildVolumeReadStream() override;
    std::string GetSessionId() override;
    std::string GetStatus() override;
    ~VolumeReadSessionImpl() {}

private:
    void CreateVolumeReadSession();
    void GetVolumeReadSession(const std::string& sessionId);

private:
    Configuration mConf;
    std::string mProject;
    std::string mVolume;
    std::string mPartition;
    std::string mPath;
    std::string mSessionId;
    std::string mStatus;
    std::string mQuotaName;
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif