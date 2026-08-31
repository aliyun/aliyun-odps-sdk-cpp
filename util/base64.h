#ifndef APSARA_ODPS_SDK_UTIL_BASE64_H
#define APSARA_ODPS_SDK_UTIL_BASE64_H

#include <iostream>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

// 流式 base64 编码:每 3 字节输入产出 4 字符,不足部分以 makeupChar 补齐。
// 语义与原 apsara 实现完全一致(签名路径等依赖该行为)。
void Base64Encoding(std::istream& is, std::ostream& os,
                    char makeupChar = '=',
                    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

// 流式 base64 解码:输入必须为 4 的倍数,'=' 只允许出现在末尾第三/第四位,
// 非法输入抛 OdpsException(LOCAL_ERROR, ...)。
void Base64Decoding(std::istream& is, std::ostream& os,
                    char plus = '+', char slash = '/');

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
