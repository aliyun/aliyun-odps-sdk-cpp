/** MD5,底层为 OpenSSL EVP(EVP_md5)。搬自 compat 垫片,命名空间
 * 由 apsara::ErrorDetection 改为 apsara::odps::sdk::util。
 */

#include "md5.h"

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

const char HEX[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string BytesToHexString(const uint8_t* input, size_t length)
{
    std::string out;
    out.reserve(length * 2);
    for (size_t i = 0; i < length; ++i)
    {
        out.push_back(HEX[(input[i] >> 4) & 0x0F]);
        out.push_back(HEX[input[i] & 0x0F]);
    }
    return out;
}

enum { BUFFER_SIZE = 1024 };

} // anonymous namespace

struct MD5::Impl
{
    EVP_MD_CTX* ctx;
    uint8_t digest[16];
    bool finished;

    Impl()
    {
        ctx = MdCtxNew();
        finished = false;
        memset(digest, 0, sizeof(digest));
        EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    }

    ~Impl()
    {
        MdCtxFree(ctx);
    }

    void Final()
    {
        if (!finished)
        {
            unsigned int digestLen = 0;
            EVP_DigestFinal_ex(ctx, digest, &digestLen);
            finished = true;
        }
    }

private:
    Impl(const Impl&);
    Impl& operator=(const Impl&);
};

void DoMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t md5[16])
{
    unsigned int digestLen = 16;
    EVP_Digest(poolIn, static_cast<size_t>(inputBytesNum), md5, &digestLen, EVP_md5(), NULL);
}

bool CheckMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, const uint8_t md5[16])
{
    uint8_t actual[16];
    DoMd5(poolIn, inputBytesNum, actual);
    return memcmp(actual, md5, 16) == 0;
}

MD5::MD5() : mImpl(new Impl())
{
}

MD5::MD5(const void* input, size_t length) : mImpl(new Impl())
{
    update(input, length);
}

MD5::MD5(const std::string& str) : mImpl(new Impl())
{
    update(str);
}

MD5::MD5(std::ifstream& in) : mImpl(new Impl())
{
    update(in);
}

MD5::~MD5()
{
    delete mImpl;
}

void MD5::update(const void* input, size_t length)
{
    if (length > 0)
    {
        mImpl->finished = false;
        EVP_DigestUpdate(mImpl->ctx, input, length);
    }
}

void MD5::update(const std::string& str)
{
    update(str.data(), str.size());
}

void MD5::update(std::ifstream& in)
{
    char buffer[BUFFER_SIZE];
    while (in.good())
    {
        in.read(buffer, sizeof(buffer));
        std::streamsize r = in.gcount();
        if (r > 0)
        {
            update(buffer, static_cast<size_t>(r));
        }
    }
}

const uint8_t* MD5::digest()
{
    mImpl->Final();
    return mImpl->digest;
}

std::string MD5::toString()
{
    return BytesToHexString(digest(), 16);
}

void MD5::reset()
{
    delete mImpl;
    mImpl = new Impl();
}

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara
