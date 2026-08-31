#include "sql_task.h"
#include "rest_path.h"
#include "tinyxml2.h"
#include "util/utils.h"

namespace apsara { namespace odps { namespace sdk {

std::shared_ptr<ISQLTask> ISQLTask::Create(const std::map<std::string,std::string>& props)
{
     return std::make_shared<internal::SQLTask>(props);
}

namespace internal {

SQLTask::SQLTask(const map<string, string>& props) : properties(props){}

SQLTask::~SQLTask(){}

void SQLTask::SetName(const std::string& name)
{
    this->name = name;
}

const std::string& SQLTask::GetName() const
{
    return this->name;
}

void SQLTask::SetQuery(const std::string& query)
{
    this->query = query;
}

const std::string& SQLTask::GetQuery() const
{
    return this->query;
}

void SQLTask::SetProperty(const string& key, const string& value)
{
    this->properties[key] = value;
}

std::string SQLTask::GetProperty(const string& key) const
{
    auto it = this->properties.find(key);
    if (it != this->properties.end())
    {
        return it->second;
    }
    return "";
}


void SQLTask::SetDefaultHints(const std::map<std::string, std::string>& hints)
{
    this->defaultHints = hints;
}

void SQLTask::RemoveDefaultHints()
{
    this->defaultHints.clear();
}


IODPSInstancePtr SQLTask::Run(IODPSPtr odps, const std::string& sql)
{
    map<std::string, std::string> hints;
    map<std::string, std::string> aliases;
    return SQLTask::Run(odps, odps->GetProject(), sql, "AnonymousSQLTask", hints, aliases, 3, "sql");
}

IODPSInstancePtr SQLTask::Run(IODPSPtr odps, const std::string &sql, const map<string, string>& hints)
{
    map<string, string> aliases;
    return SQLTask::Run(odps, odps->GetProject(), sql, "AnonymousSQLTask", hints, aliases, 3, "sql");
}

IODPSInstancePtr SQLTask::Run(IODPSPtr odps, const std::string& project, const std::string& sql, const std::string& taskName,
    map<std::string, std::string> hints, const map<std::string, std::string>& aliases,
    int priority, const std::string& type)
{
    map<std::string, std::string> props;
    std::shared_ptr<SQLTask> taskPtr = std::make_shared<SQLTask>(props);
    taskPtr->SetName(taskName);
    taskPtr->SetQuery(sql);
    taskPtr->SetProperty("type", "sql");

    if(hints.empty() && !this->defaultHints.empty())
    {
        hints = this->defaultHints;
    }

    if(!hints.empty())
    {
        std::string settingsJsonStr = util::ToJsonCompactString(hints);
        taskPtr->SetProperty("settings", settingsJsonStr);
    }

    std::string aliasesJsonStr = util::ToJsonCompactString(aliases);
    taskPtr->SetProperty("aliases", aliasesJsonStr);

    tinyxml2::XMLDocument doc;
    const char *declaration = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
    doc.Parse(declaration);

    tinyxml2::XMLElement *instance = doc.NewElement("Instance");
    doc.InsertEndChild(instance);

    tinyxml2::XMLElement *job = doc.NewElement("Job");
    instance->InsertEndChild(job);

    if (priority >= 0)
    {
        tinyxml2::XMLElement *priorityNode = doc.NewElement("Priority");
        priorityNode->InsertNewText(std::to_string(priority).c_str());
        job->InsertEndChild(priorityNode);
    }

    tinyxml2::XMLElement *tasks = doc.NewElement("Tasks");
    job->InsertEndChild(tasks);

    tinyxml2::XMLElement *sqlTask = doc.NewElement("SQL");
    tasks->InsertEndChild(sqlTask);

    tinyxml2::XMLElement *name = doc.NewElement("Name");
    name->InsertNewText(taskPtr->GetName().c_str());
    sqlTask->InsertEndChild(name);

    tinyxml2::XMLElement *config = doc.NewElement("Config");
    tinyxml2::XMLElement *prop1 = doc.NewElement("Property");
    tinyxml2::XMLElement *prop1Name = doc.NewElement("Name");
    prop1Name->InsertNewText("type");
    tinyxml2::XMLElement *prop1Value = doc.NewElement("Value");
    prop1Value->InsertNewText(taskPtr->GetProperty("type").c_str());
    prop1->InsertEndChild(prop1Name);
    prop1->InsertEndChild(prop1Value);
    config->InsertEndChild(prop1);

    if(!taskPtr->GetProperty("settings").empty()){
        tinyxml2::XMLElement *prop2 = doc.NewElement("Property");
        tinyxml2::XMLElement *prop2Name = doc.NewElement("Name");
        prop2Name->InsertNewText("settings");
        tinyxml2::XMLElement *prop2Value = doc.NewElement("Value");
        prop2Value->InsertNewText(taskPtr->GetProperty("settings").c_str());
        prop2->InsertEndChild(prop2Name);
        prop2->InsertEndChild(prop2Value);
        config->InsertEndChild(prop2);
    }

    tinyxml2::XMLElement *prop3 = doc.NewElement("Property");
    tinyxml2::XMLElement *prop3Name = doc.NewElement("Name");
    prop3Name->InsertNewText("uuid");
    tinyxml2::XMLElement *prop3Value = doc.NewElement("Value");
    prop3Value->InsertNewText(util::GenerateUUID().c_str());
    prop3->InsertEndChild(prop3Name);
    prop3->InsertEndChild(prop3Value);
    config->InsertEndChild(prop3);

    sqlTask->InsertEndChild(config);

    tinyxml2::XMLElement *query = doc.NewElement("Query");
    query->InsertNewText(taskPtr->GetQuery().c_str());
    sqlTask->InsertEndChild(query);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    std::string jobStr(printer.CStr());

    // std::cout<<"sqltask: "<<std::endl;
    // std::cout<<jobStr<<std::endl;

    IODPSInstancePtr instancePtr = odps->CreateInstance(jobStr, odps->GetProject());

    bool isCreateInstanceSuccessful = instancePtr->Create();
    if (!isCreateInstanceSuccessful)
    {
        util::OdpsThrow(INTERNAL_ERROR, "create instance failed!");
    }

    return instancePtr;
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara
