#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "core/rest_client.h"
#include "gtest/gtest.h"
#include "include/configuration.h"
#include "test/common/test_util.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace internal
{

class HttpClientTest : public testing::Test
{
public:
    void SetUp() override
    {
        success_count = 0;
        failure_count = 0;
        total_bytes = 0;
    }

    std::atomic<long> success_count;
    std::atomic<long> failure_count;
    std::atomic<long> total_bytes;
    const std::string endpoint = Utils::GetTunnelEndpoint();
    const std::string resource = "ops/buildinfo";
};

#ifdef PERF_TEST

TEST_F(HttpClientTest, HighQPSStressTest)
{
    const int THREAD_COUNT = 256;
    const int REQUESTS_PER_THREAD = 5000000;
    const int TEST_DURATION_SEC = 8;

    Configuration conf;
    auto start_time = std::chrono::steady_clock::now();

    auto worker = [&](int thread_id)
    {
        std::mt19937 gen(thread_id);
        std::uniform_int_distribution<> dist(0, 100);

        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::seconds(TEST_DURATION_SEC);

        int requests_made = 0;

        while (std::chrono::steady_clock::now() < end &&
               requests_made < REQUESTS_PER_THREAD)
        {
            try
            {
                RequestPtr req(new Request());
                req->SetMethod("GET");
                req->SetEndpoint(endpoint);
                req->SetResourcePath(resource);
                HttpConnectionPtr conn =
                    std::make_shared<HttpConnection>(conf, true);
                conn->SetRequest(req);
                conn->Open();
                conn->CloseUpstream();
                conn->WaitResponse();

                ResponsePtr resp = conn->GetResponse();
                if (resp->GetStatusCode() == 200)
                {
                    std::string content;
                    resp->ReadBody(content);
                    success_count++;
                    total_bytes += content.size();
                }
                else
                {
                    failure_count++;
                }

                requests_made++;
            }
            catch (const std::exception& e)
            {
                failure_count++;
                std::cerr << "Thread " << thread_id << " error: " << e.what()
                          << std::endl;
            }
        }
    };

    // 启动监控线程
    std::thread monitor(
        [&]()
        {
            auto start = std::chrono::steady_clock::now();
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                   now - start)
                                   .count();

                if (elapsed >= TEST_DURATION_SEC) break;

                long current_success = success_count.load();
                long current_failure = failure_count.load();
                long current_bytes = total_bytes.load();

                auto poolStats =
                    CurlConnectionPool::GetInstance().GetPoolStats(kDefaultPoolName);
                std::cout << "[" << elapsed << "s] "
                          << "QPS: " << (current_success / (elapsed + 1))
                          << ", Success: " << current_success
                          << ", Failures: " << current_failure
                          << ", Throughput: "
                          << (current_bytes / 1024 / (elapsed + 1)) << " KB/s"
                          << ", TotalConnection: " << poolStats.mTotalConnections
                          << ", InUseConnection: " << poolStats.mInUseConnections
                          << ", IdelConnection: " << poolStats.mIdleConnections
                          << std::endl;

                ASSERT_TRUE(CurlConnectionPool::GetInstance()
                                .GetPoolStats(kDefaultPoolName)
                                .mTotalConnections <= (std::size_t)THREAD_COUNT);
            }
        });

    // 启动工作线程
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        threads.emplace_back(worker, i);
    }

    // 等待测试完成
    for (auto& t : threads)
    {
        t.join();
    }
    monitor.join();

    // 最终统计
    auto end_time = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time)
            .count();

    std::cout << "\n===== Final Results =====" << std::endl;
    std::cout << "Test duration: " << duration << " seconds" << std::endl;
    std::cout << "Total requests: " << (success_count + failure_count)
              << std::endl;
    std::cout << "Successful requests: " << success_count << std::endl;
    std::cout << "Failed requests: " << failure_count << std::endl;
    std::cout << "Average QPS: " << (success_count / duration) << std::endl;
    std::cout << "Success rate: "
              << (success_count * 100.0 / (success_count + failure_count))
              << "%" << std::endl;
    std::cout << "Total throughput: " << (total_bytes / 1024 / 1024) << " MB"
              << std::endl;
    std::cout << "Average throughput: " << (total_bytes / 1024 / duration)
              << " KB/s" << std::endl;
    std::cout << "Total Connection: "
              << CurlConnectionPool::GetInstance()
                     .GetPoolStats(kDefaultPoolName)
                     .mTotalConnections
              << std::endl;

    ASSERT_TRUE(CurlConnectionPool::GetInstance()
                    .GetPoolStats(kDefaultPoolName)
                    .mTotalConnections <= (std::size_t)THREAD_COUNT);
}

#endif

}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara