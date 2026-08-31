#include <unistd.h>
#include "util/string_util.h"
#include "instance.h"
#include "tinyxml2.h"


namespace apsara { namespace odps { namespace sdk {

InstanceStatus IODPSInstance::ParseStatus(const std::string& status){
    if(status == "Running"){
        return InstanceStatus::RUNNING;
    } else if(status == "Suspended"){
        return InstanceStatus::SUSPENDED;
    } else if(status == "Terminated"){
        return InstanceStatus::TERMINATED;
    } else{
        throw OdpsException("InvaildInstanceStatus", "Invaild instance status:" + status);
    }
}

TaskStatus IODPSInstance::ParseTaskStatus(const std::string& status){
    if(status == "Waiting"){
        return TaskStatus::WAITING;
    } else if(status == "Running"){
        return TaskStatus::RUNNING;
    } else if(status == "Success"){
        return TaskStatus::SUCCESS;
    } else if(status == "Failed"){
        return TaskStatus::FAILED;
    } else if(status == "Suspended"){
        return TaskStatus::SUSPENDED;
    } else if(status == "Cancelled"){
        return TaskStatus::CANCELLED;
    } else{
        throw OdpsException("InvaildTaskStatus", "Invaild Task Status:" + status);
    }
}

std::string IODPSInstance::GetInstanceStatusName(InstanceStatus status){
    switch(status){
        case InstanceStatus::RUNNING:
            return "Running";
        case InstanceStatus::SUSPENDED:
            return "Suspended";
        case InstanceStatus::TERMINATED:
            return "Terminated";
        default:
            throw OdpsException("UnknownInstanceStatus", "Unknown instance status:" + std::to_string((int) status));
    }
}

std::string IODPSInstance::GetTaskStatusName(TaskStatus status){
    switch (status)
    {
        case TaskStatus::RUNNING:
            return "Running";
        case TaskStatus::SUSPENDED:
            return "Suspended";
        case TaskStatus::WAITING:
            return "Waiting";
        case TaskStatus::SUCCESS:
            return "Success";
        case TaskStatus::FAILED:
            return "Failed";
        case TaskStatus::CANCELLED:
            return "Cancelled";
        default:
            throw OdpsException("UnknownTaskStatus", "Unknown task status:" + std::to_string((int) status));
    }
}

namespace internal {

const static std::string HOST_DEFAULT = "http://logview.odps.aliyun-inc.com:8080";

Instance::Instance(const Configuration& conf,
    const std::string& project,
    const std::string& job):
    job(job),
    project(project),
    mConf(conf),
    mRestClient(std::make_shared<RestClient>(conf)),
    sm(std::make_shared<SecurityManager>(project, mRestClient))
{}


const std::string& Instance::GetInstanceId()
{
    return this->id;
}

void Instance::SetInstanceId(const std::string& id)
{
    this->id = id;
}

bool Instance::Create()
{
    std::string rest_path = RestResourceBuilder::BuildInstancesRest(project);

    // 设置请求头
    std::map<std::string, std::string> headers;
    headers[CONTENT_TYPE] = "application/xml";
    headers[CONTENT_LENGTH] = to_string(this->job.length());
    headers[CONTENT_MD5] = util::GenerateMd5Signature(this->job);

    RestResponsePtr detail = mRestClient->DoRequest(rest_path, HTTP_METHOD_POST, headers, {}, this->job, false);

    // 获取Instance id
    if (detail->is_successful)
    {
        std::string resp_loc = detail->GetHeader("Location");
        if (resp_loc == "" || util::TrimString(resp_loc) == "")
        {
            util::OdpsThrow(INTERNAL_ERROR, "Server response does not contain redirection header");
        }
        std::size_t found = resp_loc.find_last_of("/");
        this->id = resp_loc.substr(found + 1);
    }
    else
    {
        util::OdpsThrow(INTERNAL_ERROR, "Create instance failed! Status code: " + std::to_string(detail->status_code) + ", reason:\n" + detail->body + "\n");
    }
    return detail->is_successful;
}

void Instance::Stop()
{
    std::string rest_path = RestResourceBuilder::BuildInstanceRest(
        project, this->id);

    // 请求body
    tinyxml2::XMLDocument doc;
    doc.Parse("<?xml version=\"1.0\" ?>");

    tinyxml2::XMLElement *instance_node = doc.NewElement("Instance");
    doc.InsertEndChild(instance_node);

    tinyxml2::XMLElement *status_node = doc.NewElement("Status");
    status_node->InsertNewText("Terminated");
    instance_node->InsertEndChild(status_node);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    std::string job_str(printer.CStr());

    // 设置请求头
    map<std::string, std::string> headers;
    headers[CONTENT_TYPE] = "application/xml";
    headers[CONTENT_LENGTH] = to_string(job_str.length());
    headers[CONTENT_MD5] = util::GenerateMd5Signature(job_str);

    RestResponsePtr detail = mRestClient->DoRequest(rest_path, HTTP_METHOD_PUT, headers, {}, job_str, true);

    if (!util::StartWith(to_string(detail->status_code), "2"))
    {
        const std::string& error_code = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Code")->GetText();
        const std::string& error_message = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Message")->GetText();
        if (!(error_code == "InvalidStateSetting" &&
            error_message.find("from [Terminated] to [Terminated]") != std::string::npos)) // 已经Terminated，视为成功
        {
            util::OdpsThrow(error_code, "Failed to stop instance", id, ":", error_message);
        }
    }
}

std::string Instance::GetTaskDetailJson(const std::string& task_name)
{
    std::string rest_path =
        RestResourceBuilder::BuildInstanceRest(
            project, this->id);
    // 设置请求参数
    std::map<std::string, std::string> params;
    params["instancedetail"] = "";
    params["taskname"] = task_name;

    std::map<std::string, std::string> headers;

    RestResponsePtr detail = mRestClient->DoRequest(rest_path, HTTP_METHOD_GET, headers, params, "", false);

    if (!detail->is_successful)
    {
        util::OdpsThrow(INTERNAL_ERROR, "GetTaskDetailJson failed! Status code: " + std::to_string(detail->status_code) + ", reason:\n" + detail->body + "\n");
    }
    std::string body;
    bool unzip_res = util::GUnZip(detail->body, body);
    if (!unzip_res)
    {
        util::OdpsThrow(INTERNAL_ERROR, "Server returned garbage", "Instance ID:", this->id);
    }
    return body;
}

void Instance::WaitForSuccess(uint32_t sleepTimeoutMs, uint16_t sleepIntervalMs){
    InstanceStatus status;
    uint32_t sleepTimeMs = 0;

    while((status = this->AcquireStatus(false)) != InstanceStatus::TERMINATED){
        sleepTimeMs += sleepIntervalMs;
        if(sleepTimeMs > sleepTimeoutMs){
            util::OdpsThrow(INTERNAL_ERROR, "timeout exceeded timeout:", sleepIntervalMs, " ms");
        }

        usleep(sleepIntervalMs *1000);
    }
    if(!this->IsSuccessful()){
        auto status = this->AcquireTaskStatus();
        for (auto iter = status.begin(); iter!=status.end(); ++iter) {
            if(iter->second == TaskStatus::FAILED) {
                auto res = this->GetTaskResults();
                util::OdpsThrow(INTERNAL_ERROR, res[iter->first]);
            } else if(iter->second != TaskStatus::SUCCESS) {
                util::OdpsThrow(INTERNAL_ERROR, iter->first, " Status: ", GetTaskStatusName(iter->second));
            }
        }
    }
}

bool Instance::IsSuccessful(){
    bool res = true;
    auto status = this->AcquireTaskStatus();
    for (auto iter = status.begin(); iter!=status.end(); ++iter) {
        if(iter->second != TaskStatus::SUCCESS){
            res = false;
            break;
        }
    }
    return res;
}


InstanceStatus Instance::AcquireStatus(bool isBlock){
    std::string restPath =
        RestResourceBuilder::BuildInstanceRest(
            this->project, this->id);
    // 设置请求参数
    std::map<std::string, std::string> params;
    params["instancestatus"] = "";

    std::map<std::string, std::string> headers;

    RestResponsePtr detail = std::dynamic_pointer_cast<RestResponse>(
        mRestClient->DoRequest(
        restPath, HTTP_METHOD_GET, headers, params, "", true)
    );

    if (!util::StartWith(to_string(detail->status_code), "2"))
    {
        const std::string& error_code = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Code")->GetText();
        const std::string& error_message = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Message")->GetText();

        util::OdpsThrow(error_code, "Failed to acquire status! instance: ", this->id, ":", error_message);
    }

    const std::string& status = detail->xml_body.FirstChildElement("Instance")->FirstChildElement("Status")->GetText();
    return ParseStatus(status);
}

const std::unordered_map<std::string, TaskStatus>& Instance::AcquireTaskStatus(){
    unordered_map<std::string, TaskStatus> tasks_status;
    if(!this->mTaskStatus.empty()){
        return this->mTaskStatus;
    }
    const std::string& restPath =
        RestResourceBuilder::BuildInstanceRest(
            this->project, this->id);
    // 设置请求参数
    std::map<std::string, std::string> params;
    params["taskstatus"] = "";

    std::map<std::string, std::string> headers;

    RestResponsePtr detail = std::dynamic_pointer_cast<RestResponse>(
        mRestClient->DoRequest(
        restPath, HTTP_METHOD_GET, headers, params, "", true)
    );
    // XMLPrinter printer;
    // detail->xml_body.Accept(&printer);
    // std::string results(printer.CStr());
    // std::cout<<"TaskStatus: "<<std::endl;//具体形式打印出来，暂无解析
    // std::cout<<results<<std::endl;

    if (!util::StartWith(to_string(detail->status_code), "2"))
    {
        const std::string& errorCode = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Code")->GetText();
        const std::string& errorMessage = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Message")->GetText();

        util::OdpsThrow(errorCode, "Failed to acquire task status instance: ", this->id, ":", errorMessage);
    }

    tinyxml2::XMLElement *tasks = detail->xml_body.FirstChildElement("Instance")->FirstChildElement("Tasks");

    while(tasks) {
        tinyxml2::XMLElement* taskChild = tasks->FirstChildElement();
        std::string taskName = taskChild->FirstChildElement("Name")->GetText();
        std::string taskStatus  = taskChild->FirstChildElement("Status")->GetText();
        this->mTaskStatus[taskName] = ParseTaskStatus(taskStatus);
        tasks = tasks->NextSiblingElement();
    }
    return this->mTaskStatus;
}

const std::unordered_map<std::string, std::string>& Instance::GetTaskResults()
{
    if(!this->mTaskResults.empty()){
        return this->mTaskResults;
    }

    const std::string& restPath =
        RestResourceBuilder::BuildInstanceRest(
            this->project, this->id);
    // 设置请求参数
    std::map<std::string, std::string> params;
    params["result"] = "";

    std::map<std::string, std::string> headers;

    RestResponsePtr detail = std::dynamic_pointer_cast<RestResponse>(
        mRestClient->DoRequest(
        restPath, HTTP_METHOD_GET, headers, params, "", true)
    );

    // tinyxml2::XMLPrinter printer;
    // detail->xml_body.Accept(&printer);
    // std::string results(printer.CStr());
    // std::cout<<"TaskResults: "<<std::endl;//具体形式打印出来
    // std::cout<<results<<std::endl;

    if (!util::StartWith(to_string(detail->status_code), "2"))
    {
        const std::string& errorCode = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Code")->GetText();
        const std::string& errorMessage = detail->xml_body.FirstChildElement("Error")->FirstChildElement("Message")->GetText();

        util::OdpsThrow(errorCode, "Failed to get task results instance:", this->id, ":", errorMessage);
    }

    tinyxml2::XMLElement *tasks = detail->xml_body.FirstChildElement("Instance")->FirstChildElement("Tasks");

    while(tasks){
        tinyxml2::XMLElement* taskChild = tasks->FirstChildElement();
        tinyxml2::XMLElement* taskResult = taskChild->FirstChildElement("Result");

        const std::string& task_name = taskChild->FirstChildElement("Name")->GetText();
        std::string text = taskResult->GetText();

        const tinyxml2::XMLAttribute * result_attribute = taskResult->FindAttribute("Transform");
        std::string transform("");
        if(result_attribute != nullptr){
            transform = result_attribute->Value();
        }

        if(!transform.empty() && transform == "Base64"){
            text = util::Base64Decode(text);
        }

        this->mTaskResults[task_name] = text;
        tasks = tasks->NextSiblingElement();
    }
    return this->mTaskResults;
}

std::string Instance::GenerateLogViewHost()
{
    if (this->mLogViewHost == "")
    {
        try
        {
            std::string rest_path("logview/host");
            map<std::string, std::string> headers;
            RestResponsePtr resp = mRestClient->DoRequest(rest_path, HTTP_METHOD_GET, headers, {}, "", false);
            std::string lw = resp->body;
            if (lw.find("CURLMSG:CURLMSG_DONE,CURLCode:0,CODEMsg:No error") != std::string::npos)
            {
                this->mLogViewHost = HOST_DEFAULT;
            }
            else
            {
                this->mLogViewHost = lw;
            }
        }
        catch (...)
        {
            this->mLogViewHost = HOST_DEFAULT;
        }
    }
    return this->mLogViewHost;
}

std::string Instance::GeneratePolicy(long hours)
{
    std::string policy;
    policy = policy +
             "{\n" +
             "    \"expires_in_hours\": " + to_string(hours) + ",\n" +
             "    \"policy\": {\n" +
             "        \"Statement\": [{\n" +
             "            \"Action\": [\"odps:Read\"],\n" +
             "            \"Effect\": \"Allow\",\n" +
             "            \"Resource\": \"acs:odps:*:projects/" + project + "/instances/" + this->id + "\"\n" +
             "        }],\n" +
             "        \"Version\": \"1\"\n" +
             "    }\n" +
             "}";
    return policy;
}

std::string Instance::GenerateToken(long hours)
{
    return this->sm->GenerateAuthorizationToken(this->GeneratePolicy(hours), "BEARER");
}

std::string Instance::GenerateLogView(long hours)
{
    std::stringstream logView;
    logView << GenerateLogViewHost() <<
               "/logview/?h=" << mConf.GetEndpoint() <<
               "&p=" << project <<
               "&i=" << this->id <<
               "&token=" << GenerateToken(hours);
    return logView.str();
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
