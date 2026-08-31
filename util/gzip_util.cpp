#include "gzip_util.h"
#include "util/base64.h"
#include <iostream>
#include <sstream>
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/io/gzip_stream.h"
#include "utils.h"

using namespace std;
using namespace google::protobuf::io;

namespace apsara
{
namespace odps
{
namespace sdk{ namespace util{
#define ZIP_BUFFER_SIZE 262144

// TODO: totally remove this, replace corresponding code with GzipOutputStreams directly

int GZipUtil::GZip(const std::string &str, std::string &out)
{
    // char *pStr = (char *)str.c_str();
    // CA2GZIPT<ZIP_BUFFER_SIZE, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY> gzip(pStr, str.length()); // do compressing here
    // out = string((char *)gzip.pgzip, gzip.Length);
    google::protobuf::io::GzipOutputStream::Options tmpOption;
    tmpOption.format = google::protobuf::io::GzipOutputStream::GZIP;
    tmpOption.compression_level = Z_BEST_COMPRESSION;
    tmpOption.compression_strategy = Z_DEFAULT_STRATEGY;
    std::shared_ptr<StringOutputStream> os = std::make_shared<StringOutputStream>(&out);
    std::shared_ptr<GzipOutputStream> gos = std::make_shared<GzipOutputStream>(os.get(), tmpOption);
    std::shared_ptr<ArrayInputStream> is = std::make_shared<ArrayInputStream>(str.c_str(), str.size());
    util::PipeBetweenZeroCopyStreams(is.get(), gos.get());
    gos->Flush();
    return 0;
}

int GZipUtil::GUnZip(const std::string &str, std::string &out)
{
    std::shared_ptr<StringOutputStream> os = std::make_shared<StringOutputStream>(&out);
    std::shared_ptr<ArrayInputStream> is = std::make_shared<ArrayInputStream>(str.c_str(), str.size());
    std::shared_ptr<GzipInputStream> gis = std::make_shared<GzipInputStream>(is.get());
    util::PipeBetweenZeroCopyStreams(gis.get(), os.get());
    return 0;
}

int GZipUtil::GZipBase64(const std::string &str, std::string &out)
{
    string outstr;
    int code = GZip(str, outstr);

    std::istringstream ins(outstr);
    std::ostringstream outs;
    Base64Encoding(ins, outs);
    out = outs.str();
    return code;
}

int GZipUtil::GUnZipBase64(const std::string &str, std::string &out)
{

    std::istringstream ins(str);
    std::ostringstream outs;
    Base64Decoding(ins, outs);
    string str2 = outs.str();

    return GUnZip(str2, out);
}

}}
} // namespace odps
} // namespace apsara
