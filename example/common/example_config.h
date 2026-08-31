#ifndef ODPS_SDK_CPP_EXAMPLE_CONFIG_H
#define ODPS_SDK_CPP_EXAMPLE_CONFIG_H

// example 统一配置。
//
// 所有 example 的连接信息(endpoint / 凭证 / 默认项目)都从
// conf/testing.conf 读取,代码和命令行参数里不再携带敏感信息。
//
// 首次使用:
//     cp conf/testing.conf.sample conf/testing.conf
// 按模板填入实际值后,在仓库根目录下运行 example。

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

class ExampleConfig
{
public:
    std::string mAccessId;
    std::string mAccessKey;
    std::string mOdpsEndpoint;
    std::string mTunnelEndpoint;
    std::string mProjectName;
};

inline void to_json(nlohmann::json& j, const ExampleConfig& c)
{
    j = nlohmann::json{
        {"AccessId", c.mAccessId},
        {"AccessKey", c.mAccessKey},
        {"OdpsEndpoint", c.mOdpsEndpoint},
        {"TunnelEndpoint", c.mTunnelEndpoint},
        {"ProjectName", c.mProjectName},
    };
}

inline void from_json(const nlohmann::json& j, ExampleConfig& c)
{
    c.mAccessId = j.value("AccessId", "");
    c.mAccessKey = j.value("AccessKey", "");
    c.mOdpsEndpoint = j.value("OdpsEndpoint", "");
    c.mTunnelEndpoint = j.value("TunnelEndpoint", "");
    c.mProjectName = j.value("ProjectName", "");
}

// 从当前工作目录读取配置(默认路径 conf/testing.conf,请在仓库根目录运行)。
// 失败时打印配置指引并返回 false。
inline bool LoadExampleConfig(ExampleConfig& config,
                              const std::string& path = "conf/testing.conf")
{
    std::ifstream ifs(path.c_str());
    if (!ifs.good())
    {
        std::cerr << "Cannot open config file: " << path << std::endl;
        std::cerr << "Please run:" << std::endl;
        std::cerr << "    cp conf/testing.conf.sample conf/testing.conf" << std::endl;
        std::cerr << "then fill in your endpoint / credentials / project, "
                     "and run the example from the repository root directory."
                  << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    nlohmann::json::parse(buffer.str()).get_to(config);
    return true;
}

#endif // ODPS_SDK_CPP_EXAMPLE_CONFIG_H
