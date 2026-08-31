#include "record_writer.h"
#include "common/http_connection.h"
#include "common/logging.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;

static apsara::odps::sdk::logging::Logger* sLogger = apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

RecordWriter::RecordWriter(ODPSTableSchemaPtr s, const ConnectionManagerPtr& connManager, bool const compress, CompressOption* option)
    : mSchema(s),mOutput(NULL)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    int bufferSize = std::max(64*1024, option->bsize);
    mConn = connManager->OpenWriterConnection();
    mOutput = new HttpOutputStream(mConn, bufferSize);
    mSerializer = ProtoSerializerPtr(new ProtoSerializer(mOutput, mSchema.get(), compress, false, DEFAULT_TOTAL_BYTES_LIMIT, option));
    LOG_DEBUG(sLogger,("conn code",mConn->GetResponse()->GetStatusCode()));
}

RecordWriter::~RecordWriter()
{
    mSerializer.reset();
    if (mOutput) delete mOutput;
}

bool RecordWriter::Write(const ODPSTableRecord& r)
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    bool ret = mSerializer->Serialize(r);
    return ret;
}

void RecordWriter::Close()
{
    TimerGuard timerGuard(mMetrics.ClientProcessCost);
    LOG_DEBUG(sLogger,("Start to Close Record Writer.",""));
    mSerializer->Complete();
    mOutput->Flush();
    mConn->CloseUpstream();

    ResponsePtr resp = mConn->GetResponse();
    ostringstream oss;
    char buff[512];
    int64_t bytes;
    while(0 != (bytes = resp->ReadBody(buff, 512))) {
        oss << string(buff,bytes);
    }
    if (!resp->isSuccessful()) {
        LOG_ERROR(sLogger,("close record writer failed", oss.str()));
        resp->GetError(oss.str());
    }
    mTraceId = resp->GetHeader(HEADER_ODPS_REQUEST_ID);
    std::string metrics = resp->GetHeader(HEADER_ODPS_TUNNEL_METRICS);
    if(!metrics.empty())
    {
        Metrics serverMetrics;
        FromJsonString(serverMetrics, metrics);
        mMetrics.ServerIOCost += serverMetrics.ServerIOCost;
        mMetrics.ServerTotalCost += serverMetrics.ServerTotalCost;
        mMetrics.PanguIOCost += serverMetrics.PanguIOCost;
        mMetrics.RateLimitCost += serverMetrics.RateLimitCost;
    }
    mResponseSize = bytes;
    LOG_DEBUG(sLogger,("Close Record Writer OK.", mOutput->ByteCount()));
    mConn->Close();
    mMetrics.ClientIOCost = mConn->GetIOCost();
}

std::string RecordWriter::GetMetrics()
{
    return GetTunnelMetrics(mMetrics);
}