#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_http_stream.h"

using namespace apsara::odps::sdk::internal::tunnel;

ZeroCopyStreamReader::ZeroCopyStreamReader(std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> input):
    mInput(input) {}

int64_t ZeroCopyStreamReader::Read(char* dstbuf, int64_t size)
{
    // read until sufficient
    int64_t read = 0;
    while(read < size)
    {
        const void* buffer = nullptr;
        int buffersize = 0;
        do
        {
            if (mInput->Next(&buffer, &buffersize) == false)
            {
                return read == 0 ? -1 : read;
            }
        }
        while(buffer == nullptr);
        int64_t shouldRead = (int64_t(buffersize) > (size - read)) ? (size - read) : buffersize;
        memcpy(dstbuf, buffer, shouldRead);
        dstbuf += shouldRead;
        read += shouldRead;
        if (shouldRead < int64_t(buffersize))
        {
            mInput->BackUp(buffersize - int(shouldRead));
        }
    }
    return read;
}

ZeroCopyStreamWriter::ZeroCopyStreamWriter(std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> output):
    mOutput(output) {}

int64_t ZeroCopyStreamWriter::Write(char* srcbuf, int64_t size)
{
    int64_t written = 0;
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
        int64_t shouldWrite = (int64_t(buffersize) > (size - written)) ? (size - written) : buffersize;
        memcpy(buffer, srcbuf, shouldWrite);
        srcbuf += shouldWrite;
        written += shouldWrite;
        if (shouldWrite < int64_t(buffersize))
        {
            mOutput->BackUp(buffersize - int(shouldWrite));
        }
    }
    return written;
}

#endif