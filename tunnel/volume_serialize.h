#ifndef APSARA_ODPS_TUNNEL_VOLUME_SERIALIZE_H
#define APSARA_ODPS_TUNNEL_VOLUME_SERIALIZE_H

#include <stdint.h>
#include <memory>
#include "crc_32c.h"

#include "google/protobuf/stubs/common.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/coded_stream.h"
#include "include/configuration.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

class VolumeSerializer
{
public:
    VolumeSerializer(google::protobuf::io::ZeroCopyOutputStream* out, bool isCompress, int chunkSize);
    VolumeSerializer(google::protobuf::io::ZeroCopyOutputStream* out, const CompressOption& compressOption, int chunkSize);
    virtual ~VolumeSerializer();

    virtual void Write(const char* buf, int len);
    virtual void Close();

    int64_t ByteCount() { return mBytes; }

private:
    void WriteInternal(const char* buf, int len);
    void WriteUInt32(uint32_t);
    void CRCInt32(uint32_t);
    void Init(google::protobuf::io::ZeroCopyOutputStream* out);

private:
    google::protobuf::io::ZeroCopyOutputStream* mOutput;
    int64_t mBytes;
    CompressOption mCompressOption;
    boost::crc_32_type mCrc;
    uint32_t mChunkSize;
    uint32_t mChunkOffset;
    bool mIsHeaderCompleted;
};

typedef std::shared_ptr<VolumeSerializer> VolumeSerializerPtr;

class VolumeDeserializer
{
public:
    VolumeDeserializer(google::protobuf::io::ZeroCopyInputStream* in, int blockSize, bool isCompress);
    VolumeDeserializer(google::protobuf::io::ZeroCopyInputStream* in, int blockSize, const CompressOption& compressOption);
    virtual ~VolumeDeserializer();

    virtual int Read(char* buf, int size);
    virtual void Close();

    int GetBufferSize() { return mBufferSize; }
    bool IsEOF() { return mEOF; }

    int64_t ByteCount() { return mBytes; }

protected:
    virtual int ReadBuffer(char* buf, int size);
    virtual int ReadInternal(char* buf, int size);
    virtual void FillBuffer();
    uint32_t GetInt(char* buf);
    void Init(google::protobuf::io::ZeroCopyInputStream* in);

private:
    google::protobuf::io::ZeroCopyInputStream* mInput;
    //apsara::odps::sdk::server::http::RequestPtr mRequest;
    int64_t mBytes;
    int32_t mBufferSize;
    int32_t mDataSize;
    char* mBuffer;
    int mPos;
    CompressOption mCompressOption;
    bool mEOF;
    boost::crc_32_type mCrc;
    uint32_t mChunkSize;
    uint32_t mChunkOffset;
    bool mIsHeaderCompleted;
};

typedef std::shared_ptr<VolumeDeserializer> VolumeDeserializerPtr;

}}}}}

#endif
