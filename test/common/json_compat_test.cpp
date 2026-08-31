/** 离线回归测试:不依赖任何线上服务/凭证。
 * 覆盖 JSON 序列化契约(nlohmann 后端)与 string_tools 语义。
 *
 * JSON 部分验证从 Jsonizable 框架迁移到 nlohmann 直写后的行为契约:
 *  - 两参语义(必填字段)缺失时抛异常,三参语义(缺省值)按缺省填充;
 *  - BOOL_AS_STRING、嵌套 JSON 字符串、容器、大整数精度等既有语义。
 */
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include <nlohmann/json.hpp>

#include "include/odps_exception.h"
#include "util/string_util.h"

#include "common/json_serialize.h"
#include "tunnel/stream_upload.h"
#include "util/utils.h"

namespace sdkutil = apsara::odps::sdk::util;

namespace {

struct ShimPayload
{
    int64_t mNumber;
    std::string mName;
    bool mFlag;
    std::vector<std::string> mTags;
    uint64_t mDefaulted;

    ShimPayload() : mNumber(0), mFlag(false), mDefaulted(42) {}
};

inline void to_json(nlohmann::json& j, const ShimPayload& p)
{
    j = nlohmann::json{
        {"Number", p.mNumber},
        {"Name", p.mName},
        {"Flag", p.mFlag},
        {"Tags", p.mTags},
        {"Defaulted", p.mDefaulted},
    };
}

inline void from_json(const nlohmann::json& j, ShimPayload& p)
{
    p.mNumber = j.at("Number").get<int64_t>();
    p.mName = j.at("Name").get<std::string>();
    p.mFlag = j.at("Flag").get<bool>();
    p.mTags = j.at("Tags").get<std::vector<std::string> >();
    p.mDefaulted = j.value("Defaulted", static_cast<uint64_t>(42));
}

} // anonymous namespace

// ---------- util::ToJsonString / FromJsonString 契约 ----------

TEST(JsonContractTest, RoundTripStruct)
{
    ShimPayload out;
    out.mNumber = -12345;
    out.mName = "hello";
    out.mFlag = true;
    out.mTags.push_back("a");
    out.mTags.push_back("b");
    out.mDefaulted = 7;

    std::string text = apsara::odps::sdk::util::ToJsonString(out);

    ShimPayload in;
    apsara::odps::sdk::util::FromJsonString(in, text);
    EXPECT_EQ(out.mNumber, in.mNumber);
    EXPECT_EQ(out.mName, in.mName);
    EXPECT_EQ(out.mFlag, in.mFlag);
    EXPECT_EQ(out.mTags, in.mTags);
    EXPECT_EQ(out.mDefaulted, in.mDefaulted);
}

TEST(JsonContractTest, DefaultValueApplied)
{
    ShimPayload in;
    apsara::odps::sdk::util::FromJsonString(
        in, "{\"Number\": 1, \"Name\": \"x\", \"Flag\": false, \"Tags\": []}");
    EXPECT_EQ(static_cast<uint64_t>(42), in.mDefaulted);
}

TEST(JsonContractTest, MissingRequiredKeyThrows)
{
    ShimPayload in;
    // 必填字段走 j.at(),缺失时抛 nlohmann::json::out_of_range
    EXPECT_THROW(apsara::odps::sdk::util::FromJsonString(in, "{}"),
                 nlohmann::json::out_of_range);
}

TEST(JsonContractTest, ContainersRoundTrip)
{
    std::vector<int32_t> v;
    v.push_back(1);
    v.push_back(2);
    std::string text = apsara::odps::sdk::util::ToJsonString(v);
    std::vector<int32_t> v2;
    apsara::odps::sdk::util::FromJsonString(v2, text);
    EXPECT_EQ(v, v2);

    std::map<std::string, std::string> m;
    m["k1"] = "v1";
    m["k2"] = "v2";
    std::string mtext = apsara::odps::sdk::util::ToJsonString(m);
    std::map<std::string, std::string> m2;
    apsara::odps::sdk::util::FromJsonString(m2, mtext);
    EXPECT_EQ(m, m2);
}

TEST(JsonContractTest, PrettyAndCompactOutput)
{
    ShimPayload p;
    p.mName = "v";
    std::string pretty = apsara::odps::sdk::util::ToJsonString(p);
    EXPECT_NE(std::string::npos, pretty.find('\n'));

    std::string compact = apsara::odps::sdk::util::ToJsonCompactString(p);
    EXPECT_EQ(std::string::npos, compact.find(' '));
    EXPECT_EQ(std::string::npos, compact.find('\n'));
    EXPECT_NE(std::string::npos, compact.find("\"Name\":\"v\""));
}

