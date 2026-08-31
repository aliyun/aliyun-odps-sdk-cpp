/** 离线回归测试(工具模块):不依赖任何线上服务/凭证。
 * 覆盖 base64 流式编解码、timer、Address 解析、Network 地址转换、
 * logging 空实现与 curl 调试开关,确保迁移后的对外行为与原实现一致。
 * timer/Address/Network/base64 均已搬入 apsara::odps::sdk::util
 * (T4/T5);原 apsara 基础库已整体移除(见 docs/compat_layer.md)。
 */
#include <sstream>
#include <string>

#include "gtest/gtest.h"

#include "common/logging.h"
#include "include/odps_exception.h"
#include "util/address.h"
#include "util/base64.h"
#include "util/network.h"
#include "util/timer.h"
#include "common/curl_util.h"

namespace sdkutil = apsara::odps::sdk::util;

// ---------- base64 流式 API(现位于 apsara::odps::sdk::util) ----------

TEST(Base64CompatTest, StreamEncodePadding)
{
    {
        std::istringstream in("abc");
        std::ostringstream out;
        sdkutil::Base64Encoding(in, out);
        EXPECT_EQ("YWJj", out.str());
    }
    {   // 剩余 2 字节 → 一个 '='
        std::istringstream in("ab");
        std::ostringstream out;
        sdkutil::Base64Encoding(in, out);
        EXPECT_EQ("YWI=", out.str());
    }
    {   // 剩余 1 字节 → 两个 '='
        std::istringstream in("a");
        std::ostringstream out;
        sdkutil::Base64Encoding(in, out);
        EXPECT_EQ("YQ==", out.str());
    }
    {   // 空输入
        std::istringstream in("");
        std::ostringstream out;
        sdkutil::Base64Encoding(in, out);
        EXPECT_EQ("", out.str());
    }
    {   // 二进制数据(含 \0)
        std::string raw("x\x01\x02y", 4);
        std::istringstream in(raw);
        std::ostringstream out;
        sdkutil::Base64Encoding(in, out);
        EXPECT_EQ("eAECeQ==", out.str());
    }
}

TEST(Base64CompatTest, StreamDecodeRoundTrip)
{
    std::string raw = "hello \x01 world";
    raw.push_back('\0');
    std::istringstream enc_in(raw);
    std::ostringstream encoded;
    sdkutil::Base64Encoding(enc_in, encoded);

    std::istringstream dec_in(encoded.str());
    std::ostringstream decoded;
    sdkutil::Base64Decoding(dec_in, decoded);
    EXPECT_EQ(raw, decoded.str());
}

TEST(Base64CompatTest, DecodeInvalidThrows)
{
    {   // 长度不是 4 的倍数
        std::istringstream in("YWI");
        std::ostringstream out;
        EXPECT_THROW(sdkutil::Base64Decoding(in, out), apsara::odps::sdk::OdpsException);
    }
    {   // 非法字符
        std::istringstream in("YW$j");
        std::ostringstream out;
        EXPECT_THROW(sdkutil::Base64Decoding(in, out), apsara::odps::sdk::OdpsException);
    }
    {   // '=' 后不允许追加内容
        std::istringstream in("YQ==x");
        std::ostringstream out;
        EXPECT_THROW(sdkutil::Base64Decoding(in, out), apsara::odps::sdk::OdpsException);
    }
    {   // '=' 只能出现在第三/第四位
        std::istringstream in("=YWJ");
        std::ostringstream out;
        EXPECT_THROW(sdkutil::Base64Decoding(in, out), apsara::odps::sdk::OdpsException);
    }
}

// ---------- curl 调试开关(原 apsara flag tunnel_sdk_PrintDebugError) ----------

TEST(CurlDebugSwitchTest, DefaultOffAndToggle)
{
    EXPECT_FALSE(apsara::odps::sdk::internal::IsDebugErrorEnabled());
    apsara::odps::sdk::internal::SetDebugErrorEnabled(true);
    EXPECT_TRUE(apsara::odps::sdk::internal::IsDebugErrorEnabled());
    // 还原,避免影响其他用例
    apsara::odps::sdk::internal::SetDebugErrorEnabled(false);
    EXPECT_FALSE(apsara::odps::sdk::internal::IsDebugErrorEnabled());
}

// ---------- timer(现位于 apsara::odps::sdk::util) ----------

TEST(TimerCompatTest, MonotonicClocks)
{
    int64_t ms = sdkutil::timing::GetCurrentTimeInMilliSeconds();
    int64_t us = sdkutil::timing::GetCurrentTimeInMicroSeconds();
    EXPECT_GT(ms, 0);
    EXPECT_GT(us, 0);
    // 微秒与毫秒应在同一量级(允许 1 秒内的偏差)
    EXPECT_NEAR(static_cast<double>(ms), static_cast<double>(us) / 1000.0, 1000.0);

    sdkutil::Timer timer;
    timer.Stop();
    EXPECT_GE(timer.Stop(), 0);
}

// ---------- address(现位于 apsara::odps::sdk::util) ----------

TEST(AddressCompatTest, ParseFullUrl)
{
    sdkutil::Address addr("http://example.com:8080/path/to");
    EXPECT_EQ("http", addr.GetProtocol());
    EXPECT_EQ("example.com", addr.GetHost());
    EXPECT_TRUE(addr.HasPort());
    EXPECT_EQ(8080, addr.GetPort());
    EXPECT_EQ("/path/to", addr.GetPath());
    EXPECT_FALSE(addr.IsEmptyPath());
    EXPECT_EQ("http://example.com:8080/path/to", addr.GetWholePath());
}

TEST(AddressCompatTest, ProtocolOnlyUsage)
{
    // SDK 唯一用法:从 tunnel endpoint 提取协议
    sdkutil::Address addr("https://dt.odps.aliyun.com");
    EXPECT_EQ("https", addr.GetProtocol());
    EXPECT_EQ("dt.odps.aliyun.com", addr.GetHost());
    EXPECT_FALSE(addr.HasPort());
    EXPECT_TRUE(addr.IsEmptyPath());
    EXPECT_THROW(addr.GetPort(), apsara::odps::sdk::OdpsException);
}

TEST(AddressCompatTest, BadFormatThrows)
{
    EXPECT_THROW(sdkutil::Address("no-protocol-here"), apsara::odps::sdk::OdpsException);
    EXPECT_THROW(sdkutil::Address(":host"), apsara::odps::sdk::OdpsException);
    sdkutil::Address a("tcp://host:1");
    sdkutil::Address b("tcp://host:1");
    sdkutil::Address c("tcp://host:2");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

// ---------- network(现位于 apsara::odps::sdk::util) ----------

TEST(NetworkCompatTest, IpConversionRoundTrip)
{
    uint32_t ip = sdkutil::Network::IpString2UInt("10.1.2.3");
    EXPECT_NE(0u, ip);
    EXPECT_EQ("10.1.2.3", sdkutil::Network::IpUint2String(ip));
    EXPECT_EQ("127.0.0.1", sdkutil::Network::IpUint2String(sdkutil::Network::IpString2UInt("127.0.0.1")));
    // 非法输入返回 0
    EXPECT_EQ(0u, sdkutil::Network::IpString2UInt("not-an-ip"));
}

// ---------- logging(no-op 桩,现位于 apsara::odps::sdk::logging) ----------

TEST(LoggingCompatTest, NoOpMacrosCompileAndRun)
{
    apsara::odps::sdk::logging::Logger* logger =
        apsara::odps::sdk::logging::GetLogger("/test/logger");
    EXPECT_TRUE(logger == NULL);
    apsara::odps::sdk::logging::InitLoggingSystem();
    LOG_DEBUG(logger, ("k", "v"));
    LOG_INFO(logger, ("k", "v"));
    LOG_WARNING(logger, ("k", "v"));
    LOG_ERROR(logger, ("k", "v"));
}
