#ifndef APSARA_ODPS_TUNNEL_ARROW_H
#define APSARA_ODPS_TUNNEL_ARROW_H
#ifdef ODPS_SDK_ENABLE_ARROW
#include <arrow/api.h>
#include <arrow/io/api.h>
#include "crc_32c.h"
#include <memory>
#include <arpa/inet.h>
#include "google/protobuf/io/zero_copy_stream.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal { namespace tunnel{

static inline void Byte4ToUint32(const char (&in)[4], uint32_t& out) {
    const uint32_t* network = reinterpret_cast<const uint32_t*>(&(in[0]));
    out = ntohl(*network);
}

static inline void Uint32ToByte4(const uint32_t& in, char (&out)[4]) {
    uint32_t* network = reinterpret_cast<uint32_t*>(&(out[0]));
    *network = htonl(in);
}

class IArrowInputStream : public arrow::io::InputStream
{
public:
    virtual bool IsEOF() = 0;
};

template <typename StreamType>
class ArrowInputStream : public IArrowInputStream
{
public:
    using StreamTypePtr = std::shared_ptr<StreamType>;
    ArrowInputStream(const StreamTypePtr& reponse);
    ArrowInputStream(StreamType* response):
        ArrowInputStream(StreamTypePtr(response)) {};
    virtual ~ArrowInputStream();

    // Implement the InputStream interface
    arrow::Status Close() override;
    arrow::Result<int64_t> Tell() const override;

    arrow::Result<int64_t> Read(int64_t nbytes, void* out) override;
    arrow::Result<std::shared_ptr<arrow::Buffer>> Read(int64_t nbytes) override;
    virtual bool IsEOF() override;

    bool closed() const override { return !mIsOpen;};

private:
    StreamTypePtr mInputStreamPtr;

    uint32_t mCrcChunkSize;

    char* mBuffer;
    int64_t mPos;
    int64_t mGlobalPos;
    int64_t mSize;

    crc_32c_type mGlobalCrc;
    crc_32c_type mChunkCrc;

    bool mIsOpen;
    bool mLastChunk;

private:
    arrow::Status ReadChunk();
};

// HttpConnection has to provide Write(buf, size) method.

class IArrowOutputStream: public arrow::io::OutputStream
{
};


template <typename StreamType>
class ArrowOutputStream: public IArrowOutputStream
{
public:
    using StreamTypePtr = std::shared_ptr<StreamType>;
    ArrowOutputStream(const StreamTypePtr& request,
                          uint32_t chunksize = 1024);
    ArrowOutputStream(StreamType* request, uint32_t chunksize = 1024):
        ArrowOutputStream(StreamTypePtr(request), chunksize) {};

    virtual ~ArrowOutputStream();

    // Implement the OutputStream interface
    arrow::Status Write(const void* data, int64_t bytes) override;
    arrow::Status Flush() override;

    arrow::Status Close() override;
    arrow::Result<int64_t> Tell() const override;

    bool closed() const override {return !mIsOpen;};

private:
    arrow::Status WriteChunk(char* data, int64_t bytes, int64_t& written);
    arrow::Status WriteUint32(uint32_t number);

private:
    StreamTypePtr mOutputStreamPtr;

    uint32_t mCrcChunkSize;
    int64_t mOffset;
    int64_t mPos;

    crc_32c_type mGlobalCrc;
    crc_32c_type mChunkCrc;

    bool mIsOpen;
    bool mIsGlobalCheck;
    bool mIsWriteChunkSize;
};

class TunnelArrowBuffer : public arrow::MutableBuffer
{
public:
    TunnelArrowBuffer(int64_t size): MutableBuffer(new uint8_t[size], size){}
    virtual ~TunnelArrowBuffer()
    {
        delete [] data_;
    }
};

//-------the Implementation of HttpInputputStream---------
template <typename StreamType>
ArrowInputStream<StreamType>::ArrowInputStream(const StreamTypePtr& response):
    mInputStreamPtr(response),
    mCrcChunkSize(0),
    mBuffer(nullptr),
    mPos(0),
    mGlobalPos(0),
    mSize(0),
    mIsOpen(true),
    mLastChunk(false){}

template <typename StreamType>
ArrowInputStream<StreamType>::~ArrowInputStream()
{
    delete [] mBuffer;
}

