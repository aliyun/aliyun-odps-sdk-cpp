#include "common/odps_table_schema.h"
#include "error_code.h"
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string/trim.hpp"

namespace apsara { namespace odps { namespace sdk {

static bool CheckTypeInfoChar(unsigned char c)
{
    return std::isalnum(c) || c == '_' || c == '.' || std::isspace(c);
}

static std::vector<std::string> TypeInfoStringTokenize(const std::string& infoString)
{
    std::vector<std::string> buffer;
    for (size_t begin = 0, end = 1; end <= infoString.length(); )
    {
        if(end == infoString.length() ||
            !CheckTypeInfoChar(infoString[end]) ||
            !CheckTypeInfoChar(infoString[end - 1]))
        {
            std::string tok = infoString.substr(begin, end - begin);
            if (!tok.empty())
            {
                boost::trim(tok);
                // if not using lambda expr to cast to unsigned char, there's a potential UB in std::toupper
                transform(tok.begin(), tok.end(), tok.begin(), [](unsigned char c){ return std::toupper(c); });
                buffer.push_back(tok);
            }
            begin = end;
        }
        end++;
    }
    return buffer;
}

static ODPSColumnType TypeInfoTokenToEnumerator(const std::string& token)
{
    static std::unordered_map<std::string, ODPSColumnType> sMap = {
        {"BIGINT", ODPS_BIGINT},
        {"DOUBLE", ODPS_DOUBLE},
        {"BOOLEAN", ODPS_BOOLEAN},
        {"DATETIME", ODPS_DATETIME},
        {"STRING", ODPS_STRING},
        {"DECIMAL", ODPS_DECIMAL},
        {"TINYINT", ODPS_TINYINT},
        {"SMALLINT", ODPS_SMALLINT},
        {"INT", ODPS_INTEGER}, // differs from INTEGER
        {"CHAR", ODPS_CHAR},
        {"VARCHAR", ODPS_VARCHAR},
        {"BINARY", ODPS_BINARY},
        {"DATE", ODPS_DATE},
        {"TIMESTAMP", ODPS_TIMESTAMP},
        {"TIMESTAMP_NTZ", ODPS_TIMESTAMP_NTZ},
        {"FLOAT", ODPS_FLOAT},
        {"INTERVAL_YEAR_MONTH", ODPS_INTERVAL_YEAR_MONTH},
        {"INTERVAL_DAY_TIME", ODPS_INTERVAL_DAY_TIME},
        {"ARRAY", ODPS_ARRAY},
        {"MAP", ODPS_MAP},
        {"STRUCT", ODPS_STRUCT},
        {"JSON", ODPS_JSON},
    };
    auto it = sMap.find(token);
    if (it == sMap.end())
    {
        throw OdpsTunnelException(NOT_IMPLEMENTED, "Unsupported type name: " + token);
    }
    return it->second;
}

static inline bool ExpectToken(std::vector<std::string>::const_iterator& it,
    const std::vector<std::string>::const_iterator& end,
    const std::string& str,
    bool optional = false)
{
    if (it == end)
    {
        if (optional)
        {
            return false;
        }
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo string corrupted: expect " + str + " , but found terminated.");
    }
    if (*it != str)
    {
        if (optional)
        {
            return false;
        }
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo string corrupted: expect " + str + " , but found: " + *it);
    }
    it++;
    return true;
}

static inline int32_t ExpectInteger(std::vector<std::string>::const_iterator& it,
    const std::vector<std::string>::const_iterator& end)
{
    if (it == end)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo string corrupted: expect integer literal, but found terminated.");
    }
    try
    {
        int32_t tmp = boost::lexical_cast<int32_t>(*it);
        it++;
        return tmp;
    }
    catch (const boost::bad_lexical_cast&)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo string corrupted: expect integer literal, but found: " + *it);
    }
}

static inline std::string ExpectString(std::vector<std::string>::const_iterator& it,
    const std::vector<std::string>::const_iterator& end)
{
    if (it == end)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo string corrupted: expect string, but found terminated.");
    }
    std::string tmp = *it;
    it++;
    return tmp;
}

