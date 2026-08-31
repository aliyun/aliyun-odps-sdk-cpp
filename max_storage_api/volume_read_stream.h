#ifndef APSARA_ODPS_MAX_STORAGE_API_VOLUME_READ_STREAM_H
#define APSARA_ODPS_MAX_STORAGE_API_VOLUME_READ_STREAM_H

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

IMPLEMENT_BUILDER(VolumeFSReadStreamBuilderImpl, IVolumeFSReadStreamBuilder, IVolumeReadStreamPtr,
    DEFINE_BUILDER_PARAM(IVolumeFSReadStreamBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(IVolumeFSReadStreamBuilder, Volume, std::string);
    DEFINE_BUILDER_PARAM(IVolumeFSReadStreamBuilder, Path, std::string);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IVolumeFSReadStreamBuilder, Offset, int64_t, 0);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IVolumeFSReadStreamBuilder, Count, int64_t, -1);
);

IMPLEMENT_BUILDER(VolumeReadStreamBuilderImpl, IVolumeReadStreamBuilder, IVolumeReadStreamPtr,
    DEFINE_BUILDER_PARAM(VolumeReadStreamBuilderImpl, Project, std::string);
    DEFINE_BUILDER_PARAM(VolumeReadStreamBuilderImpl, Volume, std::string);
    DEFINE_BUILDER_PARAM(VolumeReadStreamBuilderImpl, Partition, std::string);
    DEFINE_BUILDER_PARAM(VolumeReadStreamBuilderImpl, Path, std::string);
    DEFINE_BUILDER_PARAM(VolumeReadStreamBuilderImpl, SessionId, std::string);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IVolumeReadStreamBuilder, Offset, int64_t, 0);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IVolumeReadStreamBuilder, Count, int64_t, -1);
);

class VolumeReadStreamImpl : public IVolumeReadStream
{
public:
    VolumeReadStreamImpl(const VolumeFSReadStreamBuilderImpl& builder);
    VolumeReadStreamImpl(const VolumeReadStreamBuilderImpl& builder);
    ~VolumeReadStreamImpl();

public:
    virtual int64_t Read(char* buf, int64_t len) override;
    virtual void Close() override;

private:
    void CreateReadStream();
    std::shared_ptr<internal::tunnel::HttpVolumeInputStream> OpenInputStream();

private:
    Configuration mConf;
    std::string mProject;
    std::string mVolume;
    std::string mPartition;
    std::string mPath;
    int64_t mOffset;
    int64_t mCount;
    std::string mStreamId;
    std::string mSessionId;
    bool mClosed;
};

typedef std::shared_ptr<VolumeReadStreamImpl> VolumeReadStreamPtr;

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
