#include <string.h>
#include <iostream>
#include "google/protobuf/io/gzip_stream.h"
#include "util/string_util.h"
#include "serialize.h"
#include "volume_serialize.h"
#include "odps_tunnel.h"    //for including OdpsTunnelException class
#include "tunnel/zstd_stream.h"
#include "tunnel/lz4_stream.h"

using namespace std;
using namespace google::protobuf::io;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;

static const int CHECKSUM_SIZE = sizeof(uint32_t);
static const uint32_t MAX_CHUNKSIZE = 256 * 1024 * 1024;
static const uint32_t MIN_CHUNKSIZE = 1;

VolumeSerializer::VolumeSerializer(google::protobuf::io::ZeroCopyOutputStream* out, bool isCompress, int chunkSize)
  : mOutput(NULL)
  , mBytes(0)
  , mCompressOption(isCompress ? CompressOption::ZLIB_COMPRESS : CompressOption::NO_COMPRESS)
  , mChunkSize(static_cast<uint32_t>(chunkSize))
  , mChunkOffset(0)
  , mIsHeaderCompleted(false)
{
    Init(out);
}

VolumeSerializer::VolumeSerializer(google::protobuf::io::ZeroCopyOutputStream* out, const CompressOption& compressOption, int chunkSize)
    : mOutput(NULL)
    , mBytes(0)
    , mCompressOption(compressOption)
    , mChunkSize(static_cast<uint32_t>(chunkSize))
    , mChunkOffset(0)
    , mIsHeaderCompleted(false)
{
    Init(out);
}

VolumeSerializer::~VolumeSerializer()
{
    if (mOutput)
    {
        if (mCompressOption.algorithm != CompressOption::ODPS_RAW)
        {
            delete mOutput;
            mOutput = NULL;
        }
    }
}

void VolumeSerializer::Write(const char* buf, int size)
{
    if (!mIsHeaderCompleted)
    {
        mIsHeaderCompleted = true;
        WriteUInt32(mChunkSize);
        CRCInt32(mChunkSize);
        mChunkOffset = 0;
    }
    int bytes = 0;
    while (bytes < size)
    {
        if (mChunkOffset == mChunkSize)
        {
            uint32_t crc = mCrc.checksum();
            WriteUInt32(crc);
            mChunkOffset = 0;
        }
        else
        {
            int len = size - bytes > (int)(mChunkSize-mChunkOffset) ? (int)(mChunkSize-mChunkOffset) : size - bytes;
            mCrc.process_bytes(buf + bytes, len);
            WriteInternal(buf + bytes, len);
            bytes += len;
            mChunkOffset += len;
        }
    }
}

void VolumeSerializer::WriteInternal(const char* buf, int size)
{

    void* out;
    int n = 0, bytes = 0;
    while ( bytes < size && mOutput->Next(&out, &n) )
    {
        int len = size-bytes > n ? n : size-bytes;
        memcpy(out, buf+bytes, len);
        bytes += len;

        if (len < n) {
            mOutput->BackUp(n-len);
            break;
        }
    }

    if ( bytes != size)
    {
        throw OdpsTunnelException("VolumeSerializer io error.");
    }

    mBytes += size;
}

void VolumeSerializer::WriteUInt32(uint32_t value)
{
    char buf[4];
    buf[0] = (value >> 24) & 0x000000ff;
    buf[1] = (value >> 16) & 0x000000ff;
    buf[2] = (value >> 8) & 0x000000ff;
    buf[3] = value & 0x000000ff;

    WriteInternal(buf, 4);
    mBytes += 4;
}

void VolumeSerializer::CRCInt32(uint32_t value)
{
    mCrc.process_byte((value >> 24) & 0x000000ff);
    mCrc.process_byte((value >> 16) & 0x000000ff);
    mCrc.process_byte((value >> 8) & 0x000000ff);
    mCrc.process_byte(value & 0x000000ff);
}

void VolumeSerializer::Init(google::protobuf::io::ZeroCopyOutputStream* out)
{
    if (mCompressOption.algorithm == CompressOption::ODPS_ZLIB)
    {
        google::protobuf::io::GzipOutputStream::Options tmpOption;
        tmpOption.format = GzipOutputStream::ZLIB;
        tmpOption.compression_level = mCompressOption.level;
        tmpOption.compression_strategy = mCompressOption.strategy;
        mOutput = new GzipOutputStream(out, tmpOption);
    }
    else if (mCompressOption.algorithm == CompressOption::ODPS_ZSTD)
    {
        mOutput = new ZstdOutputStream(out, mCompressOption.level, mCompressOption.bsize, mCompressOption.mode > 0);
    }
    else if (mCompressOption.algorithm == CompressOption::ODPS_LZ4_FRAME)
    {
        mOutput = new Lz4OutputStream(out);
    }
    else if (mCompressOption.algorithm == CompressOption::ODPS_RAW)
    {
        mOutput = out;
    }
    else
    {
        throw OdpsTunnelException(INVALID_ARGUMENT, "Unsupported compression " + CompressOptionToEncoding(mCompressOption));
    }
}

void VolumeSerializer::Close()
{
    if (mChunkOffset != 0)
    {
        uint32_t crc = mCrc.checksum();
        WriteUInt32(crc);
    }

    if (mOutput)
    {
        if (mCompressOption.algorithm != CompressOption::ODPS_RAW)
        {
            delete mOutput;
            mOutput = NULL;
        }
    }
}

