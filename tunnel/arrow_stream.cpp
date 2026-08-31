#ifdef ODPS_SDK_ENABLE_ARROW

#include "arrow_stream.h"
#include "arrow/buffer.h"
#include "util/utils.h"

using namespace std;
using namespace apsara::odps::tunnel::serialize;
using namespace apsara::odps::sdk::util;

namespace {
class TunnelArrowBuffer : public arrow::MutableBuffer
{
public:
    TunnelArrowBuffer(int64_t size): MutableBuffer(new uint8_t[size], size){}
    ~TunnelArrowBuffer() override
    {
        delete [] data_;
    }
};
}

arrow::Result<std::shared_ptr<arrow::Buffer>> IArrowInputStream::Read(int64_t nbytes)
{
    std::shared_ptr<arrow::Buffer> const out = std::make_shared<TunnelArrowBuffer>(nbytes);

    int64_t remainBytes = nbytes;
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

arrow::Result<int64_t> ArrowInputStream::Read(int64_t nbytes, void* out)
{
    if (mClosed)
    {
        return arrow::Status::Invalid("Stream Closed");
    }

    if (mEOF)
    {
        return arrow::Status::Invalid("EOF reached");
    }

    if (nbytes < 0)
    {
        return arrow::Status::Invalid("Cannot read a negative number of bytes from BufferReader.");
    }

    int64_t const readBytes = mReader->Read((char*)out, nbytes);
    if (readBytes <= 0)
    {
        mEOF = true;
        return arrow::Result<int64_t>(0);
    }
    else
    {
        mPos += readBytes;
    }

    return arrow::Result<int64_t>(readBytes);
}

arrow::Status ArrowOutputStream::Write(const void* data, int64_t bytes)
{
    if (mClosed)
    {
        return arrow::Status::IOError("OutputStream is closed");
    }

    if (mError)
    {
        return arrow::Status::IOError("OutputStream is in error state");
    }

    int64_t const writtenBytes = mWriter->Write(data, bytes);
    if (writtenBytes != bytes)
    {
        mError = true;
        return arrow::Status::IOError("OutputStream write failed.");
    }
    else
    {
        mPos += writtenBytes;
    }

    return arrow::Status::OK();
}

int64_t ZeroCopyStreamReader::Read(char* dstbuf, int64_t size)
{
    const void* buffer = nullptr;
    int buffersize = 0;
    if (mInput->Next(&buffer, &buffersize) == false)
    {
        return 0;
    }

    int64_t const read = std::min(int64_t(buffersize), size);
    memcpy(dstbuf, buffer, read);
    if (read < int64_t(buffersize))
    {
        mInput->BackUp(buffersize - read);
    }
    return read;
}

int64_t ZeroCopyStreamWriter::Write(const void* srcbuf, int64_t size)
{
    int64_t written = 0;
    const char* src = (const char*)srcbuf;
    while(written < size)
    {
        void* buffer = nullptr;
        int buffersize = 0;
        do
        {
            if (mOutput->Next(&buffer, &buffersize) == false)
            {
                return -1; // failed.
            }
        }
        while(buffer == nullptr);
        int64_t const shouldWrite = (int64_t(buffersize) > (size - written)) ? (size - written) : buffersize;
        memcpy(buffer, src, shouldWrite);
        src += shouldWrite;
        written += shouldWrite;
        if (shouldWrite < int64_t(buffersize))
        {
            mOutput->BackUp(buffersize - int(shouldWrite));
        }
    }
    return written;
}
#endif  // ODPS_SDK_ENABLE_ARROW
