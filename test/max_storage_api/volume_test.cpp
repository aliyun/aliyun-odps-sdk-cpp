#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>

#include "core/volume_task.h"
#include "include/max_storage_api.h"
#include "include/odps_api.h"
#include "include/odps_exception.h"
#include "test/common/test_util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::max_storage_api;

class VolumeBuilderTest : public ::testing::Test
{
protected:
    MaxStorageApi mMaxStorageApi;

public:
    void SetUp() override
    {
        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());
        mMaxStorageApi.Init(config);
    }

    void TearDown() override
    {
    }
};

TEST_F(VolumeBuilderTest, TestMaxStorageApiBuilderMethods)
{
    // 测试各个Builder方法是否能正常返回对象
    auto fsWriteStreamBuilder = mMaxStorageApi.BuildVolumeFSWriteStream();
    ASSERT_NE(nullptr, fsWriteStreamBuilder);

    auto fsReadStreamBuilder = mMaxStorageApi.BuildVolumeFSReadStream();
    ASSERT_NE(nullptr, fsReadStreamBuilder);

    auto volumeReadSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    ASSERT_NE(nullptr, volumeReadSessionBuilder);

    auto volumeWriteSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    ASSERT_NE(nullptr, volumeWriteSessionBuilder);
}

class VolumeFSStreamTest : public ::testing::Test
{
protected:
    std::string mProjectName;
    std::string mVolumeNameNew;
    MaxStorageApi mMaxStorageApi;
    IVolumeManagerPtr mVolumeManager;

public:
    void SetUp() override
    {
        mProjectName = Utils::GetProjectName();
        mVolumeNameNew = Utils::GetRandomVolumeName();

        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());

        mMaxStorageApi.Init(config);

        mVolumeManager = IVolumeManager::Create();
        mVolumeManager->SetConfiguration(config);
        mVolumeManager->SetProject(mProjectName);
        mVolumeManager->CreateVolume(mVolumeNameNew, "TestVolume", VolumeType::NEW);
        std::cout << "Volume: " << mVolumeNameNew << std::endl;
    }

    void TearDown() override {
        mVolumeManager->DeleteVolume(mVolumeNameNew);
    }
};

TEST_F(VolumeFSStreamTest, TestReadWrite)
{
    const std::string& filePath = "/" + mVolumeNameNew + "/test/example8.txt";
    const std::string& testData = "hello,world,1234567890!";


    auto writeStream = mMaxStorageApi.BuildVolumeFSWriteStream()->SetProject(mProjectName)
                           .SetVolume(mVolumeNameNew)
                           .SetPath(filePath)
                           .Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();

    auto readStream = mMaxStorageApi.BuildVolumeFSReadStream()->SetProject(mProjectName)
                          .SetVolume(mVolumeNameNew)
                          .SetPath(filePath)
                          .Build();
    char buffer[1024];
    std::string readData;
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(testData, readData);
}

TEST_F(VolumeFSStreamTest, TestMultipleWrites)
{
    std::string filePath = "/" + mVolumeNameNew + "/test/example9.txt";
    std::string testData1 = "hello,wor";
    std::string testData2 = "ld,1234567890!";
    std::string expectedData = testData1 + testData2;
    std::string readData;

    auto writeStream = mMaxStorageApi.BuildVolumeFSWriteStream()
                           ->SetProject(mProjectName)
                           .SetVolume(mVolumeNameNew)
                           .SetPath(filePath)
                           .Build();
    writeStream->Write(testData1.c_str(), testData1.length());
    writeStream->Write(testData2.c_str(), testData2.length());
    writeStream->Close();

    // 创建读取流
    auto readStream = mMaxStorageApi.BuildVolumeFSReadStream()
                          ->SetProject(mProjectName)
                          .SetVolume(mVolumeNameNew)
                          .SetPath(filePath)
                          .Build();

    char buffer[1024];
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(expectedData, readData);
}

