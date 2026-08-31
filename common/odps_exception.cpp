#include "odps_exception.h"
#include "odps_tunnel.h"
#include "common/json_serialize.h"

#include "util/utils.h"
#include "error_code.h"

namespace apsara { namespace odps { namespace sdk {

#ifdef ODPS_SDK_ENABLE_ARROW
ExactlyOnceAlreadyWrittenException::ExactlyOnceAlreadyWrittenException(const std::string& error, const std::string& message, const std::string& requestid, const std::map<std::string, std::string>& extraInfo):
    OdpsTunnelException(error, message, requestid, extraInfo)
{
    EOSInfo info;
    util::FromJsonString(info, GetExtraInfo("EOSInfoJson"));
    mSequenceId = info.mSequenceId;
    mSequenceOffset = info.mSequenceOffset;
}
#endif

}}}
