#include "zlib.h"

#include "configuration.h"
#include "odps_exception.h"
#include "util/string_util.h"
#include "sdk_version.h"
#include "util/utils.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

CompressOption CompressOption::NO_COMPRESS(CompressAlgorithm::ODPS_RAW, 0, 0, 64 * 1024, 0);
CompressOption CompressOption::ZLIB_COMPRESS(CompressAlgorithm::ODPS_ZLIB, Z_BEST_SPEED, Z_DEFAULT_STRATEGY, 64 * 1024, 0);
CompressOption CompressOption::ZSTD_COMPRESS(CompressAlgorithm::ODPS_ZSTD, 0, 0, 1024, 0);
CompressOption CompressOption::LZ4_COMPRESS(CompressAlgorithm::ODPS_LZ4_FRAME, 0, 0, 64 * 1024, 0);
CompressOption CompressOption::ODPS_LZ4_COMPRESS(CompressAlgorithm::ODPS_LZ4, 0, 0, 64 * 1024, 0);

static const std::string& GetUUID() {
    static std::string uuid = util::GenerateUUID();
    return uuid;
}

static std::string GetDefaultUserAgent(const std::string& region)
{
    std::string userAgent = std::string("C++SDK/") + SDK_GIT_BRANCH;
    std::string revision = std::string(SDK_GIT_REVISION);
    if (revision.length() > 8) {
        revision = revision.substr(0, 8);
    }
    userAgent += " (id:" + GetUUID() + ";";
    userAgent += " revision:" + revision;
    if (!region.empty())
    {
        userAgent += "; region:" + region;
    }
    userAgent += ")";
    return userAgent;
}

Configuration::Configuration()
    : chunkSize(DEFAULT_CHUNK_SIZE),
      socketConnectTimeout(DEFAULT_SOCKET_CONNECT_TIMEOUT),
      socketTimeout(DEFAULT_SOCKET_TIMEOUT),
      httpBufferSize(DEFAULT_HTTP_BUFFER_SIZE),
      volumeSerializeChunkSize(DEFAULT_VOLUME_SERIALIZE_CHUNK_SIZE),
      disableSSLVerify(false),
      option(CompressOption::ZLIB_COMPRESS)
{
    userAgent = GetDefaultUserAgent("");
#ifdef ENABLE_VIPSERVER
    useVIPServer = true;
#endif
}

Configuration::Configuration(Account _account, const std::string &_endpoint)
    : account(_account),
      chunkSize(DEFAULT_CHUNK_SIZE),
      socketConnectTimeout(DEFAULT_SOCKET_CONNECT_TIMEOUT),
      socketTimeout(DEFAULT_SOCKET_TIMEOUT),
      httpBufferSize(DEFAULT_HTTP_BUFFER_SIZE),
      volumeSerializeChunkSize(DEFAULT_VOLUME_SERIALIZE_CHUNK_SIZE),
      disableSSLVerify(false),
      odpsEndpoint(_endpoint),
      option(CompressOption::ZLIB_COMPRESS)
{
    if (!util::StartWith(odpsEndpoint, "http://") &&
        !util::StartWith(odpsEndpoint, "https://"))
    {
        throw OdpsException("Endpoint protocol invalid.");
    }
    userAgent = GetDefaultUserAgent(account.GetRegion());
#ifdef ENABLE_VIPSERVER
    useVIPServer = true;
#endif
}

void Configuration::SetAccount(const Account& account)
{
    this->account = account;
    userAgent = GetDefaultUserAgent(account.GetRegion());
}

static std::string ValidateUserAgent(const std::string& str, const std::string& defaultValue = "")
{
    static const std::string illegalChars = "() /:;";

    if (str.find_first_of(illegalChars) != std::string::npos) {
        throw OdpsException("UserAgent can not contain space or illegal characters: () / : ;");
    }

    if (str.empty())
    {
        return defaultValue;
    }
    else
    {
        return str;
    }
}

void Configuration::SetUserAgent(const UserAgent& userAgent)
{
    const auto product = ValidateUserAgent(userAgent.product, "MaxCompute-CPP-SDK-Client");
    const auto version = ValidateUserAgent(userAgent.version, "0.0.0");
    for (auto it = userAgent.properties.begin(); it != userAgent.properties.end(); ++it)
    {
        ValidateUserAgent(it->first);
        ValidateUserAgent(it->second);
    }

    this->userAgent = GetDefaultUserAgent(this->account.GetRegion());
    this->userAgent += (" " + product + "/" + version);
    if (!userAgent.properties.empty())
    {
        auto it = userAgent.properties.begin();
        this->userAgent += " (" + it->first + ":" + it->second;
        ++it;
        for (;it != userAgent.properties.end(); ++it)
        {
            this->userAgent += ("; " + it->first + ":" + it->second);
        }
        this->userAgent += ")";
    }
}

std::string Account::GetId() const
{
    if (credentialsProvider)
    {
        try
        {
            Credentials credentials = credentialsProvider->getCredentials();
            return credentials.accessKeyId();
        }
        catch (...)
        {
            return "";
        }
    }
    return this->id;
}

std::string Account::GetKey() const
{
    if (credentialsProvider)
    {
        try
        {
            Credentials credentials = credentialsProvider->getCredentials();
            return credentials.accessKeySecret();
        }
        catch (...)
        {
            return "";
        }
    }
    return this->key;
}

std::string Account::GetToken() const
{
    if (credentialsProvider)
    {
        try
        {
            Credentials credentials = credentialsProvider->getCredentials();
            return credentials.sessionToken();
        }
        catch (...)
        {
            return "";
        }
    }
    return this->token;
}

Credentials Account::GetCredentials() const
{
    if (credentialsProvider)
    {
        return credentialsProvider->getCredentials();
    }
    return Credentials(this->id, this->key, this->token);
}

} // namespace sdk
} // namespace odps
} // namespace apsara
