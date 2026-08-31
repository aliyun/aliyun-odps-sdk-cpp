#ifndef APSARA_ODPS_TUNNEL_INTERNAL_ARROW_READER_H
#define APSARA_ODPS_TUNNEL_INTERNAL_ARROW_READER_H
#ifdef ODPS_SDK_ENABLE_ARROW
#include "odps_tunnel.h"
#include "common/http_connection.h"
#include "connection_manager.h"
#include "arrow_http_stream.h"
#include "serialize.h"
#include "record_reader.h"

#include "google/protobuf/stubs/common.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/wire_format_lite_inl.h"
#include "google/protobuf/io/gzip_stream.h"

using namespace google::protobuf::internal;
using namespace google::protobuf::io;

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{


class ArrowRecordReader : public IArrowRecordReader
{
public:
    ArrowRecordReader(std::shared_ptr<arrow::Schema> schema, const ConnectionManagerPtr& connManager, const CompressOption& compress);
    virtual ~ArrowRecordReader();
    virtual bool Read(std::shared_ptr<arrow::RecordBatch>& r);
    virtual void Close();

private:
    std::shared_ptr<arrow::Schema> mSchema;
    HttpConnectionPtr mConn;
    std::shared_ptr<IArrowInputStream> mInputStream;
    bool mClosed;
    HttpInputStreamPtr mHttpInputStreamPtr;
};

typedef std::shared_ptr<ArrowRecordReader> ArrowRecordReaderPtr;

class BufferArrowRecordReader : public IBufferArrowRecordReader
{
public:
    BufferArrowRecordReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const uint64_t rawSize, const std::vector<std::string>& colNames, const CompressOption& option, const DownloadInfo downloadInfo, bool disableModifiedCheck);
    virtual ~BufferArrowRecordReader();

    virtual std::shared_ptr<arrow::RecordBatch> Read() override;
    virtual std::shared_ptr<arrow::RecordBatch> ReadWithRetry(uint64_t retryTimes) override;

private:
    uint64_t mStart;
    uint64_t mCount;
    uint64_t mBufferRecordCount;
    uint64_t mRawSize;
    std::vector<std::string> mColNames;
    CompressOption mOption;
    DownloadInfo mDownloadInfo;
    bool mDisableModifiedCheck;

    IDownloadPtr mDownload;
    std::shared_ptr<arrow::RecordBatch> mRecordBuffer;  // Cache one RecordBatch, server controls the size based on mBufferRecordCount and mRawSize

    std::shared_ptr<arrow::RecordBatch> ReadData();
};

typedef std::shared_ptr<BufferArrowRecordReader> BufferArrowRecordReaderPtr;

}}}}}

#endif
#endif
