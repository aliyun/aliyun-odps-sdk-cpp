#ifndef APSARA_ODPS_TUNNEL_INTERNAL_DOWNLOAD_H
#define APSARA_ODPS_TUNNEL_INTERNAL_DOWNLOAD_H

#include <stdint.h>
#include <deque>
#include <sstream>
#include <functional>
#include <string>
#include <vector>

#include "common/http_connection.h"
#include "odps_tunnel.h"
#include "odps_meta.h"
#include "util.h"
#include "tunnel/record_reader.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

struct CreateDownloadResult
{
    std::string mDownloadId;
    uint64_t mRecordCount;
    std::string mStatus;
    std::string mOwner;
    std::string mInitiated;
    TunnelTableSchema mSchema;
    std::string mQuotaName;
};

inline void to_json(nlohmann::json& j, const CreateDownloadResult& r)
{
    j = nlohmann::json{
        {"DownloadID", r.mDownloadId},
        {"RecordCount", r.mRecordCount},
        {"Status", r.mStatus},
        {"Owner", r.mOwner},
        {"Initiated", r.mInitiated},
        {"Schema", r.mSchema},
        {"QuotaName", r.mQuotaName},
    };
}

inline void from_json(const nlohmann::json& j, CreateDownloadResult& r)
{
    r.mDownloadId = j.at("DownloadID").get<std::string>();
    r.mRecordCount = j.value("RecordCount", static_cast<uint64_t>(0));
    r.mStatus = j.at("Status").get<std::string>();
    r.mOwner = j.at("Owner").get<std::string>();
    r.mInitiated = j.at("Initiated").get<std::string>();
    j.at("Schema").get_to(r.mSchema);
    r.mQuotaName = j.value("QuotaName", "");
}

class Download : public IDownload
{
public:
    Download(const Configuration& conf,
             const std::string& project,
             const std::string& table,
             const std::string& partition,
             const std::string& downloadId = "",
             const std::string& schemaName = "")
        : mConf(conf),
         mProject(project),
         mTable(table),
         mPartition(ReformatPartitionSpec(partition)),
         mResourcePath("projects/" + project + "/tables/" + table),
         mDownloadId(downloadId),
         mSchemaName(schemaName)
    {
        if (!mSchemaName.empty())
        {
            mResourcePath = "projects/" + project + "/schemas/" + mSchemaName + "/tables/" + table;
        }
        mConf.tunnelEndpoint = GetRouterServer(conf, project);
        if (mDownloadId.empty())
        {
            Initiate();
        }
        else
        {
            Reload();
        }
    }

    virtual ~Download() {}
    virtual std::string GetDownloadId() { return mDownloadId; }
    virtual std::string GetQuotaName() { return mQuotaName; }
    virtual IODPSTableSchema* GetSchema() { return mSchema.get(); }
    #ifdef ODPS_SDK_ENABLE_ARROW
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() override;
    virtual IArrowRecordReaderPtr OpenArrowReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames = {}, const CompressOption& compress = CompressOption::NO_COMPRESS, bool disableModifiedCheck = false, const uint64_t rawSize = 0);
    virtual IBufferArrowRecordReaderPtr OpenBufferArrowReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames = {}, const CompressOption& compress = CompressOption::NO_COMPRESS, bool disableModifiedCheck = false);
    virtual IBufferArrowRecordReaderPtr OpenBufferArrowReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const uint64_t rawSize, const std::vector<std::string>& colNames = {}, const CompressOption& compress = CompressOption::NO_COMPRESS, bool disableModifiedCheck = false);
    #endif
    virtual std::string GetStatus() { Reload(); return mStatus; }
    virtual uint64_t GetRecordCount() { return mRecordCount; }
    virtual IRecordReaderPtr OpenReader(const uint64_t start, const uint64_t count, const bool compress = false);
    virtual IRecordReaderPtr OpenReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const bool compress = false);
    virtual IRecordReaderPtr OpenReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck = false);

    virtual IBufferRecordReaderPtr OpenBufferReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck = false);

    virtual void Complete();

private:
    void Initiate();
    void Reload();
    void FromJson(const std::string& json);
    RequestPtr BuildRequest(
            const std::map<std::string, std::string>& requestArgs,
            uint64_t start, uint64_t count, const std::vector<std::string>& colNames, const CompressOption& comrpess,
            ODPSTableSchemaPtr& readingSchema, bool disableModifiedCheck, uint64_t rawSize = 0);

private:
    Configuration mConf;
    std::string mProject;
    std::string mTable;
    std::string mPartition;
    std::string mResourcePath;
    std::string mDownloadId;
    uint64_t mRecordCount;
    std::string mStatus;

    #ifdef ODPS_SDK_ENABLE_ARROW
    std::shared_ptr<arrow::Schema> mArrowSchema;
    #endif
    ODPSTableSchemaPtr mSchema;
    std::string mSchemaName;
    std::string mQuotaName;

    uint64_t mBufferSize;
    std::deque<ODPSTableRecord> mRecordBuffer;

};

typedef std::shared_ptr<Download> DownloadPtr;

}}}}}
#endif
