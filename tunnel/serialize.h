#ifndef APSARA_ODPS_TUNNEL_INTERANL_SERIALIZE_H
#define APSARA_ODPS_TUNNEL_INTERANL_SERIALIZE_H

#include <cstdint>
#include <stdint.h>
#include <bitset>
#include <memory>
#include "crc_32c.h"

#include "google/protobuf/stubs/common.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/gzip_stream.h"

#include "common/odps_table_schema.h"
#include "configuration.h"

#include "common/http_message.h"
#include "common/http_connection.h"

#include "coded_checksum.h"
#include "util.h"

#define DEFAULT_TOTAL_BYTES_LIMIT 335544320 // 320MB
#define MAX_ODPS_COLUMNS 10000


namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

class Serializer;
class Deserializer;
class ProtoSerializer;
class ProtoDeserializer;
typedef std::shared_ptr<Serializer> SerializerPtr;
typedef std::shared_ptr<Deserializer> DeserializerPtr;
typedef std::shared_ptr<ProtoSerializer> ProtoSerializerPtr;
typedef std::shared_ptr<ProtoDeserializer> ProtoDeserializerPtr;

class Serializer
{
public:
    virtual ~Serializer() {}

    virtual bool Serialize(const ODPSTableRecord& r) = 0;
};

class Deserializer
{
public:
    virtual ~Deserializer() {}

    virtual bool Deserialize(ODPSTableRecord& r) = 0;
};

/**
 * for google::protobuf::io::CodedInputStream read data from http connction
 */
class HttpInputStream : public google::protobuf::io::ZeroCopyInputStream
{
public:
    // construct a HttpInputStream use a http request object.
    HttpInputStream(const ResponsePtr response, const int bufferSize);

    virtual ~HttpInputStream();

    virtual bool Next(const void** data, int* size);
    virtual void BackUp(int count);
    virtual bool Skip(int count);
    /**
     * NOTE: ZeroCopyInputStream returns int64
     */
    virtual int64_t ByteCount() const { return mBytes; }
    virtual int64_t Cost() const { return mLatency; }

private:
    ResponsePtr mResponsePtr;
    int mBufferSize;

    char* mBuffer;
    int mPos;
    int mDataSize;
    int64_t mBytes;
    int64_t mLatency;
};
typedef std::shared_ptr<HttpInputStream> HttpInputStreamPtr;

/**
 * for google::protobuf::io::CodedOutputStream write data to http connection
 */
class HttpOutputStream : public google::protobuf::io::ZeroCopyOutputStream
{
public:
    // construct a HttpOutputStream use a http response object.
    HttpOutputStream(HttpConnectionPtr request, const int bufferSize);

    virtual ~HttpOutputStream();

    virtual bool Next(void** data, int* size);
    virtual void BackUp(int count);
    /**
     * NOTE: ZeroCopyOutputStream returns int64
     */
    virtual int64_t ByteCount() const { return mBytes; }
    virtual int64_t Cost() const { return mLatency; }
    virtual int64_t TotalByteCount() const { return mPos + mBytes; }
    virtual bool Flush();

private:
    HttpConnectionPtr mRequestPtr;
    int mBufferSize;
    char* mBuffer;
    int mPos;
    int64_t mBytes;
    int64_t mLatency;
};
typedef std::shared_ptr<HttpOutputStream> HttpOutputStreamPtr;

/**
 *
 * protobuf serialize impl
 *
 */
class ProtoSerializer : public Serializer
{
public:
    ProtoSerializer(google::protobuf::io::ZeroCopyOutputStream* out,
                ODPSTableSchema* schema,
                bool compress = false,
                bool doStreamChecksum = true,
                int protobufTotalBytesLimit = DEFAULT_TOTAL_BYTES_LIMIT,
                CompressOption* option = NULL);

    virtual ~ProtoSerializer();

    virtual bool Serialize(const ODPSTableRecord& r);

    virtual void Complete();
private:
    void WriteCell(const int32_t index, const ODPSTableRecord& r, ODPSColumnType type);
    void WriteNestedValue(const void* value, const ODPSColumnTypeInfo& type);
    void WriteComplexValue(IODPSProtoSerializablePtr complexValue);

private:
    google::protobuf::io::ZeroCopyOutputStream* mOutput;
    google::protobuf::io::CodedOutputStream* mCodedOutput;
    ODPSTableSchema* mSchema;

    int mProtobufTotalBytesLimit;
    int64_t mTotalRecords;

    crc_32c_type mRecordCrc;
    crc_32c_type mCrcCrc;

    bool mDoStreamChecksum;
    CodedChecksum mCodedChecksum;

    int64_t mBytes;
    bool mIsCompress;
};

/**
 *
 * protobuf deserialize impl
 *
 */
class ProtoDeserializer : public Deserializer
{
public:
    ProtoDeserializer(google::protobuf::io::ZeroCopyInputStream* in,
                ODPSTableSchema* schema,
                bool compressed = false,
                bool doStreamChecksum = true,
                int protobufTotalBytesLimit = DEFAULT_TOTAL_BYTES_LIMIT,
                CompressOption* option = NULL);

    inline ~ProtoDeserializer()
    {
        if (mCodedInput != NULL)
        {
            delete mCodedInput;
        }

        if (mIsCompressed && mInput != NULL)
        {
            delete mInput;
        }
    }

    virtual bool Deserialize(ODPSTableRecord& r);

    bool IsCompleted()
    {
        uint32_t checksum = 0;
        if (mDoStreamChecksum)
        {
            checksum = mCodedChecksum.GetChecksum();
        }
        else
        {
            checksum = mCrcCrc.checksum();
        }
        return mReceivedTotalRecords == mTotalRecords && mReceivedChecksum == checksum;
    }

    int64_t GetServerIOCost()
    {
        return mServerMetrics.ServerIOCost;
    }

    int64_t GetServerTotalCost()
    {
        return mServerMetrics.ServerTotalCost;
    }

    int64_t GetPanguIOCost()
    {
        return mServerMetrics.PanguIOCost;
    }

    int64_t GetRateLimitCost()
    {
        return mServerMetrics.RateLimitCost;
    }

private:
    void ReadNestedValue(std::shared_ptr<void>* value, const ODPSColumnTypeInfo& type);
    IODPSProtoSerializablePtr ReadComplexValue(const ODPSColumnTypeInfo& type);
    std::string ReadMetrics();

    google::protobuf::io::ZeroCopyInputStream* mInput;
    google::protobuf::io::CodedInputStream* mCodedInput;
    ODPSTableSchema* mSchema;

    int mProtobufTotalBytesLimit;
    int64_t mTotalRecords;

    crc_32c_type mRecordCrc;
    crc_32c_type mCrcCrc;

    bool mDoStreamChecksum;
    CodedChecksum mCodedChecksum;

    std::bitset<MAX_ODPS_COLUMNS> mNullMap;
    bool mGotTotalRecords;
    bool mGotMetrics;
    int64_t mReceivedTotalRecords;
    uint32_t mReceivedChecksum;

    int64_t mBytes;
    bool mIsCompressed;

    Metrics mServerMetrics;
};

}}}}}

#endif
