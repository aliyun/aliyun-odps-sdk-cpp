#ifndef APSARA_ODPS_SDK_COMMON_UTILS_H
#define APSARA_ODPS_SDK_COMMON_UTILS_H

#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include <nlohmann/json.hpp>

#include "common/http_message.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "include/configuration.h"
#include "odps_exception.h"
#include "max_storage_api/commons.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal
{
    class ODPSTableSchema;
    using ODPSTableSchemaPtr = std::shared_ptr<ODPSTableSchema>;
}

namespace util
{

const std::string HIDDEN_CONTENT = "******";
const uint64_t BUCKET_SIZE = 102400;

extern const char* TUNNEL_DATE_TIME_FORMAT;
extern const char* TUNNEL_DATE_FORMAT;

static const int32_t NANOS_PER_SECOND = 1000000000;
static const int32_t SECONDS_PER_MINUTE = 60;
static const int32_t SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
static const int32_t SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

namespace detail
{
inline bool IsIntegerParseSpace(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f';
}

inline const char* SkipIntegerParseSpaces(const char* p)
{
    while (IsIntegerParseSpace(*p))
    {
        ++p;
    }
    return p;
}

inline void CheckIntegerParseTail(const char* end, const std::string& str)
{
    for (const char* t = SkipIntegerParseSpaces(end); *t != '\0'; ++t)
    {
        throw OdpsException(LOCAL_ERROR, "Unknown char in integer string: " + str);
    }
}
}  // namespace detail

/**
 * @brief 严格整数解析(原 apsara::StringTo<T> 的替代品)
 *
 * 允许前导空白;尾随字符必须全为空白;无符号类型拒绝前导 '-';
 * 解析失败、溢出或超出 T 的表示范围时抛 OdpsException(LOCAL_ERROR)。
 */
template <typename T>
typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, T>::type
StringTo(const std::string& str)
{
    const char* p = detail::SkipIntegerParseSpaces(str.c_str());
    if (*p == '\0')
    {
        throw OdpsException(LOCAL_ERROR, "Cannot cast empty string to integer: " + str);
    }
    errno = 0;
    char* end = NULL;
    long long num = std::strtoll(p, &end, 10);
    if (errno == ERANGE || end == p)
    {
        throw OdpsException(LOCAL_ERROR, "Cannot cast to integer: " + str);
    }
    detail::CheckIntegerParseTail(end, str);
    if (num < static_cast<long long>(std::numeric_limits<T>::min()) ||
        num > static_cast<long long>(std::numeric_limits<T>::max()))
    {
        throw OdpsException(LOCAL_ERROR, "Integer out of range: " + str);
    }
    return static_cast<T>(num);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && !std::is_signed<T>::value
                            && !std::is_same<T, bool>::value,
                        T>::type
StringTo(const std::string& str)
{
    const char* p = detail::SkipIntegerParseSpaces(str.c_str());
    if (*p == '-')
    {
        throw OdpsException(LOCAL_ERROR, "Unknown char '-' for unsigned integer: " + str);
    }
    if (*p == '\0')
    {
        throw OdpsException(LOCAL_ERROR, "Cannot cast empty string to integer: " + str);
    }
    errno = 0;
    char* end = NULL;
    unsigned long long num = std::strtoull(p, &end, 10);
    if (errno == ERANGE || end == p)
    {
        throw OdpsException(LOCAL_ERROR, "Cannot cast to integer: " + str);
    }
    detail::CheckIntegerParseTail(end, str);
    if (num > static_cast<unsigned long long>(std::numeric_limits<T>::max()))
    {
        throw OdpsException(LOCAL_ERROR, "Integer out of range: " + str);
    }
    return static_cast<T>(num);
}


apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiVolumeRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& volume);

apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiTableRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& schema,
    const std::string& table);

apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiInstanceRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& instance);

std::string GenerateUUID();

bool GUnZip(const std::string& input, std::string& output);

