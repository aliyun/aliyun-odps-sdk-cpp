#ifndef APSARA_ODPS_SDK_CONFIGURATION_H
#define APSARA_ODPS_SDK_CONFIGURATION_H

#include <string>
#include <map>
#include <memory>

#include "credentials_provider.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

/**
 *	@brief 账号类型.
 */
// 阿里云账号
const char* const ACCOUNT_ALIYUN = "aliyun";
// sts token
const char* const ACCOUNT_STS = "sts";
// 访问令牌
const char* const ACCOUNT_TOKEN = "token";
// 域账号
const char* const ACCOUNT_DOMAIN = "domain";
// 淘宝账号
const char* const ACCOUNT_TAOBAO = "taobao";
// 应用签名
const char* const ACCOUNT_APPLICATION = "app";
// ARN账号（通过CredentialsProvider动态获取凭证）
const char* const ACCOUNT_ARN = "arn";

/**
 * @class Account
 *
 * @brief 保存了访问账号相关信息的类.
 */
class Account
{
protected:
    std::string type;
    std::string token; /**< 访问令牌*/

    std::string id;  /**< 访问的账号名*/
    std::string key; /**< 访问的账号秘钥*/

    std::string applicationSignature; /**< 应用签名*/
    /**
     *	@brief 签名算法名称.
     *
     *	目前淘宝帐号有rsa\hamc-sha1两种, 云帐号只支持hamc-sha1，域帐号不支持客户端签名
     */
    std::string algorithm;

    std::string region;

    /** CredentialsProvider，用于ARN账号动态获取凭证。非ARN账号为nullptr */
    CredentialsProviderPtr credentialsProvider;

public:


    /**
     *	@brief 构造函数.
     *
     *	签名算法默认为hmac-sha1
     */
    Account() : algorithm("hmac-sha1") {}

    /**
     *	@brief 构造函数.
     *
     *	签名算法默认为hmac-sha1
     *	@param type	账号类型
     *	@param token 访问令牌
     */
    Account(std::string _type, std::string _token):
        type(_type),
        token(_token),
        algorithm("hmac-sha1")
    {
    }

    /**
     *	@brief 构造函数.
     *
     *	签名算法默认为hmac-sha1
     *	@param type	账号类型
     *	@param id 访问的账户名
     *	@param key 访问的账户秘钥
     */
    Account(std::string _type, std::string _id, std::string _key):
        type(_type),
        id(_id),
        key(_key),
        algorithm("hmac-sha1")
    {}

    /**
     * @brief ARN构造函数
     * @param provider CredentialsProvider智能指针
     */
    Account(CredentialsProviderPtr provider)
        : type(ACCOUNT_ARN), algorithm("hmac-sha1"), credentialsProvider(provider)
    {}

    /**
     * @brief ARN构造函数，支持region（使用hmac-sha256签名）
     * @param provider CredentialsProvider智能指针
     * @param regionStr region字符串
     */
    Account(CredentialsProviderPtr provider,
            const std::string& regionStr)
        : type(ACCOUNT_ARN), algorithm("hmac-sha256"), credentialsProvider(provider)
    {
        this->region = regionStr;
    }

    virtual ~Account(){}

    virtual std::string GetType() const { return this->type; }

    /**
     * @brief 获取AccessKeyId。若设置了CredentialsProvider则动态获取，否则返回静态值。
     */
    std::string GetId() const;

    /**
     * @brief 获取AccessKeySecret。若设置了CredentialsProvider则动态获取，否则返回静态值。
     */
    std::string GetKey() const;

    virtual std::string GetRegion() const { return this->region; }

    void SetId(const std::string &id) { this->id = id; }

    void SetKey(const std::string &key) { this->key = key; }

    /**
     * @brief 获取Token。若设置了CredentialsProvider则动态获取SessionToken，否则返回静态值。
     */
    virtual std::string GetToken() const;

    /**
     * @brief 获取完整的凭证信息。若设置了CredentialsProvider则动态获取，否则返回静态值。
     *       调用者应优先使用此方法以避免多次调用 getCredentials()。
     * @return Credentials 对象，包含 accessKeyId、accessKeySecret 和 sessionToken
     */
    Credentials GetCredentials() const;