TEST_F(VolumeFSStreamTest, TestWriteStreamMultipleClose)
{
    const std::string& filePath = "/" + mVolumeNameNew + "/test/multiple_close.txt";
    const std::string& testData = "Test data for multiple close";

    auto writeStreamBuilder = mMaxStorageApi.BuildVolumeFSWriteStream();
    auto writeStream = writeStreamBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameNew)
                           .SetPath(filePath)
                           .Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();

    // 多次调用Close应该不会抛出异常
    EXPECT_NO_THROW(writeStream->Close());
    EXPECT_NO_THROW(writeStream->Close());
}

TEST_F(VolumeFSStreamTest, TestWriteStreamWithReplicaCount)
{
    const std::string& filePath = "/" + mVolumeNameNew + "/test/replica_test.txt";
    const std::string& testData = "Test data with replica count";

    auto writeStreamBuilder = mMaxStorageApi.BuildVolumeFSWriteStream();
    auto writeStream = writeStreamBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameNew)
                           .SetPath(filePath)
                           .SetReplicaCount(2)
                           .Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();

    auto readStream = mMaxStorageApi.BuildVolumeFSReadStream()
                          ->SetProject(mProjectName)
                          .SetVolume(mVolumeNameNew)
                          .SetPath(filePath)
                          .Build();
    char buffer[1024];
    std::string readData;
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(testData, readData);
}

TEST_F(VolumeFSStreamTest, TestReadWithOffset)
{
    std::string filePath = "/" + mVolumeNameNew + "/test/example10.txt";
    std::string testData = "hello,world,1234567890!";
    std::string readData;

    // 写入完整数据
    auto writeStream = mMaxStorageApi.BuildVolumeFSWriteStream()
                           ->SetProject(mProjectName)
                           .SetVolume(mVolumeNameNew)
                           .SetPath(filePath)
                           .Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();

    // 从偏移量6开始读取（跳过"hello,"）
    int64_t startOffset = 6;
    int64_t length = 5; // 读取到"world"结束
    std::string expectedData = "world";

    auto readStream = mMaxStorageApi.BuildVolumeFSReadStream()
                          ->SetProject(mProjectName)
                          .SetVolume(mVolumeNameNew)
                          .SetPath(filePath)
                          .SetOffset(startOffset)
                          .SetCount(length)
                          .Build();

    char buffer[1024];
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();

    ASSERT_EQ(expectedData, readData);
    ASSERT_EQ(expectedData.length(), readData.length());
}


TEST_F(VolumeFSStreamTest, TestReadWithInvalidPath)
{
    const std::string& invalidPath = "/invalid/volume/path/nonexistent.txt";

    // 尝试读取不存在的文件
    auto readStreamBuilder = mMaxStorageApi.BuildVolumeFSReadStream();
    auto readStream = readStreamBuilder->SetProject(mProjectName)
                          .SetVolume(mVolumeNameNew)
                          .SetPath(invalidPath)
                          .Build();

    char buffer[1024];
    ASSERT_THROW({
        readStream->Read(buffer, sizeof(buffer));
    }, OdpsTunnelException);

    readStream->Close();
}

TEST_F(VolumeFSStreamTest, TestReadStreamMultipleClose)
{
    const std::string& filePath = "/" + mVolumeNameNew + "/test/read_multiple_close.txt";
    const std::string& testData = "Test data for read multiple close";

    // 先写入数据
    auto writeStreamBuilder = mMaxStorageApi.BuildVolumeFSWriteStream();
    auto writeStream = writeStreamBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameNew)
                           .SetPath(filePath)
                           .Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();

    // 读取数据
    auto readStreamBuilder = mMaxStorageApi.BuildVolumeFSReadStream();
    auto readStream = readStreamBuilder->SetProject(mProjectName)
                          .SetVolume(mVolumeNameNew)
                          .SetPath(filePath)
                          .Build();

    char buffer[1024];
    readStream->Read(buffer, sizeof(buffer));

    // 多次调用Close应该不会抛出异常
    EXPECT_NO_THROW(readStream->Close());
    EXPECT_NO_THROW(readStream->Close());
}

class VolumeSessionStreamTest : public ::testing::Test
{
protected:
    std::string mProjectName;
    std::string mVolumeNameOld;
    MaxStorageApi mMaxStorageApi;
    IVolumeManagerPtr mVolumeManager;

public:
    void SetUp() override
    {
        mProjectName = Utils::GetProjectName();
        mVolumeNameOld = Utils::GetRandomVolumeName();

        Configuration config = Utils::GetConfiguration();
        config.SetTunnelEndpoint(Utils::GetTunnelEndpoint());

        mMaxStorageApi.Init(config);

        mVolumeManager = IVolumeManager::Create();
        mVolumeManager->SetConfiguration(config);
        mVolumeManager->SetProject(mProjectName);
        mVolumeManager->CreateVolume(mVolumeNameOld, "TestVolume", VolumeType::OLD);
        std::cout << "Volume: " << mVolumeNameOld << std::endl;
    }

    void TearDown() override {
        mVolumeManager->DeleteVolume(mVolumeNameOld);
    }
};

TEST_F(VolumeSessionStreamTest, TestReadWrite)
{
    const std::string& partition = "dt20250807";
    const std::string& filePath = "session_test.txt";
    const std::string& testData = "hello,world,1234567890!TestSession";

    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();
    writeSession->Commit();
    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameOld)
                           .SetPartition(partition)
                           .SetFile(filePath)
                           .Build();

    auto readStream = readSession->BuildVolumeReadStream()->Build();
    char buffer[1024];
    std::string readData;
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(testData, readData);
}

TEST_F(VolumeSessionStreamTest, TestReadWriteWithZeroLength)
{
    const std::string& partition = "dt20250813";
    const std::string& filePath = "zero_length_test.txt";
    const std::string& testData = "";

    // 创建写会话并写入空数据
    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();
    writeSession->Commit();

    // 读取空数据
    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameOld)
                           .SetPartition(partition)
                           .SetFile(filePath)
                           .Build();

    auto readStream = readSession->BuildVolumeReadStream()->Build();
    char buffer[1024];
    int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
    readStream->Close();

    ASSERT_EQ(0, bytesRead);
}

TEST_F(VolumeSessionStreamTest, TestMultipleWrites)
{
    const std::string& partition = "dt20250812";
    const std::string& filePath = "session_test_multiple_writes.txt";
    const std::string& testData1 = "First part of the data. ";
    const std::string& testData2 = "Second part of the data. ";
    const std::string& testData3 = "Third part of the data.";
    const std::string& expectedData = testData1 + testData2 + testData3;

    // 创建写会话
    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    // 创建写流并进行多次写入
    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData1.c_str(), testData1.length());
    writeStream->Write(testData2.c_str(), testData2.length());
    writeStream->Write(testData3.c_str(), testData3.length());
    writeStream->Close();
    writeSession->Commit();

    // 创建读会话并读取数据
    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameOld)
                           .SetPartition(partition)
                           .SetFile(filePath)
                           .Build();

    auto readStream = readSession->BuildVolumeReadStream()->Build();
    char buffer[1024];
    std::string readData;
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();

    ASSERT_EQ(expectedData, readData);
}

TEST_F(VolumeSessionStreamTest, TestReadOffset)
{
    const std::string& partition = "dt20250809";
    const std::string& filePath = "session_test_offset.txt";
    const std::string& testData = "This is a long test string for offset and count test!";
    const std::string& expectedData = "long test";

    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();
    writeSession->Commit();

    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameOld)
                           .SetPartition(partition)
                           .SetFile(filePath)
                           .Build();

    // 从偏移量10开始读取，读取9个字符
    auto readStreamBuilder = readSession->BuildVolumeReadStream();
    auto readStream = readStreamBuilder->SetOffset(10)
                          .SetCount(9)
                          .Build();
    char buffer[1024];
    std::string readData;
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(expectedData, readData);
}

