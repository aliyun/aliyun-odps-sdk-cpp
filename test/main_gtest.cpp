#include "common/logging.h"
#include "gmock/gmock.h"
#include "test/common/test_util.h"

int main(int argc,char** argv)
{
    apsara::odps::sdk::logging::InitLoggingSystem();
    if (!Utils::Initialize())
    	return 1;

    ::testing::FLAGS_gmock_verbose = "error";
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
