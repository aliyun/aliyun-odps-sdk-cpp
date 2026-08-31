#ifndef APSARA_ODPS_SDK_SIGNER_H
#define APSARA_ODPS_SDK_SIGNER_H

#include "common/http_message.h"
#include "configuration.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{

class RequestSigner
{
public:
    virtual ~RequestSigner() {}
    virtual void Sign(RequestPtr request) = 0;
};

class AliyunRequestSigner : public RequestSigner
{
public:
    AliyunRequestSigner(const std::string& method,
                        const std::string& resourcePath,
                        const std::string& accessId,
                        const std::string& accessKey,
                        const std::string& region)
    {
        this->method = method;
        this->resourcePath = resourcePath;
        this->accessId = accessId;
        this->accessKey = accessKey;
        this->region = region;
    }

    virtual ~AliyunRequestSigner() {}

    void Sign(RequestPtr request) override;

private:
    std::string method;
    std::string resourcePath; // This resourcePath should not have been url encoded.
    std::string accessKey;
    std::string accessId;
    std::string region;
};

class DomainRequestSigner : public RequestSigner
{
public:
    DomainRequestSigner(const std::string& resourcePath,
                        const std::string& accountType,
                        const std::string& accountToken)
    {
        this->resourcePath = resourcePath;
        this->accountType = accountType;
        this->accountToken = accountToken;
    }

    //Override
    void Sign(RequestPtr request);

private:
    std::string resourcePath;
    std::string accountType;
    std::string accountToken;
};

//havana
class AliRequestSigner : public RequestSigner
{
public:
    AliRequestSigner(const std::string& method,
                        const std::string& resourcePath,
                        const Account& account)
        : mMethod(method),
          mResourcePath(resourcePath),
          mAccount(account)
    {
    }

    void Sign(RequestPtr request) override;

private:
    std::string mMethod;
    std::string mResourcePath;
    Account mAccount;
};

class AppRequestSigner : public RequestSigner
{
public:
    AppRequestSigner(const std::string& type,
                     const std::string& method,
                     const std::string& resourcePath,
                     const std::string& accessId,
                     const std::string& accessKey)
    {
        this->type = type;
        this->method = method;
        this->resourcePath = resourcePath;
        this->accessId = accessId;
        this->accessKey = accessKey;
    }

    virtual ~AppRequestSigner() {}

    void Sign(RequestPtr request) override;

private:
    std::string type;
    std::string method;
    std::string resourcePath; // This resourcePath should not have been url encoded.
    std::string accessKey;
    std::string accessId;
};

class StsTokenSigner: public RequestSigner
{
public:
    StsTokenSigner(const StsToken& ststok): tok(ststok.GetToken()) {}
    virtual ~StsTokenSigner() {}

    void Sign(RequestPtr request) override;
private:
    std::string tok;
};

}}}}
#endif
