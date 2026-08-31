/** @file address.h
 * SDK 内部 URL 风格地址解析(原 apsara Address 中被 SDK 使用的部分,
 * 搬入 apsara::odps::sdk::util,实现见 util/address.cpp)。
 */

#ifndef APSARA_ODPS_SDK_UTIL_ADDRESS_H
#define APSARA_ODPS_SDK_UTIL_ADDRESS_H

#include <stdint.h>
#include <string>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{
    /**
     * Address format:
     * <protocol_name>://[<host>][:<port>][<path beginning with />]
     * protocol name: cannot be an empty string
     * host name: string without "/"
     * port: 0-65535
     * path: empty or begin with /
     * 解析失败抛 OdpsException。
     */
    class Address
    {
        public:
            Address(const std::string& path);

            std::string GetWholePath() const;
            /// Read invididual part
            std::string GetProtocol() const;
            std::string GetHost() const;
            uint16_t GetPort() const;
            std::string GetPath() const;

            bool operator==(const Address& addr) const;

            /// utility
            bool HasPort() const;
            bool IsEmptyPath() const;

        private:
            std::string mWholePath;
            std::string mHost;
            std::string mProtocol;
            std::string mPort;
            std::string mPath;
            bool mPortPresent;
    };

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // APSARA_ODPS_SDK_UTIL_ADDRESS_H
