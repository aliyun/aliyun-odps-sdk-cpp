/** HMAC-SHA1,底层为 OpenSSL 一次性 HMAC()。搬自 compat 垫片,
 * 命名空间由 apsara::security 改为 apsara::odps::sdk::util。
 */

#include "util/hmac.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

HMAC::HMAC(const uint8_t *key, size_t lkey)
{
    memset(mDigest, 0, sizeof(mDigest));
    init(key, lkey);
}

HMAC::HMAC(const HMAC& hm)
    : mKey(hm.mKey), mMessage(hm.mMessage)
{
    memcpy(mDigest, hm.mDigest, sizeof(mDigest));
}

HMAC& HMAC::operator=(const HMAC& hm)
{
    if (this != &hm)
    {
        mKey = hm.mKey;
        mMessage = hm.mMessage;
        memcpy(mDigest, hm.mDigest, sizeof(mDigest));
    }
    return *this;
}

void HMAC::init(const uint8_t *key, size_t lkey)
{
    mKey.assign(reinterpret_cast<const char*>(key), lkey);
    mMessage.clear();
    memset(mDigest, 0, sizeof(mDigest));
}

uint8_t *HMAC::result()
{
    unsigned int digestLen = sizeof(mDigest);
    // OpenSSL 的一次性接口:内部处理长于块长的密钥与任意长度消息
    ::HMAC(EVP_sha1(),
           mKey.data(), static_cast<int>(mKey.size()),
           reinterpret_cast<const unsigned char*>(mMessage.data()),
           mMessage.size(),
           mDigest, &digestLen);
    return mDigest;
}

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara
