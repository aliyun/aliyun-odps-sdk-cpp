#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <openssl/crypto.h>

#include <curl/curl.h>

#include <string>
#include <map>

#include "util/string_util.h"
#include "util/timer.h"

#include "http_flags.h"
#include "odps_tunnel.h"
#include "error_code.h"
#include "http_connection.h"
#include "util/network.h"
#include "sdk_version.h"
#include "util/utils.h"
#include "curl_connection_pool.h"
#include "curl_util.h"

// 历史代码在本文件使用不带限定的 string(原 apsara flag.h 曾在全局引入)
using std::string;

static const char* kTokenDelimeter = ";";

static pthread_mutex_t *lockarray;

static inline void lock_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        pthread_mutex_lock(&(lockarray[type]));
    }
    else
    {
        pthread_mutex_unlock(&(lockarray[type]));
    }
}

static inline unsigned long thread_id()
{
    return (unsigned long)pthread_self();
}

static void init_locks()
{
    lockarray = (pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
    for (int i = 0; i < CRYPTO_num_locks(); i++)
    {
        pthread_mutex_init(&(lockarray[i]),NULL);
    }

    CRYPTO_set_id_callback(thread_id);
    CRYPTO_set_locking_callback(lock_callback);
}

static void kill_locks()
{
    CRYPTO_set_locking_callback(NULL);
    for (int i = 0; i < CRYPTO_num_locks(); i++)
    {
        pthread_mutex_destroy(&(lockarray[i]));
    }
    OPENSSL_free(lockarray);
}

class TunnelOpenSSLInitializer
{
public:
    TunnelOpenSSLInitializer()
    {
        init_locks();
    }

    ~TunnelOpenSSLInitializer()
    {
        kill_locks();
    }
};


namespace apsara { namespace odps { namespace sdk { namespace internal {

std::string HttpConnection::sLocalIp = util::Network::IpUint2String(util::Network::GetLocalIP());

HttpConnection::HttpConnection(const Configuration& conf, bool keepAlive)
  : mUpstream(conf.GetHttpBufferSize(), true),
    mDownstream(conf.GetHttpBufferSize(), false),
    mHandle(NULL),
    mStillRunning(0),
    mMultiHandle(NULL),
    mHeaders(NULL),
    mConf(conf),
    mSocketTimeout(mConf.GetSocketTimeout()),
    mUpstreamEOF(false),
    mCurlPaused(false),
    mTimeout(false)
{
    mPollFd.fd = -1;
    mPollFd.events = 0;
    mPollFd.revents = 0;
    mPollWaitTime = -1;

    mLastUpdateTime = util::timing::GetCurrentTimeInSeconds();

    mKeepAlive = keepAlive;
}

HttpConnection::~HttpConnection()
{
    Close();

    if (mMultiHandle && !mKeepAlive) {
        curl_multi_cleanup(mMultiHandle);
    }

    mMultiHandle = NULL;
}

size_t HttpConnection::header_callback(char *buffer,size_t size,size_t nitems,void *userp)
{
    uint32_t totalSize = size * nitems;
    HttpConnection* hc = reinterpret_cast<HttpConnection*>(userp);
    ResponsePtr rsp = hc->GetResponse();

    std::string s(buffer, totalSize);
    size_t pos = s.find(":");
    if(pos != std::string::npos)
    {
        std::string key = util::TrimString(s.substr(0, pos));
        std::string value = util::TrimString(util::RightTrimString(util::RightTrimString(s.substr(pos + 1),'\n'),'\r'));
        rsp->SetHeader(key,value);
        if (key == CONTENT_LENGTH)
        {
            rsp->SetContentLength(util::StringTo<long>(value));
        }
        // LOG_DEBUG(sLogger,("HEADER KEY",key)("HEADER VALUE",value));
    }
    else if (util::StartWith(s,"HTTP"))
    {
        // LOG_DEBUG(sLogger,("HTTP Status Line",s));

        std::vector<std::string> parts = util::SplitString(s);

        if (parts.size() < 3) /* like: HTTP/1.1 200 OK */
        {
            throw OdpsTunnelException(INTERNAL_ERROR,\
                "Invalid http status line: "+s);
        }

        rsp->SetStatusCode(util::StringTo<int>(util::TrimString(parts[1])));
    }

    return totalSize;
}

/* curl calls this routine to get more data */
size_t HttpConnection::write_callback(char *buffer,size_t size,size_t nitems,void *userp)
{
    size *= nitems;

    HttpConnection* hc = reinterpret_cast<HttpConnection*>(userp);
    HttpConnection::ChannelStream& stream = hc->mDownstream;

    /* remaining space in buffer */
    size_t rembuff = stream.buffer_len - stream.buffer_bytes + stream.buffer_pos;

    if (size > stream.buffer_len - stream.buffer_bytes)
    {
        if (size <= rembuff) {
            stream.reuse_buffer();
        }
        else {
            /* not enough space in buffer */
            size_t n = stream.buffer_len + size - rembuff;

            // LOG_WARNING(sLogger, ("write_callback","realloc")
            //         ("last len",stream.buffer_len)("new len",n)
            //         ("CURL_MAX_WRITE_SIZE",CURL_MAX_WRITE_SIZE));

            stream.realloc_buffer(n);
        }
    }

    stream.put_buffer(buffer, size);
    stream.total_bytes += size;
    hc->update();

    return size;
}

/* curl calls this routine to put more data */
size_t HttpConnection::read_callback(char *buffer,size_t size,size_t nitems,void *userp)
{
    size *= nitems;

    HttpConnection* hc = reinterpret_cast<HttpConnection*>(userp);
    HttpConnection::ChannelStream& stream = hc->mUpstream;

    if(hc->mPollFd.fd == -1)
    {
        hc->mCurlPaused = true;
        return CURL_READFUNC_PAUSE;
    }

    /* ensure only available data is considered */
    size_t size_left = stream.buffer_bytes - stream.buffer_pos;
    if(size_left <= 0) {
        if(hc->mUpstreamEOF) {
            return 0;
        } else {
            hc->mCurlPaused = true;
            return CURL_READFUNC_PAUSE;
        }
    }
    if(size_left < size)
      size = size_left;

    /* xfer data to caller */
    memcpy(buffer, stream.buffer + stream.buffer_pos, size);

    stream.buffer_pos += size;
    stream.total_bytes += size;

    hc->update();

    return size;
}

int HttpConnection::progress_callback(void *userp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    HttpConnection* hc = reinterpret_cast<HttpConnection*>(userp);

    // poor implement
    int64_t current = util::timing::GetCurrentTimeInSeconds();
    if (current - hc->mLastUpdateTime > hc->mSocketTimeout)
    {
        // LOG_ERROR(sLogger,("connection","timeout"));
        hc->mTimeout = true;
        return 1;
    }

    if ( hc->mCurlPaused &&
      CURLE_OK==curl_easy_pause(hc->mHandle,0) )
    {
        hc->mCurlPaused = false;
    }

    return 0;
}

int HttpConnection::timer_callback(CURLM *multi, long timeout_ms, void *userp)
{
    return 0;
}

int HttpConnection::socket_callback(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp)
{
    HttpConnection* hc = reinterpret_cast<HttpConnection*>(userp);
    struct pollfd& fd = hc->mPollFd;

    if (fd.fd != -1 && fd.fd != s) {
        fprintf(stderr,"error: input fd:%d,\
            current fd:%d\n", s, fd.fd);
    }

    fd.fd = s;
    fd.events = 0;
    fd.revents = 0;

    if (action == CURL_POLL_REMOVE) {
        fd.fd = -1;
    }
    else if (action == CURL_POLL_IN || action == CURL_POLL_INOUT) {
        fd.events |= POLLIN;
    }
    else if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT) {
        fd.events |= POLLOUT;
    }

    return 0;
}

bool HttpConnection::drive(ChannelStream& stream)
{
    if(mPollFd.fd == -1 || !mStillRunning)
        return false;

    bool ret = false;
    size_t bytes = stream.buffer_bytes - stream.buffer_pos;

    do {

        if (mPollWaitTime <= 0) mPollWaitTime = 1;
        mPollFd.events = stream.is_upstream ? POLLOUT : POLLIN;
        poll(&mPollFd, 1, mPollWaitTime);

        SAFE_MCALL(curl_multi_socket_action(mMultiHandle, mPollFd.fd, 0, &mStillRunning));

        if ((stream.is_upstream && stream.buffer_pos == stream.buffer_bytes)
          || (!stream.is_upstream && stream.buffer_bytes - stream.buffer_pos > bytes)
          || (!stream.is_upstream && mResponse->GetContentLength() == 0))
        {
            ret = true;
            break;
        }

    } while (mPollFd.fd != -1 && mStillRunning);

    if (mPollFd.fd == -1 || !mStillRunning)
    {
        // LOG_DEBUG(sLogger, ("drive isupstream", stream.is_upstream)
        //         ("bytes", bytes)("multi info", read_multi_info())
        //         ("upstream totalbytes", GetUpstreamBytes())
        //         ("downstream totalbytes", GetDownstreamBytes()));
    }

    if (stream.is_upstream && !mUpstreamEOF)
    {
        int sc = mResponse->GetStatusCode();
        if (sc)
        {
            std::string content;
            mResponse->ReadBody(content);
            throw OdpsTunnelException(std::to_string(sc),"Unexpected response:"+content);
        }
    }

    return ret;
}

void HttpConnection::ChannelStream::reuse_buffer()
{
    if (!buffer_pos)
        return;

    /* move rest down make it available for later */
    memmove(buffer, buffer+buffer_pos, buffer_bytes-buffer_pos);

    buffer_bytes -= buffer_pos;
    buffer_pos = 0;
}

void HttpConnection::ChannelStream::realloc_buffer(size_t size)
{
    if (size <= buffer_len)
        return;

    // LOG_WARNING(sLogger, ("ChannelStream","realloc")
    //     ("last len",buffer_len)("new len",size));

    char* newbuff = new char[size];
    memcpy(newbuff, buffer+buffer_pos, buffer_bytes-buffer_pos);
    delete [] buffer;

    buffer = newbuff;
    buffer_bytes -= buffer_pos;
    buffer_pos = 0;
    buffer_len = size;
}

void HttpConnection::ChannelStream::clear_buffer()
{
    buffer_pos = 0;
    buffer_bytes = 0;
    total_bytes = 0;
}

void HttpConnection::ChannelStream::put_buffer(const void* ptr, size_t n)
{
    memcpy(buffer+buffer_bytes, ptr, n);
    buffer_bytes += n;
}

size_t HttpConnection::ChannelStream::get_buffer(void* ptr, size_t n)
{
    n = buffer_bytes-buffer_pos>n?n:buffer_bytes-buffer_pos;
    memcpy(ptr, buffer+buffer_pos, n);
    buffer_pos += n;
    return n;
}

string HttpConnection::TraceError(void)
{
    return read_multi_info();
}

string HttpConnection::read_multi_info()
{
    std::ostringstream os;
    int msg_left;
    CURLMsg* msg;

    while (mMultiHandle && (msg = curl_multi_info_read(mMultiHandle, &msg_left))) {
        os << "CURLMSG:" << CurlMsg(msg->msg);
        os << ",CURLCode:" << msg->data.result;
        os << ",CODEMsg:" << curl_easy_strerror(msg->data.result) << ",";
    }

    return os.str();
}

void HttpConnection::update()
{
    if (mConn)
    {
        mLastUpdateTime = mConn->UpdateLastUsedTime();
    }
    else
    {
        mLastUpdateTime = util::timing::GetCurrentTimeInSeconds();
    }
}

std::string HttpConnection::GetEncodedURL(CURL * curl)
{
    std::string url = mRequest->GetEndpoint();
    if (!mRequest->GetResourcePath().empty())
    {
        if (!util::EndWith(url,"/"))
        {
            url.append("/").append(mRequest->GetResourcePath());
        }
        else
        {
            url.append(mRequest->GetResourcePath());
        }
    }

    std::map<std::string, std::string> parameters;
    mRequest->GetParameters(parameters);
    if (parameters.size())
    {
        url.append("?");
    }
    typeof(parameters.begin()) it = parameters.begin();
    for (; it != parameters.end(); ++it)
    {
        url.append(it->first);
        if (it->second != "")
        {
            char * encodedParam = curl_easy_escape(curl, it->second.c_str(), it->second.size());
            if (encodedParam)
            {
                url.append("=").append(encodedParam);
                curl_free(encodedParam);
            }
            else
            {
                throw OdpsTunnelException(INVALID_ARGUMENT,"Invalid Param:" + it->second);
            }
        }
        // do not append last "&"
        if (std::distance(it, parameters.end()) != 1)
        {
            url.append("&");
        }
    }
    return url;
}

void HttpConnection::Open()
{
    int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();
    if (mRequest.get() == NULL)
        throw OdpsTunnelException(INVALID_ARGUMENT,"Invalid request.");

    mResponse = ResponsePtr(new Response(this));

    if (mKeepAlive)
    {
        const std::string& token =
            mRequest->GetHeader(HEADER_ODPS_MAX_STORAGE_ROUTE_TOKEN);
        std::string poolName =
            token.empty() ? kDefaultPoolName
                          : util::StringSpliter(token, kTokenDelimeter).at(1);

        mConn = CurlConnectionPool::GetInstance().AcquireGuardedPtr(
            poolName, mConf.GetSocketConnectTimeout());
        mHandle = mConn->GetCurlHandle();
        mMultiHandle = mConn->GetCurlMultiHandle();
    }
    else
    {
        mHandle = curl_easy_init();
    }

    const std::string& url = GetEncodedURL(mHandle);

    if (util::ToLowerCaseString(url).find("https://") == 0)
    {
        static TunnelOpenSSLInitializer sOpenSSLInit4Tunnel; // for openssl multi thread
    }

    // LOG_DEBUG(sLogger,("url",url));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_URL, url.c_str()));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_VERBOSE, 0L));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_NOSIGNAL, 1));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_TCP_KEEPALIVE, 1L));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_TCP_KEEPIDLE, 120L));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_TCP_KEEPINTVL, 60L));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_TCP_NODELAY, 1L));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_SSL_CIPHER_LIST, "DEFAULT"));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_CONNECTTIMEOUT, mConf.GetSocketConnectTimeout()));

    if (mConf.disableSSLVerify)
    {
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYPEER, 0L));
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYHOST, 0L));
    }

    std::string method = mRequest->GetMethod();
    // LOG_DEBUG(sLogger,("method",method));
    std::map<std::string, std::string> m;
    mRequest->GetHeaders(m);

    m["Expect"] = ""; // disable HTTP 100 continue
    if (method == HTTP_METHOD_POST) {
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_POST, 1));
        const std::string& body = mRequest->GetBody();
        if (!body.empty()) {
            SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_POSTFIELDS, body.c_str()));
            SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_POSTFIELDSIZE, body.length()));
        }
        else
        {
            SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_READFUNCTION, read_callback));
            SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_READDATA, this));
        }
    }
    else if (method == HTTP_METHOD_GET) {
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_HTTPGET, 1));
    }
    else if (method == HTTP_METHOD_PUT) {
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_PUT, 1));
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_READFUNCTION, read_callback));
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_READDATA, this));
    }
    else if (method == HTTP_METHOD_DELETE)
    {
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_CUSTOMREQUEST, "DELETE"));
    }

    if (m.size())
    {
        /* add request headers */
        std::ostringstream oss;
        for(typeof(m.begin()) it = m.begin(); it != m.end(); ++it)
        {
            oss.str(""); //clear buffer
            oss << it->first << ": " << it->second;
            // LOG_DEBUG(sLogger, ("ADD HEADER", oss.str().c_str()));
            mHeaders = curl_slist_append(mHeaders, oss.str().c_str());
            if (it->first == "Content-Length" && util::StringTo<long>(it->second) != 0 && mRequest->GetBody().empty())
            {
                SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_INFILESIZE, util::StringTo<long>(it->second)));
            }
        }
        SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_HTTPHEADER, mHeaders));
    }

    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_HEADERDATA, this));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_HEADERFUNCTION, header_callback));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_WRITEDATA, this));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_WRITEFUNCTION, write_callback));

    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_NOPROGRESS, 0));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_PROGRESSDATA, this));
    SAFE_ECALL(curl_easy_setopt(mHandle, CURLOPT_PROGRESSFUNCTION, progress_callback));

    if(!mMultiHandle)
    {
        mMultiHandle = curl_multi_init();
    }

    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_TIMERDATA, this));
    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_TIMERFUNCTION, timer_callback));
    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_SOCKETDATA, this));
    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_SOCKETFUNCTION, socket_callback));
    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_PIPELINING , 1));
    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, 10));
    SAFE_MCALL(curl_multi_setopt(mMultiHandle, CURLMOPT_MAX_PIPELINE_LENGTH, 5));
    SAFE_MCALL(curl_multi_add_handle(mMultiHandle, mHandle));

    do {
        SAFE_MCALL(curl_multi_socket_action(mMultiHandle, CURL_SOCKET_TIMEOUT, 0, &mStillRunning));

        if (mPollWaitTime <= 0) mPollWaitTime = 1;

        poll(NULL, 0, mPollWaitTime);

    } while (mPollFd.fd == -1 && mStillRunning);

    if (mPollFd.fd == -1 || !mStillRunning)
    {
        if (mConn) mConn->MarkAsFailed();

        throw OdpsTunnelException(CONNECTION_ERROR,\
            "Open HttpConnection failed, url:" + url + "," + read_multi_info());
    }

    mIOCost += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
}

