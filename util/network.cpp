/** SDK 内部本机 IP 探测实现:语义与原 apsara network.cpp 一致,
 * 异常统一改为 OdpsException。
 */
#include "network.h"

#include <unistd.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>

#include "include/odps_exception.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

uint32_t Network::GetLocalIPWithoutDns()
{
    int socketId;
    if (0 > (socketId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)))
    {
        throw OdpsException(LOCAL_ERROR, "Failed to create socket.");
    }

    struct ifconf conf;

    const int BufSize = sizeof(struct ifreq) * 10000; // support up to 10K IPs
    char* buff = new char[BufSize];
    conf.ifc_len = BufSize;
    conf.ifc_buf = buff;

    if (ioctl(socketId, SIOCGIFCONF, &conf) != 0)
    {
        delete[] buff;
        close(socketId);
        throw OdpsException(LOCAL_ERROR, "Failed to look up in ip address");
    }

    int num = conf.ifc_len / sizeof(struct ifreq);
    struct ifreq *ifr = conf.ifc_req;

    uint32_t retIp = 0;
    for(int i = 0; i < num; i++)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);

        if (ioctl(socketId, SIOCGIFFLAGS, ifr) == 0 &&
            (ifr->ifr_flags & IFF_LOOPBACK) == 0 &&
            (ifr->ifr_flags & IFF_UP))
        {
            retIp = (sin->sin_addr).s_addr;
            close(socketId);
            break;
        }

        ifr++;
    }

    if(retIp == 0)
    {
        delete[] buff;
        throw OdpsException(LOCAL_ERROR, "get ip address fail");
    }

    delete[] buff;
    return retIp;
}

void Network::GetLocalIPFromNetworkCard(std::vector<uint32_t>& vecIp)
{
    int socketId;

    if (0 > (socketId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)))
    {
        throw OdpsException(LOCAL_ERROR, "Failed to create socket.");
    }

    struct ifconf conf;
    const int BufSize = sizeof(struct ifreq) * 10000; // support up to 10K IPs
    char* buff = new char[BufSize];
    conf.ifc_len = BufSize;
    conf.ifc_buf = buff;

    if (ioctl(socketId, SIOCGIFCONF, &conf) != 0)
    {
        delete[] buff;
        close(socketId);
        throw OdpsException(LOCAL_ERROR, "Failed to look up ip address.");
    }

    int num = conf.ifc_len / sizeof(struct ifreq);
    struct ifreq *ifr = conf.ifc_req;

    uint32_t localhost = IpString2UInt("127.0.1.1");
    vecIp.push_back(localhost);

    for(int i = 0; i < num; i++)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);

        if ( ioctl(socketId, SIOCGIFFLAGS, ifr) == 0 && (ifr->ifr_flags & IFF_UP))
        {
            uint32_t retIp = (sin->sin_addr).s_addr;
            vecIp.push_back(retIp);
        }
        ifr++;
    }

    delete[] buff;
    close(socketId);
}

uint32_t Network::GetLocalIP()
{
    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)))
    {
        throw OdpsException(LOCAL_ERROR, "gethostname fail");
    }

    struct hostent  ent;
    struct hostent* result = NULL;
    std::vector<char> buff(512);
    int h = 0;
    uint32_t ip = 0;

    // gethostbyname_r 有时会返回 ERANGE,这时需要扩大 buff 重试。
    //
    // 这里只是用 gethostbyname_r 解析本机地址。只有在 hosts 中本机地址条目
    // 之前有很长的行时,gethostbyname_r 才会返回 ERANGE;绝大多数情况下本机
    // 地址出现在文件最开始的几行,不容易触发重试逻辑。
    int ret = gethostbyname_r(hostName, &ent, &buff[0], buff.size(), &result, &h);
    while (ret == ERANGE &&  // gethostbyname_r() 返回错误码(虽然同时设置 errno)
           buff.size() * 2 <= 1024 * 1024)
    {
        buff.resize(buff.size() * 2);
        ret = gethostbyname_r(hostName, &ent, &buff[0], buff.size(), &result, &h);
    }
    if (ret != 0)
    {
        return GetLocalIPWithoutDns();
    }

    if (!result)
    {
        return GetLocalIPWithoutDns();
    }

    char** pptr;
    for(pptr = result->h_addr_list; *pptr != NULL; pptr++)
    {
        ip = *(uint32_t*)(*pptr);
    }

    if(ip == Network::IpString2UInt("127.0.0.1") ||
       ip == Network::IpString2UInt("127.0.1.1"))
    {
        return GetLocalIPWithoutDns();
    }
    return ip;
}

uint32_t Network::IpString2UInt(const std::string& ip)
{
    uint32_t ui;
    if(inet_pton(AF_INET, ip.c_str(), &ui) == 1)
        return ui;
    else
        return 0;
}

std::string Network::IpUint2String(uint32_t uintIp)
{
    std::string strIp;
    strIp.resize(32);
    if(inet_ntop(AF_INET, (void*)&uintIp, (char*)(strIp.c_str()), 32) == NULL)
    {
        throw OdpsException(LOCAL_ERROR, "inet_ntop error");
    }
    size_t pos = strIp.find(char(0));
    strIp.resize(pos);
    return strIp;
}

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara
