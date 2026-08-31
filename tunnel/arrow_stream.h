#ifndef APSARA_ODPS_TUNNEL_ARROW_STREAM_H
#define APSARA_ODPS_TUNNEL_ARROW_STREAM_H
#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow/io/interfaces.h"
#include "arrow/result.h"
#include "arrow/status.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include <memory>

namespace apsara {
namespace odps {
namespace tunnel {
namespace serialize {

//-------Interface---------

class IArrowInputStream : public arrow::io::InputStream
{
public:
    virtual bool IsEOF() = 0;
    virtual arrow::Status Close() override = 0;
    virtual arrow::Result<int64_t> Tell() const override = 0;
    virtual arrow::Result<int64_t> Read(int64_t nbytes, void* out) override = 0;
    arrow::Result<std::shared_ptr<arrow::Buffer>> Read(int64_t nbytes) override;
};
using IArrowInputStreamPtr = std::shared_ptr<IArrowInputStream>;

class IArrowOutputStream: public arrow::io::OutputStream
{
public:
    virtual arrow::Status Write(const void* data, int64_t bytes) = 0;
    virtual arrow::Status Flush() = 0;
    virtual arrow::Status Close() = 0;
    virtual arrow::Result<int64_t> Tell() const = 0;
};
using IArrowOutputStreamPtr = std::shared_ptr<IArrowOutputStream>;

class IArrowStreamReader
{
public:
    virtual ~IArrowStreamReader() {}
    virtual int64_t Read(char* dstbuf, int64_t size) = 0;
};
using IArrowStreamReaderPtr = std::shared_ptr<IArrowStreamReader>;

class IArrowStreamWriter
{
public:
    virtual ~IArrowStreamWriter() {}
    virtual int64_t Write(const void* srcbuf, int64_t size) = 0;
};
using IArrowStreamWriterPtr = std::shared_ptr<IArrowStreamWriter>;

//-------Implementation---------

class ArrowInputStream : public IArrowInputStream
{
public:
    ArrowInputStream(const IArrowStreamReaderPtr& reader)
        : mReader(reader)
    {
    }

    virtual ~ArrowInputStream()
    {
    }

public:
    bool IsEOF() override
    {
        return mEOF;
    }

    arrow::Status Close() override
    {
        mClosed = true;
        return arrow::Status::OK();
    }

    arrow::Result<int64_t> Tell() const override
    {
        return arrow::Result<int64_t>(mPos);
    }

    bool closed() const override
    {
        return mClosed;
    }

    arrow::Result<int64_t> Read(int64_t nbytes, void* out) override;

private:
    bool mEOF = false;
    bool mClosed = false;
    int64_t mPos = 0;
    IArrowStreamReaderPtr mReader;
};

class ArrowOutputStream : public IArrowOutputStream
{
public:
    ArrowOutputStream(const IArrowStreamWriterPtr& writer)
        : mWriter(writer)
    {
    }

    virtual ~ArrowOutputStream()
    {
    }

public:
    arrow::Status Write(const void* data, int64_t bytes) override;

    arrow::Status Flush() override
    {
        return arrow::Status::OK();
    }

    arrow::Status Close() override
    {
        mClosed = true;
        return arrow::Status::OK();
    }

    arrow::Result<int64_t> Tell() const override
    {
        return arrow::Result<int64_t>(mPos);
    }

    bool closed() const override
    {
        return mClosed;
    }

private:
    int64_t mPos = 0;
    bool mClosed = false;
    bool mError = false;
    IArrowStreamWriterPtr mWriter;
};

class ZeroCopyStreamReader : public IArrowStreamReader
{
public:
    ZeroCopyStreamReader(
        std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> input)
        : mInput(input)
    {
    }

    virtual ~ZeroCopyStreamReader()
    {
    }

public:
    int64_t Read(char* dstbuf, int64_t size) override;

private:
    std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> mInput;
};


class ZeroCopyStreamWriter : public IArrowStreamWriter
{
public:
    ZeroCopyStreamWriter(
        std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> output)
        : mOutput(output)
    {
    }

    virtual ~ZeroCopyStreamWriter()
    {
    }

public:
    int64_t Write(const void* srcbuf, int64_t size) override;

private:
    std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> mOutput;
};

}
}
}
}

#endif  // ODPS_SDK_ENABLE_ARROW
#endif
