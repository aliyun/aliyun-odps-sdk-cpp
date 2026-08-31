#include "lz4_stream.h"
#include "common/logging.h"
#include "odps_exception.h"
#include <iostream>
#include <string>
#include <lz4frame.h>
#include "util/utils.h"

using namespace std;
using namespace apsara::odps::sdk;

static apsara::odps::sdk::logging::Logger* sLogger = apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

namespace google {
namespace protobuf {
namespace io {


Lz4InputStream::Lz4InputStream(ZeroCopyInputStream* subStream,
            int desBufferSize):
    mSubStream(subStream),
    mByteCount(0),
    mOutputBuffer(new char[desBufferSize]),
    mOutputBufferSize(desBufferSize),
    mOutputBufferFilled(0),
    mOutputBufferRemains(0),
    mInputBuffer(nullptr),
    mInputBufferSize(0),
    mInputBufferOffset(0),
    mDecompressTimer(0),
    mFailed(false)
{
    size_t ret = LZ4F_createDecompressionContext(&mDecompressContext,LZ4F_VERSION);
    if (LZ4F_isError(ret) != 0)
    {
        util::OdpsThrow(INTERNAL_ERROR, "create LZ4 decompressionContext failed: " +
            std::string(LZ4F_getErrorName(ret)));
    }
}

Lz4InputStream::~Lz4InputStream()
{
    LZ4F_freeDecompressionContext(mDecompressContext);
    delete[] mOutputBuffer;
}

bool Lz4InputStream::Next(const void **data, int *size)
{
    if (mFailed)
    {
        return false;
    }
    if (mOutputBufferRemains > 0)
    {
        // move data pointer
        int moveBias = mOutputBufferFilled - mOutputBufferRemains;
        *data = mOutputBuffer + moveBias;
        *size = mOutputBufferRemains;
        mOutputBufferRemains = 0; // assume will be fully consumed; unless call BackUp()
        return true;
    }
    Decompress();
    if (mOutputBufferRemains == 0)
    {
        *size = 0;
        //when false,no more data to read
        return (mFailed != true); // failure detect
    }
    *data = mOutputBuffer;
    *size = mOutputBufferFilled;
    mOutputBufferRemains = 0;// assume will be fully consumed;
    return true;
}

void Lz4InputStream::BackUp(int count)
{
    if (mFailed)
    {
        return;
    }
    if (mOutputBufferFilled - mOutputBufferRemains < count)
    {
        mFailed = true;
        util::OdpsThrow(INTERNAL_ERROR, "lz4InputStream BackUp failed: ");
    }
    mOutputBufferRemains += count;
}

bool Lz4InputStream::Skip(int count)
{
    if (mFailed)
    {
        return false;
    }
    if (count < 0)
    {
        return false;
    }
    const void* buf = nullptr;
    int bufSize = 0;
    int skipped = 0;
    while (skipped < count)
    {
        bool success = Next(&buf, &bufSize);
        if (!success)
        {
            return false;
        }
        skipped += bufSize;
    }
    if (skipped > count)
    {
        BackUp(skipped - count);
    }
    return true;
}

void Lz4InputStream::Decompress()
{
    size_t outputSize = 0;
    if ((int)mInputBufferOffset == mInputBufferSize)
    {
        ReadData();
        mInputBufferOffset = 0;
    }

    if (mInputBufferSize == 0)
    {
        //no more data to read
        return;
    }

    size_t srcSize = mInputBufferSize - mInputBufferOffset;
    size_t destSize = mOutputBufferSize;
    size_t ret = LZ4F_decompress(mDecompressContext,
                                mOutputBuffer,
                                &destSize,
                                (const char*)mInputBuffer + mInputBufferOffset,
                                &srcSize,
                                nullptr);
    if (LZ4F_isError(ret) != 0)
    {
        util::OdpsThrow(INTERNAL_ERROR, "LZ4 Decompression failed: " +
            std::string(LZ4F_getErrorName(ret)));
    }

    outputSize = destSize;
    mInputBufferOffset += srcSize;

    mOutputBufferRemains = outputSize;
    mOutputBufferFilled = outputSize;
    mByteCount += mOutputBufferFilled;
}

void Lz4InputStream::ReadData()
{
    do
    {
        bool success = mSubStream->Next(&mInputBuffer, &mInputBufferSize);
        if (!success)
        {
            mFailed = true;
            mInputBufferSize = 0;
            return;
        }
    }
    while (mInputBufferSize == 0);
}

Lz4OutputStream::Lz4OutputStream(ZeroCopyOutputStream* substream, int srcBufferSize):
    mSubStream(substream),
    mByteCount(0),
    mOutputSize(0),
    mInputBuffer(new char[srcBufferSize]),
    mInputFilled(0),
    mInputSize(srcBufferSize),
    mCompressTimer(0),
    mClosed(false),
    mNextOutputBuffer(nullptr),
    mNextOutputBufferSize(0),
    mNextOutputBufferFilled(0)
{
    size_t ret = LZ4F_createCompressionContext(&mCompressContext, LZ4F_VERSION);
    if (LZ4F_isError(ret) != 0)
    {
        util::OdpsThrow(INTERNAL_ERROR, "create LZ4 compressionContext failed: " +
            std::string(LZ4F_getErrorName(ret)));
    }
    mOutputSize = LZ4F_compressBound(srcBufferSize, nullptr);
    mOutputBuffer = new char[mOutputSize];
    WriteHeader();
}

Lz4OutputStream::~Lz4OutputStream()
{
    //close this object
    try
    {
        Close();
    }
    catch (const std::exception& e)
    {
        LOG_INFO(sLogger, ("Lz4OutputClose", "Failed") ("Exception", e.what()));
    }
    catch (...)
    {
        LOG_INFO(sLogger, ("Lz4OutputClose", "Failed") ("EXCEPTION", "UNKNOWN"));
    }
}

bool Lz4OutputStream::Next(void **data, int *size)
{
    if (mClosed)
    {
        return false;
    }
    if (mInputFilled > 0)
    {
        // last buffer was written. flush to substream
        CompressUpdate();
    }
    *data = mInputBuffer;
    *size = mInputSize;
    // we assume the user will fill the input
    mInputFilled = mInputSize;
    return true;
}

void Lz4OutputStream::BackUp(int count)
{
    if (mClosed)
    {
        return;
    }
    if (count < 0 || count > mInputFilled)
    {
        // Illegal input
        util::OdpsThrow(INTERNAL_ERROR, "invalid backup count");
    }
    mInputFilled -= count;
}

void Lz4OutputStream::CompressUpdate()
{
    if (mClosed)
    {
        return;
    }
    //data will be buffered or compressed
    size_t ret = LZ4F_compressUpdate(mCompressContext, mOutputBuffer,
                    mOutputSize, mInputBuffer, mInputFilled, nullptr);
    if (LZ4F_isError(ret) != 0) {
        util::OdpsThrow(INTERNAL_ERROR, "LZ4 Compression failed: " +
            std::string(LZ4F_getErrorName(ret)));
    }
    //ret = 0 means data is buffered in mCompressContext
    WriteBytes(mOutputBuffer, ret);
    mByteCount += ret;
    //input data will be fully comsumed or buffered
    mInputFilled = 0;
}

void Lz4OutputStream::WriteBytes(const char* bytes, int len)
{
    if (mClosed)
    {
        return;
    }
    if (nullptr == bytes || len < 0)
    {
        util::OdpsThrow(INTERNAL_ERROR, "WriteBytes parament is invalid ");
    }
    if (len > 0)
    {
        const char* bytesEnd = bytes + len;
        while (bytes < bytesEnd)
        {
            if (mNextOutputBuffer == nullptr 
                || mNextOutputBufferFilled == mNextOutputBufferSize)
            {
                if (!mSubStream->Next((void **)&mNextOutputBuffer, &mNextOutputBufferSize))
                {
                    util::OdpsThrow(INTERNAL_ERROR, "Get Buffer Failed");
                }
                mNextOutputBufferFilled = 0;
            }
            int writeSize = min(len, mNextOutputBufferSize - mNextOutputBufferFilled);
            memcpy(mNextOutputBuffer + mNextOutputBufferFilled, bytes, writeSize);
            bytes += writeSize;
            len -= writeSize;
            mNextOutputBufferFilled += writeSize;
        }
    }
}

void Lz4OutputStream::WriteHeader()
{
    if (mClosed)
    {
        return;
    }
    size_t ret = LZ4F_compressBegin(mCompressContext, mOutputBuffer, mOutputSize, nullptr);
    if (LZ4F_isError(ret) != 0)
    {
        util::OdpsThrow(INTERNAL_ERROR, "Failed to start LZ4 compression: " +
            std::string(LZ4F_getErrorName(ret)));
    }
    WriteBytes(mOutputBuffer, ret);
    mByteCount += ret;
}

void Lz4OutputStream::WriteFooter()
{
    if (mClosed)
    {
        return;
    }
    size_t ret = LZ4F_compressEnd(mCompressContext, mOutputBuffer, mOutputSize, nullptr);
    if (LZ4F_isError(ret) != 0)
    {
        util::OdpsThrow(INTERNAL_ERROR, "Failed to end LZ4 compression: " +
            std::string(LZ4F_getErrorName(ret)));
    }
    WriteBytes(mOutputBuffer, ret);
    mByteCount += ret;
}

void Lz4OutputStream::Flush()
{
    if (mClosed)
    {
        return;
    }
    if (mInputFilled > 0)
    {
        //flush mInputFilled data
        CompressUpdate();
    }
    //flush data buffered in mCompressContext
    size_t ret = LZ4F_flush(mCompressContext, mOutputBuffer,
                   mOutputSize, nullptr);
    if (LZ4F_isError(ret) != 0)
    {
       util::OdpsThrow(INTERNAL_ERROR, "Failed to flush LZ4 buffer: " +
           std::string(LZ4F_getErrorName(ret)));
    }
    WriteBytes(mOutputBuffer, ret);
    mByteCount += ret;
}

void Lz4OutputStream::Close()
{
    if (mClosed)
    {
        return;
    }
    if (mInputFilled > 0)
    {
        //Flush()
        CompressUpdate();
    }
    //WriteFooter will flush the data remained,call LZ4F_flush
    WriteFooter();
    LZ4F_freeCompressionContext(mCompressContext);

    int backCount = mNextOutputBufferSize - mNextOutputBufferFilled;
    if (backCount > 0)
    {
        mSubStream->BackUp(backCount);
    }

    delete[] mInputBuffer;
    delete[] mOutputBuffer;
    mClosed = true;
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
