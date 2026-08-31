#include <unistd.h>

#include "test_util.h"
#include "util/string_util.h"
#include "util/timer.h"
#include "util/utils.h"
#include <time.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <vector>
#include "sys/stat.h"
#include "sys/types.h"
#include "include/odps_api.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::util;

TestingConfig Utils::sConfig;
AtomicCounter Utils::sCounter;
std::string Utils::sStartTime;
std::string Utils::sOdpsConsole;

bool Utils::Initialize()
{
    sStartTime = std::to_string(Timer::GetCurrentTimeInNanoSeconds());

    const int MAXPATH = 256;
    char buffer[MAXPATH];
    if (getcwd(buffer, MAXPATH) == NULL)
    {
        std::cerr << "getcwd failed " << strerror(errno) << std::endl;
        return false;
    }
    std::string pdir(buffer);
    std::string configFile = pdir + "/conf/testing.conf";
    sOdpsConsole = pdir + "/odpsctl/bin/odpscmd";
    std::cerr << "odpscmd: " << sOdpsConsole << std::endl;

    std::cerr << "Load configurations from " << configFile << std::endl;
    std::ifstream ifs(configFile.c_str());
    if (!ifs.good())
    {
        std::cerr << "Open config file failed " << configFile << std::endl;
        return false;
    }

    std::string json;
    std::string line;
    while (getline(ifs, line))
    {
        json += line;
    }
    ifs.close();

    FromJsonString(sConfig, json);
    std::cerr << "Configurations Configs loaded." << std::endl;
    std::cerr << "Overwriting the odpscmd configuration with generated ones..." << std::endl;
    std::cerr << "ODPSEndpoint: " << sConfig.mOdpsEndpoint << std::endl;
    Utils::CreateDirectoryIfNotExists(pdir + "/odpsctl/conf");
    std::fstream odpsctlConfig(pdir + "/odpsctl/conf/odps_config.ini",
        std::fstream::out | std::fstream::trunc);
    odpsctlConfig << "project_name=" << sConfig.mProjectName << std::endl;
    odpsctlConfig << "access_id=" << sConfig.mAccessId << std::endl;
    odpsctlConfig << "access_key=" << sConfig.mAccessKey << std::endl;
    odpsctlConfig << "end_point=" << sConfig.mOdpsEndpoint << std::endl;
    odpsctlConfig << "tunnel_endpoint=" << sConfig.mEndpoint << std::endl;
    odpsctlConfig << "namespace_id=" << sConfig.mNamespaceId << std::endl;
    odpsctlConfig << "use_instance_tunnel=true" << std::endl;
    odpsctlConfig << "enable_interactive_mode=true" << std::endl;
    odpsctlConfig.flush();
    odpsctlConfig.close();
    return true;
}

void Utils::CreateDirectoryIfNotExists(const std::string &path)
{
    ::mkdir(path.c_str(), 0755);
}

std::string Utils::GetVersion()
{
    return "odps_cppsdk_";
}

const std::string Utils::GetTestUser()
{
    std::string user;
    if (sConfig.mTestAccessType == ACCOUNT_ALIYUN)
    {
        user = "ALIYUN$" + sConfig.mTestUser;
    }
    else if (sConfig.mTestAccessType == ACCOUNT_TAOBAO)
    {
        user = "TAOBAO$" + sConfig.mTestUser;
    }
    else
    {
        user = "DOMAIN$" + sConfig.mTestUser;
    }
    return user;
}

const std::string Utils::GetProjectName()
{
    return sConfig.mProjectName;
}

const std::string Utils::GetSchemaEnabledProjectName()
{
    return sConfig.mSchemaEnabledProject;
}

const std::string Utils::GetTunnelEndpoint()
{
    return sConfig.mEndpoint;
}

const std::string Utils::GetNamespaceId()
{
    return sConfig.mNamespaceId;
}

const std::string Utils::GetTags()
{
    return sConfig.mTags;
}

