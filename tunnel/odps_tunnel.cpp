#include <stdlib.h>
#include <time.h>

#include "odps_tunnel.h"
#include "download.h"
#include "upload.h"
#include "volume_download.h"
#include "volume_upload.h"
#include "stream_upload.h"
#include "record_pack.h"
#include "upsert.h"
#ifdef ENABLE_VIPSERVER
#include "vipclient_helper.hpp"
#endif

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

class Initializer
{
public:
    Initializer()
    {
        curl_global_init(CURL_GLOBAL_ALL);
    #ifdef ENABLE_VIPSERVER
        if (!middleware::vipclient::helper::GlobalInit())
        {
            throw OdpsTunnelException("VIPServer client init failed");
        }
    #endif
        srand(time(NULL));
    }

    ~Initializer()
    {
    #ifdef ENABLE_VIPSERVER
        middleware::vipclient::helper::GlobalUnInit();
    #endif
        curl_global_cleanup();
    }
};

OdpsTunnel::OdpsTunnel()
{
    static Initializer _globalInitializer;
}

IDownloadPtr OdpsTunnel::CreateDownload(const string& project, const string& table, const string& partition, const string& downloadId, const std::string& schemaName)
{
    return std::dynamic_pointer_cast<IDownload>(DownloadPtr(new Download(conf, project, table, partition, downloadId, schemaName)));
}

IUploadPtr OdpsTunnel::CreateUpload(const string& project, const string& table, const string& partition, const string& uploadId, bool overwrite, const std::string& schemaName)
{
    return std::dynamic_pointer_cast<IUpload>(UploadPtr(new Upload(conf, project, table, partition, uploadId, overwrite, schemaName)));
}

IStreamUploadPtr OdpsTunnel::CreateStreamUpload(const std::string& project, const std::string& table)
{
    return CreateStreamUpload(project, table, "", false, 0, CompressOption::ZLIB_COMPRESS, "");
}

IStreamUploadPtr OdpsTunnel::CreateStreamUpload(const std::string& project, const std::string& table, int32_t slotNum, const CompressOption& compress)
{
    return CreateStreamUpload(project, table, "", false, slotNum, compress, "");
}

IStreamUploadPtr OdpsTunnel::CreateStreamUpload(const std::string& project, const std::string& table, int32_t slotNum, const CompressOption& compress, const std::string& schemaName)
{
    return CreateStreamUpload(project, table, "", false, slotNum, compress, schemaName);
}

IStreamUploadPtr OdpsTunnel::CreateStreamUpload(const std::string& project, const std::string& table, const std::string& partition)
{
    return CreateStreamUpload(project, table, partition, false, 0, CompressOption::ZLIB_COMPRESS, "");
}

IStreamUploadPtr OdpsTunnel::CreateStreamUpload(const std::string& project, const std::string& table, const std::string& partition, bool createPartition, int32_t slotNum, const CompressOption& compress, const std::string& schemaName)
{
    return IStreamUploadPtr(new StreamUpload(conf, project, table, partition, createPartition, slotNum, compress, schemaName));
}

IUpsertPtr OdpsTunnel::CreateUpsert(const std::string& project, const std::string& table, const std::string& partition, const std::string& upsertId, const std::string& schemaName)
{
    return IUpsertPtr(new UpsertSession(conf, project, table, partition, upsertId, schemaName));
}

IVolumeDownloadPtr OdpsTunnel::CreateVolumeDownload(const string& project, const string& volume, const string& partition, const string& fileName, const string& downloadId)
{
    return IVolumeDownloadPtr(new VolumeDownload(conf, project, volume, partition, fileName, downloadId));
}

IVolumeUploadPtr OdpsTunnel::CreateVolumeUpload(const string& project, const string& volume, const string& partition, const string& uploadId)
{
    return IVolumeUploadPtr(new VolumeUpload(conf, project, volume, partition, uploadId));
}
