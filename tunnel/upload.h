#ifndef APSARA_ODPS_TUNNEL_INTERNAL_UPLOAD_H
#define APSARA_ODPS_TUNNEL_INTERNAL_UPLOAD_H

#include <stdint.h>
#include <sstream>
#include <memory>
#include <functional>
#include <vector>

#include "odps_tunnel.h"
#include "odps_meta.h"
#include "util.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

struct BlockEntry
{
    int64_t mBlockID;
    std::string mDate;
};

inline void to_json(nlohmann::json& j, const BlockEntry& b)
{
    j = nlohmann::json{{"BlockID", b.mBlockID}, {"Date", b.mDate}};
}

inline void from_json(const nlohmann::json& j, BlockEntry& b)
{
    b.mBlockID = j.at("BlockID").get<int64_t>();
    b.mDate = j.at("Date").get<std::string>();
}

struct CreateUploadResult
{
    std::string mUploadId;
    std::string mStatus;
    std::string mOwner;
    std::string mInitiated;
    bool mIsOverwrite;
    TunnelTableSchema mSchema;
    std::string mQuotaName;
    int64_t mMaxFieldSize;
};

inline void to_json(nlohmann::json& j, const CreateUploadResult& r)
{
    j = nlohmann::json{
        {"UploadID", r.mUploadId},
        {"Status", r.mStatus},
        {"Owner", r.mOwner},
        {"Initiated", r.mInitiated},
        {"Schema", r.mSchema},
        {"IsOverwrite", r.mIsOverwrite},
        {"QuotaName", r.mQuotaName},
        {"MaxFieldSize", r.mMaxFieldSize},
    };
}

inline void from_json(const nlohmann::json& j, CreateUploadResult& r)
{
    r.mUploadId = j.at("UploadID").get<std::string>();
    r.mStatus = j.at("Status").get<std::string>();
    r.mOwner = j.at("Owner").get<std::string>();
    r.mInitiated = j.at("Initiated").get<std::string>();
    j.at("Schema").get_to(r.mSchema);
    r.mIsOverwrite = j.value("IsOverwrite", false);
    r.mQuotaName = j.value("QuotaName", "");
    r.mMaxFieldSize = j.value("MaxFieldSize", static_cast<int64_t>(0));
}

struct QueryUploadResult : public CreateUploadResult
{
    std::vector<BlockEntry> blockList;
};

inline void to_json(nlohmann::json& j, const QueryUploadResult& r)
{
    to_json(j, static_cast<const CreateUploadResult&>(r));
    j["UploadedBlockList"] = r.blockList;
}

inline void from_json(const nlohmann::json& j, QueryUploadResult& r)
{
    from_json(j, static_cast<CreateUploadResult&>(r));
    j.at("UploadedBlockList").get_to(r.blockList);
}

struct CloseUploadResult
{
    std::vector<BlockEntry> blockList;
};

inline void to_json(nlohmann::json& j, const CloseUploadResult& r)
{
    j = nlohmann::json{{"UploadedBlockList", r.blockList}};
}

inline void from_json(const nlohmann::json& j, CloseUploadResult& r)
{
    j.at("UploadedBlockList").get_to(r.blockList);
}

class Upload : public IUpload
{
public:
    Upload(const Configuration& conf,
             const std::string& project,
             const std::string& table,
             const std::string& partition = "",
             const std::string& uploadId = "",
             bool overwrite = false,
             const std::string& shcemaName = "")
    {
        this->conf = conf;
        this->project = project;
        this->table = table;
        this->partition = ReformatPartitionSpec(partition);
        this->count = 0;
        this->uploadId = uploadId;
        this->mOverWriteMode = overwrite;
        this->mSchemaName = shcemaName;

        this->conf.tunnelEndpoint = GetRouterServer(conf, project);

        if("" == uploadId){
            Initiate();
        }else{
            Reload("false");
        }
    }

    virtual ~Upload() {}
    virtual std::string GetUploadId() override { return uploadId; }
    virtual std::string GetQuotaName() override { return mQuotaName; }
    virtual IODPSTableSchema* GetSchema() override { return schema.get(); }
    #ifdef ODPS_SDK_ENABLE_ARROW
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() override;
    virtual IArrowRecordWriterPtr OpenArrowWriter(const uint32_t blockId, const CompressOption& compress = CompressOption::NO_COMPRESS) override;
    #endif
    virtual std::string GetStatus() override { Reload(); return status; }
    virtual IRecordWriterPtr OpenWriter(const uint32_t blockId, const bool compress = false) override;
    virtual IRecordWriterPtr OpenWriter(const uint32_t blockId, const CompressOption& compress) override;
    std::vector<int64_t> GetBlockList() override;
    void CommitUpload();
    void Commit() override;
    void Commit(const std::vector<uint32_t>& blocks) override;
    virtual ODPSTableRecordPtr CreateBufferRecord() override { return ODPSTableRecordPtr(new ODPSTableRecord(schema)); }

private:
    std::string getResourcePath()
    {
        std::ostringstream rp;
        rp << "projects/" << project << "/";
        if (!mSchemaName.empty())
        {
            rp << "schemas/" << mSchemaName << "/";
        }
        rp << "tables/" << table;
        return rp.str();
    }

    void Initiate();
    void Reload(const std::string& getblockid = "true");
    void ParseCreateResult(const std::string& json);
    void ParseQueryResult(const std::string& json);
    void ParseCloseResult(const std::string& json);
    RequestPtr BuildRequest(
            const std::map<std::string, std::string>& requestArgs,
            uint64_t blockId, const CompressOption& compress);


private:
    bool    mOverWriteMode;
    std::string uploadId;
    std::string project;
    std::string table;
    std::string partition;
    uint64_t count;
    std::string status;

    #ifdef ODPS_SDK_ENABLE_ARROW
    std::shared_ptr<arrow::Schema> mArrowSchema;
    #endif
    ODPSTableSchemaPtr schema;
    Configuration conf;
    std::vector<BlockEntry> blockList;
    std::string mSchemaName;
    std::string mQuotaName;
};

typedef std::shared_ptr<Upload> UploadPtr;

}}}}}
#endif