OdpsTunnel Utils::GetTunnelInstance(const bool useRouter, CompressOption opt)
{
    OdpsTunnel dt;
    Account account(sConfig.mAccessType, sConfig.mAccessId, sConfig.mAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(sConfig.mEndpoint);
    conf.SetNamespaceId(sConfig.mNamespaceId);
    conf.SetTags(sConfig.mTags);
    if(useRouter == true) conf.SetEndpoint(sConfig.mOdpsEndpoint);
    conf.option = opt;

    dt.Init(conf);
    return dt;
}

OdpsTunnel Utils::GetTunnelInstanceWithAppSignature(const bool useRouter)
{
    OdpsTunnel dt;
    Account account(sConfig.mAccessType, sConfig.mAccessId, sConfig.mAccessKey);
    AppAccount appAccount(sConfig.mTestAccessId, sConfig.mTestAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetAppAccount(appAccount);
    conf.SetTunnelEndpoint(sConfig.mEndpoint);
    if(useRouter == true) conf.SetEndpoint(sConfig.mOdpsEndpoint);

    dt.Init(conf);
    return dt;
}

OdpsTunnel Utils::GetTunnelInstanceForV4Signature()
{
    OdpsTunnel dt;
    AliyunAccount account(sConfig.mAccessId, sConfig.mAccessKey, "test_region");

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(sConfig.mEndpoint);

    dt.Init(conf);
    return dt;
}

Configuration Utils::GetConfiguration()
{
    Account account(sConfig.mAccessType, sConfig.mAccessId, sConfig.mAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetEndpoint(sConfig.mOdpsEndpoint);

    return conf;
}

IODPSPtr Utils::GetODPS(const std::string& project)
{
    const Configuration& conf = GetConfiguration();
    if (project.empty())
    {
        return IODPS::Create(conf, sConfig.mProjectName);
    }
    else
    {
        return IODPS::Create(conf, project);
    }
}

OdpsTunnel Utils::GetTunnelInstanceForSecurity()
{
    OdpsTunnel dt;
    Account account(sConfig.mTestAccessType, sConfig.mTestAccessId, sConfig.mTestAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(sConfig.mEndpoint);

    dt.Init(conf);
    return dt;
}

OdpsTunnel Utils::GetTunnelInstanceForArn()
{
    OdpsTunnel dt;
    CredentialsProviderPtr provider =
        std::make_shared<TestStaticCredentialsProvider>(
            sConfig.mAccessId, sConfig.mAccessKey);

    Account account(provider);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(sConfig.mEndpoint);
    conf.SetNamespaceId(sConfig.mNamespaceId);
    conf.SetTags(sConfig.mTags);

    dt.Init(conf);
    return dt;
}

std::string Utils::GetRandomSchemaName()
{
    return GetVersion() + "schema_" + sStartTime + "_" + std::to_string(sCounter.IncAndReturn());
}

std::string Utils::GetRandomTableName()
{
    return GetVersion() + "table_" + sStartTime + "_" + std::to_string(sCounter.IncAndReturn());
}

std::string Utils::GetRandomVolumeName()
{
    return GetVersion() + "volume_" + sStartTime + "_" + std::to_string(sCounter.IncAndReturn());
}

std::string Utils::GetRandomPartitionName()
{
    return "pt_" + sStartTime + "_" + std::to_string(sCounter.IncAndReturn());
}

std::string Utils::GetRandomFileName()
{
    return GetVersion() + "file_" + sStartTime + "_" + std::to_string(sCounter.IncAndReturn());
}

bool Utils::CreateTable(const std::string &tableName)
{
    return Utils::ExecSql("create table " + tableName + " (c1 bigint, c2 double, c3 boolean, c4 datetime, c5 string)");
}

bool Utils::DropTableIfExists(const std::string &tableName)
{
    return Utils::ExecSql("drop table if exists " + tableName);
}

bool Utils::DropAndCreateTable(const std::string &tableName)
{
    if (Utils::DropTableIfExists(tableName))
    {
        if (Utils::CreateTable(tableName))
            return true;
    }
    return false;
}

bool Utils::CreateVolume(const std::string &volumeName)
{
    return Utils::ExecSql("fs -mkv " + volumeName + " 'tunnel testing'");
}

bool Utils::RemoveVolume(const std::string &volumeName)
{
    return Utils::ExecSql("fs -rmv " + volumeName);
}

bool Utils::RecreateVolume(const std::string &volumeName)
{
    Utils::RemoveVolume(volumeName);
    return Utils::CreateVolume(volumeName);
}

bool Utils::InvokeAuth(const std::string &command)
{
    return Utils::ExecSql(command);
}

bool Utils::ExecSql(const std::string &sql)
{
    std::vector<std::string> sqlVec;
    sqlVec = util::SplitString(sql, ";");

    for (size_t i = 0; i < sqlVec.size(); ++i)
    {
        if (!Utils::ExecSqlImpl(sqlVec[i]))
            return false;
    }
    return true;
}

bool Utils::ExecSqlTogether(const std::string &sql)
{
    std::string tmpsql = sql;
    if (!util::EndWith(tmpsql, ";"))
    {
        tmpsql += ";";
    }
    if (!Utils::ExecSqlImpl(tmpsql))
    {
        return false;
    }
    return true;
}

bool Utils::ExecSqlImpl(const std::string &sql)
{
    int okCount = sql.find("insert") != std::string::npos ? 2 : 1;

    bool result = false;
    const std::string command = sOdpsConsole + " -e \"" + sql + ";\" 2>&1";

    std::cerr << sql << ";" << std::endl;

    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL) return false;

    char buffer[2048];
    int  len = 2047;
    std::string output;

    buffer[len] = 0;
    while (fgets(buffer, len, fp) != NULL)
    {
        output.append(buffer);
        if (strstr(buffer, "OK") != NULL)
        {
            okCount--;
        }

        if (!okCount)
        {
            result = true;
            break;
        }
    }

    pclose(fp);

    if (!result)
    {
        std::cerr << "exec SQL: " << sql << " failed, output:" << output << std::endl;
    }

    return result;
}

void Utils::ExecSql(const std::string& project,
    const std::string& sql,
    std::map<std::string, std::string>& hints,
    uint32_t timeoutInMs)
{
    std::cout << sql << std::endl;
    IODPSPtr odps = GetODPS(project);
    const ISQLTaskPtr& task = ISQLTask::Create();
    const IODPSInstancePtr& instance = task->Run(odps, sql, hints);
    std::cout << instance->GenerateLogView(24) << std::endl;
    instance->WaitForSuccess(timeoutInMs);
}

std::pair<std::string, int64_t> Utils::ExecSqlByMCQA1(const std::string &sql)
{
    const std::string command = sOdpsConsole + " -e \"" + sql + ";\" 2>&1";
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL) return std::make_pair<std::string, int64_t>("", -1);

    std::string output;
    char buffer[2048];
    int  len = 2047;
    buffer[len] = 0;
    while (fgets(buffer, len, fp) != NULL)
    {
        output.append(buffer);
    }
    pclose(fp);
    std::cout << output << std::endl;

    // Try to parse from logview i=
    std::string instanceId;
    size_t iPos = output.find("i=");
    if (iPos != std::string::npos)
    {
        size_t start = iPos + 2;
        size_t end = output.find('&', start);
        if (end == std::string::npos)
        {
            instanceId = output.substr(start);
        }
        instanceId = output.substr(start, end - start);
    }

    int64_t queryId = -1;
    size_t subQueryPos = output.find("subQuery=");
    if (subQueryPos != std::string::npos)
    {
        size_t subStart = subQueryPos + 9; // length of "subQuery="
        size_t subEnd = output.find('&', subStart);
        std::string subQueryString = output.substr(subStart,
            subEnd == std::string::npos ? std::string::npos : subEnd - subStart);

        // Convert to integer
        try
        {
            queryId = std::stoll(subQueryString);
        }
        catch (...)
        {
        }
    }

    return std::make_pair(instanceId, queryId);
}
