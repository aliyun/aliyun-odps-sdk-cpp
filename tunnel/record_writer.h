#ifndef APSARA_ODPS_TUNNEL_INTERNAL_RECORD_WRITER_H
#define APSARA_ODPS_TUNNEL_INTERNAL_RECORD_WRITER_H

#include "connection_manager.h"
#include "error_code.h"
#include "odps_tunnel.h"
#include "common/http_connection.h"
#include "serialize.h"
#include "util.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

class RecordWriter : public IRecordWriter
{
public:
    RecordWriter(ODPSTableSchemaPtr s, const ConnectionManagerPtr& connManager, bool const compress = false, CompressOption* option = NULL);
    virtual bool Write(const ODPSTableRecord& r);
    virtual ~RecordWriter();
    virtual void Close();
    virtual std::string GetTraceId() { return mTraceId; }
    virtual int64_t GetWrittenSize() {return (mOutput != NULL ? mOutput->ByteCount() : 0);}
    virtual int64_t GetResponseSize() {return mResponseSize;}
    virtual std::string GetMetrics() override;

private:
    ODPSTableSchemaPtr mSchema;
    HttpConnectionPtr mConn;
    HttpOutputStream* mOutput;
    ProtoSerializerPtr mSerializer;
    std::string mTraceId;
    int64_t mResponseSize;
    Metrics mMetrics;
};

typedef std::shared_ptr<RecordWriter> RecordWriterPtr;

class RecordPackWriter : public IRecordWriter
{
public:
    RecordPackWriter(ODPSTableSchemaPtr s, std::string* target, bool const compress = false, CompressOption* option = NULL);
    virtual bool Write(const ODPSTableRecord& r) override;
    virtual ~RecordPackWriter();
    virtual void Close() override;
    virtual std::string GetTraceId() override { return ""; }
    virtual int64_t GetWrittenSize() override {return (mOutputStream != nullptr ? mOutputStream->ByteCount() : 0);}
    virtual int64_t GetResponseSize() override {return 0;}
    virtual std::string GetMetrics() override { return ""; }

private:
    ODPSTableSchemaPtr mSchema;
    std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> mOutputStream;
    ProtoSerializerPtr mSerializer;
    std::string* mTargetString;
};

}}}}}
#endif
