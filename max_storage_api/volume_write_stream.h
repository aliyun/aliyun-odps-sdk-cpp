#ifndef APSARA_ODPS_MAX_STORAGE_API_VOLUME_WRITE_STREAM_H
#define APSARA_ODPS_MAX_STORAGE_API_VOLUME_WRITE_STREAM_H

#include <memory>

#include "include/max_storage_api.h"
#include "max_storage_api/common_macro_define.h"
#include "tunnel/volume_reader_writer.h"
#include "tunnel/util.h"
#include "util/utils.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace max_storage_api
{

IMPLEMENT_BUILDER(VolumeFSWriteStreamBuilderImpl, IVolumeFSWriteStreamBuilder, IVolumeWriteStreamPtr,
    DEFINE_BUILDER_PARAM(IVolumeFSWriteStreamBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(IVolumeFSWriteStreamBuilder, Volume, std::string);
    DEFINE_BUILDER_PARAM(IVolumeFSWriteStreamBuilder, Path, std::string);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IVolumeFSWriteStreamBuilder, ReplicaCount, int64_t, 3);
);

IMPLEMENT_BUILDER(VolumeWriteStreamBuilderImpl, IVolumeWriteStreamBuilder, IVolumeWriteStreamPtr,
    DEFINE_BUILDER_PARAM(VolumeWriteStreamBuilderImpl, Project, std::string);
    DEFINE_BUILDER_PARAM(VolumeWriteStreamBuilderImpl, Volume, std::string);
    DEFINE_BUILDER_PARAM(VolumeWriteStreamBuilderImpl, Partition, std::string);
    DEFINE_BUILDER_PARAM(IVolumeWriteStreamBuilder, File, std::string);
    DEFINE_BUILDER_PARAM(VolumeWriteStreamBuilderImpl, SessionId, std::string);
);

class VolumeWriteStreamImpl : public IVolumeWriteStream
{
public:
    VolumeWriteStreamImpl(const VolumeFSWriteStreamBuilderImpl& builder);
    VolumeWriteStreamImpl(const VolumeWriteStreamBuilderImpl& builder);
    ~VolumeWriteStreamImpl();

public:
    void Write(const char* buf, int64_t len) override;
    void Close() override;

private:
    void CreateWriteStream();
    std::shared_ptr<internal::tunnel::HttpVolumeOutputStream> OpenOutputStream();
    void CloseWriteStream();

private:
    Configuration mConf;
    std::string mProject;
    std::string mVolume;
    std::string mPartition;
    CompressOption mCompress = CompressOption::LZ4_COMPRESS;
    std::string mPath;
    int64_t mReplicaCount;
    std::string mStreamId;
    std::string mSessionId;
    bool mClosed;
};

typedef std::shared_ptr<VolumeWriteStreamImpl> VolumeWriteStreamPtr;

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif