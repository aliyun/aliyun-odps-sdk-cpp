#ifndef APSARA_ODPS_TUNNEL_SLOT_H
#define APSARA_ODPS_TUNNEL_SLOT_H

#include "util/string_util.h"


namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

struct Slot
{
    int32_t mSlotId;
    std::string mServerAddr;
    std::string mServerIp;
    int64_t mDisableTime;

    Slot(int32_t id, const std::string& serverAddr)
        : mSlotId(id)
        , mServerAddr(serverAddr)
        , mDisableTime(-1)
    {
        if (serverAddr.empty())
        {
            throw OdpsTunnelException(INVALID_ARGUMENT,
                                      "Invalid server address" + serverAddr);
        }
        mServerIp = util::SplitString(serverAddr, ":").at(0);
    }

};

}  // namespace tunnel
}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