std::string ODPSColumnTypeInfo::ToTypeString() const
{
    std::string tmpstr = GetTypeName(mType);
    switch(mType)
    {
        case ODPS_DECIMAL:
        {
            // for decimal, we should format its precision and scale.
            if (mPrecision != 0 || mScale != 0)
            {
                tmpstr += "(" + std::to_string(mPrecision) + "," + std::to_string(mScale) + ")";
            }
        }
        break;
        case ODPS_CHAR:
        case ODPS_VARCHAR:
        {
            tmpstr += "(" + std::to_string(mSpecifiedLength) + ")";
        }
        break;
        case ODPS_ARRAY:
        {
            tmpstr += "<" + mSubTypes.at(0).ToTypeString() + ">";
        }
        break;
        case ODPS_MAP:
        {
            tmpstr += "<" + mSubTypes.at(0).ToTypeString() + "," + mSubTypes.at(1).ToTypeString() + ">";
        }
        break;
        case ODPS_STRUCT:
        {
            tmpstr += "<";
            for (size_t i = 0; i < mSubTypes.size(); i++)
            {
                tmpstr += mSubTypes[i].ToTypeString();
                if (i != (mSubTypes.size() - 1))
                {
                    tmpstr += ",";
                }
            }
            tmpstr += ">";
            if (mSubTypes.size() == 0)
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo implies empty struct?");
            }
        }
        break;
        default: // nothing special, just return.
        break;
    }
    return tmpstr;
}

ODPSColumnTypeInfo::ODPSColumnTypeInfo(std::vector<std::string>::const_iterator& begin,
    std::vector<std::string>::const_iterator end)
{
    if (begin == end)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo string corrupted: expect a type, but terminated.");
    }
    // the type of myself
    mType = TypeInfoTokenToEnumerator(*begin);
    begin++;
    switch(mType)
    {
        case ODPS_DECIMAL:
        {
            // for decimal, we should extract its precision and scale.
            bool new_decimal = ExpectToken(begin, end, "(", true);
            if (new_decimal)
            {
                mPrecision = ExpectInteger(begin, end);
                ExpectToken(begin, end, ",");
                mScale = ExpectInteger(begin, end);
                ExpectToken(begin, end, ")");
            }
            else
            {
                // set precision 54 and scale 18 to indicate legacy decimal
                mPrecision = 54;
                mScale = 18;
            }
        }
        break;
        case ODPS_CHAR:
        case ODPS_VARCHAR:
        {
            ExpectToken(begin, end, "(");
            mSpecifiedLength = ExpectInteger(begin, end);
            ExpectToken(begin, end, ")");
        }
        break;
        case ODPS_ARRAY:
        {
            ExpectToken(begin, end, "<");
            mSubTypes.push_back(ODPSColumnTypeInfo(begin, end));
            ExpectToken(begin, end, ">");
        }
        break;
        case ODPS_MAP:
        {
            ExpectToken(begin, end, "<");
            mSubTypes.push_back(ODPSColumnTypeInfo(begin, end));
            ExpectToken(begin, end, ",");
            mSubTypes.push_back(ODPSColumnTypeInfo(begin, end));
            ExpectToken(begin, end, ">");
        }
        break;
        case ODPS_STRUCT:
        {
            ExpectToken(begin, end, "<");
            while(*begin != ">")
            {
                // NAME:TYPE
                std::string name = ExpectString(begin, end);
                ExpectToken(begin, end, ":");
                ODPSColumnTypeInfo memberType(begin, end);
                memberType.mMemberName = name;
                mSubTypes.push_back(memberType);
                if (*begin != ">")
                {
                    ExpectToken(begin, end, ",");
                }
            }
            ExpectToken(begin, end, ">");
            if (mSubTypes.size() == 0)
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo implies empty struct?");
            }
        }
        break;
        default: // nothing special, just return.
        break;
    }
}

ODPSColumnTypeInfo ODPSColumnTypeInfo::ParseTypeInfoString(const std::string &infoString)
{
    std::vector<std::string> tokens = TypeInfoStringTokenize(infoString);
    auto cbegin = tokens.cbegin();
    ODPSColumnTypeInfo tmp(cbegin, tokens.cend());
    if (cbegin != tokens.cend())
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "TypeInfo has trailing characters: " + *cbegin);
    }
    return tmp;
}

}}}