TEST_F(VolumeSessionStreamTest, TestWriteSessionAbort)
{
    const std::string& partition = "dt20250810";
    const std::string& filePath = "session_test_abort.txt";
    const std::string& testData = "This data should not be committed";

    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    ASSERT_EQ(writeSession->GetStatus(), "normal");

    const std::string& sessionId = writeSession->GetSessionId();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();

    // 不执行Commit而是执行Abort
    writeSession->Abort();

    // 验证文件未被创建或提交
    // 这里我们主要验证接口调用成功，具体的文件是否存在需要通过其他方式验证
    ASSERT_THROW({
        auto writeStream = writeSession->BuildWriteStream()->SetFile("another_file").Build();
        writeStream->Write(testData.c_str(), testData.length());
    }, OdpsTunnelException);

    writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .SetSessionId(sessionId)
                            .Build();
    ASSERT_EQ(writeSession->GetStatus(), "canceled");
}

TEST_F(VolumeSessionStreamTest, TestBuildSessionWithSessionId)
{
    const std::string& partition = "dt20250820";
    const std::string& filePath = "get_session_id_test.txt";
    const std::string& testData = "Test data for GetSessionId";

    // 创建写会话
    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                                            .SetVolume(mVolumeNameOld)
                                            .SetPartition(partition)
                                            .Build();

    // 验证能够获取到 sessionId
    std::string writeSessionId = writeSession->GetSessionId();
    ASSERT_FALSE(writeSessionId.empty());

    writeSession = writeSessionBuilder->SetProject(mProjectName)
                                        .SetVolume(mVolumeNameOld)
                                        .SetPartition(partition)
                                        .SetSessionId(writeSessionId)
                                        .Build();

    // 写入数据并提交
    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();
    writeSession->Commit();

    // 创建读会话
    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                                        .SetVolume(mVolumeNameOld)
                                        .SetPartition(partition)
                                        .SetFile(filePath)
                                        .Build();

    // 验证能够获取到 sessionId
    std::string readSessionId = readSession->GetSessionId();
    ASSERT_FALSE(readSessionId.empty());

    readSession = readSessionBuilder->SetProject(mProjectName)
                                        .SetVolume(mVolumeNameOld)
                                        .SetPartition(partition)
                                        .SetFile(filePath)
                                        .SetSessionId(readSessionId)
                                        .Build();

    // 读取数据验证
    auto readStream = readSession->BuildVolumeReadStream()->Build();
    char buffer[1024];
    std::string readData;
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(testData, readData);
}

TEST_F(VolumeSessionStreamTest, TestInvalidSessionId)
{
    const std::string& partition = "dt20250819";
    const std::string& filePath = "invalid_session_id_test.txt";
    const std::string& invalidSessionId = "invalid_session_id_12345";

    // 使用无效的sessionId应该抛出异常
    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    ASSERT_THROW({
        auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                                .SetVolume(mVolumeNameOld)
                                .SetPartition(partition)
                                .SetSessionId(invalidSessionId)
                                .Build();
    }, OdpsTunnelException);

    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    ASSERT_THROW({
        auto readSession = readSessionBuilder->SetProject(mProjectName)
                                .SetVolume(mVolumeNameOld)
                                .SetPartition(partition)
                                .SetFile("file")
                                .SetSessionId(invalidSessionId)
                                .Build();
    }, OdpsTunnelException);
}

TEST_F(VolumeSessionStreamTest, TestWriteStreamUseAfterClose)
{
    const std::string& partition = "dt20250814";
    const std::string& filePath = "use_after_close_test.txt";
    const std::string& testData = "Test data";

    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Close();

    // 在关闭后尝试写入应该抛出异常
    ASSERT_THROW({
        writeStream->Write(testData.c_str(), testData.length());
    }, OdpsTunnelException);
}

TEST_F(VolumeSessionStreamTest, TestReadStreamUseAfterClose)
{
    const std::string& partition = "dt20250815";
    const std::string& filePath = "read_use_after_close_test.txt";
    const std::string& testData = "Test data";

    // 先写入数据
    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();
    writeSession->Commit();

    // 读取数据
    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameOld)
                           .SetPartition(partition)
                           .SetFile(filePath)
                           .Build();

    auto readStream = readSession->BuildVolumeReadStream()->Build();
    readStream->Close();

    // 在关闭后尝试读取应该抛出异常
    char buffer[1024];
    ASSERT_THROW({
        readStream->Read(buffer, sizeof(buffer));
    }, OdpsTunnelException);
}

TEST_F(VolumeSessionStreamTest, TestVolumeSessionWithLargeData)
{
    const std::string& partition = "dt20250817";
    const std::string& filePath = "large_data_test.txt";

    // 创建大约1MB的数据
    std::string testData;
    testData.reserve(1024 * 1024);
    for (int i = 0; i < 1024 * 64; ++i) {
        testData += "This is a test line number " + std::to_string(i) + ". ";
    }

    auto writeSessionBuilder = mMaxStorageApi.BuildVolumeWriteSession();
    auto writeSession = writeSessionBuilder->SetProject(mProjectName)
                            .SetVolume(mVolumeNameOld)
                            .SetPartition(partition)
                            .Build();

    auto writeStream = writeSession->BuildWriteStream()->SetFile(filePath).Build();
    writeStream->Write(testData.c_str(), testData.length());
    writeStream->Close();
    writeSession->Commit();

    auto readSessionBuilder = mMaxStorageApi.BuildVolumeReadSession();
    auto readSession = readSessionBuilder->SetProject(mProjectName)
                           .SetVolume(mVolumeNameOld)
                           .SetPartition(partition)
                           .SetFile(filePath)
                           .Build();

    auto readStream = readSession->BuildVolumeReadStream()->Build();
    std::string readData;
    char buffer[8192];  // 使用较大缓冲区
    while (true)
    {
        int64_t bytesRead = readStream->Read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            break;
        }

        readData.append(buffer, bytesRead);
    }
    readStream->Close();
    ASSERT_EQ(testData, readData);
}

#ifdef PERF_TEST
TEST_F(VolumeStreamTest, HighQPSWriteeadTest)
{
    const int THREAD_COUNT = 512;
    const int WRITE_DURATION_SEC = 5;
    const int READ_DURATION_SEC = 10;
    const std::string TEST_FILE_PREFIX = "/" + sVolumeNameNew + "/stress_";
    const int TEST_DATA_SIZE = 1024 * 512; // 512KB
    const std::string TEST_DATA(TEST_DATA_SIZE, 'a');

    // Statistics for write phase
    std::atomic<long> writeSuccessCount(0);
    std::atomic<long> writeFailureCount(0);
    std::atomic<long> totalBytesWritten(0);

    // Statistics for read phase
    std::atomic<long> readSuccessCount(0);
    std::atomic<long> readFailureCount(0);
    std::atomic<long> totalBytesRead(0);

    // ================== Write Phase ==================
    {
        std::cout << "\n===== Starting Write Test Phase =====\n";
        auto writeStartTime = std::chrono::steady_clock::now();

        auto writer = [&](int threadId) {
            std::string filePath = TEST_FILE_PREFIX + std::to_string(threadId) + ".dat";

            auto writeStream = (*sMaxStorageApi.BuildVolumeWriteStream())
                                .SetProject(sProjectName)
                                .SetVolume(sVolumeNameNew)
                                .SetPath(filePath)
                                .Build();

            auto end = std::chrono::steady_clock::now() +
                       std::chrono::seconds(WRITE_DURATION_SEC);

            while (std::chrono::steady_clock::now() < end) {
                try {
                    writeStream->Write(TEST_DATA.c_str(), TEST_DATA.length());
                    totalBytesWritten += TEST_DATA.length();
                    writeSuccessCount++;
                } catch (const std::exception& e) {
                    writeFailureCount++;
                    std::cerr << "Write Thread " << threadId << " error: "
                              << e.what() << std::endl;
                }
            }
            writeStream->Close();
        };

        // Start write threads
        std::vector<std::thread> writeThreads;
        for (int i = 0; i < THREAD_COUNT; ++i) {
            writeThreads.emplace_back(writer, i);
        }

        // Monitor write progress
        for (int elapsed = 1; elapsed <= WRITE_DURATION_SEC; ++elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "[Write][" << elapsed << "s] "
                      << "OPS: " << (writeSuccessCount / elapsed)
                      << ", Success: " << writeSuccessCount.load()
                      << ", Failures: " << writeFailureCount.load()
                      << ", Throughput: " << (totalBytesWritten / 1024 / 1024 / elapsed)
                      << " MB/s" << std::endl;
        }

        // Wait for write threads to complete
        for (auto& t : writeThreads) {
            t.join();
        }

        auto writeDuration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - writeStartTime).count();

        std::cout << "\n===== Write Test Results =====" << std::endl;
        std::cout << "Duration: " << writeDuration << " seconds" << std::endl;
        std::cout << "Total writes: " << (writeSuccessCount + writeFailureCount) << std::endl;
        std::cout << "Success rate: "
                  << (writeSuccessCount * 100.0 / (writeSuccessCount + writeFailureCount))
                  << "%" << std::endl;
        std::cout << "Total written: " << (totalBytesWritten / 1024 / 1024)
                  << " MB" << std::endl;
        std::cout << "Average throughput: "
                  << (totalBytesWritten / 1024 / 1024) / writeDuration
                  << " MB/s" << std::endl;
        std::cout << "Average QPS: " << (writeSuccessCount) / writeDuration
                  << " op/s" << std::endl;
    }

    // ================== Read Phase ==================
    {
        std::cout << "\n===== Starting Read Test Phase =====\n";
        auto readStartTime = std::chrono::steady_clock::now();

        auto reader = [&](int threadId) {
            std::string filePath = TEST_FILE_PREFIX + std::to_string(threadId) + ".dat";
            auto end = std::chrono::steady_clock::now() +
                       std::chrono::seconds(READ_DURATION_SEC);

            while (std::chrono::steady_clock::now() < end) {
                try {
                    auto readStream = (*sMaxStorageApi.BuildVolumeReadStream())
                                        .SetProject(sProjectName)
                                        .SetVolume(sVolumeNameNew)
                                        .SetPath(filePath)
                                        .Build();
                    char buffer[TEST_DATA_SIZE];
                    long bytesRead = 0;
                    while (true) {
                        int64_t ret = readStream->Read(buffer, sizeof(buffer));
                        if (ret <= 0) break;
                        bytesRead += ret;
                    }

                    totalBytesRead += bytesRead;
                    readSuccessCount++;
                    readStream->Close();
                } catch (const std::exception& e) {
                    readFailureCount++;
                    std::cerr << "Read Thread " << threadId << " error: "
                              << e.what() << std::endl;
                }
            }
        };

        // Start read threads
        std::vector<std::thread> readThreads;
        for (int i = 0; i < THREAD_COUNT; ++i) {
            readThreads.emplace_back(reader, i);
        }

        // Monitor read progress
        for (int elapsed = 1; elapsed <= READ_DURATION_SEC; ++elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "[Read][" << elapsed << "s] "
                      << "OPS: " << (readSuccessCount / elapsed)
                      << ", Success: " << readSuccessCount.load()
                      << ", Failures: " <
                      < readFailureCount.load()
                      << ", Throughput: " << (totalBytesRead / 1024 / 1024 / elapsed)
                      << " MB/s" << std::endl;
        }

        // Wait for read threads to complete
        for (auto& t : readThreads) {
            t.join();
        }

        auto readDuration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - readStartTime).count();

        std::cout << "\n===== Read Test Results =====" << std::endl;
        std::cout << "Duration: " << readDuration << " seconds" << std::endl;
        std::cout << "Total reads: " << (readSuccessCount + readFailureCount) << std::endl;
        std::cout << "Success rate: "
                  << (readSuccessCount * 100.0 / (readSuccessCount + readFailureCount))
                  << "%" << std::endl;
        std::cout << "Total read: " << (totalBytesRead / 1024 / 1024)
                  << " MB" << std::endl;
        std::cout << "Average throughput: "
                  << (totalBytesRead / 1024 / 1024) / readDuration
                  << " MB/s" << std::endl;
        std::cout << "Average QPS: " << (readSuccessCount) / readDuration
                  << " op/s" << std::endl;
    }
}

#endif