TEST(JsonContractTest, BigNumbersKeepPrecision)
{
    // 2^64-1 与超过 2^53 的整数必须精确保留
    nlohmann::json j = {
        {"max", std::numeric_limits<uint64_t>::max()},
        {"big", 9007199254740993ULL},
        {"neg", std::numeric_limits<int64_t>::min()},
    };
    std::string text = j.dump();
    EXPECT_NE(std::string::npos, text.find("18446744073709551615"));
    EXPECT_NE(std::string::npos, text.find("9007199254740993"));
    EXPECT_NE(std::string::npos, text.find("-9223372036854775808"));

    nlohmann::json parsed = nlohmann::json::parse(text);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), parsed.at("max").get<uint64_t>());
    EXPECT_EQ(9007199254740993ULL, parsed.at("big").get<uint64_t>());
    EXPECT_EQ(std::numeric_limits<int64_t>::min(), parsed.at("neg").get<int64_t>());
}

TEST(JsonContractTest, ParseErrorThrows)
{
    ShimPayload in;
    EXPECT_THROW(apsara::odps::sdk::util::FromJsonString(in, "{bad json"),
                 nlohmann::json::parse_error);
    EXPECT_THROW(apsara::odps::sdk::util::FromJsonString(in, ""),
                 nlohmann::json::parse_error);
    EXPECT_THROW(apsara::odps::sdk::util::FromJsonString(in, "[1, 2"),
                 nlohmann::json::parse_error);
}

// ---------- 公开类型序列化契约(原 INJECT_JSONIZABLE 替代品) ----------

TEST(JsonContractTest, BoolAsStringRoundTrip)
{
    // BOOL_AS_STRING:服务端以字符串 "true"/"false" 传输布尔值
    apsara::odps::sdk::TableExtendedReservedInfo out;
    out.mHasRowAccessPolicy = true;
    out.mIsTransactional = false;
    out.mClusterType = "hash";
    out.mBucketNum = 8;
    out.mClusterCols.push_back("c1");
    apsara::odps::sdk::SortColumn sc;
    sc.mColumn = "c2";
    sc.mOrder = "asc";
    out.mSortCols.push_back(sc);
    out.mPrimaryKey.push_back("pk");

    std::string text = apsara::odps::sdk::util::ToJsonString(out);
    EXPECT_NE(std::string::npos, text.find("\"HasRowAccessPolicy\": \"true\""));
    EXPECT_NE(std::string::npos, text.find("\"Transactional\": \"false\""));

    apsara::odps::sdk::TableExtendedReservedInfo in;
    apsara::odps::sdk::util::FromJsonString(in, text);
    EXPECT_TRUE(in.mHasRowAccessPolicy);
    EXPECT_FALSE(in.mIsTransactional);
    EXPECT_EQ(out.mClusterType, in.mClusterType);
    EXPECT_EQ(out.mBucketNum, in.mBucketNum);
    EXPECT_EQ(out.mClusterCols, in.mClusterCols);
    ASSERT_EQ(1u, in.mSortCols.size());
    EXPECT_EQ("c2", in.mSortCols[0].mColumn);
    EXPECT_EQ("asc", in.mSortCols[0].mOrder);
    EXPECT_EQ(out.mPrimaryKey, in.mPrimaryKey);
}

TEST(JsonContractTest, TableExtendedReservedInfoDefaults)
{
    apsara::odps::sdk::TableExtendedReservedInfo in;
    in.mBucketNum = 99;  // 应被缺省值覆盖
    apsara::odps::sdk::util::FromJsonString(in, "{}");
    EXPECT_FALSE(in.mHasRowAccessPolicy);
    EXPECT_FALSE(in.mIsTransactional);
    EXPECT_EQ("", in.mClusterType);
    EXPECT_EQ(static_cast<int64_t>(-1), in.mBucketNum);
    EXPECT_TRUE(in.mClusterCols.empty());
    EXPECT_TRUE(in.mSortCols.empty());
    EXPECT_TRUE(in.mPrimaryKey.empty());
}

