#include "common/logging.h"
#include "include/odps_api.h"
#include "test/common/test_util.h"
#include "gtest/gtest.h"
#include <iostream>
#include <stdlib.h>

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

class CoreResourceTest : public testing::Test
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
        mConf = Utils::GetConfiguration();
        mProjectName = Utils::GetProjectName();
        mResourceName = "test_resource_for_cpp_sdk";
    }

    virtual void TearDown()
    {
        mResource->Delete();
        std::cerr << "Delete resource [" + mResourceName + "] successfully!" << std::endl;
    }

    Configuration mConf;
    std::string mProjectName;
    std::string mResourceName;
    IODPSResourcePtr mResource;

    void UploadReource(std::stringstream &content)
    {
        IODPSPtr odps = IODPS::Create(mConf, mProjectName);
        mResource = odps->CreateResource(mResourceName, content);
        if (mResource->Exists())
        {
            std::cerr << "Resource [" + mResourceName + "] exists!" << std::endl;
            if (mResource->Delete())
            {
                std::cerr << "Delete [" + mResourceName + "] successfully!" << std::endl;
            }
            else
            {
                throw std::runtime_error("Delete [" + mResourceName + "] failed!");
            }
        }
        std::cerr << "Resource [" + mResourceName + "] does not exist!" << std::endl;
        if (mResource->Create(false))
        {
            std::cerr << "Create [" + mResourceName + "] successfully!" << std::endl;
        }
        else
        {
            throw std::runtime_error("Create [" + mResourceName + "] failed!");
        }
    }
};

TEST_F(CoreResourceTest, TestCoreResource)
{
    std::stringstream content;
    content << "This is a test!";

    UploadReource(content);
}
