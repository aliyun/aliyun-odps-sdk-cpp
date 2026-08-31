#ifndef APSARA_ODPS_TUNNEL_INTERNAL_RECORD_READER_H
#define APSARA_ODPS_TUNNEL_INTERNAL_RECORD_READER_H

#include <deque>

#include "odps_tunnel.h"
#include "common/http_connection.h"
#include "connection_manager.h"
#include "serialize.h"
#include "util.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

class Download;
class RecordReader : public IRecordReader
{
public:
    RecordReader(ODPSTableSchemaPtr s, const ConnectionManagerPtr& connManager, bool const compress = false, CompressOption* option = NULL);
    virtual ~RecordReader();
    virtual bool Read(ODPSTableRecord& r) override;
    virtual void Close() override;
    virtual IODPSTableSchema* GetSchema() override { return mSchema.get(); }
    virtual ODPSTableRecordPtr CreateBufferRecord() override { return ODPSTableRecordPtr(new ODPSTableRecord(mSchema)); }
    virtual std::string GetMetrics() override;
private:
    ODPSTableSchemaPtr mSchema;
    HttpConnectionPtr mConn;
    HttpInputStream* in;
    ProtoDeserializerPtr mDeserializer;
    bool mClosed;
    Metrics mMetrics;
};


struct DownloadInfo
{
    Configuration conf;
    std::string project;
    std::string table;
    std::string partition;
    std::string downloadId;
    std::string schemaName;
};
class BufferRecordReader : public IBufferRecordReader
{
public:
    BufferRecordReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames, const CompressOption& option, const DownloadInfo downloadInfo, bool disableModifiedCheck);
    virtual ~BufferRecordReader();

    virtual ODPSTableRecordPtr Read() override;
    virtual ODPSTableRecordPtr ReadWithRetry(uint64_t retryTimes) override;
    virtual void Close() override;

private:
    uint64_t mStart;
    uint64_t mCount;
    uint64_t mBufferRecordCount;
    std::vector<std::string> mColNames;
    CompressOption mOption;
    DownloadInfo mDownloadInfo;
    bool mDisableModifiedCheck;

    IDownloadPtr mDownload;
    std::deque<ODPSTableRecordPtr> mRecordBuffer;

    void ReadData();

};

typedef std::shared_ptr<RecordReader> RecordReaderPtr;
typedef std::shared_ptr<BufferRecordReader> BufferRecordReaderPtr;

}}}}}
#endif
