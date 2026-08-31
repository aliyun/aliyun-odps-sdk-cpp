#include "signer.h"
#include "util/base64.h"
#include "util/hmac.h"
#include "util/sha1.h"
#include "util/string_util.h"
#include "common/http_connection.h"
#include "error_code.h"
#include "openssl/hmac.h"
#include "util/utils.h"
#include <sstream>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

using util::HMAC;

#define NEW_LINE "\n"

static std::string buildCanonicalizedResource(
    const std::string &resourcePath,
    const std::map<std::string, std::string> &parameters)
{
    std::ostringstream builder;
    builder << resourcePath;

    char separater = '?';
    typeof(parameters.begin()) it = parameters.begin();
    for (; it != parameters.end(); ++it)
    {
        builder << separater;
        builder << it->first;
        if (it->second != "")
        {
            builder << "=" << it->second;
        }
        separater = '&';
    }

    return builder.str();
}

static std::string buildCanonicalString(
    const std::string &method,
    const std::string &resourcePath,
    RequestPtr request,
    const std::string &prefix)
{
    std::ostringstream builder;
    builder << method << NEW_LINE;

    std::map<std::string, std::string> headers;
    request->GetHeaders(headers);
    std::map<std::string, std::string> headersToSign;

    typeof(headers.begin()) it = headers.begin();
    for (; it != headers.end(); ++it)
    {
        std::string lowerKey = util::ToLowerCaseString(it->first);

        if (lowerKey == util::ToLowerCaseString(CONTENT_TYPE) || lowerKey == util::ToLowerCaseString(CONTENT_MD5) || lowerKey == util::ToLowerCaseString(DATE) || util::StartWith(lowerKey, prefix))
        {
            headersToSign[lowerKey] = it->second;
        }
    }

    if (headersToSign.find(util::ToLowerCaseString(CONTENT_TYPE)) == headersToSign.end())
    {
        headersToSign[util::ToLowerCaseString(CONTENT_TYPE)] = "";
    }
    if (headersToSign.find(util::ToLowerCaseString(CONTENT_MD5)) == headersToSign.end())
    {
        headersToSign[util::ToLowerCaseString(CONTENT_MD5)] = "";
    }

    // Add params that have the prefix "x-oss-"
    std::map<std::string, std::string> params;
    request->GetParameters(params);
    it = params.begin();
    for (; it != params.end(); ++it)
    {
        if (util::StartWith(it->first, prefix))
        {
            headersToSign[it->first] = it->second;
        }
    }

    // Add all headers to sign to the builder
    it = headersToSign.begin();
    for (; it != headersToSign.end(); ++it)
    {
        std::string key = it->first;
        std::string value = it->second;
        if (util::StartWith(key, prefix))
        {
            builder << key << ":" << value;
        }
        else
        {
            builder << value;
        }

        builder << "\n";
    }

    // Add canonical resource
    builder << buildCanonicalizedResource(resourcePath, params);

    return builder.str();
}

static std::string HmacSHA(const std::string& data, const std::string& key, const EVP_MD* md)
{
    int resultLen = EVP_MD_size(md);  // 获取哈希算法输出长度

    uint8_t result[resultLen];
    unsigned int hmac_len = static_cast<unsigned int>(resultLen);
    ::HMAC(md,
           key.c_str(),
           static_cast<int>(key.size()),
           reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
           result, &hmac_len);

    return std::string(reinterpret_cast<const char *>(result), resultLen);
}

static std::string HmacSHA1(const std::string& data, const std::string& key)
{
    return HmacSHA(data, key, EVP_sha1());
}

static std::string HmacSha256(const std::string& data, const std::string& key)
{
    return HmacSHA(data, key, EVP_sha256());
}

static std::string HmacSHA1Base64(const std::string& data, const std::string& key)
{
    const std::string& sha1 = HmacSHA1(data, key);
    std::istringstream iss(sha1);
    std::ostringstream oss;
    util::Base64Encoding(iss, oss);
    return oss.str();
}

static std::string GetV4SignatureKey(const std::string& key, const std::string& date, const std::string& region)
{
    const std::string& kSecret = "aliyun_v4" + key;
    const std::string& kDate = HmacSha256(date, kSecret);
    const std::string& kRegion = HmacSha256(region, kDate);
    const std::string& kService = HmacSha256("odps", kRegion);
    return HmacSha256("aliyun_v4_request", kService);
}

