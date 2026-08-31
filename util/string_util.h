/** @file string_util.h
 *
 * SDK 内部字符串工具(原 apsara string_tools 中被 SDK 使用的函数,
 * 搬入 apsara::odps::sdk::util;解析/格式错误统一抛 OdpsException)。
 * 实现见 util/string_util.cpp。
 */

#ifndef APSARA_ODPS_SDK_UTIL_STRING_UTIL_H
#define APSARA_ODPS_SDK_UTIL_STRING_UTIL_H

#include <stdint.h>
#include <string>
#include <vector>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

    /** Find the starting offset of the next character after pos.
     *  @param str string input string, utf-8 format assumed
     *  @param pos string::size_type position in bytes, not in character
     *
     *  @return string::size_type the start pos of the next utf8 character.
     *   return str.size() if pos belongs to [lastCharPos, str.size())
     *   throw OdpsException if pos >= str.size() or the input is not utf-8.
     */
    std::string::size_type FindNextCharUtf8(const std::string& str, std::string::size_type pos);

    /**
     * convert num to its hex expression in char format,
     * such as ToHexDigit(3) == '3', ToHexDigit(12) == 'C'
     */
    char ToHexDigit(uint8_t data);

    /**
     * convert numerical data to its hex expression in string format
     * such as ToHexString(0x375f) == "375f", ToHexString(0x0000ff) == "ff"
     */
    template<typename T>
    std::string ToHexString(const T &data)
    {
        uint32_t size = sizeof(T) * 8;
        T dataCopy = data;
        std::string str;
        do {
            uint8_t n = dataCopy & 0x0f;
            char c = static_cast<char>(n < 10 ? ('0' + n) : ('A' + n - 10));
            str.insert(str.begin(), c);
        } while ((dataCopy >>= 4) && (size -= 4));
        return str;
    }
    template<>
    std::string ToHexString(const std::string& data);

    /** Split string by delimeter.
     *  Each character in /delimeter/ is treated as a separator (find_first_of
     *  semantics); empty pieces are kept. Empty input yields an empty vector;
     *  empty delimeter throws OdpsException.
     */
    std::vector<std::string> SplitString(
        const std::string& str,
        const std::string& delimeter=" ");

    /**
     * Uses the whole delim as a separator, scans the target str from begin
     * to end and drops "".
     * @return vector of substrings split by delim, without ""
     */
    std::vector<std::string> StringSpliter(
        const std::string& str,
        const std::string& delim);

    /** Remove whitespaces in the beginning and the end of a string */
    std::string TrimString(
            const std::string& str,
            const char leftTrimChar = ' ',
            const char rightTrimChar = ' ');
    std::string LeftTrimString(const std::string& str, const char trimChar = ' ');
    std::string RightTrimString(const std::string& str, const char trimChar = ' ');

    std::string ToLowerCaseString(const std::string& orig);
    std::string ToUpperCaseString(const std::string& orig);

    /** Replace all (non-overlapping) occurrences of old_value with new_value.
        e.g. ReplaceString("122212","12","1") == "1221"
    */
    std::string ReplaceString(const std::string& origin_string,
        const std::string& old_value, const std::string& new_value);

    /**
     * @brief Returns whether the std::string begins with the pattern passed in.
     */
    bool StartWith(const std::string& input, const std::string& pattern);

    /**
     * @brief Returns whether the std::string ends with the pattern passed in.
     */
    bool EndWith(const std::string& input, const std::string& pattern);

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // APSARA_ODPS_SDK_UTIL_STRING_UTIL_H
