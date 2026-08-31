#include "odps_api.h"
#include "example/common/example_config.h"
#include <iostream>

using namespace std;
using namespace apsara::odps::sdk;



int main(void)
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    AliyunAccount account(config.mAccessId, config.mAccessKey);
    Configuration conf(account, config.mOdpsEndpoint);

    IODPSPtr odps = IODPS::Create(conf, config.mProjectName);

    ISQLTaskPtr sqlTaskPtr = ISQLTask::Create();

    std::string createDdl = "create table if not exists test_sql_task_t (id bigint);";

    std::string insertSql = "insert into test_sql_task_t values (12);";

    std::string selectSql = "select * from test_sql_task_t;";

    std::string dropDdl = "drop table if exists test_sql_task_t;";


    std::cout<<"/***create table***/"<<endl;

    IODPSInstancePtr instancePtr = sqlTaskPtr->Run(odps, createDdl);

    try{
        instancePtr->WaitForSuccess(60000);
    } catch(OdpsException& e){
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    std::unordered_map<std::string, TaskStatus>tasksStatus = instancePtr->AcquireTaskStatus();
    for(auto iter = tasksStatus.begin();iter!= tasksStatus.end();++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"task status: "<<IODPSInstance::GetTaskStatusName(iter->second)<<endl;
    }

    std::unordered_map<string,string>taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"result: "<<(iter->second)<<endl;
    }


    std::cout<<"/*** insert ***/"<<endl;

    instancePtr = sqlTaskPtr->Run(odps, insertSql);

    try{
        instancePtr->WaitForSuccess(60000);
    } catch(OdpsException& e){
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    tasksStatus = instancePtr->AcquireTaskStatus();
    for(auto iter = tasksStatus.begin();iter!= tasksStatus.end();++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"task status: "<<IODPSInstance::GetTaskStatusName(iter->second)<<endl;
    }

    taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"result: "<<(iter->second)<<endl;
    }

    
    std::cout<<"/*** select ***/"<<endl;

    instancePtr = sqlTaskPtr->Run(odps, selectSql);

    try{
        instancePtr->WaitForSuccess(60000);
    } catch(OdpsException& e){
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    tasksStatus = instancePtr->AcquireTaskStatus();
    for(auto iter = tasksStatus.begin();iter!= tasksStatus.end();++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"task status: "<<IODPSInstance::GetTaskStatusName(iter->second)<<endl;
    }

    taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"result: "<<(iter->second)<<endl;
    }


    std::cout<<"/*** drop table ***/"<<endl;

    instancePtr = sqlTaskPtr->Run(odps, dropDdl);

    try{
        instancePtr->WaitForSuccess(60000);
    } catch(OdpsException& e){
        cout<<e.GetErrorCode()<<e.GetErrorMsg()<<endl;
    }

    tasksStatus = instancePtr->AcquireTaskStatus();
    for(auto iter = tasksStatus.begin();iter!= tasksStatus.end();++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"task status: "<<IODPSInstance::GetTaskStatusName(iter->second)<<endl;
    }

    taskResult = instancePtr->GetTaskResults();
    for(auto iter = taskResult.begin();iter!=taskResult.end(); ++iter){
        std::cout<<"task name: "<<iter->first<<endl;
        std::cout<<"result: "<<(iter->second)<<endl;
    }

    return 0;
}