/** SHA-1 摘要,底层为 OpenSSL EVP。搬自 compat 垫片,命名空间
 * 由 apsara::security 改为 apsara::odps::sdk::util。
 */

#include "sha1.h"

#include <openssl/evp.h>
#include <string.h>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

namespace {

#if OPENSSL_VERSION_NUMBER < 0x10100000L
inline EVP_MD_CTX* MdCtxNew() { return EVP_MD_CTX_create(); }
inline void MdCtxFree(EVP_MD_CTX* ctx) { EVP_MD_CTX_destroy(ctx); }
#else
inline EVP_MD_CTX* MdCtxNew() { return EVP_MD_CTX_new(); }
inline void MdCtxFree(EVP_MD_CTX* ctx) { EVP_MD_CTX_free(ctx); }
#endif

} // anonymous namespace

struct SHA1::Impl
{
    EVP_MD_CTX* ctx;

    Impl()
    {
        ctx = MdCtxNew();
        EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    }

    Impl(const Impl& other)
    {
        ctx = MdCtxNew();
        EVP_MD_CTX_copy_ex(ctx, other.ctx);
    }

    ~Impl()
    {
        MdCtxFree(ctx);
    }

private:
    Impl& operator=(const Impl&);
};

SHA1::SHA1() : mImpl(new Impl()), mFinalized(false)
{
    memset(mDigest, 0, sizeof(mDigest));
}

SHA1::SHA1(const SHA1& s) : mImpl(new Impl(*s.mImpl)), mFinalized(s.mFinalized)
{
    memcpy(mDigest, s.mDigest, sizeof(mDigest));
}

SHA1::~SHA1()
{
    delete mImpl;
}

void SHA1::init()
{
    delete mImpl;
    mImpl = new Impl();
    mFinalized = false;
    memset(mDigest, 0, sizeof(mDigest));
}

void SHA1::add(const uint8_t *data, size_t len)
{
    mFinalized = false;
    EVP_DigestUpdate(mImpl->ctx, data, len);
}

uint8_t *SHA1::result()
{
    // 与原实现一致:重复调用 result() 返回同一份摘要缓存
    if (!mFinalized)
    {
        unsigned int digestLen = 0;
        EVP_DigestFinal_ex(mImpl->ctx, mDigest, &digestLen);
        mFinalized = true;
    }
    return mDigest;
}

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara
