/** MD5 摘要(搬自原 apsara ErrorDetection::MD5,底层为 OpenSSL
 * EVP;摘要类型统一为 uint8_t)。
 */

#ifndef APSARA_ODPS_SDK_UTIL_MD5_H
#define APSARA_ODPS_SDK_UTIL_MD5_H

#include <stdint.h>
#include <cstring>
#include <string>
#include <fstream>

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace util
{

/** Calculate Md5 for a byte stream, result is stored in md5[16]
 *
 * @param poolIn Input data
 * @param inputBytesNum Length of input data
 * @param md5[16] A 128-bit pool for storing md5
 */
void DoMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t md5[16]);

/** check correctness of a Md5-calculated byte stream
 *
 * @param poolIn Input data
 * @param inputBytesNum Length of input data
 * @param md5[16] A 128-bit md5 value for checking
 *
 * @return true if no error detected, false if error detected
 */
bool CheckMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, const uint8_t md5[16]);

/* MD5 declaration. */
class MD5
{
public:
    MD5();
    MD5(const void* input, size_t length);
    MD5(const std::string& str);
    MD5(std::ifstream& in);
    ~MD5();
    void update(const void* input, size_t length);
    void update(const std::string& str);
    void update(std::ifstream& in);
    const uint8_t* digest();
    std::string toString();
    void reset();

private:
    struct Impl;
    Impl* mImpl;

    /* class uncopyable */
    MD5(const MD5&);
    MD5& operator=(const MD5&);
};

} // namespace util
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