    virtual std::string GetApplicationSignature() const { return this->applicationSignature; }

    virtual std::string GetAlgorithm() const { return this->algorithm; }

    virtual void SetApplicationSignature(std::string applicationSignature)
    {
        this->applicationSignature = applicationSignature;
    }

    virtual void SetAlgorithm(std::string algorithm)
    {
        this->algorithm = algorithm;
    }

    /**
     * @brief 获取CredentialsProvider
     */
    CredentialsProviderPtr GetCredentialsProvider() const
    {
        return credentialsProvider;
    }

    /**
     *	@brief 是否有效. 这个函数不会发请求到服务器，仅检查需要的字段是否已填充。
     */
    virtual bool IsValid() const { return true; }
};

typedef std::shared_ptr<Account> AccountPtr;

class AliyunAccount : public Account
{
public:
    AliyunAccount(std::string access_id, std::string access_key):
        Account(ACCOUNT_ALIYUN, access_id, access_key) {}

    AliyunAccount(const std::string& access_id, const std::string& access_key, const std::string& region):
        Account(ACCOUNT_ALIYUN, access_id, access_key)
    {
        this->region = region;
        this->algorithm = "hmac-sha256";
    }

    virtual bool IsValid() const override
    {
        return id.size() != 0 && key.size() != 0;
    }
};

class AppAccount : public Account
{
public:
    AppAccount(): Account(ACCOUNT_APPLICATION, "", "") {}
    AppAccount(const Account &account): Account(ACCOUNT_APPLICATION, account.GetId(), account.GetKey()) {}

    AppAccount(const std::string &access_id, const std::string &access_key):
        Account(ACCOUNT_APPLICATION, access_id, access_key) {}

    virtual bool IsValid() const override
    {
        return id.size() != 0 && key.size() != 0;
    }
};

class StsToken : public Account
{
public:
    StsToken(): Account(ACCOUNT_STS, "") {}
    StsToken(const Account &account): Account(ACCOUNT_STS, account.GetToken()) {}
    StsToken(const std::string& stsToken): Account(ACCOUNT_STS, stsToken) {}

    virtual bool IsValid() const override
    {
        return token.size() != 0;
    }
};

typedef std::shared_ptr<AppAccount> AppAccountPtr;

class CompressOption
{
public:
    enum CompressAlgorithm
    {
        ODPS_RAW,
        ODPS_ZLIB,
        ODPS_ZSTD,
        ODPS_LZ4_FRAME,
        ODPS_LZ4
    };

    CompressOption(CompressAlgorithm a, int l, int s, int b = 64 * 1024, int m = 0)
    {
        algorithm = a;
        level = l;
        strategy = s;
        bsize = b;
        mode = m;
    }

    static CompressOption NO_COMPRESS;
    static CompressOption ZLIB_COMPRESS;
    static CompressOption ZSTD_COMPRESS;
    static CompressOption LZ4_COMPRESS;
    static CompressOption ODPS_LZ4_COMPRESS;

public:
    CompressAlgorithm algorithm;
    int  mode;       // 0: default(frame), 1: block
    int  bsize;      // default=64K
    int  level;      // 0-9, default=-1
    int  strategy;   // 1-4, default=0
};

struct UserAgent
{
    UserAgent(const std::string& product, const std::string& version) : product(product), version(version)
    {
    }
    std::string product;
    std::string version;
    std::map<std::string, std::string> properties;
};

/**
 * @class Configuration
 *
 * @brief 账号和访问的相关配置.
 */
class Configuration
{
public:
    static const int DEFAULT_CHUNK_SIZE =
        1500 - 4; /**< HTTP chunked编码的默认chunk size*/
    static const int DEFAULT_SOCKET_CONNECT_TIMEOUT =
        180;                                       /**< Socket连接超时默认时间，单位是秒*/
    static const int DEFAULT_SOCKET_TIMEOUT = 300; /**< Socket超时默认时间，单位是秒*/
    static const int DEFAULT_HTTP_BUFFER_SIZE =
        32 * 1024; /**< HTTP读写缓冲区默认大小，单位是字节*/
    static const int DEFAULT_VOLUME_SERIALIZE_CHUNK_SIZE =
        512 * 1024; /**< Volume序列化CRC校验的默认chunk大小，单位是字节*/

