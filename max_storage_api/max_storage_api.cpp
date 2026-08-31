#include "include/max_storage_api.h"
#include "include/odps_exception.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/table_preview_stream.h"
#endif
#include "max_storage_api/volume_write_stream.h"
#include "max_storage_api/volume_read_stream.h"
#include "max_storage_api/volume_read_session.h"
#include "max_storage_api/volume_write_session.h"
#include "max_storage_api/instance_read_session.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/instance_read_stream.h"
#endif
#include "max_storage_api/table_read_session.h"
#include "max_storage_api/table_write_session.h"

using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

ITablePreviewStreamBuilderPtr MaxStorageApi::BuildTablePreviewStream()
{
#ifdef ODPS_SDK_ENABLE_ARROW
    return std::make_shared<TablePreviewStreamBuilderImpl>(mConf);
#else
    throw OdpsException("TablePreviewStream requires arrow support, rebuild with WITH_ARROW=ON");
#endif
}

IVolumeFSWriteStreamBuilderPtr MaxStorageApi::BuildVolumeFSWriteStream()
{
    return std::make_shared<VolumeFSWriteStreamBuilderImpl>(mConf);
}

IVolumeFSReadStreamBuilderPtr MaxStorageApi::BuildVolumeFSReadStream()
{
    return std::make_shared<VolumeFSReadStreamBuilderImpl>(mConf);
}

IVolumeReadSessionBuilderPtr MaxStorageApi::BuildVolumeReadSession()
{
    return std::make_shared<VolumeReadSessionBuilderImpl>(mConf);
}

IVolumeWriteSessionBuilderPtr MaxStorageApi::BuildVolumeWriteSession()
{
    return std::make_shared<VolumeWriteSessionBuilderImpl>(mConf);
}

IInstanceReadSessionBuilderPtr MaxStorageApi::BuildInstanceReadSession()
{
    return std::make_shared<InstanceReadSessionBuilderImpl>(mConf);
}

IInstanceDirectReadStreamBuilderPtr MaxStorageApi::BuildInstanceDirectReadStream()
{
#ifdef ODPS_SDK_ENABLE_ARROW
    return std::make_shared<InstanceDirectReadStreamBuilderImpl>(mConf);
#else
    throw OdpsException("InstanceDirectReadStream requires arrow support, rebuild with WITH_ARROW=ON");
#endif
}

ITableReadSessionBuilderPtr MaxStorageApi::BuildTableReadSession()
{
    return std::make_shared<TableReadSessionBuilderImpl>(mConf);
}

ITableWriteSessionBuilderPtr MaxStorageApi::BuildTableWriteSession()
{
    return std::make_shared<TableWriteSessionBuilderImpl>(mConf);
}
