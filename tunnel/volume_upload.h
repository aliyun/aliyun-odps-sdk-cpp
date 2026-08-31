#ifndef APSARA_ODPS_TUNNEL_INTERNAL_VOLUMEUPLOAD_H
#define APSARA_ODPS_TUNNEL_INTERNAL_VOLUMEUPLOAD_H
#include <stdint.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "odps_tunnel.h"
#include "volume_reader_writer.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

struct FileEntry
{
    std::string mFileName;
    int64_t mFileLength;
    std::string mQuotaName;
};

inline void to_json(nlohmann::json& j, const FileEntry& f)
{
    j = nlohmann::json{
        {"FileName", f.mFileName},
        {"FileLength", f.mFileLength},
        {"QuotaName", f.mQuotaName},
    };
}

inline void from_json(const nlohmann::json& j, FileEntry& f)
{
    f.mFileName = j.at("FileName").get<std::string>();
    f.mFileLength = j.at("FileLength").get<int64_t>();
    f.mQuotaName = j.value("QuotaName", "");
}

class VolumeUpload : public IVolumeUpload
{
public:
    VolumeUpload(const Configuration& conf,
            const std::string& project,
            const std::string& volume,
            const std::string& partition,
            const std::string& uploadId = "");

    ~VolumeUpload() {}

    IVolumeOutputStreamPtr OpenOutputStream(const std::string& fileName, const bool compress = false);

    std::string GetUploadId() { return mUploadId; }

    std::string GetStatus() { return mStatus; }

    std::string GetQuotaName() { return mQuotaName; }

    void Commit(const std::vector<std::string>& fileNames);

private:
    void Initiate(void);
    void FromJson(const std::string& jsonContent);

private:
    Configuration mConfig;
    std::string mProject;
    std::string mVolume;
    std::string mPartition;
    std::string mResourcePath;
    std::string mUploadId;
    std::string mStatus;
    std::string mQuotaName;
    std::vector<FileEntry> mFileList;
};
}}}}}

#endif