// Perform CRC Check and recover a chunk to mBuffer
template <typename StreamType>
arrow::Status ArrowInputStream<StreamType>::ReadChunk()
{
    if (mSize > mPos)
    {
        return arrow::Status::OK();
    }

    if (!mBuffer)
    {   //GetChunkSize
        char temp[4];
        if (mInputStreamPtr->Read(temp, 4) != 4)
        {
            return arrow::Status::IOError("InputStream Read() for chunk size failed.");
        }
        Byte4ToUint32(temp, mCrcChunkSize);
        if (mCrcChunkSize == 0)
        {
            // empty chunk. means there's no records to return.
            // to make IsEOF() work, set lastchunk here
            mLastChunk = true;
            return arrow::Status::IOError("EOF");
        }
        mBuffer = new char[mCrcChunkSize + 4];
    }

    int64_t byteread = mCrcChunkSize + 4;
    int64_t read = 0;
    while (byteread > 0)
    {
        int64_t readed = mInputStreamPtr->Read(mBuffer + read, byteread);
        if (readed <= 0)
        {
            break;
        }
        byteread -= readed;
        read += readed;
    }

    if (read < 4)
    {
        return arrow::Status::IOError("InputStream Read() for crc32 failed.");
    }
    uint32_t crc = 0, crc_ = 0;
    Byte4ToUint32(reinterpret_cast<char (&)[4]>(*(mBuffer + read - 4)), crc_);
    mGlobalCrc.process_bytes(mBuffer, read - 4);

    if (read < mCrcChunkSize + 4)
    {
        mLastChunk = true;
        crc = mGlobalCrc.checksum();
    }
    else
    {
        mChunkCrc.reset();
        mChunkCrc.process_bytes(mBuffer, read - 4);
        crc = mChunkCrc.checksum();
    }

    if (crc != crc_)
    {
        return arrow::Status::SerializationError("CRC Check failed.");
    }
    mSize = read - 4;
    mPos = 0;
    return arrow::Status::OK();
}

template <typename StreamType>
arrow::Status ArrowInputStream<StreamType>::Close()
{
    mIsOpen = false;
    return arrow::Status::OK();
}

template <typename StreamType>
arrow::Result<int64_t> ArrowInputStream<StreamType>::Tell() const
{
    return arrow::Result<int64_t>(mGlobalPos);
}

template <typename StreamType>
bool ArrowInputStream<StreamType>::IsEOF()
{
    return mLastChunk && mSize == mPos;
}

template <typename StreamType>
arrow::Result<int64_t> ArrowInputStream<StreamType>::Read(int64_t bytes, void* out)
{
    if (!mIsOpen)
    {
        return arrow::Status::Invalid("Operation forbidden on closed BufferReader");
    }

    if (bytes < 0)
    {
        return arrow::Status::IOError("Cannot read a negative number \
                                            of bytes from BufferReader.");
    }

    if (IsEOF())
    {
        return arrow::Status::IOError("Complete read.");
    }

    arrow::Status readStat = ReadChunk();
    if(!readStat.ok())
    {
        return readStat;
    }

    int64_t readBytes = (mSize - mPos) > bytes ? bytes : (mSize - mPos);
    memcpy(out, mBuffer + mPos, readBytes);
    mPos += readBytes;
    mGlobalPos += readBytes;
    return arrow::Result<int64_t>(readBytes);
}

template <typename StreamType>
arrow::Result<std::shared_ptr<arrow::Buffer>> ArrowInputStream<StreamType>::Read(int64_t bytes)
{
    if (!mIsOpen)
    {
        return arrow::Status::Invalid("Operation forbidden on closed BufferReader");
    }

    if (bytes < 0)
    {
        return arrow::Status::Invalid("Cannot read a negative number of bytes from BufferReader.");
    }

    std::shared_ptr<arrow::Buffer> out = std::make_shared<TunnelArrowBuffer>(bytes);

    int64_t remainBytes = bytes;
    uint8_t* readPosition = out->mutable_data();
    while(remainBytes > 0)
    {
        int64_t readBytes;
        ARROW_ASSIGN_OR_RAISE(readBytes, Read(remainBytes, readPosition));
        readPosition += readBytes;
        remainBytes -= readBytes;
    }
    return arrow::Result<std::shared_ptr<arrow::Buffer>>(out);
}


