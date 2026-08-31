#pragma once
#include <string>
#include <unordered_map>

#include "curl/curl.h"
#include "odps_exception.h"

namespace apsara { namespace odps { namespace sdk { namespace internal {

/**
 * 进程级调试开关(原 apsara flag tunnel_sdk_PrintDebugError 的替代品)。
 * 开启后,SAFE_ECALL/SAFE_MCALL 在 curl 调用失败时会把
 * strerror(errno) 打印到 stderr。默认关闭。
 */
bool IsDebugErrorEnabled();
void SetDebugErrorEnabled(bool enabled);

#define SAFE_ECALL(EXP)                                                   \
    {                                                                     \
        CURLcode res = (EXP);                                             \
        if (res != CURLE_OK)                                              \
        {                                                                 \
            if (errno && ::apsara::odps::sdk::internal::IsDebugErrorEnabled()) \
                fprintf(stderr, #EXP ", error: %s\n", strerror(errno));   \
            throw OdpsTunnelException(                                    \
                INTERNAL_ERROR, #EXP " return " + std::to_string(res)); \
        }                                                                 \
    }

#define SAFE_MCALL(EXP)                                                        \
    {                                                                          \
        CURLMcode res = (EXP);                                                 \
        if (res == CURLM_UNKNOWN_OPTION)                                       \
        {                                                                      \
            std::cerr << "WARNING: Your cURL version does not support "        \
                      << #EXP                                                  \
                      << ". This normally not indicates a fatal error, but "   \
                         "your program may disbehave later."                   \
                      << std::endl;                                            \
        }                                                                      \
        else if (res != CURLM_OK)                                              \
        {                                                                      \
            const char* err_desc = curl_multi_strerror(res);                   \
            if (errno && ::apsara::odps::sdk::internal::IsDebugErrorEnabled()) \
                fprintf(stderr, #EXP ", system error: %s\n", strerror(errno)); \
            throw OdpsTunnelException(INTERNAL_ERROR,                          \
                                      std::string(#EXP " failed: ") +          \
                                          err_desc + std::string(", code:") +  \
                                          MultiCode(res));                    \
        }                                                                      \
    }


std::string CurlMsg(CURLMSG msg);

std::string MultiCode(CURLMcode code);

}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara