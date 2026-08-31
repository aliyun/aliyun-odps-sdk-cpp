#ifndef APSARA_ODPS_TUNNEL_INTERNAL_ARROW_UTIL_H
#define APSARA_ODPS_TUNNEL_INTERNAL_ARROW_UTIL_H

#ifdef ODPS_SDK_ENABLE_ARROW
#include "arrow_record_writer.h"
#include "common/logging.h"
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include "serialize.h"
#include "lz4_stream.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace internal {
namespace tunnel {

#if ARROW_VERSION_MAJOR >= 13
inline std::shared_ptr<arrow::util::Codec> CreateCodecOrThrow(arrow::Compression::type type, int level, const std::string& errorPrefix)
{
    auto res = arrow::util::Codec::Create(type, level);
    if (!res.ok())
    {
        throw OdpsTunnelException(errorPrefix + res.status().ToString());
    }

    return std::move(*res);
}
#endif

inline arrow::ipc::IpcWriteOptions GetWriteOptions(const CompressOption& compress)
{
    auto&& options = arrow::ipc::IpcWriteOptions::Defaults();
    switch (compress.algorithm)
    {
        case CompressOption::ODPS_ZSTD:
#if ARROW_VERSION_MAJOR >= 13
            options.codec = CreateCodecOrThrow(arrow::Compression::ZSTD, compress.level, "Failed to create ZSTD codec: ");
#else
            options.compression = arrow::Compression::ZSTD;
#endif
            break;
        case CompressOption::ODPS_LZ4_FRAME:
#if ARROW_VERSION_MAJOR >= 13
            options.codec = CreateCodecOrThrow(arrow::Compression::LZ4_FRAME, compress.level, "Failed to create LZ4-Frame codec: ");
#else
            options.compression = arrow::Compression::LZ4_FRAME;
#endif
            break;
        default:
            break;
    }

    return options;
}

}}}}}
#endif
#endif
