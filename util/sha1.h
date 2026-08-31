/** SHA-1 摘要(搬自原 apsara security::SHA1,底层为 OpenSSL EVP)。 */

#ifndef APSARA_ODPS_SDK_UTIL_SHA1_H
#define APSARA_ODPS_SDK_UTIL_SHA1_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

const size_t SHA1_DIGEST_WORDS = 5;
const size_t SHA1_DIGEST_BYTES = SHA1_DIGEST_WORDS * sizeof(uint32_t);

class SHA1
{
public:
    SHA1();
    SHA1(const SHA1& s);
    ~SHA1();

    void init();
    void add(const uint8_t *data, size_t len);
    uint8_t *result();

private:
    struct Impl;
    Impl* mImpl;
    uint8_t mDigest[SHA1_DIGEST_BYTES];
    bool mFinalized;
};

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
