#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_reader.h"
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include <iostream>
#include "serialize.h"
#include "lz4_stream.h"
#include "download.h"
#include "util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

ArrowRecordReader::ArrowRecordReader(std::shared_ptr<arrow::Schema> schema,
                                     const ConnectionManagerPtr& connManager, const CompressOption& compress):
    mSchema(schema),
    mClosed(false)
{
    mConn = connManager->OpenReaderConnection();
    if(compress.algorithm != CompressOption::CompressAlgorithm::ODPS_LZ4)
    {
        mInputStream.reset(new ArrowInputStream<HttpConnection>(mConn));
    }
    else
    {
        mHttpInputStreamPtr = std::make_shared<HttpInputStream>(mConn->GetResponse(), 64 * 1024);
        std::shared_ptr<Lz4InputStream> lz4Stream = std::make_shared<Lz4InputStream>(mHttpInputStreamPtr.get());
        mInputStream.reset(new ArrowInputStream<ZeroCopyStreamReader>(new ZeroCopyStreamReader(lz4Stream)));
    }
}

ArrowRecordReader::~ArrowRecordReader()
{
    Close();
}

bool ArrowRecordReader::Read(std::shared_ptr<arrow::RecordBatch>& r)
{
    auto&& options = arrow::ipc::IpcReadOptions::Defaults();
    auto result = arrow::ipc::ReadRecordBatch(mSchema, nullptr, options, mInputStream.get());
    if (result.ok() && (r = result.ValueUnsafe()))
    {
        return true;
    }
    else
    {
        if (mInputStream->IsEOF())
        {
            return false;
        }
        else
        {
            throw OdpsTunnelException("ArrowHttpInputStream Deserialize Exception");
        }
    }
}

void ArrowRecordReader::Close()
{
    if (!mClosed)
    {
        mConn->Close();
        mClosed = true;
    }
}

BufferArrowRecordReader::BufferArrowRecordReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const uint64_t rawSize, const std::vector<std::string>& colNames, const CompressOption& option, const DownloadInfo downloadInfo, bool disableModifiedCheck)
 : mStart(start), mCount(count), mBufferRecordCount(bufferRecordCount), mRawSize(rawSize), mColNames(colNames), mOption(option), mDownloadInfo(downloadInfo), mDisableModifiedCheck(disableModifiedCheck)
{
    mDownload = std::dynamic_pointer_cast<IDownload>(DownloadPtr(new Download(mDownloadInfo.conf, mDownloadInfo.project, mDownloadInfo.table, mDownloadInfo.partition, mDownloadInfo.downloadId, mDownloadInfo.schemaName)));
}

BufferArrowRecordReader::~BufferArrowRecordReader()
{
}

std::shared_ptr<arrow::RecordBatch> BufferArrowRecordReader::ReadWithRetry(uint64_t retryTimes)
{
    return apsara::odps::sdk::internal::tunnel::ReadWithRetry(retryTimes, [this]() { return Read(); });
}

std::shared_ptr<arrow::RecordBatch> BufferArrowRecordReader::Read()
{
    if (!mRecordBuffer)
    {
        mRecordBuffer = ReadData();
        if (!mRecordBuffer)
        {
            return nullptr;
        }
    }
    return std::move(mRecordBuffer);
}

std::shared_ptr<arrow::RecordBatch> BufferArrowRecordReader::ReadData()
{
    if (mCount <= 0) return nullptr;

    uint64_t requestCount = mBufferRecordCount > 0 ? std::min(mCount, mBufferRecordCount) : mCount;

    // Server will control the returned data size based on requestCount and rawSize
    // Read single RecordBatch - server ensures it doesn't exceed rawSize
    IArrowRecordReaderPtr reader = mDownload->OpenArrowReader(mStart, requestCount, mColNames, mOption, mDisableModifiedCheck, mRawSize);

    // If the requested bufferRecordCount records exceed rawSize, server will return fewer records
    std::shared_ptr<arrow::RecordBatch> batch;
    if (reader->Read(batch) && batch)
    {
        // Update mStart and mCount based on actual records read (may be less than requestCount if rawSize limit applies)
        uint64_t actualRecordsRead = batch->num_rows();
        mStart += actualRecordsRead;  // Next read will start from this offset
        mCount -= actualRecordsRead;   // Remaining records to read
        reader->Close();
        return batch;
    }

    reader->Close();
    return nullptr;
}

#endif