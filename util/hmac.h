/** HMAC-SHA1(搬自原 apsara security::HMAC)。
 * 接口不变(构造/拷贝构造、init、add、result),底层为 OpenSSL 的
 * 一次性 HMAC() 实现:
 * - OpenSSL 1.0 / 1.1 / 3.x 均可用;
 * - 不使用 3.0 起被弃用的低层 HMAC_CTX_* API,避免 -Werror 下编译失败。
 * 注意:类名与 OpenSSL 的 ::HMAC() 函数同名,需要时用全限定名区分。
 */

#ifndef APSARA_ODPS_SDK_UTIL_HMAC_H
#define APSARA_ODPS_SDK_UTIL_HMAC_H

#include <stdint.h>
#include <string.h>
#include <string>

#include "util/sha1.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

const size_t HMAC_KEY_SIZE = SHA1_DIGEST_BYTES;
const size_t HMAC_SIZE = SHA1_DIGEST_BYTES;
const size_t HMAC_WORDS = SHA1_DIGEST_WORDS;

class HMAC
{
public:
    HMAC(const uint8_t *key, size_t lkey);
    HMAC(const HMAC& hm);
    HMAC& operator=(const HMAC& hm);

    void init(const uint8_t *key, size_t lkey);

    void add(const uint8_t *data, size_t len)
    {
        mMessage.append(reinterpret_cast<const char*>(data), len);
    }

    uint8_t *result();

private:
    std::string mKey;
    std::string mMessage;
    uint8_t mDigest[SHA1_DIGEST_BYTES];
};

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
