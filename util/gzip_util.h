#ifndef ODPS_GZIPUTIL_H
#define ODPS_GZIPUTIL_H

#include <string>

namespace apsara
{
namespace odps
{
namespace sdk{ namespace util{
class GZipUtil
{
public:
    //return 0:success -1:failed
    static int GZip(const std::string &str, std::string &out);
    //return 0:success -1:failed
    static int GUnZip(const std::string &str, std::string &out);

    //return 0:success -1:failed
    static int GZipBase64(const std::string &str, std::string &out);
    //return 0:success -1:failed
    static int GUnZipBase64(const std::string &str, std::string &out);

private:
};

}
}
} // namespace odps
} // namespace apsara

#endif
