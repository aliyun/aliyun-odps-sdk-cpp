/** SDK 内部 URL 风格地址解析实现:语义与原 apsara address.cpp 一致,
 * 异常统一改为 OdpsException。
 */
#include "address.h"

#include "include/odps_exception.h"
#include "util/utils.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

Address::Address(const std::string& address)
{
    static const char *protocolSeperator = ":";
    static const char *protocolSeperatorSuffix = "//"; // optional
    static const char *hostSeperator = "/";
    static const char *portSeperator = ":";
    static const int protocolSeperatorSize =
        std::string(protocolSeperator).size();
    static const int protocolSeperatorSuffixSize =
        std::string(protocolSeperatorSuffix).size();
    static const int portSeperatorSize =
        std::string(portSeperator).size();

    /// find the protocolSeperator (://)
    size_t pos = address.find(protocolSeperator);
    /// if not found, throw
    if (pos == std::string::npos || pos == 0)
    {
        throw OdpsException(LOCAL_ERROR,
            "Unknown address:" + address + " at part protocol");
    }

    /// extract the protocol
    mProtocol = address.substr(0, pos);
    pos += protocolSeperatorSize;

    /// skip possible suffix "//" after ":"
    if (address.substr(pos, protocolSeperatorSuffixSize) == protocolSeperatorSuffix)
    {
        pos += protocolSeperatorSuffixSize;
    }

    /// find the end of [host][:port]
    size_t pos2 = address.find(hostSeperator, pos);
    if (pos2 == std::string::npos)
    {
        pos2 = address.size();
    }

    /// extract the [host][:port]
    mHost = address.substr(pos, pos2-pos);
    size_t posPort = mHost.rfind(portSeperator);
    mPortPresent = (posPort != std::string::npos);
    if (mPortPresent)
    {
        mPort = mHost.substr(posPort + portSeperatorSize);
        mHost = mHost.substr(0, posPort);
    }

    mPath = address.substr(pos2);

    mWholePath = address;
}

std::string Address::GetWholePath() const
{
    return mWholePath;
}

std::string Address::GetProtocol() const
{
    return mProtocol;
}

std::string Address::GetHost() const
{
    return mHost;
}

uint16_t Address::GetPort() const
{
    if (mPortPresent)
    {
        return StringTo<uint16_t>(mPort);
    }
    else
    {
        throw OdpsException(LOCAL_ERROR, "Port not present");
    }
}

std::string Address::GetPath() const
{
    return mPath;
}

bool Address::HasPort() const
{
    return mPortPresent;
}

bool Address::IsEmptyPath() const
{
    return mPath.size() == 0;
}

bool Address::operator==(const Address& addr) const
{
    return mWholePath == addr.mWholePath;
}

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara
