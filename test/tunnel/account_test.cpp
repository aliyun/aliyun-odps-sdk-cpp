#include <stdlib.h>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"

#include "tunnel/util.h"

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

class TunnelAccountTest : public testing::Test
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
    }

    virtual void TearDown()
    {
    }
};

TEST_F(TunnelAccountTest, StsTunnelAccountTest)
{
    Configuration mConf;
    RequestPtr req(new Request());
    AliyunAccount account("access_id", "access_key");
    StsToken ststok("sts_token");

    mConf.SetAccount(account);
    mConf.SetStsToken(ststok);
    HttpConnectionPtr conn(new HttpConnection(mConf));
    conn->SetRequest(req);

    ASSERT_TRUE(!req->GetHeader("Authorization").empty());
    ASSERT_EQ(req->GetHeader("authorization-sts-token"), "sts_token");
}
