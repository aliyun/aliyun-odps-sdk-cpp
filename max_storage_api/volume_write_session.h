#ifndef APSARA_ODPS_MAX_STORAGE_API_VOLUME_WRITE_SESSION_H
#define APSARA_ODPS_MAX_STORAGE_API_VOLUME_WRITE_SESSION_H

#include <memory>

#include "include/max_storage_api.h"
#include "max_storage_api/common_macro_define.h"
#include "tunnel/util.h"
#include "tunnel/volume_reader_writer.h"
#include "util/utils.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace max_storage_api
{

IMPLEMENT_BUILDER(VolumeWriteSessionBuilderImpl, IVolumeWriteSessionBuilder, IVolumeWriteSessionPtr,
    DEFINE_BUILDER_PARAM(IVolumeWriteSessionBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(IVolumeWriteSessionBuilder, Volume, std::string);
    DEFINE_BUILDER_PARAM(IVolumeWriteSessionBuilder, Partition, std::string);
    DEFINE_BUILDER_PARAM(IVolumeWriteSessionBuilder, SessionId, std::string);
);

class VolumeWriteSessionImpl : public IVolumeWriteSession
{
public:
    VolumeWriteSessionImpl(const VolumeWriteSessionBuilderImpl& builder);

    IVolumeWriteStreamBuilderPtr BuildWriteStream() override;
    void Commit() override;
    void Abort() override;
    std::string GetSessionId() override;
    std::string GetStatus() override;
    ~VolumeWriteSessionImpl() {}

private:
    void CreateVolumeWriteSession();
    void GetVolumeWriteSession(const std::string& sessionId);

private:
    Configuration mConf;
    std::string mProject;
    std::string mVolume;
    std::string mPartition;
    std::string mSessionId;
    std::string mStatus;
    std::string mQuotaName;
};

}  // namespace max_storage_api

}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif