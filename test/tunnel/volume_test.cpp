#include <limits>
#include <stdlib.h>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "gtest/gtest.h"
#include <string>
#include "test/common/test_util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;

class TunnelVolumeTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        mProjectName = Utils::GetProjectName();
        mVolumeName = Utils::GetRandomVolumeName();
        mPartitionName = Utils::GetRandomPartitionName();
        mTunnel = Utils::GetTunnelInstance();
        Utils::RecreateVolume(mVolumeName);
    }

    virtual void TearDown()
    {
        Utils::RemoveVolume(mVolumeName);
    }

    std::string mProjectName;
    std::string mVolumeName;
    std::string mPartitionName;
    OdpsTunnel mTunnel;
};

TEST_F(TunnelVolumeTest, UploadDownload)
{
    const static std::string _buf = "hello world!";
    IVolumeUploadPtr upload = mTunnel.CreateVolumeUpload(mProjectName, mVolumeName, mPartitionName);
    std::cout << upload->GetUploadId() << std::endl;
    IVolumeOutputStreamPtr output = upload->OpenOutputStream("test_file");
    output->Write(_buf.c_str(), _buf.size());
    output->Close();
    upload->Commit({"test_file"});
    std::cout << "UPLOAD COMPLETE." << std::endl;
    IVolumeDownloadPtr download = mTunnel.CreateVolumeDownload(mProjectName, mVolumeName, mPartitionName, "test_file");
    std::cout << download->GetDownloadId() << std::endl;
    ASSERT_EQ(download->GetFileLength(), _buf.size());
    IVolumeInputStreamPtr input = download->OpenInputStream(0, std::numeric_limits<int64_t>::max(), true);
    char readbuf[100] = {0};
    int readsize = input->Read(readbuf, 100);
    ASSERT_EQ(readsize, int(_buf.size()));
    ASSERT_EQ(std::string(readbuf, _buf.size()), _buf);
    input->Close();
    download->Complete();
}