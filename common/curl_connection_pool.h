#pragma once

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "configuration.h"
#include "curl/curl.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{
static constexpr const char* kDefaultPoolName = "default";
static constexpr int64_t kDefaultMaxIdleTimeInSec = 600;    // 10 minutes
static constexpr int64_t kDefaultConnectTimeoutInSec = 30;  // 30 seconds
static constexpr int64_t kCleanupIntervalInSec = 120;       // 2 minutes
static constexpr std::size_t kMaxIdleConnections = 128;
static constexpr std::size_t kMaxTotalConnections = 65535;

// Forward declaration
class CurlConnectionPool;

class CurlConnection
{
private:
    struct PrivateTag
    {
    };

public:
    // Only create via CurlConnectionPool
    CurlConnection(PrivateTag,
                   int64_t connectTimeout = kDefaultConnectTimeoutInSec,
                   int64_t maxIdleTime = kMaxIdleConnections,
                   const std::string& poolName = kDefaultPoolName);

    ~CurlConnection();

    // Non-copyable, non-movable
    CurlConnection(const CurlConnection&) = delete;
    CurlConnection& operator=(const CurlConnection&) = delete;
    CurlConnection(CurlConnection&&) = delete;
    CurlConnection& operator=(CurlConnection&&) = delete;

    // Status methods
    bool IsExpired(int64_t now) const noexcept;
    bool IsFailed() const noexcept { return mFailed; }
    void MarkAsFailed() noexcept { mFailed = true; }
    int64_t UpdateLastUsedTime() noexcept;

    // Getters
    int64_t GetLastUpdateTime() const noexcept { return mLastUpdateTime; }
    const std::string& GetPoolName() const noexcept { return mPoolName; }
    CURL* GetCurlHandle() const noexcept { return mCurl; }
    CURLM* GetCurlMultiHandle() const noexcept { return mCurlMulti; }

    friend class CurlConnectionPool;

private:
    void Reset();
    void Close() noexcept;
    void SetDefaultOptions();

    const int64_t mConnectTimeout;
    const int64_t mMaxIdleTime;
    const std::string mPoolName;
    std::atomic<int64_t> mLastUpdateTime;
    std::atomic<bool> mFailed;
    CURL* mCurl = nullptr;
    CURLM* mCurlMulti = nullptr;
};

using CurlConnectionPtr = std::unique_ptr<CurlConnection>;

// RAII wrapper for automatic connection release
class CurlConnectionGuard
{
public:
    explicit CurlConnectionGuard(CurlConnectionPtr conn,
                                 CurlConnectionPool* pool);
    ~CurlConnectionGuard();

    // Non-copyable, but movable
    CurlConnectionGuard(const CurlConnectionGuard&) = delete;
    CurlConnectionGuard& operator=(const CurlConnectionGuard&) = delete;
    CurlConnectionGuard(CurlConnectionGuard&& other) noexcept;
    CurlConnectionGuard& operator=(CurlConnectionGuard&& other) noexcept;

    int64_t UpdateLastUsedTime() const noexcept
    {
        return mConnection ? mConnection->GetLastUpdateTime() : 0;
    }

    void MarkAsFailed() const noexcept
    {
        if (mConnection) mConnection->MarkAsFailed();
    }

    CURL* GetCurlHandle() const noexcept
    {
        return mConnection ? mConnection->GetCurlHandle() : nullptr;
    }

    CURLM* GetCurlMultiHandle() const noexcept
    {
        return mConnection ? mConnection->GetCurlMultiHandle() : nullptr;
    }

    explicit operator bool() const noexcept { return mConnection != nullptr; }

    void Release();

private:
    CurlConnectionPtr mConnection;
    CurlConnectionPool* mPool;
};

using CurlConnectionGuardPtr = std::unique_ptr<CurlConnectionGuard>;

class CurlConnectionPool
{
public:
    static CurlConnectionPool& GetInstance()
    {
        static CurlConnectionPool sPool;
        return sPool;
    }

    CurlConnectionPtr Acquire(
        const std::string& poolName = kDefaultPoolName,
        int64_t connectTimeout = kDefaultConnectTimeoutInSec,
        int64_t maxIdleTime = kDefaultMaxIdleTimeInSec);
    // RAII interface - automatic release
    CurlConnectionGuard AcquireGuarded(
        const std::string& poolName = kDefaultPoolName,
        int64_t connectTimeout = kDefaultConnectTimeoutInSec,
        int64_t maxIdleTime = kDefaultMaxIdleTimeInSec);

    // Factory method for unique_ptr usage - recommended for member storage
    CurlConnectionGuardPtr AcquireGuardedPtr(
        const std::string& poolName = kDefaultPoolName,
        int64_t connectTimeout = kDefaultConnectTimeoutInSec,
        int64_t maxIdleTime = kDefaultMaxIdleTimeInSec);

    void Release(CurlConnectionPtr conn);

    struct PoolStats
    {
        std::size_t mIdleConnections = 0;
        std::size_t mInUseConnections = 0;
        std::size_t mTotalConnections = 0;
    };
    PoolStats GetPoolStats(const std::string& poolName) const;
    std::unordered_map<std::string, PoolStats> GetAllStats() const;

public:
    ~CurlConnectionPool();
    CurlConnectionPool(const CurlConnectionPool&) = delete;
    CurlConnectionPool& operator=(const CurlConnectionPool&) = delete;
    CurlConnectionPool(CurlConnectionPool&&) = delete;
    CurlConnectionPool& operator=(CurlConnectionPool&&) = delete;

private:
    CurlConnectionPool() = default;
    struct ConnectionPoolState
    {
        std::list<CurlConnectionPtr> mIdleConnections;
        std::size_t mInUseCount = 0;
    };

private:
    bool ShouldTriggerCleanup(int64_t now);

    // These functions are not internally locked; please add locking at the call
    // sites.
    void CleanupConnections(int64_t now);

private:
    mutable std::mutex mMutex;
    std::unordered_map<std::string, ConnectionPoolState> mConnectionPools;

    std::atomic<int64_t> mLastCleanTimeInSec{0};
};

}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara