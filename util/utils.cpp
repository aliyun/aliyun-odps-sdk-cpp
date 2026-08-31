#include "utils.h"
#include "util/md5.h"
#include "util/base64.h"
#include "util/string_util.h"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/algorithm/string.hpp"
#include "util/gzip_util.h"
#include <sstream>
#include "include/configuration.h"
#include "common/http_flags.h"
#include "include/odps_exception.h"
#include "common/odps_table_schema.h"

using namespace std;
using namespace apsara::odps::sdk::max_storage_api;

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util {

const char* TUNNEL_DATE_TIME_FORMAT = "%Y-%m-%d %H:%M:%S";
const char* TUNNEL_DATE_FORMAT = "%Y-%m-%d";

// 通用函数：创建MaxStorage API请求
apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& target)
{
    internal::RequestPtr request(new internal::Request());
    request->SetMethod(HTTP_METHOD_POST);
    request->SetEndpoint(conf.tunnelEndpoint);
    request->SetResourcePath(PATH_MAX_STORAGE_API_V2);
    request->SetParameter(PARAM_ACTION, action);
    request->SetParameter(PARAM_TARGET, target);
    if (!conf.namespaceId.empty())
    {
        request->SetHeader(HEADER_ODPS_NAMESPACE_ID, conf.namespaceId);
    }
    return request;
}
// Volume类型请求
apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiVolumeRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& volume)
{
    const std::string& target = TARGET_PREFIX_PROJECT + "." + project + "." + TARGET_PREFIX_VOLUMES + "." + volume;
    return CreateMaxStorageApiRequest(conf, action, project, target);
}

// Table类型请求
apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiTableRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& schema,
    const std::string& table)
{
    const std::string& target = TARGET_PREFIX_PROJECT + "." + project + "." + TARGET_PREFIX_SCHEMAS + "." + schema + "." + TARGET_PREFIX_TABLES + "." + table;
    return CreateMaxStorageApiRequest(conf, action, project, target);
}

// Instance类型请求
apsara::odps::sdk::internal::RequestPtr CreateMaxStorageApiInstanceRequest(
    const Configuration& conf,
    const std::string& action,
    const std::string& project,
    const std::string& instance)
{
    const std::string& target = TARGET_PREFIX_PROJECT +"."+ project + "." + TARGET_PREFIX_INSTANCES +"."+ instance;
    return CreateMaxStorageApiRequest(conf, action, project, target);
}

void TunnelThrow(internal::Response& response, const std::string& endpoint)
{
    const std::string& reqId = response.GetHeader(HEADER_ODPS_REQUEST_ID);
    const std::string& httpStatus = std::to_string(response.GetStatusCode());
    std::string body;
    response.ReadBody(body);
    std::string code;
    std::string message;
    std::map<std::string, std::string> extraInfo;
    try
    {
        FromJsonString(extraInfo, body);
        if (extraInfo.count("Code") > 0 && extraInfo.count("Message") > 0)
        {
            code = extraInfo["Code"];
            message = extraInfo["Message"];
            extraInfo.erase("Code");
            extraInfo.erase("Message");
            extraInfo["HttpStatus"] = httpStatus;
            extraInfo["Endpoint"] = endpoint;
            throw OdpsTunnelException(code, message, reqId, extraInfo);
        }
    }
    catch (const nlohmann::json::exception&) {}

    throw OdpsTunnelException(httpStatus, body, reqId, extraInfo);
}

void TunnelThrow(const std::string& code, const std::string& message, const std::string& reqId, const std::string& endpoint)
{
    std::map<std::string, std::string> extraInfo;
    if (!endpoint.empty())
    {
        extraInfo["Endpoint"] = endpoint;
    }
    throw OdpsTunnelException(code, message, reqId, extraInfo);
}

void TunnelThrow(const std::string& code, const std::string& message)
{
    TunnelThrow(code, message, "", "");
}

void TunnelThrow(const std::string& message)
{
    TunnelThrow(LOCAL_ERROR, message, "", "");
}

#ifdef SUBSTITUDE_UUID_WITH_RANDOM
string GenerateUUID()
{
    const static char* ALPHA_NUM = "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(64);

    for (int i = 0; i < 64; ++i) {
        tmp_s += ALPHA_NUM[rand() % 61];
    }
    return tmp_s;
}
#else
string GenerateUUID()
{

    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    string uuid_string = boost::uuids::to_string(a_uuid);
    return uuid_string;
}
#endif

bool GUnZip(const std::string &input, std::string &output)
{
    int gzip_res = GZipUtil::GUnZip(input, output);
    return gzip_res == 0;
}

std::string GenerateMd5Signature(const string &content)
{
    MD5 md5;
    md5.update(content);
    return md5.toString();
}

std::string GenerateMd5Signature(std::stringstream &content, long &length)
{
    MD5 md5;
    vector<char> buffer;
    buffer.resize(BUCKET_SIZE);

    long total_size = 0L;

    int r;
    do
    {
        content.read(buffer.data(), buffer.size());
        r = content.gcount();
        md5.update(buffer.data(), r);
        total_size += r;
    } while (r > 0);
    length = total_size;

    return md5.toString();
}

const std::vector<std::string> SENSITIVE_KEYS = {"password", "secret", "access", "eventlog"};

bool IsSensitive(const std::string &key)
{
    std::string key_in_lower = util::ToLowerCaseString(key);
    for(auto i: SENSITIVE_KEYS)
    {
        if (key_in_lower.find(i) != string::npos)
        {
            return true;
        }
    }
    return false;
}

std::string Base64Encode(const std::string &input)
{
    std::istringstream iss(input);
    std::ostringstream oss;
    Base64Encoding(iss, oss);
    return oss.str();
}

std::string Base64Decode(const std::string &input)
{
    std::istringstream iss(input);
    std::ostringstream oss;
    Base64Decoding(iss, oss);
    return oss.str();
}

std::string StringListJoin(const std::vector<string> &input, const string &delim)
{
    return boost::algorithm::join(input, delim);
}

static void ZeroCopyStreamWrite(google::protobuf::io::ZeroCopyOutputStream* output,
    const void* writebuf, int writesize)
{
    int written = 0;
    while(written < writesize)
    {
        void* buf = nullptr;
        int bufsz = 0;
        if(!output->Next(&buf, &bufsz))
        {
            util::OdpsThrow(MALFORMED_DATA_STREAM, "Failed to read data from stream.");
        }
        if (bufsz > 0)
        {
            int shouldWrite = std::min(bufsz, writesize - written);
            memcpy(buf, static_cast<const char*>(writebuf) + written, shouldWrite);
            written += shouldWrite;
            if (shouldWrite < bufsz)
            {
                output->BackUp(bufsz - shouldWrite);
            }
        }
    }
}

void PipeBetweenZeroCopyStreams(google::protobuf::io::ZeroCopyInputStream* input,
    google::protobuf::io::ZeroCopyOutputStream* output)
{
    const void* buf = nullptr;
    int bufsz = 0;
    while(input->Next(&buf, &bufsz))
    {
        ZeroCopyStreamWrite(output, buf, bufsz);
    }
}

std::string GetDate()
{
    time_t timer;
    char buffer[50];
    memset(buffer, 0, 50);
    struct tm* tm_info;
    struct tm result;
    time(&timer);
    tm_info = gmtime_r(&timer, &result);

    //RFC822 format date
    strftime(buffer, 50, "%Y%m%d", tm_info);
    return std::string(buffer, strlen(buffer));
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
