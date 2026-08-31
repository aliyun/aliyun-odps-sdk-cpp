#include "zstd_stream.h"
#include <string>
#include <zstd.h>
#include "common/logging.h"
#include "odps_exception.h"
#include <iostream>
#include "util/utils.h"

using namespace std;
using namespace apsara::odps::sdk;

namespace google { namespace protobuf { namespace io {


ZstdOutputStream::ZstdOutputStream(ZeroCopyOutputStream* substream, int compressLevel, int bufferSize, bool blockMode):
    mSubStream(substream),
    mByteCount(0),
    mCompressContext(ZSTD_createCCtx()),
    mOutputBuffer(nullptr),
    mOutputFilled(0),
    mOutputSize(0),
    mInputBuffer(new char[bufferSize]),
    mInputFilled(0),
    mInputSize(bufferSize),
    mBlockMode(blockMode),
    mFinished(false)
{
    if (compressLevel > ZSTD_maxCLevel() || compressLevel < ZSTD_minCLevel())
    {
        util::OdpsThrow(INVALID_ARGUMENT,
            "Specified zstd compress level", compressLevel, "is not supported.",
            "Supported range:", ZSTD_minCLevel(), "~", ZSTD_maxCLevel());
    }
    ZSTD_CCtx_setParameter(mCompressContext, ZSTD_c_compressionLevel, compressLevel);
}

ZSTD_inBuffer ZstdOutputStream::CreateInBuffer()
{
    ZSTD_inBuffer input = {0};
    input.src = mInputBuffer;
    input.size = mInputFilled;
    input.pos = 0;
    return input;
}

ZSTD_outBuffer ZstdOutputStream::CreateOutBuffer()
{
    ZSTD_outBuffer output = {0};
    output.dst = mOutputBuffer;
    output.size = mOutputSize;
    output.pos = mOutputFilled;
    return output;
}

void ZstdOutputStream::Allocate()
{
    do
    {
        bool success = mSubStream->Next(&mOutputBuffer, &mOutputSize);
        if (!success)
        {
            mFinished = true;
            return;
        }
    }
    while(mOutputSize == 0);
    mOutputFilled = 0;
}

void ZstdOutputStream::Continue()
{
    if (mFinished) return;
    if (mOutputBuffer == nullptr) Allocate();
    if (mInputFilled == 0) return;
    ZSTD_inBuffer input = CreateInBuffer();
    ZSTD_outBuffer output = CreateOutBuffer();

    size_t ret = -1;
    while (!mBlockMode ? (input.pos != input.size) : (ret != 0)) //flush whole frame on block mode
    {
        ret = ZSTD_compressStream2(mCompressContext, &output, &input, !mBlockMode ? ZSTD_e_continue : ZSTD_e_end);
        if (ZSTD_isError(ret))
        {
            mFinished = true;
            util::OdpsThrow(INTERNAL_ERROR, "zstd compression failed:", std::string(ZSTD_getErrorName(ret)));
        }
        // check output filled
        if (output.size == output.pos)
        {
            Allocate();
            if (mFinished)
            {
                return;
            }
            output = CreateOutBuffer();
        }
        mByteCount += ret;
    }

    mInputFilled = 0;
    // update output booking
    mOutputFilled = output.pos;
}

bool ZstdOutputStream::Next(void **data, int *size)
{
    if (mFinished)
    {
        return false;
    }

    if (mOutputBuffer == nullptr)
    {
        Allocate();
    }

    if (mInputFilled != 0)
    {
        // last buffer was written. flush to substream
        Continue();
        if (mFinished)
        {
            return false;
        }
    }
    *data = mInputBuffer;
    *size = mInputSize;
    // we assume the user will fill the input
    mInputFilled = mInputSize;
    return true;
}

void ZstdOutputStream::BackUp(int count)
{
    if (mFinished)
    {
        return;
    }
    if (count < 0 || count > mInputFilled)
    {
        // Illegal input
        util::OdpsThrow(INTERNAL_ERROR, "zstd stream backup count invalid");
    }
    mInputFilled -= count;
    Continue();
}

void ZstdOutputStream::Flush()
{
    if (mFinished || mOutputBuffer == nullptr)
    {
        return;
    }

    ZSTD_inBuffer input = CreateInBuffer();
    ZSTD_outBuffer output = CreateOutBuffer();

    if (!mBlockMode || input.size > 0)
    {
        while (true)
        {
            size_t ret = ZSTD_compressStream2(mCompressContext, &output, &input, ZSTD_e_end);
            if (ZSTD_isError(ret))
            {
                mFinished = true;
                util::OdpsThrow(INTERNAL_ERROR, "zstd compression failed:", std::string(ZSTD_getErrorName(ret)));
            }

            if (ret == 0)
            {
                break;
            }
            mByteCount += ret;

            // check output filled
            if (output.size == output.pos)
            {
                Allocate();
                output = CreateOutBuffer();
            }
        }
    }

    mInputFilled = 0;
    int backupCount = output.size - output.pos;
    mSubStream->BackUp(backupCount);
}

ZstdOutputStream::~ZstdOutputStream()
{
    Flush(); // flush all remaining data to underlying stream
    ZSTD_freeCCtx(mCompressContext);
    delete[] mInputBuffer;
}

ZstdInputStream::ZstdInputStream(ZeroCopyInputStream* subStream, int bufferSize):
    mSubStream(subStream),
    mByteCount(0),
    mDecompressContext(ZSTD_createDStream()),
    mUnderlyingFailed(false),
    mFailed(false),
    mOutputBuffer(new char[bufferSize]),
    mOutputBufferSize(bufferSize),
    mOutputBufferFilled(0),
    mOutputBufferRemains(0),
    mInputBuffer(nullptr),
    mInputBufferSize(0),
    mInputBufferConsumed(0)
{
}

void ZstdInputStream::Refill()
{
    do
    {
        bool success = mSubStream->Next(&mInputBuffer, &mInputBufferSize);
        if (!success)
        {
            mUnderlyingFailed = true;
            mInputBufferConsumed = 0;
            return;
        }
    }
    while(mInputBufferSize == 0);
    mInputBufferConsumed = 0;
}

ZSTD_inBuffer ZstdInputStream::CreateInBuffer()
{
    ZSTD_inBuffer input;
    input.src = mInputBuffer;
    input.size = mInputBufferSize;
    input.pos = mInputBufferConsumed;
    return input;
}

ZSTD_outBuffer ZstdInputStream::CreateOutBuffer()
{
    ZSTD_outBuffer output;
    output.dst = mOutputBuffer;
    output.size = mOutputBufferSize;
    output.pos = 0;
    return output;
}

void ZstdInputStream::Continue()
{
    if (mFailed)
    {
        return;
    }
    if (mInputBuffer == nullptr || mInputBufferConsumed == mInputBufferSize)
    {
        Refill();
    }
    // outputbuffer should be fully consumed, we are safe to rewrite
    // condition: underlying failed(no more data) or failed(zstd decomp failure)
    if (mUnderlyingFailed)
    {
        // no more data, but we need to fully extract remaining in stream
        ZSTD_outBuffer output = CreateOutBuffer();
        ZSTD_inBuffer input = {0};
        size_t ret = ZSTD_decompressStream(mDecompressContext, &output, &input);
        if (ZSTD_isError(ret))
        {
            mFailed = true;
            util::OdpsThrow(INTERNAL_ERROR, "zstd Decompression failed:", std::string(ZSTD_getErrorName(ret)));
        }
        if (ret == 0 || ZSTD_isError(ret) || output.pos == 0)
        {
            mFailed = true;
            mOutputBufferRemains = 0;
            return;
        }
        // update output info
        mOutputBufferRemains = output.pos;
        mOutputBufferFilled = output.pos;
        mByteCount += mOutputBufferFilled;
    }
    else
    {
        ZSTD_outBuffer output = CreateOutBuffer();
        ZSTD_inBuffer input = CreateInBuffer();
        size_t ret = ZSTD_decompressStream(mDecompressContext, &output, &input);
        if (ZSTD_isError(ret))
        {
            mFailed = true;
            mOutputBufferRemains = 0;
            util::OdpsThrow(INTERNAL_ERROR, "zstd Decompression failed:", std::string(ZSTD_getErrorName(ret)));
        }
        // update input info
        mInputBufferConsumed = input.pos;
        // update output info
        mOutputBufferRemains = output.pos;
        mOutputBufferFilled = output.pos;
        mByteCount += mOutputBufferFilled;
    }
}

bool ZstdInputStream::Next(const void **data, int *size)
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
    Continue();
    if (mOutputBufferRemains == 0)
    {
        *size = 0;
        return (mFailed != true); // failure detect
    }
    *data = mOutputBuffer;
    *size = mOutputBufferFilled;
    mOutputBufferRemains = 0; // assume will be fully consumed;
    return true;
}

ZstdInputStream::~ZstdInputStream()
{
    ZSTD_freeDStream(mDecompressContext);
    delete[] mOutputBuffer;
}

void ZstdInputStream::BackUp(int count)
{
    if (mOutputBufferFilled - mOutputBufferRemains < count)
    {
        return;
    }
    mOutputBufferRemains += count;
}

bool ZstdInputStream::Skip(int count)
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
    int bufsz = 0;
    int skipped = 0;
    while(skipped < count)
    {
        bool success = Next(&buf, &bufsz);
        if (!success)
        {
            return false;
        }
        skipped += bufsz;
    }
    if (skipped > count)
    {
        BackUp(skipped - count);
    }
    return true;
}


} } }