//-------the Implementation of ArrowOutputStream------------------------
template <typename StreamType>
ArrowOutputStream<StreamType>::ArrowOutputStream(
            const StreamTypePtr& request,
            uint32_t chunksize)
          : mOutputStreamPtr(request),
            mCrcChunkSize(chunksize),
            mOffset(0),
            mPos(0),
            mIsOpen(true),
            mIsGlobalCheck(false),
            mIsWriteChunkSize(false){}

template <typename StreamType>
ArrowOutputStream<StreamType>::~ArrowOutputStream(){}

template <typename StreamType>
arrow::Status ArrowOutputStream<StreamType>::WriteChunk(char* data, int64_t bytes, int64_t& written)
{
    int remain = mCrcChunkSize - mOffset;
    size_t toWriteBytes = remain > bytes ? bytes : remain;
    size_t writtenBytes = mOutputStreamPtr->Write(data, toWriteBytes);
    if (writtenBytes != toWriteBytes)
    {
        written = writtenBytes;
        return arrow::Status::IOError("OutputStream Write() failed.");
    }
    mChunkCrc.process_bytes(data, writtenBytes);
    if (remain > bytes)
    {
        mOffset += bytes;
    }
    else
    {
        uint32_t crc = mChunkCrc.checksum();
        arrow::Status crcWriteStat = WriteUint32(crc);
        if (!crcWriteStat.ok())
        {
            return crcWriteStat;
        }
        mChunkCrc.reset();
        mOffset = 0;
    }
    written = writtenBytes;
    return arrow::Status::OK();
}

template <typename StreamType>
arrow::Status ArrowOutputStream<StreamType>::Write(const void* data, int64_t bytes)
{
    char* buffer = (char*)data;
    if (!mIsOpen)
    {
        return arrow::Status::IOError("OutputStream is closed");
    }

    if (!mIsWriteChunkSize)
    {
        arrow::Status sizeWriteStat = WriteUint32(mCrcChunkSize);
        if (!sizeWriteStat.ok())
        {
            return sizeWriteStat;
        }
        mIsWriteChunkSize = true;
    }

    mGlobalCrc.process_bytes(buffer, bytes);
    mPos += bytes;

    do
    {
        int64_t writtenBytes = 0;
        arrow::Status writeStat = WriteChunk(buffer, bytes, writtenBytes);
        if(!writeStat.ok())
        {
            return writeStat;
        }
        buffer += writtenBytes;
        bytes -= writtenBytes;
    } while (bytes > 0);

    return arrow::Status::OK();
}

// Perform CrcCheck for remain data
template <typename StreamType>
arrow::Status ArrowOutputStream<StreamType>::Flush()
{
    if (!mIsGlobalCheck)
    {
        uint32_t crc = mGlobalCrc.checksum();
        arrow::Status crcWriteStat = WriteUint32(crc);
        if (!crcWriteStat.ok())
        {
            return crcWriteStat;
        }
        mIsGlobalCheck = true;
    }
    return arrow::Status::OK();

}

template <typename StreamType>
arrow::Status ArrowOutputStream<StreamType>::Close()
{
    Flush();
    mIsOpen = false;
    return arrow::Status::OK();
}

template <typename StreamType>
arrow::Result<int64_t> ArrowOutputStream<StreamType>::Tell() const
{
    return arrow::Result<int64_t>(mPos);
}

template <typename StreamType>
arrow::Status ArrowOutputStream<StreamType>::WriteUint32(uint32_t number)
{
    char buffer[sizeof(uint32_t)] = {0};
    Uint32ToByte4(number, buffer);

    char* bufferPosition = buffer;
    int64_t toWrite = sizeof(uint32_t);
    while (toWrite > 0)
    {
        int64_t written = mOutputStreamPtr->Write(bufferPosition, toWrite);
        if (written <= 0)
        {
            return arrow::Status::IOError("OutputStream Write() failed.");
        }
        toWrite -= written;
        if (toWrite > 0)
        {
            bufferPosition += written;
        }
    }
    return arrow::Status::OK();
}

class ZeroCopyStreamReader
{
public:
    ZeroCopyStreamReader(std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> input);
    int64_t Read(char* dstbuf, int64_t size);

private:
    std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> mInput;
};
class ZeroCopyStreamWriter
{
public:
    ZeroCopyStreamWriter(std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> output);
    int64_t Write(char* srcbuf, int64_t size);
private:
    std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> mOutput;
};

}}}}}
#endif

#endif