TEST(JsonContractTest, NestedReservedJsonString)
{
    // Reserved 字段本身是一个内嵌的 JSON 字符串
    apsara::odps::sdk::ODPSTableExtendedInfo out;
    out.mIsArchived = true;
    out.mPhysicalSize = 1234;
    out.mFileNum = 5;
    out.mReserved.mBucketNum = 3;
    out.mReserved.mClusterType = "range";

    std::string text = apsara::odps::sdk::util::ToJsonString(out);

    apsara::odps::sdk::ODPSTableExtendedInfo in;
    apsara::odps::sdk::util::FromJsonString(in, text);
    EXPECT_TRUE(in.mIsArchived);
    EXPECT_EQ(static_cast<int64_t>(1234), in.mPhysicalSize);
    EXPECT_EQ(static_cast<int64_t>(5), in.mFileNum);
    EXPECT_EQ(static_cast<int64_t>(3), in.mReserved.mBucketNum);
    EXPECT_EQ("range", in.mReserved.mClusterType);
}

TEST(JsonContractTest, PartitionExtendedReservedInfoDefaults)
{
    apsara::odps::sdk::PartitionExtendedReservedInfo in;
    apsara::odps::sdk::util::FromJsonString(in, "{}");
    EXPECT_EQ("", in.mClusterType);
    EXPECT_EQ(static_cast<int64_t>(-1), in.mBucketNum);
    EXPECT_TRUE(in.mClusterCols.empty());
    EXPECT_TRUE(in.mSortCols.empty());
}

TEST(JsonContractTest, StreamUploadInfoSlotsArePairArrays)
{
    // 服务端真实协议:slots 是 [slotId(数字), serverAddr(字符串)] 二元组数组。
    // 原 Jsonizable 框架对非字符串键 map 一律序列化为二元组数组
    // (只有 map<string, T> 才是 JSON 对象),不能写成对象形式。
    const std::string payload =
        "{"
        "\"quota_name\": \"default\","
        "\"session_name\": \"sess-123\","
        "\"status\": \"normal\","
        "\"schema\": {\"columns\": [{\"name\": \"col1\", \"type\": \"bigint\"}],"
        " \"partitionKeys\": []},"
        "\"slots\": [[0, \"10.1.2.3:8080\"], [1, \"10.1.2.4:8080\"]]"
        "}";
    apsara::odps::sdk::internal::tunnel::StreamUploadInfo info;
    apsara::odps::sdk::util::FromJsonString(info, payload);
    EXPECT_EQ("default", info.mQuotaName);
    EXPECT_EQ("sess-123", info.mSessionName);
    EXPECT_EQ("normal", info.mStatus);
    ASSERT_EQ(2u, info.mSlots.size());
    EXPECT_EQ("10.1.2.3:8080", info.mSlots[0]);
    EXPECT_EQ("10.1.2.4:8080", info.mSlots[1]);

    // 回环:序列化输出同样保持二元组数组格式
    nlohmann::json j = nlohmann::json::parse(apsara::odps::sdk::util::ToJsonString(info));
    ASSERT_TRUE(j.at("slots").is_array());
    ASSERT_EQ(2u, j.at("slots").size());
    ASSERT_TRUE(j.at("slots").at(0).is_array());
    EXPECT_EQ(0, j.at("slots").at(0).at(0).get<int32_t>());
    EXPECT_EQ("10.1.2.3:8080", j.at("slots").at(0).at(1).get<std::string>());
    EXPECT_EQ(1, j.at("slots").at(1).at(0).get<int32_t>());
    EXPECT_EQ("10.1.2.4:8080", j.at("slots").at(1).at(1).get<std::string>());
}

// ---------- string_tools(现位于 apsara::odps::sdk::util) ----------

TEST(StringToolsCompatTest, SplitStringKeepsEmptyPieces)
{
    std::vector<std::string> parts = sdkutil::SplitString("a,,b", ",");
    ASSERT_EQ(3u, parts.size());
    EXPECT_EQ("a", parts[0]);
    EXPECT_EQ("", parts[1]);
    EXPECT_EQ("b", parts[2]);

    parts = sdkutil::SplitString("a,", ",");
    ASSERT_EQ(2u, parts.size());
    EXPECT_EQ("", parts[1]);

    EXPECT_TRUE(sdkutil::SplitString("", ",").empty());
    EXPECT_THROW(sdkutil::SplitString("a", ""), apsara::odps::sdk::OdpsException);

    // 每个字符都是分隔符
    parts = sdkutil::SplitString("a,b;c", ",;");
    ASSERT_EQ(3u, parts.size());
}