    Account account;              /**< Acount类*/
    AppAccount appAccount; /**< AppAccount类，用于双签名*/
    StsToken stsToken;

    std::string accessId;  /**< 访问账户的ID*/
    std::string accessKey; /**< 访问账户的秘钥*/
    std::string tunnelEndpoint;  /**< 关闭路由功能时的tunnel endpoint*/
    std::string regionId;  /**< 内部请求所在的regionID*/
    std::string namespaceId; /**< 访问命名空间*/
    std::string tags; /**< 任务打标Tags*/

    int chunkSize;            /**< HTTP chunked编码的chunk size*/
    int socketConnectTimeout; /**< Socket连接超时时间*/
    int socketTimeout;        /**< Socket超时时间*/
    int httpBufferSize;       /**< HTTP读写缓冲区大小*/
    int volumeSerializeChunkSize; /**< Volume序列化CRC校验的chunk大小*/
    bool disableSSLVerify;    /**< 只有在测试无CA证书服务器时使用*/
    std::string odpsEndpoint; /**< 开启路由功能时的ODPS endpoint*/
    CompressOption option;
    std::string defaultProject;

    std::string userAgent;

    std::string tunnelQuotaName;

    /**
     *	@brief 构造函数.
     *
     *	chunk size设置为默认值
     *	socket连接超时设置为默认值
     *	socket超时设置为默认值
     *	SSL验证默认开启
     */
    Configuration();

    /**
     *	@brief 构造函数.
     *
     *	SSL验证默认开启
     *	@param account 账号
     *	@param endpoint 访问终端地址
     */
    Configuration(Account account, const std::string &endpoint);

    /**
     *	@brief 成员变量account的getter函数.
     *
     *	@return 返回account成员变量的一个拷贝
     */
    const Account& GetAccount() const { return this->account; }

    const AppAccount& GetAppAccount() const { return this->appAccount; }

    void SetAppAccount(AppAccount app) { this->appAccount = app; }

    /**
     *	@brief 设置ODPS tunnelEndpoint并启用tunnel路由功能
     *
     *	@param ODPS tunnelEndpoint
     *	@return 无返回值
     */
    void SetTunnelEndpoint(const std::string &ep) { this->tunnelEndpoint = ep; }

    /**
     *	@brief 成员变量tunnelEndpoint的getter函数.
     *
     *	@return 返回tunnelEndpoint成员变量的一个拷贝
     */
    const std::string& GetTunnelEndpoint()
    {
        return this->tunnelEndpoint;
    }

    /**
     *	@brief 成员变量account的setter函数.
     *
     *	@param account Account类的引用
     *	@return 无返回值
     */
    void SetAccount(const Account& account);

    /**
     *	@brief 设置 STS Token
     *
     *	@param tok STS Token
     *	@return 无返回值
     */
    void SetStsToken(const StsToken& tok) { this->stsToken = tok; }

    const StsToken& GetStsToken() const { return this->stsToken; }

    /**
     *	@brief 成员变量odpsEndpoint的getter函数.
     *
     *	@return 返回odpsEndpoint成员变量的一个拷贝
     */
    const std::string& GetEndpoint() const { return odpsEndpoint; }

    /**
     *	@brief 成员变量odpsEndpoint的setter函数.
     *
     *	@param endpoint 标准string类的引用
     *	@return 无返回值
     */
    void SetEndpoint(const std::string &endpoint) { this->odpsEndpoint = endpoint; }

    /**
     *	@brief 成员变量chunkSize的getter函数.
     *
     *	@return 返回chunk的大小
     */
    int GetChunkSize() { return chunkSize; }

    /**
     *	@brief 成员变量CompressOption的getter函数.
     *
     *	@return 返回CompressOption类型
     */
    const CompressOption& GetCompressOption() const { return option; }

    void SetCompressOption(const CompressOption& opt) { option = opt; }

    /**
     *	@brief 成员变量chunkSize的setter函数.
     *
     *	@param chunkSize chunk的大小
     *	@return 无返回值
     */
    void SetChunkSize(int chunkSize) { this->chunkSize = chunkSize; }

    /**
     *	@brief 成员变量socketConnectTimeout的getter函数.
     *
     *	@return 返回socket连接超时的时间阈值
     */
    int GetSocketConnectTimeout() { return socketConnectTimeout; }

