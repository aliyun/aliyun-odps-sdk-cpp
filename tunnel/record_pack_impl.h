#ifndef APSARA_ODPS_TUNNEL_INTERNAL_RECORD_PACK_IMPL_H
#define APSARA_ODPS_TUNNEL_INTERNAL_RECORD_PACK_IMPL_H

#include "odps_tunnel.h"

namespace apsara { namespace odps { namespace sdk { namespace internal { namespace tunnel{

class RecordPackImpl : public IRecordPack
{
public:
    RecordPackImpl()
        : mEnableOffset(false)
        , mFileOffset(-1)
        , mRecordOffset(-1)
    {
    }
    ~RecordPackImpl()
    {
    }

public:
    void SetEnableOffset(bool enable)
    {
        mEnableOffset = enable;
    }
    bool GetEnableOffset() const
    {
        return mEnableOffset;
    }
    void SetFileName(const std::string& name)
    {
        mFileName = name;
    }
    std::string GetFileName() const
    {
        return mFileName;
    }
    void SetFileOffset(int64_t offset)
    {
        mFileOffset = offset;
    }
    int64_t GetFileOffset() const
    {
        return mFileOffset;
    }
    void SetRecordOffset(int64_t offset)
    {
        mRecordOffset = offset;
    }
    int64_t GetRecordOffset() const
    {
        return mRecordOffset;
    }

private:
    bool mEnableOffset;
    std::string mFileName;
    int64_t mFileOffset;
    int64_t mRecordOffset;
};

typedef std::shared_ptr<RecordPackImpl> RecordPackImplPtr;

}
}
}
}
}

#endif