TEST(StringToolsCompatTest, StringSpliterDropsEmpty)
{
    std::vector<std::string> parts = sdkutil::StringSpliter("a,,b", ",");
    ASSERT_EQ(2u, parts.size());
    EXPECT_EQ("a", parts[0]);
    EXPECT_EQ("b", parts[1]);

    parts = sdkutil::StringSpliter("abc", "");
    ASSERT_EQ(1u, parts.size());
    EXPECT_EQ("abc", parts[0]);

    EXPECT_TRUE(sdkutil::StringSpliter("", ",").empty());

    parts = sdkutil::StringSpliter("ab", "longer-than-src");
    ASSERT_EQ(1u, parts.size());
    EXPECT_EQ("ab", parts[0]);
}

TEST(StringToolsCompatTest, TrimAndCase)
{
    EXPECT_EQ("x", sdkutil::TrimString("  x  "));
    EXPECT_EQ("x  ", sdkutil::LeftTrimString("  x  "));
    EXPECT_EQ("  x", sdkutil::RightTrimString("  x  "));
    EXPECT_EQ("abc", sdkutil::ToLowerCaseString("AbC"));
    EXPECT_EQ("ABC", sdkutil::ToUpperCaseString("aBc"));
}

TEST(StringToolsCompatTest, ReplaceString)
{
    EXPECT_EQ("1221", sdkutil::ReplaceString("122212", "12", "1"));
    EXPECT_EQ("axb", sdkutil::ReplaceString("a-b", "-", "x"));
    EXPECT_THROW(sdkutil::ReplaceString("abc", "", "x"), apsara::odps::sdk::OdpsException);
}

TEST(StringToolsCompatTest, StartEndWith)
{
    EXPECT_TRUE(sdkutil::StartWith("foobar", "foo"));
    EXPECT_FALSE(sdkutil::StartWith("foobar", "obar"));
    EXPECT_TRUE(sdkutil::EndWith("foobar", "bar"));
    EXPECT_FALSE(sdkutil::EndWith("foobar", "foo"));
    EXPECT_FALSE(sdkutil::StartWith("fo", "foobar"));
    EXPECT_FALSE(sdkutil::EndWith("fo", "foobar"));
}

TEST(StringToolsCompatTest, StrictIntegerParse)
{
    // util::StringTo<T>(util/utils.h)是原 apsara::StringTo 整数部分的严格替代
    EXPECT_EQ(123, sdkutil::StringTo<int32_t>("123"));
    EXPECT_EQ(123, sdkutil::StringTo<int32_t>(" 123 "));
    EXPECT_EQ(-7, sdkutil::StringTo<int32_t>("-7"));
    EXPECT_EQ(static_cast<uint16_t>(65535), sdkutil::StringTo<uint16_t>("65535"));
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), sdkutil::StringTo<uint64_t>("18446744073709551615"));
    EXPECT_EQ(std::numeric_limits<int64_t>::min(), sdkutil::StringTo<int64_t>("-9223372036854775808"));

    EXPECT_THROW(sdkutil::StringTo<int32_t>("2147483648"), apsara::odps::sdk::OdpsException);
    EXPECT_THROW(sdkutil::StringTo<uint32_t>("-1"), apsara::odps::sdk::OdpsException);
    EXPECT_THROW(sdkutil::StringTo<uint16_t>("65536"), apsara::odps::sdk::OdpsException);
    EXPECT_THROW(sdkutil::StringTo<int32_t>(""), apsara::odps::sdk::OdpsException);
    EXPECT_THROW(sdkutil::StringTo<int32_t>("12x"), apsara::odps::sdk::OdpsException);
}

TEST(StringToolsCompatTest, ToHexString)
{
    EXPECT_EQ("375F", sdkutil::ToHexString<uint32_t>(0x375f));
    EXPECT_EQ("FF", sdkutil::ToHexString<uint8_t>(0xff));
    EXPECT_EQ("0", sdkutil::ToHexString<uint32_t>(0));
    EXPECT_EQ("4142", sdkutil::ToHexString(std::string("AB")));
    EXPECT_EQ('C', sdkutil::ToHexDigit(12));
    EXPECT_EQ('3', sdkutil::ToHexDigit(3));
}

TEST(StringToolsCompatTest, FindNextCharUtf8)
{
    // "a中b": 中 为 3 字节 (E4 B8 AD)
    std::string s = "a\xe4\xb8\xad" "b";
    EXPECT_EQ(1u, sdkutil::FindNextCharUtf8(s, 0));
    EXPECT_EQ(4u, sdkutil::FindNextCharUtf8(s, 1));
    EXPECT_EQ(5u, sdkutil::FindNextCharUtf8(s, 4));
    EXPECT_THROW(sdkutil::FindNextCharUtf8(s, 5), apsara::odps::sdk::OdpsException);
}