    /**
     *	@brief 成员变量socketConnectTimeout的setter函数.
     *
     *	@param timeout socket连接超时的时间阈值
     *	@return 无返回值
     */
    void SetSocketConnectTimeout(int timeout) { this->socketConnectTimeout = timeout; }

    /**
     *	@brief 成员变量socketTimeout的getter函数.
     *
     *	@return 返回socket超时的时间阈值
     */
    int GetSocketTimeout() { return socketTimeout; }

    /**
     *	@brief 成员变量socketTimeout的setter函数.
     *
     *	@param timeout socket超时的时间阈值，单位是秒
     *	@return 无返回值
     */
    void SetSocketTimeout(int timeout) { this->socketTimeout = timeout; }

    /**
     *	@brief 成员变量httpBufferSize的getter函数.
     *
     *	@return 返回HTTP读写缓冲区大小，单位是字节
     */
    int GetHttpBufferSize() const { return httpBufferSize; }

    /**
     *	@brief 成员变量httpBufferSize的setter函数.
     *
     *	@param bufferSize HTTP读写缓冲区大小，单位是字节
     *	@return 无返回值
     */
    void SetHttpBufferSize(int bufferSize) { this->httpBufferSize = bufferSize; }

    /**
     *	@brief 成员变量volumeSerializeChunkSize的getter函数.
     *
     *	@return 返回Volume序列化CRC校验的chunk大小，单位是字节
     */
    int GetVolumeSerializeChunkSize() const { return volumeSerializeChunkSize; }

    /**
     *	@brief 成员变量volumeSerializeChunkSize的setter函数.
     *
     *	@param chunkSize Volume序列化CRC校验的chunk大小，单位是字节
     *	@return 无返回值
     */
    void SetVolumeSerializeChunkSize(int chunkSize) { this->volumeSerializeChunkSize = chunkSize; }

    /**
     *	@brief 成员变量defaultProject的getter函数.
     */
    const std::string& GetDefaultProject() { return defaultProject; }

    /**
     *	@brief 成员变量defaultProject的setter函数.
     */
    void SetDefaultProject(const std::string &project) { this->defaultProject = project; }

    /**
     *	@brief 未实现.
     */
    void loadConfig(const std::string& resource) {}

#ifdef ENABLE_VIPSERVER
    bool useVIPServer;
    /**
     * @brief 成员变量useVIPServer的setter函数, 仅在vipserver模式编译下有效
     */
    void SetUseVIPServer(bool useVIP) { useVIPServer = useVIP; }

    /**
     * @brief 成员变量useVIPServer的getter函数.
     */
    bool IsUseVIPServer() const { return useVIPServer; }
#endif
    /**
     *	@brief 成员变量regionId的getter函数.
     *
     *	@return 返回regionId成员变量的一个拷贝
     */
    std::string GetRegionId() { return regionId; }

    /**
     *	@brief 成员变量regionId的setter函数.
     *
     *	@param regionId 标准string类的引用
     *	@return 无返回值
     */
    void SetRegionId(const std::string &regionId) { this->regionId = regionId; }

    std::string GetNamespaceId() {return namespaceId;}

    void SetNamespaceId(const std::string& namespaceId)
    {
        this->namespaceId = namespaceId;
    }

    std::string GetTags() {return tags;}

    void SetTags (const std::string& tags) { this->tags = tags; }

    /**
     *	@brief 成员变量userAgent的setter函数.
     *
     *	@param userAgent 用户标识
     *	@return 无返回值
     */
    void SetUserAgent(const UserAgent& userAgent);

    /**
     *	@brief 成员变量userAgent的setter函数.
     *
     *	@param userAgent 用户标识
     *	@return 无返回值
     */
    std::string GetUserAgent() const {
        return userAgent;
    }

    void SetTunnelQuotaName(const std::string& quotaName)
    {
        tunnelQuotaName = quotaName;
    }

    std::string GetTunnelQuotaName() const
    {
        return tunnelQuotaName;
    }
};

typedef std::shared_ptr<Configuration> ConfigurationPtr;

} // namespace sdk
} // namespace odps
} // namespace apsara
#endif
