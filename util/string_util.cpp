/** SDK 内部字符串工具实现:语义与原 apsara string_tools 中被 SDK 使用的
 * 函数保持一致,异常统一改为 OdpsException。
 */
#include "string_util.h"

#include <ctype.h>
#include <sstream>

#include "include/odps_exception.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

char ToHexDigit(uint8_t data)
{
    data = data & 0x0F;
    return static_cast<char>(data < 10 ? ('0' + data) : ('A' + data - 10));
}

/** Split: each char of delim is a separator, empty pieces are kept. **/
std::vector<std::string> SplitString(const std::string& string, const std::string& delim)
{
    if (delim.empty())
    {
        throw OdpsException(LOCAL_ERROR, "Empty delim");
    }
    std::vector<std::string> part;
    if (string.empty())
    {
        return part;
    }
    size_t pos = 0;
    while (pos <= string.size())
    {
        size_t pos2 = string.find_first_of(delim, pos);
        if (pos2 == std::string::npos)
        {
            pos2 = string.size();
        }
        part.push_back(string.substr(pos, pos2 - pos));
        pos = pos2 + 1;
    }
    return part;
}

std::vector<std::string> StringSpliter(const std::string& str, const std::string& delim)
{
    std::vector<std::string> v;
    if (str == "")
    {
        return std::vector<std::string>();
    }
    if (delim == "")
    {
        v.push_back(str);
        return v;
    }
    typedef std::string::size_type size_type;
    size_type s_size = str.size();
    size_type d_size = delim.size();
    if (d_size > s_size)
    {
        v.push_back(str);
        return v;
    }
    size_type pos = 0;
    size_type top = s_size - d_size;
    while (pos <= top)
    {
        size_type pos2 = str.find(delim, pos);
        if (pos2 == std::string::npos)
        {
            pos2 = s_size;
        }
        if (pos2 != pos)
        {
            v.push_back(str.substr(pos, pos2 - pos));
        }
        pos = pos2 + d_size;
    }
    if (pos < s_size)
    {
        v.push_back(str.substr(pos));
    }
    return v;
}

/** Trim, LeftTrim and RightTrim */
std::string LeftTrimString(const std::string& string, const char trimChar)
{
    size_t pos = 0;
    while (pos < string.size() && string[pos] == trimChar) ++pos;
    return string.substr(pos);
}

std::string RightTrimString(const std::string& string, const char trimChar)
{
    size_t pos = string.size() - 1;
    while (pos != (size_t)-1 && string[pos] == trimChar) --pos;
    return string.substr(0, pos + 1);
}

std::string TrimString(
        const std::string& string,
        const char leftTrimChar,
        const char rightTrimChar)
{
    return LeftTrimString(RightTrimString(string, rightTrimChar), leftTrimChar);
}

std::string ToLowerCaseString(const std::string& orig)
{
    std::string lowerCase(orig);
    std::string::size_type size = lowerCase.size();
    std::string::size_type pos = 0;
    for (; pos < size; ++pos)
    {
        if (isupper(lowerCase[pos]))
        {
            lowerCase[pos] = tolower(lowerCase[pos]);
        }
    }
    return lowerCase;
}

std::string ToUpperCaseString(const std::string& orig)
{
    std::string upperCase(orig);
    std::string::size_type size = upperCase.size();
    std::string::size_type pos = 0;
    for (; pos < size; ++pos)
    {
        if (islower(upperCase[pos]))
        {
            upperCase[pos] = toupper(upperCase[pos]);
        }
    }
    return upperCase;
}

bool StartWith(const std::string& input, const std::string& pattern)
{
    if (input.length() < pattern.length())
    {
        return false;
    }

    size_t i = 0;
    while (i < pattern.length()
        && input[i] == pattern[i])
    {
        i++;
    }

    return i == pattern.length();
}

bool EndWith(const std::string& input, const std::string& pattern)
{
    if (input.length() < pattern.length())
    {
        return false;
    }

    std::string::const_reverse_iterator it1 = input.rbegin();
    std::string::const_reverse_iterator it2 = pattern.rbegin();

    while (it2 != pattern.rend()
        && *it1 == *it2)
    {
        ++it1;
        ++it2;
    }

    return it2 == pattern.rend();
}

/* string replace */
std::string ReplaceString(const std::string& origin_string,
        const std::string& old_value,
        const std::string& new_value)
{
    if (old_value.empty())
    {
        throw OdpsException(LOCAL_ERROR, "Empty old_value in ReplaceString: " + origin_string);
    }

    std::ostringstream s;
    std::string::size_type pos = 0;
    std::string::size_type pos_previous = 0;
    for (;std::string::npos != pos;)
    {
        pos_previous = pos;
        if ((pos = origin_string.find(old_value, pos)) != std::string::npos)
        {
            if (pos > pos_previous)
            {
                s << origin_string.substr(pos_previous, pos - pos_previous);
            }
            s << new_value;
            pos += old_value.length();
        }
        else
        {
            if (pos_previous + 1 <= origin_string.length())
            {
               s << origin_string.substr(pos_previous);
            }
            break;
        }
    }
    return s.str();
}

template<>
std::string ToHexString(const std::string& str)
{
    std::string ret;
    for (size_t i = 0; i < str.size(); ++i)
    {
        ret.append(ToHexString(str[i]));
    }
    return ret;
}

/** Find the beginning position of the next utf8 char after position /pos/. */
std::string::size_type FindNextCharUtf8(const std::string& str, std::string::size_type pos)
{
    std::string::size_type byteCount = str.length();
    std::string::size_type currPos = pos + 1;
    if (currPos > byteCount)
    {
        throw OdpsException(LOCAL_ERROR, "FindNextCharUtf8: index out of range");
    }
    while (currPos < byteCount)
    {
        if ((str[currPos] & 0x80) == 0x00 ||
            (str[currPos] & 0xE0) == 0xC0 ||
            (str[currPos] & 0xF0) == 0xE0 ||
            (str[currPos] & 0xF8) == 0xF0 ||
            (str[currPos] & 0xFC) == 0xF8 ||
            (str[currPos] & 0xFE) == 0xFC)
        {
            break;
        }
        if ((str[currPos] & 0xC0) != 0x80) /// Then possible value is: 1111 111x
        {
             throw OdpsException(LOCAL_ERROR,
                 "Unrecognizable utf8 byte: 0x" + ToHexString(str[currPos]));
        }
        ++currPos;
    }//while
    return currPos;
}

}  // namespace util
}  // namespace sdk
}  // namespace odps
}  // namespace apsara
