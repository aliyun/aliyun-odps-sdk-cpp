/** 离线回归测试(加密模块):不依赖任何线上服务/凭证。
 * 使用标准测试向量验证 OpenSSL 后端与原 apsara 实现的输出一致:
 * - MD5:RFC 1321 测试向量
 * - SHA-1:FIPS 180-2 测试向量
 * - HMAC-SHA1:RFC 2202 测试向量
 * MD5/SHA1/HMAC 已搬入 apsara::odps::sdk::util(T5)。
 */
#include <string>
#include <string.h>

#include "gtest/gtest.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "util/md5.h"
#include "util/sha1.h"
#include "util/hmac.h"

namespace sdkutil = apsara::odps::sdk::util;

namespace {

std::string BytesToHex(const uint8_t* data, size_t len)
{
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i)
    {
        out.push_back(kHex[(data[i] >> 4) & 0x0F]);
        out.push_back(kHex[data[i] & 0x0F]);
    }
    return out;
}

} // anonymous namespace

// ---------- MD5 ----------

TEST(CryptoCompatTest, Md5KnownVectors)
{
    EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", sdkutil::MD5(std::string("")).toString());
    EXPECT_EQ("900150983cd24fb0d6963f7d28e17f72", sdkutil::MD5(std::string("abc")).toString());
    EXPECT_EQ("c3fcd3d76192e4007dfb496cca67e13b",
              sdkutil::MD5(std::string("abcdefghijklmnopqrstuvwxyz")).toString());
}

TEST(CryptoCompatTest, Md5IncrementalEqualsOneShot)
{
    std::string payload = "The quick brown fox jumps over the lazy dog";
    sdkutil::MD5 one(payload);

    sdkutil::MD5 inc;
    inc.update(payload.data(), 10);
    inc.update(payload.data() + 10, payload.size() - 10);
    EXPECT_EQ(one.toString(), inc.toString());
    // 摘要为 16 字节("The quick brown fox..." 的 MD5)
    const uint8_t* d = inc.digest();
    EXPECT_EQ("9E107D9D372BB6826BD81D3542A419D6", BytesToHex(d, 16));

    // reset 后可重新计算
    inc.reset();
    inc.update(std::string("abc"));
    EXPECT_EQ("900150983cd24fb0d6963f7d28e17f72", inc.toString());
}

TEST(CryptoCompatTest, DoMd5AndCheck)
{
    uint8_t digest[16];
    sdkutil::DoMd5(reinterpret_cast<const uint8_t*>("abc"), 3, digest);
    EXPECT_EQ("900150983CD24FB0D6963F7D28E17F72", BytesToHex(digest, 16));
    EXPECT_TRUE(sdkutil::CheckMd5(reinterpret_cast<const uint8_t*>("abc"), 3, digest));
    digest[0] ^= 0xFF;
    EXPECT_FALSE(sdkutil::CheckMd5(reinterpret_cast<const uint8_t*>("abc"), 3, digest));
}

// ---------- SHA1 ----------

TEST(CryptoCompatTest, Sha1KnownVector)
{
    sdkutil::SHA1 sha;
    sha.add(reinterpret_cast<const uint8_t*>("abc"), 3);
    EXPECT_EQ("A9993E364706816ABA3E25717850C26C9CD0D89D",
              BytesToHex(sha.result(), sdkutil::SHA1_DIGEST_BYTES));
    // 重复 result() 返回同一份摘要
    EXPECT_EQ("A9993E364706816ABA3E25717850C26C9CD0D89D",
              BytesToHex(sha.result(), sdkutil::SHA1_DIGEST_BYTES));

    // init 后可重用
    sha.init();
    sha.add(reinterpret_cast<const uint8_t*>("abc"), 3);
    EXPECT_EQ("A9993E364706816ABA3E25717850C26C9CD0D89D",
              BytesToHex(sha.result(), sdkutil::SHA1_DIGEST_BYTES));
}

TEST(CryptoCompatTest, Sha1CopyKeepsState)
{
    sdkutil::SHA1 sha;
    sha.add(reinterpret_cast<const uint8_t*>("a"), 1);
    sdkutil::SHA1 copy(sha);
    sha.add(reinterpret_cast<const uint8_t*>("bc"), 2);
    copy.add(reinterpret_cast<const uint8_t*>("bc"), 2);
    EXPECT_EQ("A9993E364706816ABA3E25717850C26C9CD0D89D",
              BytesToHex(sha.result(), sdkutil::SHA1_DIGEST_BYTES));
    EXPECT_EQ("A9993E364706816ABA3E25717850C26C9CD0D89D",
              BytesToHex(copy.result(), sdkutil::SHA1_DIGEST_BYTES));
}

// ---------- HMAC-SHA1 ----------

TEST(CryptoCompatTest, HmacSha1Rfc2202Case1)
{
    // RFC 2202 case 1:key = 0x0b * 20,data = "Hi There"
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    sdkutil::HMAC hmac(key, sizeof(key));
    hmac.add(reinterpret_cast<const uint8_t*>("Hi There"), 8);
    EXPECT_EQ("B617318655057264E28BC0B6FB378C8EF146BE00",
              BytesToHex(hmac.result(), sdkutil::HMAC_SIZE));
}

TEST(CryptoCompatTest, HmacSha1MatchesOpenSslOneShot)
{
    // 与 signer.cpp 使用的 OpenSSL 一次性 HMAC 结果互验
    std::string key = "some-access-key-secret";
    std::string data = "GET\n\n\nWed, 26 Aug 2026 08:00:00 GMT\n/projects/p/tables";

    sdkutil::HMAC hmac(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    hmac.add(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    std::string shim = BytesToHex(hmac.result(), sdkutil::HMAC_SIZE);

    uint8_t expect[EVP_MAX_MD_SIZE];
    unsigned int expectLen = 0;
    ::HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()),
           reinterpret_cast<const unsigned char*>(data.data()), data.size(),
           expect, &expectLen);
    ASSERT_EQ(static_cast<unsigned int>(sdkutil::HMAC_SIZE), expectLen);
    EXPECT_EQ(BytesToHex(expect, expectLen), shim);

    // init 后可重用
    hmac.init(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    hmac.add(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    EXPECT_EQ(shim, BytesToHex(hmac.result(), sdkutil::HMAC_SIZE));
}
