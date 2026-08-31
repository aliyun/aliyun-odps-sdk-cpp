
#include "curl_connection_pool.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/timer.h"
#include "curl_util.h"
#include "odps_exception.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

CurlConnection::CurlConnection(PrivateTag,
                               int64_t connectTimeout,
                               int64_t maxIdleTime,
                               const std::string& poolName)
    : mConnectTimeout(connectTimeout),
      mMaxIdleTime(maxIdleTime),
      mPoolName(poolName),
      mLastUpdateTime(util::timing::GetCurrentTimeInSeconds()),
      mFailed(false)
{
    // Initialize curl handle
    mCurl = curl_easy_init();
    if (!mCurl)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "curl_easy_init failed");
    }

    // Initialize multi handle
    mCurlMulti = curl_multi_init();
    if (!mCurlMulti)
    {
        curl_easy_cleanup(mCurl);
        mCurl = nullptr;
        throw OdpsTunnelException(INTERNAL_ERROR, "curl_multi_init failed");
    }

    try
    {
        SetDefaultOptions();
    }
    catch (...)
    {
        Close();
        throw;
    }
}

CurlConnection::~CurlConnection() { Close(); }

void CurlConnection::SetDefaultOptions()
{
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_VERBOSE, 0L));
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_NOSIGNAL, 1));
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_TCP_KEEPALIVE, 1L));
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_TCP_KEEPIDLE, 120L));
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_TCP_KEEPINTVL, 60L));
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_TCP_NODELAY, 1L));
    SAFE_ECALL(curl_easy_setopt(mCurl, CURLOPT_SSL_CIPHER_LIST, "DEFAULT"));
    SAFE_ECALL(
        curl_easy_setopt(mCurl, CURLOPT_CONNECTTIMEOUT, mConnectTimeout));
    SAFE_ECALL(
        curl_easy_setopt(mCurl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1));
}

int64_t CurlConnection::UpdateLastUsedTime() noexcept
{
    mLastUpdateTime = util::timing::GetCurrentTimeInSeconds();
    return mLastUpdateTime;
}

bool CurlConnection::IsExpired(int64_t now) const noexcept
{
    return mLastUpdateTime + mMaxIdleTime < now;
}

void CurlConnection::Reset()
{
    if (mCurl)
    {
        curl_easy_reset(mCurl);
        SetDefaultOptions();
    }

    if (mCurlMulti && mCurl)
    {
        curl_multi_remove_handle(mCurlMulti, mCurl);
    }

    mFailed = false;
    UpdateLastUsedTime();
}

void CurlConnection::Close() noexcept
{
    if (mCurlMulti && mCurl)
    {
        curl_multi_remove_handle(mCurlMulti, mCurl);
    }

    // Clean up curl handle
    if (mCurl)
    {
        curl_easy_cleanup(mCurl);
        mCurl = nullptr;
    }

    // Clean up multi handle
    if (mCurlMulti)
    {
        curl_multi_cleanup(mCurlMulti);
        mCurlMulti = nullptr;
    }
}

// CurlConnectionGuard implementation
CurlConnectionGuard::CurlConnectionGuard(CurlConnectionPtr conn,
                                         CurlConnectionPool* pool)
    : mConnection(std::move(conn)), mPool(pool)
{
}

CurlConnectionGuard::~CurlConnectionGuard() { Release(); }

CurlConnectionGuard::CurlConnectionGuard(CurlConnectionGuard&& other) noexcept
    : mConnection(std::move(other.mConnection)), mPool(other.mPool)
{
    other.mPool = nullptr;
}

CurlConnectionGuard& CurlConnectionGuard::operator=(
    CurlConnectionGuard&& other) noexcept
{
    if (this != &other)
    {
        Release();  // Release current connection
        mConnection = std::move(other.mConnection);
        mPool = other.mPool;
        other.mPool = nullptr;
    }
    return *this;
}

void CurlConnectionGuard::Release()
{
    if (mConnection && mPool)
    {
        mPool->Release(std::move(mConnection));
        mConnection.reset();
        mPool = nullptr;
    }
}

