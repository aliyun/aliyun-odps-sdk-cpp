#ifndef APSARA_ODPS_SDK_CREDENTIALS_PROVIDER_H
#define APSARA_ODPS_SDK_CREDENTIALS_PROVIDER_H

#include <memory>
#include <string>

namespace apsara
{
namespace odps
{
namespace sdk
{

/**
 * @brief 轻量凭证,替代对零信任库 Credentials 的依赖。
 */
class Credentials
{
public:
    Credentials() {}
    Credentials(const std::string& accessKeyId,
                const std::string& accessKeySecret,
                const std::string& sessionToken = "")
        : mAccessKeyId(accessKeyId),
          mAccessKeySecret(accessKeySecret),
          mSessionToken(sessionToken)
    {}

    const std::string& accessKeyId() const     { return mAccessKeyId; }
    const std::string& accessKeySecret() const { return mAccessKeySecret; }
    const std::string& sessionToken() const    { return mSessionToken; }

    void setAccessKeyId(const std::string& v)     { mAccessKeyId = v; }
    void setAccessKeySecret(const std::string& v) { mAccessKeySecret = v; }
    void setSessionToken(const std::string& v)    { mSessionToken = v; }

private:
    std::string mAccessKeyId;
    std::string mAccessKeySecret;
    std::string mSessionToken;
};

/**
 * @brief 抽象凭证提供者。用户继承并实现 getCredentials() 以提供动态凭证
 *        (原零信任/ARN 能力)。SDK 不再依赖外部零信任库。
 */
class CredentialsProvider
{
public:
    virtual ~CredentialsProvider() {}
    virtual Credentials getCredentials() = 0;
};

typedef std::shared_ptr<CredentialsProvider> CredentialsProviderPtr;

} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