void HttpConnection::Close()
{
    int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();

    if (mHandle) {
        /* make sure the easy handle is not in the multi handle anymore */
        if (mMultiHandle)
            curl_multi_remove_handle(mMultiHandle, mHandle);

        if (mConn)
        {
            // keepalive
            mConn->Release();
            mConn.reset();
        }
        else
        {
            /* cleanup */
            curl_easy_cleanup(mHandle);
        }

        mHandle = nullptr;

        if (mHeaders)
        {
            curl_slist_free_all(mHeaders);
            mHeaders = nullptr;
        }
    }

    /* free any allocated buffer space */
    mUpstream.clear_buffer();
    mDownstream.clear_buffer();
    mUpstreamEOF = false;
    mIOCost += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
}

size_t HttpConnection::Read(char* ptr, size_t size)
{
    int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();
    if (mDownstream.buffer_bytes == mDownstream.buffer_pos)
    {
        mDownstream.reuse_buffer();
        drive(mDownstream);
    }
    /* xfer data to caller */
    size_t bufferSize = mDownstream.get_buffer(ptr, size);
    mIOCost += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
    return bufferSize;
}

size_t HttpConnection::Write(const char* ptr, size_t size)
{
    int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();
    size_t len = size;

    while (len > 0)
    {
        mUpstream.reuse_buffer();
        /* remaining space in buffer */
        size_t rembuff = mUpstream.buffer_len - mUpstream.buffer_bytes;
        size_t n = len > rembuff ? rembuff : len;

        mUpstream.put_buffer(ptr+size-len, n);

        while (mUpstream.buffer_bytes > mUpstream.buffer_pos)
        {
            if (!drive(mUpstream))
                throw OdpsTunnelException(CONNECTION_ERROR, "Connection error.");
        }

        len -= n;
    }
    mIOCost += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
    return size - len;
}

