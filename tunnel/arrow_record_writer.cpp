#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_writer.h"
#include "arrow_util.h"
#include "common/logging.h"
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include "serialize.h"
#include "lz4_stream.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

ArrowRecordWriter::ArrowRecordWriter(const ConnectionManagerPtr& connManager, const CompressOption& compress):
    mCompress(compress)
{
    mConn = connManager->OpenWriterConnection();
    if(compress.algorithm != CompressOption::CompressAlgorithm::ODPS_LZ4)
    {
        mOutput.reset(new ArrowOutputStream<HttpConnection>(mConn));
    }
    else
    {
        mHttpOutputStreamPtr = std::make_shared<HttpOutputStream>(mConn, 64 * 1024);
        std::shared_ptr<Lz4OutputStream> lz4Stream = std::make_shared<Lz4OutputStream>(mHttpOutputStreamPtr.get());
        mOutput.reset(new ArrowOutputStream<ZeroCopyStreamWriter>(new ZeroCopyStreamWriter(lz4Stream), 64 * 1024));
    }
}

ArrowRecordWriter::~ArrowRecordWriter()
{
}

bool ArrowRecordWriter::Write(const arrow::RecordBatch& r)
{
    arrow::Status status;
    const auto& options = GetWriteOptions(mCompress);
    status = arrow::ipc::SerializeRecordBatch(r, options, mOutput.get());
    if (status.ok())
    {
        return true;
    }
    else
    {
        throw OdpsTunnelException("ArrowHttpOutputStream Serialize Exception");
    }
}

void ArrowRecordWriter::Close()
{
    mOutput->Flush();
    mOutput.reset();
    if(mHttpOutputStreamPtr != nullptr)
    {
        mHttpOutputStreamPtr->Flush();
        mHttpOutputStreamPtr.reset();
    }
    mConn->CloseUpstream();

    ResponsePtr resp = mConn->GetResponse();
    ostringstream oss;
    char buff[512];
    int64_t bytes;
    while(0 != (bytes = resp->ReadBody(buff, 512)))
    {
        oss << string(buff,bytes);
    }

    if (!resp->isSuccessful())
    {
        resp->GetError(oss.str());
    }

    mConn->Close();
}
#endif