CurlConnectionPtr CurlConnectionPool::Acquire(const std::string& poolName,
                                              int64_t connectTimeout,
                                              int64_t maxIdleTime)
{
    const int64_t now = util::timing::GetCurrentTimeInSeconds();
    std::lock_guard<std::mutex> lock(mMutex);

    auto& pool = mConnectionPools[poolName];

    while (!pool.mIdleConnections.empty())
    {
        CurlConnectionPtr conn = std::move(pool.mIdleConnections.front());
        pool.mIdleConnections.pop_front();

        if (conn->IsExpired(now) || conn->IsFailed())
        {
            continue;
        }

        pool.mInUseCount++;
        conn->UpdateLastUsedTime();

        return conn;
    }

    const std::size_t mTotalConnections =
        pool.mIdleConnections.size() + pool.mInUseCount;
    if (mTotalConnections >= kMaxTotalConnections)
    {
        throw OdpsTunnelException(
            "ConnectionPoolLimitReached",
            "Connection pool limit reached for poolName: " + poolName);
    }

    auto conn = std::make_unique<CurlConnection>(
        CurlConnection::PrivateTag{}, connectTimeout, maxIdleTime, poolName);

    pool.mInUseCount++;
    return conn;
}

CurlConnectionGuard CurlConnectionPool::AcquireGuarded(
    const std::string& poolName, int64_t connectTimeout, int64_t maxIdleTime)
{
    auto conn = Acquire(poolName, connectTimeout, maxIdleTime);
    return CurlConnectionGuard(std::move(conn), this);
}

CurlConnectionGuardPtr CurlConnectionPool::AcquireGuardedPtr(
    const std::string& poolName, int64_t connectTimeout, int64_t maxIdleTime)
{
    auto conn = Acquire(poolName, connectTimeout, maxIdleTime);
    return std::make_unique<CurlConnectionGuard>(std::move(conn), this);
}

void CurlConnectionPool::Release(CurlConnectionPtr conn)
{
    if (!conn) return;

    const std::string& poolName = conn->GetPoolName();
    const int64_t now = util::timing::GetCurrentTimeInSeconds();

    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConnectionPools.find(poolName);
    if (it == mConnectionPools.end())
    {
        return;
    }

    auto& pool = it->second;

    if (pool.mInUseCount > 0)
    {
        pool.mInUseCount--;
    }

    if (conn->IsExpired(now) || conn->IsFailed())
    {
        return;
    }

    if (pool.mIdleConnections.size() >= kMaxIdleConnections)
    {
        return;
    }

    // If reset fails, just let the connection be destroyed
    try
    {
        conn->Reset();
        pool.mIdleConnections.push_back(std::move(conn));
    }
    catch (...)
    {
    }

    if (ShouldTriggerCleanup(now)) CleanupConnections(now);
}

CurlConnectionPool::~CurlConnectionPool()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mConnectionPools.clear();
}

bool CurlConnectionPool::ShouldTriggerCleanup(int64_t now)
{
    return now - mLastCleanTimeInSec > kCleanupIntervalInSec;
}

void CurlConnectionPool::CleanupConnections(int64_t now)
{
    for (auto& poolEntry : mConnectionPools)
    {
        auto& pool = poolEntry.second;
        auto& connections = pool.mIdleConnections;

        connections.remove_if(
            [now](const auto& conn)
            { return conn->IsExpired(now) || conn->IsFailed(); });
    }

    mLastCleanTimeInSec = now;
}

CurlConnectionPool::PoolStats CurlConnectionPool::GetPoolStats(
    const std::string& poolName) const
{
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConnectionPools.find(poolName);
    if (it == mConnectionPools.end())
    {
        return PoolStats{};
    }

    const auto& pool = it->second;
    PoolStats stats;
    stats.mIdleConnections = pool.mIdleConnections.size();
    stats.mInUseConnections = pool.mInUseCount;
    stats.mTotalConnections = stats.mIdleConnections + stats.mInUseConnections;
    return stats;
}

std::unordered_map<std::string, CurlConnectionPool::PoolStats>
CurlConnectionPool::GetAllStats() const
{
    std::lock_guard<std::mutex> lock(mMutex);

    std::unordered_map<std::string, PoolStats> allStats;
    allStats.reserve(mConnectionPools.size());

    for (const auto& poolEntry : mConnectionPools)
    {
        const auto& poolName = poolEntry.first;
        const auto& pool = poolEntry.second;

        PoolStats stats;
        stats.mIdleConnections = pool.mIdleConnections.size();
        stats.mInUseConnections = pool.mInUseCount;
        stats.mTotalConnections =
            stats.mIdleConnections + stats.mInUseConnections;

        allStats.emplace(poolName, stats);
    }

    return allStats;
}

}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara
