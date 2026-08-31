#ifndef APSARA_ODPS_TUNNEL_INTERNAL_VOLUMEDOWNLOAD_H
#define APSARA_ODPS_TUNNEL_INTERNAL_VOLUMEDOWNLOAD_H

#include <stdint.h>
#include <iostream>
#include <sstream>
#include <string>
#include <limits>

#include <nlohmann/json.hpp>
#include "odps_tunnel.h"
#include "volume_reader_writer.h"


namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

struct VolumeDownloadResult
{
    struct File
    {
        int64_t mFileLength;
    };

    std::string mDownloadId;
    std::string mStatus;
    File mFile;
    std::string mQuotaName;

    int64_t GetFileLength()
    {
        return mFile.mFileLength;
    }
};

inline void to_json(nlohmann::json& j, const VolumeDownloadResult::File& f)
{
    j = nlohmann::json{{"FileLength", f.mFileLength}};
}

inline void from_json(const nlohmann::json& j, VolumeDownloadResult::File& f)
{
    f.mFileLength = j.at("FileLength").get<int64_t>();
}

inline void to_json(nlohmann::json& j, const VolumeDownloadResult& r)
{
    j = nlohmann::json{
        {"DownloadID", r.mDownloadId},
        {"Status", r.mStatus},
        {"File", r.mFile},
        {"QuotaName", r.mQuotaName},
    };
}

inline void from_json(const nlohmann::json& j, VolumeDownloadResult& r)
{
    r.mDownloadId = j.at("DownloadID").get<std::string>();
    r.mStatus = j.at("Status").get<std::string>();
    j.at("File").get_to(r.mFile);
    r.mQuotaName = j.value("QuotaName", "");
}

class VolumeDownload : public IVolumeDownload
{
public:
    VolumeDownload(const Configuration& conf,
                const std::string& project,
                const std::string& volume,
                const std::string& partition,
                const std::string& fileName,
                const std::string& downloadId = ""
                );

    ~VolumeDownload() {}

    IVolumeInputStreamPtr OpenInputStream(const uint64_t start = 0 , const uint64_t length = std::numeric_limits<uint64_t>::max() / 2, const bool compress = false);

    void Complete(void);

    std::string GetDownloadId(void) { return mDownloadId; }
    uint64_t GetFileLength(void) { return mFileLength; }
    std::string GetStatus(void) { return mStatus; }
    std::string GetQuotaName(void) { return mQuotaName; }

private:
    void Initiate(void);
    void FromJson(const std::string& jsonContent);

private:
    Configuration mConfig;
    std::string mProject;
    std::string mVolume;
    std::string mPartition;
    std::string mFileName;
    std::string mResourcePath;
    int64_t mFileLength;
    std::string mStatus;
    std::string mDownloadId;
    std::string mQuotaName;
};

}}}}}

#endif