void AliyunRequestSigner::Sign(RequestPtr request)
{
    if (accessId.length() > 0 && accessKey.length() > 0)
    {
        if (region.empty())
        {
            std::string stringToSign = buildCanonicalString(method, resourcePath, request, HEADER_ODPS_PREFIX);

            HMAC hmac(reinterpret_cast<const uint8_t *>(accessKey.data()), accessKey.size());
            hmac.add(reinterpret_cast<const uint8_t *>(stringToSign.data()), stringToSign.size());

            std::istringstream iss(std::string(reinterpret_cast<const char *>(hmac.result()), util::SHA1_DIGEST_BYTES));
            std::ostringstream oss;
            util::Base64Encoding(iss, oss);

            std::string signature = oss.str();
            request->SetHeader(AUTHORIZATION, "ODPS " + accessId + ":" + signature);
        }
        else
        {
            const std::string& stringToSign = buildCanonicalString(method, resourcePath, request, HEADER_ODPS_PREFIX);
            const std::string& date = util::GetDate();
            const std::string& key = GetV4SignatureKey(accessKey, date, region);
            const std::string& signature = HmacSHA1Base64(stringToSign, key);
            const std::string& credential = accessId + "/" + date + "/" + region + "/odps/aliyun_v4_request";
            request->SetHeader(AUTHORIZATION, "ODPS " + credential + ":" + signature);
        }
    }
    else if (accessId.length() > 0)
    {
        request->SetHeader(AUTHORIZATION, accessId);
    }
}

void DomainRequestSigner::Sign(RequestPtr request)
{
    if (accountType == ACCOUNT_DOMAIN && !accountToken.empty())
    {
        std::string authorization = "account_provider:" + accountType + ",access_token:" + accountToken;
        request->SetHeader(AUTHORIZATION, authorization);
        return;
    }
}

void AliRequestSigner::Sign(RequestPtr request)
{
    if (!mAccount.GetToken().empty())
    {
        request->SetHeader(HEADER_ALI_DATA_SERVICE, "ODPS");
        request->SetHeader(AUTHORIZATION, "Bearer " + mAccount.GetToken());
        return;
    }
    else
    {
        std::string stringToSign = buildCanonicalString(mMethod, mResourcePath,
                                                        request, HEADER_ALI_DATA_PREFIX);

        std::string signtype = mAccount.GetAlgorithm();
        std::string signature = "";

        if (signtype == "hmac-sha1")
        {
            HMAC hmac(reinterpret_cast<const uint8_t *>(mAccount.GetKey().data()), mAccount.GetKey().size());
            hmac.add(reinterpret_cast<const uint8_t *>(stringToSign.data()), stringToSign.size());

            std::istringstream iss(std::string(reinterpret_cast<const char *>(hmac.result()), util::SHA1_DIGEST_BYTES));
            std::ostringstream oss;
            util::Base64Encoding(iss, oss);

            signature = oss.str();
        }
        else
        {
            throw OdpsException(INTERNAL_ERROR, "Sign algorithm not support.");
        }

        request->SetHeader(AUTHORIZATION, mAccount.GetId() + ":" + signature);
    }
}

void AppRequestSigner::Sign(RequestPtr request)
{
    if (accessId.length() > 0 && accessKey.length() > 0)
    {
        std::string stringToSign = request->GetHeader(AUTHORIZATION);
        if (stringToSign.empty())
        {
            throw OdpsException(INTERNAL_ERROR, "String to sign cannot be empty. Please contact developer.");
        }

        HMAC hmac(reinterpret_cast<const uint8_t *>(accessKey.data()), accessKey.size());
        hmac.add(reinterpret_cast<const uint8_t *>(stringToSign.data()), stringToSign.size());

        std::istringstream iss(std::string(reinterpret_cast<const char *>(hmac.result()), util::SHA1_DIGEST_BYTES));
        std::ostringstream oss;
        util::Base64Encoding(iss, oss);

        std::string signature = oss.str();
        std::ostringstream formatted_signature;
        formatted_signature << "account_provider:" << util::ToLowerCaseString(type);
        formatted_signature << ",signature_method:"
                            << "hmac-sha1";
        formatted_signature << ",access_id:" << accessId;
        formatted_signature << ",signature:" << signature;

        request->SetHeader(APP_AUTHENTICATION, formatted_signature.str());
    }
    else
    {
        throw OdpsException(INVALID_ARGUMENT, "App Account's accessId and accessKey cannot be empty.");
    }
}

void StsTokenSigner::Sign(RequestPtr request)
{
    if (!tok.empty())
    {
        request->SetHeader("authorization-sts-token", tok);
    }
}

} // namespace internal
} // namespace sdk
} // namespace odps
} // namespace apsara