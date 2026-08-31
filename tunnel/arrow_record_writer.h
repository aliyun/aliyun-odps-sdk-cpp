#ifndef APSARA_ODPS_TUNNEL_INTERNAL_ARROW_WRITER_H
#define APSARA_ODPS_TUNNEL_INTERNAL_ARROW_WRITER_H
#ifdef ODPS_SDK_ENABLE_ARROW
#include "error_code.h"
#include "odps_tunnel.h"
#include "common/http_connection.h"
#include "connection_manager.h"
#include <arrow/api.h>
#include "tunnel/arrow_http_stream.h"
#include "serialize.h"

#include "google/protobuf/stubs/common.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/wire_format_lite_inl.h"
#include "google/protobuf/io/gzip_stream.h"

using namespace google::protobuf::internal;
using namespace google::protobuf::io;

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{


class ArrowRecordWriter : public IArrowRecordWriter
{
public:
    ArrowRecordWriter(const ConnectionManagerPtr& connManager, const CompressOption& compress);
    virtual bool Write(const arrow::RecordBatch& r);
    virtual ~ArrowRecordWriter();
    virtual void Close();

private:
    HttpConnectionPtr mConn;
    std::shared_ptr<IArrowOutputStream> mOutput;
    CompressOption mCompress;
    HttpOutputStreamPtr mHttpOutputStreamPtr;
};

typedef std::shared_ptr<ArrowRecordWriter> ArrowRecordWriterPtr;

}}}}}
#endif
#endif
