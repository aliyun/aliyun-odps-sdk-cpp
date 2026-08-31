#ifndef APSARA_ODPS_TUNNEL_VOLUME_READER_WRITER_H
#define APSARA_ODPS_TUNNEL_VOLUME_READER_WRITER_H

#include "volume_serialize.h"
#include "common/http_connection.h"
#include "serialize.h"
#include <string>
#include <sstream>

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

class HttpVolumeInputStream : public IVolumeInputStream
{
public:
    HttpVolumeInputStream(HttpConnectionPtr conn, const bool compress = false);
    HttpVolumeInputStream(HttpConnectionPtr conn, CompressOption compress);

    virtual int Read(char* buf, int size)
    {
        return mVolumeDeser->Read(buf, size);
    }

    virtual void Close()
    {
        mVolumeDeser->Close();
    }

    ~HttpVolumeInputStream()
    {
        if (mIn) delete mIn;
    }
private:
    HttpConnectionPtr mConn;
    HttpInputStream* mIn;
    VolumeDeserializerPtr mVolumeDeser;
};

class HttpVolumeOutputStream : public IVolumeOutputStream
{
public:
    HttpVolumeOutputStream(HttpConnectionPtr conn, const bool compress = false);
    HttpVolumeOutputStream(HttpConnectionPtr conn, CompressOption compress);

    void Write(const char* buf, int len) override
    {
        return mVolumeSer->Write(buf, len);
    }

    void Close() override;

    ~HttpVolumeOutputStream()
    {
        if (mOut) delete mOut;
    }
private:
    HttpConnectionPtr mConn;
    HttpOutputStream* mOut;
    VolumeSerializerPtr mVolumeSer;
};
}}}}}
#endif
