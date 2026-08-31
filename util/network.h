/** @file network.h
 * SDK 内部本机 IP 工具(原 apsara Network 中被 SDK 使用的部分,
 * 搬入 apsara::odps::sdk::util,实现见 util/network.cpp)。
 */

#ifndef APSARA_ODPS_SDK_UTIL_NETWORK_H
#define APSARA_ODPS_SDK_UTIL_NETWORK_H

#include <stdint.h>
#include <string>
#include <vector>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

class Network
{
public:
    static uint32_t GetLocalIP();
    static uint32_t GetLocalIPWithoutDns();
    static void GetLocalIPFromNetworkCard(std::vector<uint32_t>& vecIp);

    /// IPv4 dotted string -> network-order uint32; returns 0 on invalid input
    static uint32_t IpString2UInt(const std::string& ip);
    /// network-order uint32 -> IPv4 dotted string; throws OdpsException on error
    static std::string IpUint2String(uint32_t uintIp);
};

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // APSARA_ODPS_SDK_UTIL_NETWORK_H