void HttpConnection::CloseUpstream()
{
    mUpstreamEOF = true;
}

void HttpConnection::SetRequest(RequestPtr req, bool connectOdpsServer)
{
    mRequest = req;

    time_t timer;
    char buffer[50];
    memset(buffer, 0, 50);
    struct tm* tm_info;
    struct tm result;
    time(&timer);
    tm_info = gmtime_r(&timer, &result);

    //RFC822 format date
    strftime(buffer, 50, "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    std::string date(buffer, strlen(buffer));

    const std::string& namespaceId = mConf.GetNamespaceId();
    if (!namespaceId.empty())
    {
        req->SetHeader(HEADER_ODPS_NAMESPACE_ID, namespaceId);
    }

    const std::string& tags = mConf.GetTags();
    if (!tags.empty())
    {
        req->SetHeader(HEADER_ODPS_TUNNEL_TAGS, tags);
    }

    req->SetHeader(DATE, date);
    req->SetHeader(USER_AGENT, mConf.GetUserAgent());
    if (req->GetHeader(CONTENT_TYPE) == "")
    {
        req->SetHeader(CONTENT_TYPE, "");
    }
    req->SetParameter(PARAM_REGION_ID, mConf.GetRegionId());

    Account acc = mConf.GetAccount();
    std::string type = util::ToLowerCaseString(acc.GetType());

    AppAccount app_account = mConf.GetAppAccount();
    StsToken ststok = mConf.GetStsToken();

    if (type == "" || type == ACCOUNT_ALIYUN || type == ACCOUNT_ARN)
    {
        Credentials credentials = acc.GetCredentials();
        AliyunRequestSigner signer(req->GetMethod(), "/" + req->GetResourcePath(), 
                                   credentials.accessKeyId(), credentials.accessKeySecret(), acc.GetRegion());
        signer.Sign(req);
        if (app_account.IsValid()) //存在APP Account，则双签名
        {
            AppRequestSigner app_signer(ACCOUNT_ALIYUN, req->GetMethod(), "/" + req->GetResourcePath(),
                                        app_account.GetId(), app_account.GetKey());
            app_signer.Sign(req);
        }
        if (ststok.IsValid())
        {
            StsTokenSigner signer(ststok);
            signer.Sign(req);
        }
        else if (type == ACCOUNT_ARN)
        {
            std::string arnSessionToken = credentials.sessionToken();
            if (!arnSessionToken.empty())
            {
                StsToken arnStsToken(arnSessionToken);
                StsTokenSigner signer(arnStsToken);
                signer.Sign(req);
            }
        }
    }
    else if (type == ACCOUNT_DOMAIN)
    {
        DomainRequestSigner signer(req->GetResourcePath(), acc.GetType(), acc.GetToken());
        signer.Sign(req);
    }
    else if (type == ACCOUNT_TOKEN)
    {
        req->SetHeader(HEADER_ODPS_BEARER_TOKEN, acc.GetToken());
        if (connectOdpsServer)
        {
            req->SetHeader("x-odps-user-util", "data-url-api");
            req->SetHeader("authorization_token", acc.GetToken());
            req->SetHeader("account_provider", "aliyun");
        }
        else
        {
            req->SetHeader(AUTHORIZATION, "ODPS bearer token, no need to sign");
        }
    }
    else if (type == ACCOUNT_TAOBAO) // havana
    {
        AliRequestSigner signer(req->GetMethod(), req->GetResourcePath(), acc);
        signer.Sign(req);
    }
    else
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "Unsupported authorization type in C++ SDK.");
    }
    if(!acc.GetApplicationSignature().empty())
    {
        req->SetHeader("application-authentication", acc.GetApplicationSignature());
    }
}

void HttpConnection::WaitResponse()
{
    int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();
    while (!mResponse->GetStatusCode() && mPollFd.fd != -1 && mStillRunning)
    {
        SAFE_MCALL(curl_multi_socket_action(mMultiHandle, mPollFd.fd, 0, &mStillRunning));

        if (mPollWaitTime <= 0) mPollWaitTime = 1;

        poll(&mPollFd, 1, mPollWaitTime);
    }

    if (!mResponse->GetStatusCode())
    {
        throw OdpsTunnelException(CONNECTION_ERROR, "Connection error.");
    }
    mIOCost += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
}

size_t HttpConnection::GetUpstreamData(void* ptr, size_t n)
{
    n = mUpstream.total_bytes > n?n:mUpstream.total_bytes;
    memcpy(ptr, mUpstream.buffer, n);
    return n;
}

bool HttpConnection::IsTimeout()
{
    return mTimeout;
}

}}}}
