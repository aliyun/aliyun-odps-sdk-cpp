
#ifndef APSARA_ODPS_TUNNEL_TESTING_UTILS_H
#define APSARA_ODPS_TUNNEL_TESTING_UTILS_H

#include <nlohmann/json.hpp>
#include "include/odps_api.h"
#include "include/odps_tunnel.h"
#include "include/credentials_provider.h"
#include "util/utils.h"
#include <string>

namespace apsara { namespace odps { namespace sdk {
class TestStaticCredentialsProvider : public CredentialsProvider {
public:
    TestStaticCredentialsProvider(const std::string& id, const std::string& key)
        : mId(id), mKey(key) {}
    Credentials getCredentials() override { return Credentials(mId, mKey); }
private:
    std::string mId, mKey;
};
}}}

using namespace apsara::odps::sdk;

class AtomicCounter
{
public:
    AtomicCounter()
    {
        counter = 0;
    }

    inline int GetValue() const
    {
        return counter;
    }

    inline void SetValue(int c)
    {
        counter = c;
    }

    inline void Add(int i)
    {
//        asm volatile(
//            LOCK "addl %1,%0"
//            :"=m" (counter)
//            :"ir" (i), "m" (counter));
        __sync_fetch_and_add(&counter, i);  //lxk, for x86 and arm
	}

    inline void Sub(int i)
    {
//        asm volatile(
//            LOCK "subl %1,%0"
//            :"=m" (counter)
//            :"ir" (i), "m" (counter));
        __sync_fetch_and_sub(&counter, i);  //lxk, for x86 and arm
	}

    inline int AddAndReturn(int i)
    {
//		int ret = i;
//        asm volatile(
//            LOCK "xaddl %0, %1"
//            :"+r" (i), "+m" (counter)
//            : : "memory");
//        return i + ret;
        return __sync_add_and_fetch( &counter, i ) ;  //lxk, for x86 and arm
	}

    inline int SubAndReturn(int i)
    {
        return AddAndReturn(-i);
    }

    inline int SubAndTest(int i)
    {
//        unsigned char c;
//        asm volatile(
//            LOCK "subl %2,%0; sete %1"
//            :"=m" (counter), "=qm" (c)
//            :"ir" (i), "m" (counter) : "memory");
//        return c;
        unsigned char c = counter == i;
        __sync_fetch_and_sub( &counter, i );  //lxk, for x86 and arm
        return c;
    }

    inline void Inc()
    {
//        asm volatile(
//            LOCK "incl %0"
//            :"=m" (counter)
//            :"m" (counter));
        __sync_add_and_fetch( &counter, 1 );  //lxk, for x86 and arm
	}

    inline void Dec()
    {
//        asm volatile(
//            LOCK "decl %0"
//            :"=m" (counter)
//            :"m" (counter));
        __sync_sub_and_fetch( &counter, 1 );  //lxk, for x86 and arm
	}

    inline bool IncAndTest()
    {
//        unsigned char c;
//        asm volatile(
//            LOCK "incl %0; sete %1"
//            :"=m" (counter), "=qm" (c)
//            :"m" (counter) : "memory");
//        return c != 0;
        __sync_add_and_fetch( &counter, 1 );  //lxk, for x86 and arm
        return counter == 0;                  //lxk, for x86 and arm
	}

    inline bool DecAndTest()
    {
//        unsigned char c;
//        asm volatile(
//            LOCK "decl %0; sete %1"
//            :"=m" (counter), "=qm" (c)
//            :"m" (counter) : "memory");
//        return c != 0;
        __sync_sub_and_fetch( &counter, 1 );  //lxk, for x86 and arm
        return counter == 0;                  //lxk, for x86 and arm
	}

    inline int AddNegative(int i)
    {
//        unsigned char c;
//        asm volatile(
//            LOCK "addl %2,%0; sets %1"
//            :"=m" (counter), "=qm" (c)
//            :"ir" (i), "m" (counter) : "memory");
//        return c;
        __sync_add_and_fetch( &counter, i );  //lxk, for x86 and arm
        return counter < 0;                   //lxk, for x86 and arm
	}

    inline void ClearMask(int mask)
    {
//        asm volatile(
//            LOCK "andl %0,%1"
//            : : "r" (~(mask)),"m" (counter) : "memory");
        __sync_and_and_fetch( &counter, ~mask );  //lxk, for x86 and arm
    }

    inline void SetMask(int mask)
    {
//        asm volatile(
//            LOCK "orl %0,%1"
//            : : "r" (mask),"m" (counter) : "memory");
        __sync_or_and_fetch( &counter, mask );  //lxk, for x86 and arm
    }

    inline int IncAndReturn()
    {
        return AddAndReturn(1);
    }

    inline int DecAndReturn()
    {
        return SubAndReturn(1);
    }

protected:
    volatile int counter;
};

class TestingConfig
{
public:
    std::string mEndpoint;
    std::string mOdpsEndpoint;
    std::string mProjectName;
    std::string mSchemaEnabledProject;
    std::string mNamespaceId;
    std::string mTags;

    std::string mAccessId;
    std::string mAccessKey;
    std::string mAccessType;

    std::string mTestUser;
    std::string mTestAccessId;
    std::string mTestAccessKey;
    std::string mTestAccessType;

    TestingConfig() { }
};

inline void to_json(nlohmann::json& j, const TestingConfig& c)
{
    j = nlohmann::json{
        {"TunnelEndpoint", c.mEndpoint},
        {"OdpsEndpoint", c.mOdpsEndpoint},
        {"ProjectName", c.mProjectName},
        {"SchemaEnabledProject", c.mSchemaEnabledProject},
        {"NamespaceId", c.mNamespaceId},
        {"Tags", c.mTags},
        {"AccessId", c.mAccessId},
        {"AccessKey", c.mAccessKey},
        {"AccessType", c.mAccessType},
        {"TestUser", c.mTestUser},
        {"TestAccessId", c.mTestAccessId},
        {"TestAccessKey", c.mTestAccessKey},
        {"TestAccessType", c.mTestAccessType},
    };
}

inline void from_json(const nlohmann::json& j, TestingConfig& c)
{
    c.mEndpoint = j.value("TunnelEndpoint", "127.0.0.1");
    c.mOdpsEndpoint = j.value("OdpsEndpoint", "127.0.0.1");
    c.mProjectName = j.value("ProjectName", "");
    c.mSchemaEnabledProject = j.value("SchemaEnabledProject", "");
    c.mNamespaceId = j.value("NamespaceId", "");
    c.mTags = j.value("Tags", "");
    c.mAccessId = j.value("AccessId", "");
    c.mAccessKey = j.value("AccessKey", "");
    c.mAccessType = j.value("AccessType", std::string(ACCOUNT_ALIYUN));
    c.mTestUser = j.value("TestUser", "");
    c.mTestAccessId = j.value("TestAccessId", "");
    c.mTestAccessKey = j.value("TestAccessKey", "");
    c.mTestAccessType = j.value("TestAccessType", std::string(ACCOUNT_ALIYUN));
}

class Utils
{
public:
    static bool Initialize();
    static void CreateDirectoryIfNotExists(const std::string& path);

    static std::string GetVersion();
    static const std::string GetTestUser();
    static const std::string GetProjectName();
    static const std::string GetSchemaEnabledProjectName();
    static const std::string GetTunnelEndpoint();
    static const std::string GetNamespaceId();
    static const std::string GetTags();

    static bool InvokeAuth(const std::string &command);
    static bool ExecSql(const std::string &sql);
    static bool ExecSqlTogether(const std::string &sql);
    static void ExecSql(const std::string& project, const std::string& sql, std::map<std::string, std::string>& hints, uint32_t timeoutInMs);
    static std::pair<std::string, int64_t> ExecSqlByMCQA1(const std::string &sql);

    static OdpsTunnel GetTunnelInstance(const bool useRouter = false, CompressOption opt = CompressOption::NO_COMPRESS);
    static OdpsTunnel GetTunnelInstanceWithAppSignature(const bool useRouter = false);
    static OdpsTunnel GetTunnelInstanceForSecurity();
    static OdpsTunnel GetTunnelInstanceForV4Signature();
    static OdpsTunnel GetTunnelInstanceForArn();

    static std::string GetRandomSchemaName();
    static std::string GetRandomTableName();
    static std::string GetRandomVolumeName();
    static std::string GetRandomPartitionName();
    static std::string GetRandomFileName();

    static Configuration GetConfiguration();
    static IODPSPtr GetODPS(const std::string& project = "");

    static bool CreateTable(const std::string &tableName);
    static bool DropTableIfExists(const std::string &tableName);
    static bool DropAndCreateTable(const std::string &tableName);

    static bool CreateVolume(const std::string& volumeName);
    static bool RemoveVolume(const std::string& volumeName);
    static bool RecreateVolume(const std::string& volumeName);

private:
    static bool ExecSqlImpl(const std::string &sql);

private:
    static TestingConfig sConfig;
    static AtomicCounter sCounter;
    static std::string sStartTime;
    static std::string sOdpsConsole;
};

class TunnelTestMetrics
{
    public:
    TunnelTestMetrics()
    {
    }
    ~TunnelTestMetrics()
    {
    }

    int64_t ClientProcessCost = 0;
    int64_t NetworkCost = 0;
    int64_t TunnelProccessCost = 0;
    int64_t PanguIOCost = 0;
    int64_t RateLimitCost = 0;
};

inline void to_json(nlohmann::json& j, const TunnelTestMetrics& m)
{
    j = nlohmann::json{
        {"ClientProcessCost", m.ClientProcessCost},
        {"NetworkCost", m.NetworkCost},
        {"TunnelProccessCost", m.TunnelProccessCost},
        {"PanguIOCost", m.PanguIOCost},
        {"RateLimitCost", m.RateLimitCost},
    };
}

inline void from_json(const nlohmann::json& j, TunnelTestMetrics& m)
{
    m.ClientProcessCost = j.value("ClientProcessCost", static_cast<int64_t>(0));
    m.NetworkCost = j.value("NetworkCost", static_cast<int64_t>(0));
    m.TunnelProccessCost = j.value("TunnelProccessCost", static_cast<int64_t>(0));
    m.PanguIOCost = j.value("PanguIOCost", static_cast<int64_t>(0));
    m.RateLimitCost = j.value("RateLimitCost", static_cast<int64_t>(0));
}

#endif // APSARA_ODPS_TUNNEL_TESTING_H