std::string GenerateMd5Signature(const std::string& content);

/**
 *  @brief 生成MD5签名字符串
 *
 *  @param content 输入流
 *  @param length  用于存储输入总字节数
 *
 *  @return MD5签名
 */
std::string GenerateMd5Signature(std::stringstream& content, long& length);

bool IsSensitive(const std::string& key);

void InitSensitiveKeys();

std::string Base64Encode(const std::string& input);

std::string Base64Decode(const std::string& input);

template <typename SearchableType, typename KeyType>
static bool ContainsKey(const SearchableType& input, const KeyType& key)
{
    return input.find(key) != input.end();
}

std::string StringListJoin(const std::vector<std::string>& input,
                           const std::string& delim);

const extern std::vector<std::string> SENSITIVE_KEYS;

template <typename... Ts>
std::string MakeString(Ts&&... vals)
{
    std::ostringstream ss;
    int dummy[] = {0, ((ss << vals << " "), 0)...};
    static_cast<void>(dummy);
    std::string tmp = ss.str();
    tmp.resize(tmp.size() - 1);
    return tmp;
}

// JSON 序列化入口(基于 nlohmann::json;原实现经 apsara::Jsonizable 转发,
// 输出格式保持兼容:pretty = dump(4),compact = dump())。
// 自定义类型通过其所在命名空间的 to_json/from_json 自由函数接入(见各头文件)。
template <typename T>
std::string ToJsonString(const T& t)
{
    return nlohmann::json(t).dump(4);
}

template <typename T>
std::string ToJsonCompactString(const T& t)
{
    return nlohmann::json(t).dump();
}

template <typename T>
void FromJsonString(T& t, const std::string& str)
{
    nlohmann::json::parse(str).get_to(t);
}

template <typename... Ts>
void OdpsThrow(const std::string& error_code, Ts&&... messages)
{
    throw OdpsException(error_code, MakeString(std::forward<Ts>(messages)...));
}

[[noreturn]] void TunnelThrow(internal::Response& response, const std::string& endpoint);

void TunnelThrow(const std::string& code, const std::string& message, const std::string& reqId, const std::string& endpoint);

void TunnelThrow(const std::string& code, const std::string& message);

void TunnelThrow(const std::string& message);

template <typename... Ts>
void OdpsThrowWithRequestId(const std::string& error_code,
                            const std::string& requestId,
                            Ts&&... messages)
{
    throw OdpsException(
        error_code, MakeString(std::forward<Ts>(messages)...), requestId);
}

template <typename... Ts>
void TunnelThrow(const std::string& error_code, Ts&&... messages)
{
    throw OdpsTunnelException(error_code,
                              MakeString(std::forward<Ts>(messages)...));
}

template <typename... Ts>
void TunnelThrowWithRequestId(const std::string& error_code,
                              const std::string& requestId,
                              Ts&&... messages)
{
    throw OdpsTunnelException(
        error_code, MakeString(std::forward<Ts>(messages)...), requestId);
}

static inline std::string gmt_strftime(int64_t t, const char* format)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[128] = "\0";
    strftime(buf, 128, format, &tm);
    return std::string(buf);
}

static inline int64_t gmt_strptime(const std::string& time, const char* format)
{
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    if (!strptime(time.c_str(), format, &tm))
    {
        throw OdpsTunnelException("InvalidArgument",
                                  "invalid date value:" + time);
    }
    return timegm(&tm);
}

// debug printers.
template <typename... Ts>
void Println(Ts&&... vals)
{
    std::cout << MakeString(vals...) << std::endl;
}

template <typename... Ts>
void Errorln(Ts&&... vals)
{
    std::cerr << MakeString(vals...) << std::endl;
}

void PipeBetweenZeroCopyStreams(
    google::protobuf::io::ZeroCopyInputStream* input,
    google::protobuf::io::ZeroCopyOutputStream* output);

std::string GetDate();

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
