/** CRC32C(搬自原 apsara ErrorDetection,仅保留 tunnel 校验和
 * 实际使用的 DoCrc32c 一族;运行期按 SSE4.2 可用性在查表法与
 * 硬件指令间选择)。
 */

#ifndef APSARA_ODPS_SDK_UTIL_CRC32C_H
#define APSARA_ODPS_SDK_UTIL_CRC32C_H

#include <inttypes.h>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

typedef uint32_t (*DoCrc32cImpl)(uint32_t, const uint8_t*, uint64_t);

DoCrc32cImpl GetDoCrc32cImpl();

/** Calculate checksum for an input byte stream
 *
 * @param initCrc Initial checksum value
 * @param data Input data to calculate checksum
 * @param length Length of input data
 *
 * @return Calculated checksum
 */
inline uint32_t DoCrc32c(uint32_t initCrc, const uint8_t* data, uint64_t length)
{
    static DoCrc32cImpl doCrc32c = GetDoCrc32cImpl();
    return doCrc32c(initCrc, data, length);
}

uint32_t DoCrc32c_Lookup(uint32_t initCrc, const uint8_t* data, uint64_t length);

uint32_t DoCrc32c_Intel(uint32_t initCrc, const uint8_t* data, uint64_t length);

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
