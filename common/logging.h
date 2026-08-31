#pragma once

/** @file logging.h
 * SDK 内部日志桩(原 apsara/common/logging.h 的替代品)。
 * LOG_* 宏当前均为空实现(参数不求值);保留调用点以便将来
 * 无缝接入真实日志实现。
 */

#include <string>

#define LOG_DEBUG(logger, fields) {}
#define LOG_INFO(logger, fields) {}
#define LOG_WARNING(logger, fields) {}
#define LOG_ERROR(logger, fields) {}

namespace apsara { namespace odps { namespace sdk { namespace logging {

struct Logger
{
};

inline Logger* GetLogger(const std::string& path)
{
    (void)path;
    return NULL;
}

inline void InitLoggingSystem() {}

}  // namespace logging
}  // namespace sdk
}  // namespace odps
}  // namespace apsara
