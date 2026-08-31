#ifndef APSARA_ODPS_TUNNEL_UPSERT_H
#define APSARA_ODPS_TUNNEL_UPSERT_H

#include <mutex>
#include <vector>

#include "common/logging.h"

#include "common/http_connection.h"
#include "odps_tunnel.h"
#include "odps_meta.h"
#include "tunnel/upsert_stream.h"

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{


struct UpsertSlot
{
    std::vector<int32_t> mBuckets;
    int32_t mSlotId;
    std::string mWorkerAddr;
};

inline void to_json(nlohmann::json& j, const UpsertSlot& s)
{
    j = nlohmann::json{
        {"buckets", s.mBuckets},
        {"slot_id", s.mSlotId},
        {"worker_addr", s.mWorkerAddr},
    };
}

inline void from_json(const nlohmann::json& j, UpsertSlot& s)
{
    s.mBuckets = j.at("buckets").get<std::vector<int32_t>>();
    s.mSlotId = j.at("slot_id").get<int32_t>();
    s.mWorkerAddr = j.at("worker_addr").get<std::string>();
}

struct UpsertInfo
{
    std::string mUpsertId;
    TunnelTableSchema mSchema;
    std::vector<std::string> mHashKeys;
    std::string mHasher;
    std::vector<UpsertSlot> mSlots;
    std::string mStatus;
    std::string mQuotaName;
    bool mSupportPartialUpadte;
};

inline void to_json(nlohmann::json& j, const UpsertInfo& info)
{
    j = nlohmann::json{
        {"id", info.mUpsertId},
        {"schema", info.mSchema},
        {"hash_key", info.mHashKeys},
        {"hasher", info.mHasher},
        {"slots", info.mSlots},
        {"status", info.mStatus},
        {"quota_name", info.mQuotaName},
        {"enable_partial_update", info.mSupportPartialUpadte},
    };
}

inline void from_json(const nlohmann::json& j, UpsertInfo& info)
{
    info.mUpsertId = j.at("id").get<std::string>();
    j.at("schema").get_to(info.mSchema);
    info.mHashKeys = j.at("hash_key").get<std::vector<std::string>>();
    info.mHasher = j.at("hasher").get<std::string>();
    j.at("slots").get_to(info.mSlots);
    info.mStatus = j.at("status").get<std::string>();
    info.mQuotaName = j.value("quota_name", "");
    info.mSupportPartialUpadte = j.value("enable_partial_update", true);
}

class UpsertSession : public IUpsert
{
public:
    UpsertSession(const Configuration& conf,
        const std::string& project,
        const std::string& table,
        const std::string& partition = "",
        const std::string& upsertId = "",
        const std::string& schemaName = "");
    virtual ~UpsertSession();

private:
    class UpsertRecordPackWriter : public IRecordPackWriter
    {
    public:
        UpsertRecordPackWriter(UpsertSession* upsert) : mUpsert(upsert)
        {
        }

        ~UpsertRecordPackWriter() {}

    public:
        virtual std::string WriteRecordPack(RecordPack& pack) override;

    public:
        void Clear();

    private:
        UpsertSession* mUpsert;
        std::mutex mLock;
    };

    typedef std::shared_ptr<UpsertRecordPackWriter> UpsertRecordPackWriterPtr;

public:
    virtual std::string GetUpsertId() override { return mUpsertId; };
    virtual std::string GetStatus() override { return mStatus; };
    virtual int64_t GetCommitTimeout() override { return mCommitTimeout; };
    virtual void SetCommmitTimeout(int64_t commitTimeout) override { mCommitTimeout = commitTimeout; };
    virtual IODPSTableSchema* GetSchema() override { return mSchema.get(); };
    virtual bool supportPartialUpdate() override { return mSupportPartialUpadte; };
    virtual void Commit(bool async) override;
    virtual void Abort() override;
    virtual ODPSTableRecordPtr CreateUpsertRecord() override;
    virtual IUpsertStreamPtr CreateUpsertStream(const CompressOption& option) override;

    Configuration GetConf() { return mConf; };
    int32_t GetSlotNum()    { return mSlotNum; };
    std::string GetPartition()  { return mPartition; };
    std::map<std::string, uint32_t> GetName2Id() {return mName2Id; };

    std::string GetResourcePath() const
    {
        std::string path = "projects/";
        path.append(mProject);
        if (!mSchemaName.empty())
        {
            path.append("/schemas/");
            path.append(mSchemaName);
        }
        path.append("/tables/");
        path.append(mTable);
        path.append("/upserts");
        return path;
    }

    std::vector<std::string> GetHashKeys() const
    {
        return mHashKeys;
    }

    std::map<int32_t, Slot> GetBuckets() const
    {
        return mBuckets;
    }
    ODPSTableSchemaPtr GetODPSSchema()
    {
        return mRecordSchema.ToODPSTableSchema();
    }

    std::string GetTable()
    {
        return mTable;
    }

    UpsertRecordPackWriterPtr GetUpsertRecordPackWriter()
    {
        return mUpsertRecordPackWriter;
    }

private:
    void Initiate();
    void Reload();
    void ParseUpsertResult(const std::string& content);
    void LoadSlots(const std::vector<UpsertSlot>& slots);
    void CommitRequest(const RequestPtr& req);
    std::string OnWriteRecordPack(RecordPack& pack);
    HttpConnectionPtr OpenWriterConnection(const Slot& slot, int32_t bucketNum, const CompressOption& option, int64_t contentLength, const std::map<std::string, std::string>& headers, const std::map<std::string, std::string>& params);
    Slot GetSlot(int32_t bucketNum);
    void UpdateSlot(int32_t id, const std::string& serverAddr);

private:
    std::string mUpsertId;
    std::string mProject;
    std::string mTable;
    std::string mPartition;
    std::string mStatus;
    Configuration mConf;
    ODPSTableSchemaPtr mSchema;
    std::mutex mLock;
    UpsertRecordPackWriterPtr mUpsertRecordPackWriter;
    std::string mSchemaName;

    std::map<int32_t, Slot> mBuckets;
    std::vector<std::string> mHashKeys;
    int32_t mSlotNum = 1;
    int64_t mCommitTimeout = 120 * 1000;
    bool mSupportPartialUpadte = false;
    TunnelTableSchema mRecordSchema;
    std::map<std::string, uint32_t> mName2Id;
};

}  // namespace tunnel
}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
