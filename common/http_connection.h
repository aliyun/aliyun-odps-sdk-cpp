#ifndef APSARA_ODPS_SDK_HTTP_CONNECTION_H
#define APSARA_ODPS_SDK_HTTP_CONNECTION_H

#include <cstdint>
#include <poll.h>
#include "curl/curl.h"
#include "configuration.h"
#include "common/http_message.h"
#include "curl_connection_pool.h"
#include "signer.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

namespace internal
{

const static std::string COMPRESS_DEFLATE = "deflate";
const static std::string COMPRESS_ZSTD = "zstd";
const static std::string COMPRESS_LZ4_FRAME = "x-lz4-frame";
const static std::string COMPRESS_LZ4 = "x-odps-lz4-frame";

class HttpConnection
{
public:
    class ChannelStream
    {
    public:
        ChannelStream(size_t size, bool up)
            : buffer(NULL),
              buffer_len(size),
              buffer_pos(0),
              buffer_bytes(0),
              total_bytes(0),
              is_upstream(up)
        {
            if (!is_upstream && buffer_len < CURL_MAX_WRITE_SIZE)
                buffer_len = CURL_MAX_WRITE_SIZE;

            buffer = new char[buffer_len];
        }

        ~ChannelStream()
        {
            delete[] buffer;
        }

        void reuse_buffer();
        void realloc_buffer(size_t size);
        void clear_buffer();

    private:
        /* caller should guarantee putting buffer safely*/
        void put_buffer(const void *ptr, size_t n);
        size_t get_buffer(void *ptr, size_t n);

    private:
        friend class HttpConnection;

        char *buffer;        /* buffer to store cached data*/
        size_t buffer_len;   /* currently allocated buffers length */
        size_t buffer_pos;   /* write/read pos in buffer*/
        size_t buffer_bytes; /* valid bytes in buffer*/
        size_t total_bytes;  /* total bytes processed in stream*/
        bool is_upstream;
    };

    HttpConnection(const Configuration &conf, bool keepAlive = false);
    virtual ~HttpConnection();

    static std::string sLocalIp;

    void Open();
    void Close();
    size_t Read(char *ptr, size_t size);
    virtual size_t Write(const char *ptr, size_t size);
    void CloseUpstream();

    void SetRequest(RequestPtr req, bool connectOdpsServer = false);
    void WaitResponse();
    RequestPtr GetRequest() { return mRequest; }
    ResponsePtr GetResponse() { return mResponse; }
    const Configuration& GetConfiguration() const { return mConf; }
    int64_t GetIOCost() {return mIOCost; }

    void SetSocketTimeOut(int64_t timeout) { mSocketTimeout = timeout; }
    int64_t GetSocketTimeOut() { return mSocketTimeout; }
    size_t GetUpstreamBytes() { return mUpstream.total_bytes; }
    size_t GetDownstreamBytes() { return mDownstream.total_bytes; }
    size_t GetUpstreamData(void *ptr, size_t n);
    std::string TraceError(void);
    bool IsTimeout();

private:
    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userp);
    static size_t write_callback(char *buffer, size_t size, size_t nitems, void *userp);
    static size_t read_callback(char *buffer, size_t size, size_t nitems, void *userp);
    static int progress_callback(void *userp, double dltotal, double dlnow, double ultotal, double ulnow);
    static int timer_callback(CURLM *multi, long timeout_ms, void *userp);
    static int socket_callback(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp);

    bool drive(ChannelStream &stream);
    std::string read_multi_info();
    void update();

    std::string GetEncodedURL(CURL *curl);

private:
    ChannelStream mUpstream;
    ChannelStream mDownstream;

    CURL *mHandle;
    int mStillRunning; /* Is background url fetch still in progress */
    CURLM *mMultiHandle;

    struct curl_slist *mHeaders;

    RequestPtr mRequest;
    ResponsePtr mResponse;
    Configuration mConf;
    int64_t mSocketTimeout; /* socket time out in seconds */
    int64_t mLastUpdateTime;
    int64_t mIOCost = 0;

    bool mUpstreamEOF;
    bool mCurlPaused;

    struct pollfd mPollFd;
    int mPollWaitTime;
    bool mTimeout;

    CurlConnectionGuardPtr mConn = nullptr;

    bool mKeepAlive = false;
};

typedef std::shared_ptr<HttpConnection> HttpConnectionPtr;

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif
