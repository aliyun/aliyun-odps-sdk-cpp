#include "record_reader.h"
#include "connection_manager.h"
#include "download.h"
#include "util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

RecordReader::RecordReader(ODPSTableSchemaPtr s, const ConnectionManagerPtr& connManager, bool const compress, CompressOption* option)
    : mSchema(s), in(NULL), mClosed(false)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    mConn = connManager->OpenReaderConnection();
    in = new HttpInputStream(mConn->GetResponse(),64*1024);
    mDeserializer = ProtoDeserializerPtr(new ProtoDeserializer(in, mSchema.get(), compress, false, DEFAULT_TOTAL_BYTES_LIMIT, option));
}

RecordReader::~RecordReader()
{
    Close();
}

bool RecordReader::Read(ODPSTableRecord& r)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    bool ret = mDeserializer->Deserialize(r);
    return ret;
}

void RecordReader::Close()
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost );
    if (!mClosed)
    {
        mMetrics.ServerIOCost += mDeserializer->GetServerIOCost();
        mMetrics.ServerTotalCost += mDeserializer->GetServerTotalCost();
        mMetrics.PanguIOCost += mDeserializer->GetPanguIOCost();
        mMetrics.RateLimitCost += mDeserializer->GetRateLimitCost();

        mDeserializer.reset();
        if (in) {
            delete in;
            in = NULL;
        }
        mConn->Close();
        mMetrics.ClientIOCost = mConn->GetIOCost();
        mClosed = true;
    }
}

std::string RecordReader::GetMetrics()
{
    return GetTunnelMetrics(mMetrics);
}


BufferRecordReader::BufferRecordReader (const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames, const CompressOption& option, const DownloadInfo downloadInfo, bool disableModifiedCheck)
 : mStart(start), mCount(count), mBufferRecordCount(bufferRecordCount), mColNames(colNames) , mOption(option), mDownloadInfo(downloadInfo), mDisableModifiedCheck(disableModifiedCheck)
{
    if (mBufferRecordCount == 0)
    {
        mBufferRecordCount = 1;
    }
    mDownload = std::dynamic_pointer_cast<IDownload>(DownloadPtr(new Download(mDownloadInfo.conf, mDownloadInfo.project, mDownloadInfo.table, mDownloadInfo.partition, mDownloadInfo.downloadId, mDownloadInfo.schemaName)));
}

BufferRecordReader::~BufferRecordReader()
{
    Close();
}

ODPSTableRecordPtr BufferRecordReader::ReadWithRetry(uint64_t retryTimes)
{
    return apsara::odps::sdk::internal::tunnel::ReadWithRetry(retryTimes, [this]() { return Read(); });
}

ODPSTableRecordPtr BufferRecordReader::Read()
{
    if (mRecordBuffer.size() == 0)
    {
        ReadData();
        if (mRecordBuffer.size() == 0)
        {
            return nullptr;
        }
    }
    ODPSTableRecordPtr _r = mRecordBuffer.front();
    mRecordBuffer.pop_front();
    return _r;
}

void BufferRecordReader::Close()
{
}

void BufferRecordReader::ReadData()
{
    if (mCount <= 0) return;
    uint64_t recordNum = std::min(mCount, mBufferRecordCount);
    IRecordReaderPtr reader = mDownload->OpenReader(mStart, recordNum, mColNames, mOption, mDisableModifiedCheck);
    mStart += recordNum;
    mCount -= recordNum;

    ODPSTableRecordPtr _r = reader->CreateBufferRecord();

    while (mRecordBuffer.size() < mBufferRecordCount && reader->Read(*_r)) {
        mRecordBuffer.push_back(_r);
        _r = reader->CreateBufferRecord();
    }
    reader->Close();
}