VolumeDeserializer::VolumeDeserializer(google::protobuf::io::ZeroCopyInputStream* in, int blockSize, bool isCompress)
  : mInput(NULL)
  , mBytes(0)
  , mBufferSize(blockSize)
  , mDataSize(0)
  , mBuffer(NULL)
  , mPos(0)
  , mCompressOption(isCompress ? CompressOption::ZLIB_COMPRESS : CompressOption::NO_COMPRESS)
  , mEOF(false)
  , mChunkSize(0)
  , mChunkOffset(0)
  , mIsHeaderCompleted(false)
{
    Init(in);
}

VolumeDeserializer::VolumeDeserializer(google::protobuf::io::ZeroCopyInputStream* in, int blockSize, const CompressOption& compressOption)
    : mInput(NULL)
    , mBytes(0)
    , mBufferSize(blockSize)
    , mDataSize(0)
    , mBuffer(NULL)
    , mPos(0)
    , mCompressOption(compressOption)
    , mEOF(false)
    , mChunkSize(0)
    , mChunkOffset(0)
    , mIsHeaderCompleted(false)
{
    Init(in);
}

VolumeDeserializer::~VolumeDeserializer()
{
    delete [] mBuffer;

    Close();
}

int VolumeDeserializer::Read(char* buf, int size)
{
    if (!mIsHeaderCompleted)
    {
        mIsHeaderCompleted = true;
        char buffer[4];
        int nread = ReadInternal(buffer, 4);

        if (nread == 0)
        {
            mEOF = true;
            return 0;
        }
        else if (nread == 4)
        {
            mCrc.process_bytes(buffer, 4);
            mChunkSize = GetInt(buffer);
            if (mChunkSize > MAX_CHUNKSIZE || mChunkSize < MIN_CHUNKSIZE)
            {
                string errMsg = "ChunkSize should be in [" + std::to_string(MIN_CHUNKSIZE) + ", " + std::to_string(MAX_CHUNKSIZE) + "], now is :" + std::to_string(mChunkSize);
                throw OdpsTunnelException(errMsg);
            }

            mChunkOffset = 0;
            // chunkSize plus  check_sum
            mBufferSize = mChunkSize + CHECKSUM_SIZE;
            mBuffer = new char[mBufferSize];
        }
        else
        {
            throw OdpsTunnelException("VolumeDeserializer io error.");
        }
    }
    int nread = ReadBuffer(buf, size);
    if (!nread)
    {
        if (!IsEOF())
        {
            FillBuffer();
            nread = ReadBuffer(buf, size);
        }
    }
    return nread;
}

void VolumeDeserializer::Close()
{
    if (mInput)
    {
        if (mCompressOption.algorithm != CompressOption::ODPS_RAW)
        {
            delete mInput;
            mInput = NULL;
        }
    }
}

int VolumeDeserializer::ReadBuffer(char* buf, int size)
{
    if (mDataSize <= mPos)
        return 0;

    int free = mDataSize - mPos;
    int len = free < size ? free : size;

    memcpy(buf, mBuffer+mPos, len);
    mPos += len;

    return len;
}

int VolumeDeserializer::ReadInternal(char* buf, int size)
{
    const void* in;
    int n = 0, bytes = 0;
    while ( bytes < size && mInput->Next(&in, &n) )
    {
        int len = size-bytes > n ? n : size-bytes;
        memcpy(buf+bytes, in, len);
        bytes += len;

        if (len < n) {
            mInput->BackUp(n-len);
            break;
        }
    }

    mBytes += bytes;
    return bytes;
}

void VolumeDeserializer::FillBuffer()
{
    if (mDataSize > mPos)
        return;

    int nread = ReadInternal(mBuffer, mBufferSize);

    if (nread == 0)
    {
        mEOF = true;
        return;
    }
    else if (nread >= CHECKSUM_SIZE)
    {
        mDataSize = nread - CHECKSUM_SIZE;
        mPos = 0;

        mCrc.process_bytes(mBuffer, mDataSize);
        uint32_t crc = GetInt(mBuffer + mDataSize);
        if (crc != mCrc.checksum())
        {
            throw OdpsTunnelException("VolumeDeserializer checksum error."+ToHexString(crc)+","+ToHexString(mCrc.checksum()));
        }
        mBytes += nread;
    }
    else
    {
        throw OdpsTunnelException("VolumeDeserializer io error.");
    }
}

uint32_t VolumeDeserializer::GetInt(char* buf)
{
    uint32_t value;
    value = buf[0] & 0x000000ff;
    value = (value << 8) | (buf[1] & 0x000000ff);
    value = (value << 8) | (buf[2] & 0x000000ff);
    value = (value << 8) | (buf[3] & 0x000000ff);
    return value;
}


void VolumeDeserializer::Init(ZeroCopyInputStream* in)
{
    if (CompressOption::ODPS_ZLIB == mCompressOption.algorithm)
    {
        mInput = new GzipInputStream(in);
    }
    else if (CompressOption::ODPS_ZSTD == mCompressOption.algorithm)
    {
        mInput = new ZstdInputStream(in);
    }
    else if (CompressOption::ODPS_LZ4_FRAME == mCompressOption.algorithm)
    {
        mInput = new Lz4InputStream(in);
    }
    else if (CompressOption::ODPS_RAW == mCompressOption.algorithm)
    {
        mInput = in;
    }
    else
    {
        throw OdpsTunnelException(INVALID_ARGUMENT, "Comparess algorithm not support.");
    }
}
