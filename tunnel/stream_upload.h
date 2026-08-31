#ifndef APSARA_ODPS_TUNNEL_STREAM_UPLOAD_H
#define APSARA_ODPS_TUNNEL_STREAM_UPLOAD_H

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "common/logging.h"

#include "common/http_connection.h"
#include "odps_tunnel.h"
#include "odps_meta.h"
#include "slot.h"

#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_pack.h"
#endif

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

// forward decl
class RecordPack;
class StreamUpload;


#ifdef ODPS_SDK_ENABLE_ARROW
class EOSStream: public IEOSStream
{
public:

    EOSStream(StreamUpload* upload, const std::string& clientId, const CompressOption& opt);

    virtual ~EOSStream(){}

    virtual EOSInfo Tell();

    virtual FlushResult Write(const arrow::RecordBatch& r, const EOSInfo& offset);

    virtual FlushResult WritePack(const ArrowRecordPack& r, const EOSInfo& offset);

    virtual void Release();

private:
    void TryReschedule();

    ArrowRecordPack mRecordPack;
    StreamUpload* mStreamUpload;
    std::string mScheduledServer;
    std::string mClientId;

    int64_t mLastRescheduleTimestamp = 0; // TODO: timed resched
};
#endif

class IRecordPackWriter
{
public:
    virtual ~IRecordPackWriter() {};
    virtual std::string WriteRecordPack(RecordPack& pack) = 0;
};
typedef std::shared_ptr<IRecordPackWriter> IRecordPackWriterPtr;

struct StreamUploadInfo
{
    std::string mQuotaName;
    std::string mSessionName;
    std::string mStatus;
    TunnelTableSchema mSchema;
    std::map<int32_t, std::string> mSlots;
};

inline void to_json(nlohmann::json& j, const StreamUploadInfo& info)
{
    // 非字符串键的 map 按原 Jsonizable 框架的规则序列化为
    // "[key, value] 二元组"数组(只有 map<string, T> 才序列化为
    // JSON 对象),服务端协议的 slots 即此格式
    nlohmann::json slots = nlohmann::json::array();
    for (std::map<int32_t, std::string>::const_iterator it = info.mSlots.begin();
         it != info.mSlots.end(); ++it)
    {
        slots.push_back(nlohmann::json::array({it->first, it->second}));
    }
    j = nlohmann::json{
        {"quota_name", info.mQuotaName},
        {"session_name", info.mSessionName},
        {"status", info.mStatus},
        {"schema", info.mSchema},
        {"slots", slots},
    };
}

inline void from_json(const nlohmann::json& j, StreamUploadInfo& info)
{
    info.mQuotaName = j.value("quota_name", "");
    info.mSessionName = j.at("session_name").get<std::string>();
    info.mStatus = j.at("status").get<std::string>();
    j.at("schema").get_to(info.mSchema);
    info.mSlots.clear();
    // slots 为 "[slotId(数字), serverAddr(字符串)]" 二元组数组
    for (const nlohmann::json& pair : j.at("slots"))
    {
        info.mSlots[pair.at(0).get<int32_t>()] = pair.at(1).get<std::string>();
    }
}

class ISlotUpdater
{
public:
    virtual ~ISlotUpdater() {};
    virtual void UpdateSlot(int32_t id,
        const std::string& serverAddr,
        const std::string& strSlotNum) = 0;
};
typedef std::shared_ptr<ISlotUpdater> ISlotUpdaterPtr;

class StreamUpload : public IStreamUpload
{
public:
    StreamUpload(const Configuration& conf,
        const std::string& project,
        const std::string& table,
        const std::string& partition,
        bool createPartition,
        int32_t slotNum,
        const CompressOption& compress,
        const std::string& schemaName);
    virtual ~StreamUpload();

public:
    virtual std::string GetUploadId() override { return mUploadId; }
    virtual void SetP2PMode(bool mode) { mP2PMode = mode; }
    virtual IODPSTableSchema* GetSchema() override { return mSchema.get(); }
    virtual ODPSTableSchemaPtr GetOdpsTableSchema() { return mSchema; }
    virtual IRecordPackPtr CreateRecordPack() override;
    virtual IRecordPackPtr CreateRecordPack(const CompressOption& option) override;
    virtual IRecordPackPtr CreateRecordPack(const CompressOption& option, size_t reserveSize) override;
    virtual ODPSTableRecordPtr CreateBufferRecord() override { return ODPSTableRecordPtr(new ODPSTableRecord(mSchema)); }
    virtual std::string GetQuotaName() override { return mQuotaName; }

    #ifdef ODPS_SDK_ENABLE_ARROW
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() override;
    virtual IEOSStreamPtr CreateEOSStream(const std::string& clientId, const CompressOption& opt) override;
    #endif

private:
    class RecordPackWriter : public IRecordPackWriter
    {
    public:
        RecordPackWriter(StreamUpload* upload)
            : mUpload(upload)
        {
        }

        ~RecordPackWriter() {}

    public:
        virtual std::string WriteRecordPack(RecordPack& pack) override;

    public:
        void Clear();

    private:
        StreamUpload* mUpload;
        std::mutex mLock;
    };
    typedef std::shared_ptr<RecordPackWriter> RecordPackWriterPtr;

    #ifdef ODPS_SDK_ENABLE_ARROW
    void OnCreateEOSClient(const std::string& clientId);
    std::string OnScheduleEOSClient(const std::string& clientId);
    void OnReleaseEOSClient(const std::string& clientId, const std::string& server);
    std::string OnWriteEOSData(
        const std::string& server,
        const std::string& client,
        const std::string& data,
        const CompressOption& option,
        const EOSInfo& offset);
    #endif

    std::string OnWriteRecordPack(RecordPack& pack);

    void UpdateSlot(int32_t id,
        const std::string& serverAddr,
        const std::string& strSlotNum);

private:
    void Initiate();
    void Reload(bool forceReload);
    HttpConnectionPtr OpenWriterConnection(const Slot& slot,
        const CompressOption& option,
        int64_t contentLength = -1,
        const std::map<std::string, std::string>& headers = {},
        const std::map<std::string, std::string>& params = {});
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
        path.append("/streams");
        return path;
    }
    void LoadSlots(const std::map<int32_t, std::string>& slots);
    Slot NextSlot();
    void UpdateSlot(int32_t id, const std::string& serverAddr);
    void DisableSlot(const std::string& disableServerAddr);

private:
    typedef std::vector<Slot> SlotListType;

    bool CheckSlots(const SlotListType& slots, const std::string& addr);


private:
    std::string mUploadId;
    bool mP2PMode;
    std::string mProject;
    std::string mTable;
    std::string mPartition;
    std::string mStatus;
    Configuration mConf;
    ODPSTableSchemaPtr mSchema;
    SlotListType mSlots;
    uint32_t mCurrentSlot;
    std::mutex mLock;
    RecordPackWriterPtr mRecordPackWriter;
    CompressOption mCompressMode;
    std::string mSchemaName;
    int32_t mSlotNum;
    bool mCreatePartition;
    int64_t mLastUpdate;

    std::string mQuotaName;

    #ifdef ODPS_SDK_ENABLE_ARROW
    std::shared_ptr<arrow::Schema> mArrowSchema;
    #endif

    friend class EOSStream;
};

}
}
}
}
}

#endif

