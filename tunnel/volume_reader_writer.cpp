#include "volume_reader_writer.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::internal::tunnel;


HttpVolumeInputStream::HttpVolumeInputStream(HttpConnectionPtr conn, const bool compress)
    : mConn(conn)
{
    mIn = new  HttpInputStream(mConn->GetResponse(), 64 * 1024);
    mVolumeDeser.reset(new VolumeDeserializer(mIn, 64 * 1024, compress));
}

HttpVolumeInputStream::HttpVolumeInputStream(HttpConnectionPtr conn, CompressOption compress)
    : mConn(conn)
{
    mIn = new  HttpInputStream(mConn->GetResponse(), 64 * 1024);
    mVolumeDeser.reset(new VolumeDeserializer(mIn, 64 * 1024, compress));
}

HttpVolumeOutputStream::HttpVolumeOutputStream(HttpConnectionPtr conn, const bool compress)
    : mConn(conn)
{
    mOut = new  HttpOutputStream(conn, 64 * 1024);
    mVolumeSer.reset(new VolumeSerializer(
        mOut, compress, conn->GetConfiguration().GetVolumeSerializeChunkSize()));
}

HttpVolumeOutputStream::HttpVolumeOutputStream(HttpConnectionPtr conn, CompressOption compress)
    : mConn(conn)
{
    mOut = new  HttpOutputStream(conn, 64 * 1024);
    mVolumeSer.reset(new VolumeSerializer(
        mOut, compress, conn->GetConfiguration().GetVolumeSerializeChunkSize()));
}

void HttpVolumeOutputStream::Close()
{
    mVolumeSer->Close();
    mOut->Flush();
    mConn->CloseUpstream();
    ResponsePtr resp = mConn->GetResponse();
    std::ostringstream oss;
    char buff[512];
    int64_t bytes;
    while(0 != (bytes = resp->ReadBody(buff, 512)))
    {
        oss << std::string(buff,bytes);
    }
    if (!resp->isSuccessful())
    {
        util::TunnelThrow(*resp, mConn->GetRequest()->GetEndpoint());
    }
    mConn->Close();
}
