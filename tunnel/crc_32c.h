#ifndef APSARA_ODPS_TUNNEL_INTERNAL_CRC_32C
#define APSARA_ODPS_TUNNEL_INTERNAL_CRC_32C

#include <cstdint>
#include <boost/crc.hpp>
#ifdef TUNNEL_USE_CRC_32C
#include "util/crc32c.h"
#endif

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

#ifndef TUNNEL_USE_CRC_32C
using crc_32c_type = boost::crc_optimal<32, 0x1edc6f41, 0xFFFFFFFF, 0xFFFFFFFF, true, true>;
#else
class ApsaraCrcWrapper
{
public:
    ApsaraCrcWrapper():mChecksum(0xFFFFFFFF){}
    void process_block(const void *start, const void *end)
    {
        if (end > start)
        {
            uint32_t len = (char *)end - (char *)start;
            process_bytes(start, len);
        }
    }
    void process_bytes(const void *data, uint64_t len)
    {
        mChecksum = util::DoCrc32c(mChecksum, (const uint8_t *)data, len);
    }
    void process_byte(uint8_t c)
    {
        process_bytes(&c, sizeof(c));
    }
    uint32_t checksum(){ return mChecksum ^ 0xFFFFFFFF; }
    void reset(){ mChecksum = 0xFFFFFFFF; }
private:
    uint32_t mChecksum;
};

using crc_32c_type = ApsaraCrcWrapper;
#endif

}}}}}

#endif