#include "curl_util.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

namespace
{
// 进程级调试开关,语义与原 apsara flag tunnel_sdk_PrintDebugError 一致:
// 普通全局变量,预期在进程启动阶段设置
bool sDebugErrorEnabled = false;
}  // namespace

bool IsDebugErrorEnabled()
{
    return sDebugErrorEnabled;
}

void SetDebugErrorEnabled(bool enabled)
{
    sDebugErrorEnabled = enabled;
}

std::string CurlMsg(CURLMSG msg)
{
    static const std::map<CURLMSG, std::string> sConvertTable = {
        {CURLMSG_NONE, "CURLMSG_NONE"},
        {CURLMSG_DONE, "CURLMSG_DONE"},
        {CURLMSG_LAST, "CURLMSG_LAST"}};

    static const std::string kUnknownCode = "CURLMSG_UNKNOWN";

    auto it = sConvertTable.find(msg);
    return it != sConvertTable.end() ? it->second : kUnknownCode;
}

std::string MultiCode(CURLMcode code)
{
    static const std::map<CURLMcode, std::string> sConvertTable = {
        {CURLM_CALL_MULTI_PERFORM, "CURLM_CALL_MULTI_PERFORM"},
        {CURLM_OK, "CURLM_OK"},
        {CURLM_BAD_HANDLE, "CURLM_BAD_HANDLE"},
        {CURLM_BAD_EASY_HANDLE, "CURLM_BAD_EASY_HANDLE"},
        {CURLM_OUT_OF_MEMORY, "CURLM_OUT_OF_MEMORY"},
        {CURLM_INTERNAL_ERROR, "CURLM_INTERNAL_ERROR"},
        {CURLM_UNKNOWN_OPTION, "CURLM_UNKNOWN_OPTION"},
        {CURLM_ADDED_ALREADY, "CURLM_ADDED_ALREADY"},
        {CURLM_RECURSIVE_API_CALL, "CURLM_RECURSIVE_API_CALL"},
        {CURLM_BAD_SOCKET, "CURLM_BAD_SOCKET"},
        {CURLM_LAST, "CURLM_LAST"}};

    static const std::string kUnknownCode = "CURLM_UNKNOWN";
    auto it = sConvertTable.find(code);
    return it != sConvertTable.end() ? it->second : kUnknownCode;
}
}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara