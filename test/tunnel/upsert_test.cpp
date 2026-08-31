#include <stdlib.h>
#include <iostream>
#include <functional>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "include/error_code.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"

#include "tunnel/upsert_stream.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;


class Listener : public IListener
{
public:
    void OnFlush(const FlushResult& result) override
    {
        std::cout<< "flush success " + result.mTraceId << std::endl;
    }
    bool OnFlushFail(const std::string& error, int retry) override
    {
        std::cout << "flush failed: " << error << std::endl;
        return false;
    }
};
typedef std::shared_ptr<Listener> ListenerPtr;

class TunnelUpsertTest : public testing::Test
{
public:
    static std::string mTableName;
    static IODPSPtr mODPS;

protected:
    static std::string sProjectName;
    static std::string sTableName;
    static std::string sSchemaName;
    static OdpsTunnel  sTunnel;
    static std::map<std::string, std::string> hints;

    enum UpsertMode{
        UPSERT_ONLY,
        UPSERT_COMPACT,
        UPSERT_COMPCT_UPSERT
    };

    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        sProjectName = Utils::GetProjectName();
        sTableName = Utils::GetRandomTableName();
        sTunnel = Utils::GetTunnelInstance();
        mTableName = "";
        sSchemaName = "";
        Utils::DropTableIfExists(sTableName);
    }

    virtual void TearDown()
    {
        Utils::DropTableIfExists(sTableName);
        if (!mTableName.empty())
        {
            std::map<std::string, std::string> hints;
            hints["odps.namespace.schema"] = "true";
            hints["odps.sql.allow.namespace.schema"] = "true";
            Utils::ExecSql(sProjectName, "drop table if exists " + mTableName + ";", hints, 120 * 1000);
        }
        if (!sSchemaName.empty())
        {
            std::map<std::string, std::string> hints;
            hints["odps.namespace.schema"] = "true";
            hints["odps.sql.allow.namespace.schema"] = "true";
            Utils::ExecSql(sProjectName, "drop schema " + sProjectName + "." + sSchemaName + ";", hints, 120 * 1000);
        }
    }

    static void CreateTableSimple()
    {
        Utils::ExecSql(sProjectName, "create table " + sTableName + 
                " (key string not null" +
                ", value string" +
                ", primary key(key)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    }

    static void CreateTablePartialColumn()
    {
        Utils::ExecSql(sProjectName, "create table " + sTableName + 
                " (key string not null" +
                ", value string , value2 string" +
                ", primary key(key)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\", \"acid.partial.fields.update.enable\"=\"true\");", hints, 120 * 1000);
    }

    static void CreateRowAuthTable()
    {
        std::map<std::string, std::string> rowAuthHints{
                {"odps.sql.row.policy.enabled", "true"},
        };
        Utils::ExecSql(sProjectName, "create table " + sTableName + "(a bigint, b string);", rowAuthHints, 120 * 1000);
        Utils::ExecSql(sProjectName, "insert overwrite table " + sTableName + " values(1L, \"1\"), (2L, \"2\"), (3L, \"3\"), (4L, \"4\");", rowAuthHints, 120 * 1000);
        Utils::ExecSql(sProjectName, "create row access policy policy01 on " + sTableName + " to default filter using (a = 2L);", rowAuthHints, 120 * 1000);
    }

    static void CreatePartitionTable()
    {
        Utils::ExecSql(sProjectName, "create table " + sTableName + 
                " (key string not null, value string, primary key(key)) partitioned by (pt string)" +
                " tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
        Utils::ExecSql(sProjectName, "alter table " + sTableName + 
                "  add if not exists partition(pt='a');", hints, 120 * 1000);
    }

    static void CreateSchemaTable()
    {
        sSchemaName = Utils::GetRandomSchemaName();
        mTableName = sProjectName + "." + sSchemaName + "." + sTableName;
        std::map<std::string, std::string> schemaHints{
                {"odps.namespace.schema", "true"},
                {"odps.sql.allow.namespace.schema", "true"},
        };
        Utils::ExecSql(sProjectName, "create schema " + sProjectName + "." + sSchemaName + ";", schemaHints, 120 * 1000);
        Utils::ExecSql(sProjectName, "drop table if exists " + mTableName + ";", schemaHints, 120 * 1000);
        Utils::ExecSql(sProjectName, "create table " + mTableName + 
                " (key string not null, value string, primary key(key))" +
                " tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", schemaHints, 120 * 1000);
    }

    static void CreateSchemaPartitionTable()
    {
        sSchemaName = Utils::GetRandomSchemaName();
        mTableName = sProjectName + "." + sSchemaName + "." + sTableName;
        std::map<std::string, std::string> schemaHints{
                {"odps.namespace.schema", "true"},
                {"odps.sql.allow.namespace.schema", "true"},
        };
        Utils::ExecSql(sProjectName, "create schema " + sProjectName + "." + sSchemaName + ";", schemaHints, 120 * 1000);  
        Utils::ExecSql(sProjectName, "drop table if exists " + mTableName + ";", schemaHints, 120 * 1000);
        Utils::ExecSql(sProjectName, "create table " + mTableName + 
                        " (key string not null, value string, primary key(key)) partitioned by (pt string)" +
                        " tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", schemaHints, 120 * 1000);
        Utils::ExecSql(sProjectName, "alter table " + mTableName + " add if not exists partition(pt='a');", schemaHints, 120 * 1000);
    }

    static void CreateTableWithMultiType()
    {
        Utils::ExecSql(sProjectName, "create table " + sTableName +
                " (key string not null" +
                ", col0 int" +
                ", col1 bigint" +
                ", col2 string" +
                ", col3 decimal(38, 18)" +
                ", col4 tinyint" +
                ", col5 smallint" +
                ", col6 double" +
                ", col7 float" +
                ", col8 boolean" +
                ", col9 date" +
                ", col10 datetime" +
                ", col11 varchar(20)" +
                ", col12 char(20)" +
                ", col13 binary" +
                ", col14 timestamp" +
                ", col15 array<bigint>" +
                ", col16 map<bigint, string>" +
                ", col17 struct<name:string, age:int, parents:map<varchar(20), char(20)>, salary:float, hobbies:array<string>>" +
                ", primary key(key)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    }

    static void CreateMultiKeyTable()
    {
        Utils::ExecSql(sProjectName, "create table " + sTableName +
                " (col0 tinyint not null" +
                ", col1 smallint not null" +
                ", col2 int not null" +
                ", col3 bigint not null" +
                ", col4 float not null" +
                ", col5 double not null" +
                ", col6 boolean not null" +
                ", col7 char(20) not null" +
                ", col8 varchar(10) not null" +
                ", col9 string not null" +
                // ", col10 binary not null" +
                ", col11 string" +
                ", col12 array<bigint>" +
                ", col13 map<bigint, string>" +
                ", col14 decimal(38, 18)" +
                // ", col14 struct<name:string, age:int, parents:map<varchar(20), char(20)>, salary:float, hobbies:array<string>>" +
                // ", primary key(col0, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10)) tblproperties (\"transactional\"=\"true\");", hints, 120 * 1000);
                ", primary key(col0, col1, col2, col3, col4, col5, col6, col7, col9)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    }

    static void CreateUnsupportKeyTable()
    {
        Utils::ExecSql(sProjectName, "create table " + sTableName +
                " (col0 tinyint not null" +
                ", col1 smallint not null" +
                ", col2 int not null" +
                ", col3 datetime not null" +
                ", col4 string" +
                ", primary key(col0, col1, col2, col3)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    }

    static IUpsertPtr CreateUpsert(const std::string &partitionName = "", const std::string &upsertId = "", const std::string& schemaName = "")
    {
        IUpsertPtr upsert = sTunnel.CreateUpsert(sProjectName, sTableName, partitionName, upsertId, schemaName);
        return upsert;
    }

    static void UpsertAndVerify(const std::string& partitionName = "", const std::string& upsertId = "", const std::string& schemaName = "",
                                const UpsertMode& mode = UpsertMode::UPSERT_ONLY)
    {
        IUpsertPtr upsert = CreateUpsert(partitionName, upsertId, schemaName);
        IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

        IListenerPtr listenerPtr = std::make_shared<Listener>();
        upsertStream->setListener(listenerPtr);

        ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
        r->SetStringValue(0, "0");
        r->SetStringValue(1, "v1");
        upsertStream->Upsert(*r);

        r->SetStringValue(0, "0");
        r->SetStringValue(1, "v2");
        upsertStream->Upsert(*r);

        r->SetStringValue(0, "0");
        r->SetStringValue(1, "v3");
        upsertStream->Upsert(*r);

        r->SetStringValue(0, "1");
        r->SetStringValue(1, "v1");
        upsertStream->Upsert(*r);

        r->SetStringValue(0, "2");
        r->SetStringValue(1, "v1");
        upsertStream->Upsert(*r);
        upsertStream->Delete(*r);

        upsertStream->Flush();
        upsertStream->Close();
        upsert->Commit(false);
        ASSERT_TRUE(upsert->GetUpsertId() != "");
        
        upsert = CreateUpsert(partitionName, "", schemaName);
        upsertStream= upsert->CreateUpsertStream();
        r = upsert->CreateUpsertRecord();
        r->SetStringValue(0, "0");
        r->SetStringValue(1, "v3");
        upsertStream->Delete(*r);
        upsertStream->Flush();
        upsertStream->Close();
        upsert->Commit(false);

        ASSERT_TRUE(upsert->GetUpsertId() != "");
        std::cerr << "create upsert session, id=" << upsert->GetUpsertId() << std::endl;
        ASSERT_STRCASEEQ("COMMITTED", upsert->GetStatus().c_str());

        ISQLTaskPtr sqlTaskPtr = ISQLTask::Create();
        std::string selectSql = "select * from " + sTableName + ";";
        std::map<std::string, std::string> queryHint;
        queryHint["odps.sql.jobconf.odps2"] = "true";
        if(!sSchemaName.empty())
        {
            selectSql = "select * from " + mTableName + ";";
            queryHint["odps.namespace.schema"] = "true";
            queryHint["odps.sql.allow.namespace.schema"] = "true";
        }
        mODPS = Utils::GetODPS(sProjectName);

        IODPSInstancePtr instancePtr = sqlTaskPtr->Run(mODPS, selectSql, queryHint);
        try
        {
            instancePtr->WaitForSuccess(60000);
        }
        catch(OdpsException& e)
        {
            cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
        }

        std::unordered_map<string,string> taskResult = instancePtr->GetTaskResults();
        for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter)
        {
            if(!partitionName.empty())
            {
                ASSERT_EQ((iter->second), "\"key\",\"value\",\"pt\"\n\"1\",\"v1\",\"a\"\n");
            }
            else
            {
                ASSERT_EQ((iter->second), "\"key\",\"value\"\n\"1\",\"v1\"\n");
            }   
        }
    }
};

std::string TunnelUpsertTest::sProjectName;
std::string TunnelUpsertTest::sTableName;
std::string TunnelUpsertTest::sSchemaName;
OdpsTunnel  TunnelUpsertTest::sTunnel;
std::string TunnelUpsertTest::mTableName;
IODPSPtr TunnelUpsertTest::mODPS;

std::map<std::string, std::string> TunnelUpsertTest::hints{
        {"odps.sql.hive.compatible", "true"},
        {"odps.sql.preparse.odps2", "hybrid"},
        {"odps.sql.planner.mode", "lot"},
        {"odps.sql.planner.parser.odps2", "true"},
        {"odps.sql.ddl.odps2", "true"},
        {"odps.sql.decimal.odps2", "true"}
    };

TEST_F(TunnelUpsertTest, UpsertTable)
{
    CreateTableSimple();
    UpsertAndVerify();
}

TEST_F(TunnelUpsertTest, UpsertPartialColumn)
{
    CreateTablePartialColumn();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();

    r->SetStringValue(0, "key");
    r->SetStringValue(1, "value1");
    r->SetStringValue(2, "value1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsert->Commit(true);

    upsert = CreateUpsert();
    upsertStream= upsert->CreateUpsertStream();
    r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "key");
    r->SetStringValue(1, "value2");
    r->SetStringValue(2, "value2");

    // only update the first value2 column
    std::vector<std::string> columnList = {"value"};
    upsertStream->Upsert(*r, columnList);
    upsertStream->Flush();
    upsert->Commit(true);

    ISQLTaskPtr sqlTaskPtr = ISQLTask::Create();
    std::string selectSql = "select * from " + sTableName + ";";
    mODPS = Utils::GetODPS(sProjectName);

    IODPSInstancePtr instancePtr = sqlTaskPtr->Run(mODPS, selectSql);
    try
    {
        instancePtr->WaitForSuccess(60000);
    }
    catch(OdpsException& e)
    {
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    std::unordered_map<string,string> taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter)
    {
        std::cout<<"result: "<<(iter->second)<<endl;
        ASSERT_EQ((iter->second), "\"key\",\"value\",\"value2\"\n\"key\",\"value2\",\"value1\"\n");
    }
}

TEST_F(TunnelUpsertTest, UpsertReload)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    std::string reloadId = upsert->GetUpsertId();
    UpsertAndVerify("", reloadId);
}

TEST_F(TunnelUpsertTest, CreateUpsertSessionWithRowAuth)
{
    CreateRowAuthTable();
    try
    {
        IUpsertPtr upsert = CreateUpsert();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("MethodNotAllowed", ex.GetErrorCode());
        ASSERT_TRUE(ex.GetErrorMsg().find("RowAccessPolicy enabled table is not support to upsert now") != std::string::npos);
    }

}

TEST_F(TunnelUpsertTest, UpsertPartition)
{
    CreatePartitionTable();
    UpsertAndVerify("pt='a'");
}

TEST_F(TunnelUpsertTest, UpsertSchemaTable)
{
    sProjectName = Utils::GetSchemaEnabledProjectName();
    CreateSchemaTable();
    UpsertAndVerify("", "", sSchemaName);
}

TEST_F(TunnelUpsertTest, UpsertSchemaPartition)
{
    sProjectName = Utils::GetSchemaEnabledProjectName();
    CreateSchemaPartitionTable();
    UpsertAndVerify("pt='a'", "", sSchemaName);
}

TEST_F(TunnelUpsertTest, AbortSessionTest)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsert->Abort();
    try
    {
        upsertStream->Flush();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("SessionAborted", ex.GetErrorCode());
    }

    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("SessionAborted", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, ConcurrentAbortSessionTest)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();

    IUpsertPtr concurrentUpsert = CreateUpsert("", upsert->GetUpsertId());
    concurrentUpsert->Abort();
    upsertStream->Upsert(*r);

    try
    {
        upsertStream->Flush();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("SessionAborted", ex.GetErrorCode());
    }

    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("SessionAborted", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, CommitSessionTest)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsert->Commit(false);

    try
    {
        upsertStream->Flush();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("SessionStatusConflict", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, ConcurrentCommitSessionTest)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);

    IUpsertPtr concurrentUpsert = CreateUpsert("", upsert->GetUpsertId());
    concurrentUpsert->Commit(false);

    upsertStream->Upsert(*r);
    try
    {
        upsertStream->Flush();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("SessionStatusConflict", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertWithEmptySession)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    
    upsert->Commit(false);
    usleep(60 * 1000 * 1000);
    upsert->Commit(false);
}

TEST_F(TunnelUpsertTest, TableNotExist)
{
    try
    {
        IUpsertPtr upsert = CreateUpsert();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("NoSuchTable", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, PartitionNotExist)
{
    Utils::ExecSql(sProjectName, "create table " + sTableName + 
                " (key string not null, value string, primary key(key)) partitioned by (pt string)" +
                " tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    try
    {
        IUpsertPtr upsert = CreateUpsert("pt='a'");
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("NoSuchPartition", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertNullable)
{
    Utils::ExecSql(sProjectName, "create table " + sTableName + 
            " (key string not null" +
            ", value string not null" +
            ", primary key(key)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");

    try
    {
        upsertStream->Upsert(*r);
        upsertStream->Flush();
        upsertStream->Close();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("DataStoreError", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertNullPrimaryKey)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(1, "v1");

    try
    {
        upsertStream->Upsert(*r);
        upsertStream->Flush();
        upsertStream->Close();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("InternalError", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertEmptyStream)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    upsertStream->Flush();
    upsertStream->Close();
}

TEST_F(TunnelUpsertTest, UpsertTableDropped)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsertStream->Close();
    Utils::DropTableIfExists(sTableName);

    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("NoSuchTable", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertTableRenamed)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsertStream->Close();
    Utils::ExecSql(sProjectName, "alter table " + sTableName + " rename to " + sTableName + "_rename;", hints, 120 * 1000);  
    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("NoSuchTable", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertTableRecreated)
{
    CreateTableSimple();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsertStream->Close();
    Utils::DropTableIfExists(sTableName);
    Utils::ExecSql(sProjectName, "create table " + sTableName + 
                " (key string not null" +
                ", value string" +
                ", primary key(key)) tblproperties (\"transactional\"=\"true\",\"acid.ingest.commit.num.check.limit\"=\"2\");", hints, 120 * 1000);
    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("TableModified", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertPartitionDropped)
{
    CreatePartitionTable();
    IUpsertPtr upsert = CreateUpsert("pt='a'");
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsertStream->Close();
    Utils::ExecSql(sProjectName, "alter table " + sTableName + 
                " drop if exists partition(pt='a');", hints, 120 * 1000);
    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("NoSuchPartition", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertPartitionRenamed)
{
    CreatePartitionTable();
    IUpsertPtr upsert = CreateUpsert("pt='a'");
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();

    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    r->SetStringValue(1, "v1");
    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsertStream->Close();
    Utils::ExecSql(sProjectName, "alter table " + sTableName + 
                " partition(pt='a') rename to partition(pt='b');", hints, 120 * 1000);
    try
    {
        upsert->Commit(false);
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ("NoSuchPartition", ex.GetErrorCode());
    }
}

TEST_F(TunnelUpsertTest, UpsertMultiType)
{
    CreateTableWithMultiType();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();
    r->SetStringValue(0, "0");
    upsertStream->Upsert(*r);

    r->SetStringValue(0, "1");
    r->SetIntegerValue(1, 1835663446);    
    r->SetBigIntValue(2, 6703564975098045492);
    r->SetStringValue(3, "abc");
    r->SetDecimalValue(4, "1.2");
    r->SetTinyIntValue(5, 57);
    r->SetSmallIntValue(6, 10578);
    r->SetDoubleValue(7, 0.10945791659491044);
    r->SetFloatValue(8, 0.3795386);
    r->SetBoolValue(9, true);
    r->SetDateValue(10, 0);
    r->SetDatetimeValue(11, 0);

    r->SetVarcharValue(12, "varchar");
    r->SetCharValue(13, "char");
    std::string src = std::to_string(0);
    r->SetBinaryValue(14, src.c_str(), src.size());
    r->SetTimestampValue(15, 1, 1 * 1000000);

    string bigintArrTypeSpec = "ARRAY<BIGINT>";
    ODPSColumnTypeInfo bigintArrType = ODPSColumnTypeInfo::ParseTypeInfoString(bigintArrTypeSpec);
    shared_ptr<ODPSArray> bigintArray = std::make_shared<ODPSArray>(bigintArrType);
    for(int i = 0; i < 10; i++)
    {
        bigintArray->AppendBigIntValue(i);
    }
    r->SetArrayValue(16, bigintArray);

    string simpleMapTypeSpec = "MAP<BIGINT,STRING>";
    ODPSColumnTypeInfo simpleMapType = ODPSColumnTypeInfo::ParseTypeInfoString(simpleMapTypeSpec);
    shared_ptr<ODPSMap> simpleMap = std::make_shared<ODPSMap>(simpleMapType);

    for (int64_t i = 0; i < 10; i++)
    {
        string tmpstr = to_string(i);
        simpleMap->SetStringValue(i, tmpstr);
    }

    r->SetMapValue(17, simpleMap);

    upsertStream->Upsert(*r);
    upsertStream->Flush();
    upsertStream->Close();

    upsert->Commit(false);

    ISQLTaskPtr sqlTaskPtr = ISQLTask::Create();
    std::string selectSql = "select * from " + sTableName + ";";
    std::map<std::string, std::string> queryHint;
    queryHint["odps.sql.jobconf.odps2"] = "true";
    

    mODPS = Utils::GetODPS(sProjectName);

    IODPSInstancePtr instancePtr = sqlTaskPtr->Run(mODPS, selectSql, queryHint);
    try
    {
        instancePtr->WaitForSuccess(60000);
    }
    catch(OdpsException& e)
    {
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    std::unordered_map<string,string> taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter)
    {
        std::cout<<"result: "<<(iter->second)<<endl;
    }
}

TEST_F(TunnelUpsertTest, UpsertMultiKey)
{
    CreateMultiKeyTable();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();

    r->SetTinyIntValue(0, 57);
    r->SetSmallIntValue(1, 10578);
    r->SetIntegerValue(2, 1835663446);
    r->SetBigIntValue(3, 6703564975098045492);
    r->SetFloatValue(4, 0.3795386);
    r->SetDoubleValue(5, 0.10945791659491044);
    r->SetBoolValue(6, true);
    r->SetCharValue(7, "Char");
    r->SetVarcharValue(8, "Varchar");
    r->SetStringValue(9, "0");
    r->SetStringValue(10, "v2");

    upsertStream->Upsert(*r);
    upsertStream->Flush();
    
    upsert->Commit(false);
    std::cerr << "create upsert session, id=" << upsert->GetUpsertId() << std::endl;
    ASSERT_STRCASEEQ("COMMITTED", upsert->GetStatus().c_str());

    ISQLTaskPtr sqlTaskPtr = ISQLTask::Create();
    std::string selectSql = "select * from " + sTableName + ";";
    std::map<std::string, std::string> queryHint;
    queryHint["odps.sql.jobconf.odps2"] = "true";
    queryHint["odps.sql.decimal.odps2"] = "true";

    mODPS = Utils::GetODPS(sProjectName);

    IODPSInstancePtr instancePtr = sqlTaskPtr->Run(mODPS, selectSql, queryHint);
    try
    {
        instancePtr->WaitForSuccess(60000);
    }
    catch(OdpsException& e)
    {
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    std::unordered_map<string,string> taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter)
    {
        std::cout<<"result: "<<(iter->second)<<endl;
        ASSERT_EQ((iter->second), ("\"col0\",\"col1\",\"col2\",\"col3\",\"col4\",\"col5\",\"col6\",\"col7\",\"col8\",\"col9\",\"col11\",\"col12\",\"col13\",\"col14\"\n57,10578,1835663446,6703564975098045492,0.3795386,0.10945791659491044,true,\"Char                \",\"Varchar\",\"0\",\"v2\",\"\\N\",\"\\N\",\"\\N\"\n"));
    }
    
}

TEST_F(TunnelUpsertTest, UpsertUnsupportKey)
{
    CreateUnsupportKeyTable();
    IUpsertPtr upsert = CreateUpsert();
    IUpsertStreamPtr upsertStream= upsert->CreateUpsertStream();
    
    ODPSTableRecordPtr r = upsert->CreateUpsertRecord();

    r->SetTinyIntValue(0, 57);
    r->SetSmallIntValue(1, 10578);
    r->SetIntegerValue(2, 1835663446);
    r->SetDatetimeValue(3, 0);
    r->SetStringValue(4, "0");

    try
    {
        upsertStream->Upsert(*r);
        upsertStream->Flush();
        ASSERT_EQ("Should Not Reach Here", "But Reached.");
    }
    catch (OdpsException& ex)
    {
        ASSERT_EQ(NOT_IMPLEMENTED, ex.GetErrorCode());
        ASSERT_TRUE(ex.GetErrorMsg().find("Unsupported upsert key type") != std::string::npos);
    }
    upsert->Commit(false);
}