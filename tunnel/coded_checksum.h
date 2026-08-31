#ifndef APSARA_ODPS_TUNNEL_INTERNAL_CODED_CHECKSUM_H
#define APSARA_ODPS_TUNNEL_INTERNAL_CODED_CHECKSUM_H

#include <stdint.h>
#include "crc_32c.h"

#include "google/protobuf/stubs/common.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/wire_format_lite_inl.h"
#include "google/protobuf/io/coded_stream.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

#define INLINE __attribute__ ((always_inline))

#define WireFormatLite google::protobuf::internal::WireFormatLite
#define CodedOutputStream google::protobuf::io::CodedOutputStream

class CodedChecksum
{
public:
    CodedChecksum()
    {
        mBlockEnd = mArray;
    }

    inline void PutTag(int fieldNum, WireFormatLite::WireType type) INLINE
    {
        mBlockEnd = WireFormatLite::WriteTagToArray(fieldNum, type, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline void PutVarint32(uint32_t v) INLINE
    {
        mBlockEnd = CodedOutputStream::WriteVarint32ToArray(v, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline void PutRaw(const void* buffer, int size) INLINE
    {
        mCrc.process_bytes(buffer, size);
    }

    inline void PutUInt32(int fieldNum, uint32_t v) INLINE
    {
        mBlockEnd = WireFormatLite::WriteUInt32ToArray(fieldNum, v, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline void PutSInt64(int fieldNum, int64_t v) INLINE
    {
        mBlockEnd = WireFormatLite::WriteSInt64ToArray(fieldNum, v, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline void PutDouble(int fieldNum, double v) INLINE
    {
        mBlockEnd = WireFormatLite::WriteDoubleToArray(fieldNum, v, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline void PutFloat(int fieldNum, float v) INLINE
    {
        mBlockEnd = WireFormatLite::WriteFloatToArray(fieldNum, v, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline void PutBool(int fieldNum, bool v) INLINE
    {
        mBlockEnd = WireFormatLite::WriteBoolToArray(fieldNum, v, mArray);
        mCrc.process_block(mArray, mBlockEnd);
    }

    inline uint32_t GetChecksum() INLINE
    {
        return mCrc.checksum();
    }
private:
    uint8_t mArray[128];
    uint8_t* mBlockEnd;
    crc_32c_type mCrc;
};
#undef CodedOutputStream
#undef WireFormatLite

#undef INLINE

}}}}} // namepsace apsara::odps::sdk::internal::tunnel
#endif //APSARA_ODPS_TUNNEL_CODED_CHECKSUM_H
/**
 * vim: ts=4: sw=4: et:
 */
