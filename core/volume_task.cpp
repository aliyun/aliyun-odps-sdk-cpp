#include "volume_task.h"
#include "util/string_util.h"
#include "rest_path.h"
#include "rest_client.h"
#include "rest_response.h"
#include "util/utils.h"
#include <sstream>

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;

// VolumeModel XML 序列化实现
std::string VolumeModel::ToXml() const {
    tinyxml2::XMLDocument doc;
    const char *declaration = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
    doc.Parse(declaration);

    tinyxml2::XMLElement *volume = doc.NewElement("Volume");
    doc.InsertEndChild(volume);

    if (!mName.empty()) {
        tinyxml2::XMLElement *nameElem = doc.NewElement("Name");
        nameElem->InsertNewText(mName.c_str());
        volume->InsertEndChild(nameElem);
    }

    if (!mComment.empty()) {
        tinyxml2::XMLElement *commentElem = doc.NewElement("Comment");
        commentElem->InsertNewText(mComment.c_str());
        volume->InsertEndChild(commentElem);
    }

    if (!mType.empty()) {
        tinyxml2::XMLElement *typeElem = doc.NewElement("Type");
        typeElem->InsertNewText(mType.c_str());
        volume->InsertEndChild(typeElem);
    }

    if (mLifecycle > 0) {
        tinyxml2::XMLElement *lifecycleElem = doc.NewElement("Lifecycle");
        lifecycleElem->InsertNewText(std::to_string(mLifecycle).c_str());
        volume->InsertEndChild(lifecycleElem);
    }

    if (!mProperties.empty()) {
        tinyxml2::XMLElement *propertiesElem = doc.NewElement("Properties");
        for (const auto& prop : mProperties) {
            tinyxml2::XMLElement *propElem = doc.NewElement("Property");
            tinyxml2::XMLElement *keyElem = doc.NewElement("Name");
            keyElem->InsertNewText(prop.first.c_str());
            tinyxml2::XMLElement *valueElem = doc.NewElement("Value");
            valueElem->InsertNewText(prop.second.c_str());
            propElem->InsertEndChild(keyElem);
            propElem->InsertEndChild(valueElem);
            propertiesElem->InsertEndChild(propElem);
        }
        volume->InsertEndChild(propertiesElem);
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

VolumeModel VolumeModel::FromXml(const std::string& xml) {
    VolumeModel model;
    tinyxml2::XMLDocument doc;

    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {
        throw OdpsException("XML_PARSE_ERROR", "Failed to parse XML");
    }

    tinyxml2::XMLElement *volume = doc.FirstChildElement("Volume");
    if (!volume) {
        throw OdpsException("XML_PARSE_ERROR", "Volume element not found");
    }

    tinyxml2::XMLElement *nameElem = volume->FirstChildElement("Name");
    if (nameElem && nameElem->GetText()) {
        model.mName = nameElem->GetText();
    }

    tinyxml2::XMLElement *commentElem = volume->FirstChildElement("Comment");
    if (commentElem && commentElem->GetText()) {
        model.mComment = commentElem->GetText();
    }

    tinyxml2::XMLElement *typeElem = volume->FirstChildElement("Type");
    if (typeElem && typeElem->GetText()) {
        model.mType = typeElem->GetText();
    }

    tinyxml2::XMLElement *lifecycleElem = volume->FirstChildElement("Lifecycle");
    if (lifecycleElem && lifecycleElem->GetText()) {
        model.mLifecycle = std::stoll(lifecycleElem->GetText());
    }

    tinyxml2::XMLElement *propertiesElem = volume->FirstChildElement("Properties");
    if (propertiesElem) {
        tinyxml2::XMLElement *propElem = propertiesElem->FirstChildElement("Property");
        while (propElem) {
            tinyxml2::XMLElement *keyElem = propElem->FirstChildElement("Name");
            tinyxml2::XMLElement *valueElem = propElem->FirstChildElement("Value");
            if (keyElem && keyElem->GetText() && valueElem && valueElem->GetText()) {
                model.mProperties[keyElem->GetText()] = valueElem->GetText();
            }
            propElem = propElem->NextSiblingElement("Property");
        }
    }

    return model;
}

// Volume 类实现
Volume::Volume(const VolumeModel& model, const std::string& projectName)
    : mModel(model), mProjectName(projectName) {}

Volume::~Volume() {}

// VolumeBuilder 实现
VolumeBuilder::VolumeBuilder()
    : mType(VolumeType::NEW), mAutoMkdir(false), mAccelerate(false) {}

VolumeBuilder& VolumeBuilder::SetProject(const std::string& projectName) {
    mProjectName = projectName;
    return *this;
}

VolumeBuilder& VolumeBuilder::SetVolumeName(const std::string& volumeName) {
    mModel.mName = volumeName;
    return *this;
}

VolumeBuilder& VolumeBuilder::SetType(VolumeType type) {
    mType = type;
    switch (type) {
        case VolumeType::OLD:
            mModel.mType = "OLD";
            break;
        case VolumeType::NEW:
            mModel.mType = "NEW";
            break;
        case VolumeType::EXTERNAL:
            mModel.mType = "EXTERNAL";
            break;
    }
    return *this;
}

VolumeBuilder& VolumeBuilder::SetComment(const std::string& comment) {
    mModel.mComment = comment;
    return *this;
}

VolumeBuilder& VolumeBuilder::SetLifecycle(int64_t lifecycle) {
    mModel.mLifecycle = lifecycle;
    return *this;
}

VolumeBuilder& VolumeBuilder::SetExtLocation(const std::string& location) {
    mExtLocation = location;
    mModel.mProperties["odps.volume.external.location"] = location;
    return *this;
}

VolumeBuilder& VolumeBuilder::SetProperties(const std::map<std::string, std::string>& props) {
    mModel.mProperties.insert(props.begin(), props.end());
    return *this;
}

VolumeBuilder& VolumeBuilder::SetAutoMkDir(bool autoMkdir) {
    mAutoMkdir = autoMkdir;
    mModel.mProperties["odps.volume.auto.mkdir"] = autoMkdir ? "true" : "false";
    return *this;
}

VolumeBuilder& VolumeBuilder::SetAccelerate(bool accelerate) {
    mAccelerate = accelerate;
    mModel.mProperties["odps.volume.accelerate"] = accelerate ? "true" : "false";
    return *this;
}

VolumeBuilder& VolumeBuilder::SetProperty(const std::string& key, const std::string& value) {
    mModel.mProperties[key] = value;
    return *this;
}

VolumeManager::VolumeManager(const std::map<std::string, std::string>& props)
    : mProperties(props), mRestClient(std::make_shared<RestClient>(Configuration())), mConfig(Configuration()) {}

VolumeManager::~VolumeManager() {}

void VolumeManager::SetProject(const std::string& project) {
    this->mProject = project;
}

const std::string& VolumeManager::GetProject() const {
    return this->mProject;
}

void VolumeManager::SetProperty(const std::string& key, const std::string& value) {
    this->mProperties[key] = value;
}

std::string VolumeManager::GetProperty(const std::string& key) const {
    auto it = this->mProperties.find(key);
    if (it != this->mProperties.end()) {
        return it->second;
    }
    return "";
}

void VolumeManager::SetConfiguration(const Configuration& config) {
    this->mConfig = config;
    this->mRestClient = std::make_shared<RestClient>(config);
}

const Configuration& VolumeManager::GetConfiguration() const {
    return this->mConfig;
}

VolumePtr VolumeManager::CreateVolume(const VolumeBuilder& builder) {
    VolumeModel model = builder.GetModel();
    std::string projectName = builder.GetProjectName().empty() ? this->mProject : builder.GetProjectName();

    if (model.mName.empty()) {
        throw OdpsException("INVALID_VOLUME_NAME", "Volume name cannot be empty");
    }

    SendCreateRequest(model, projectName);
    return std::make_shared<Volume>(model, projectName);
}

VolumePtr VolumeManager::CreateVolume(const std::string& volumeName, const std::string& comment) {
    return CreateVolume(volumeName, comment, VolumeType::OLD);
}

VolumePtr VolumeManager::CreateVolume(const std::string& volumeName, const std::string& comment, VolumeType type) {
    return CreateVolume(volumeName, comment, type, 0);
}

VolumePtr VolumeManager::CreateVolume(const std::string& volumeName, const std::string& comment, VolumeType type, int64_t lifecycle) {
    VolumeBuilder builder;
    builder.SetVolumeName(volumeName)
           .SetComment(comment)
           .SetType(type)
           .SetLifecycle(lifecycle);

    return CreateVolume(builder);
}



void VolumeManager::DeleteVolume(const std::string& volumeName) {
    SendDeleteRequest(this->mProject, volumeName);
}

std::string VolumeManager::BuildVolumeResource(const std::string& projectName, const std::string& volumeName) const {
    return "projects/" + projectName + "/volumes/" + volumeName;
}

std::string VolumeManager::BuildVolumesResource(const std::string& projectName) const {
    return "projects/" + projectName + "/volumes";
}

void VolumeManager::SendCreateRequest(const VolumeModel& model, const std::string& projectName) {
    std::string resource = BuildVolumesResource(projectName);
    std::string xmlContent = model.ToXml();

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/xml";

    RestResponsePtr response = std::dynamic_pointer_cast<RestResponse>(
        mRestClient->DoRequest(resource, "POST", headers, std::map<std::string, std::string>(), xmlContent, true)
    );

    if (!util::StartWith(std::to_string(response->status_code), "2")) {
        const std::string& errorCode = response->xml_body.FirstChildElement("Error")->FirstChildElement("Code")->GetText();
        const std::string& errorMessage = response->xml_body.FirstChildElement("Error")->FirstChildElement("Message")->GetText();
        util::OdpsThrow(errorCode, "Failed to create volume: ", errorMessage);
    }
}



void VolumeManager::SendDeleteRequest(const std::string& projectName, const std::string& volumeName) {
    std::string resource = BuildVolumeResource(projectName, volumeName);

    std::map<std::string, std::string> headers;

    RestResponsePtr response = std::dynamic_pointer_cast<RestResponse>(
        mRestClient->DoRequest(resource, "DELETE", headers, std::map<std::string, std::string>(), "", true)
    );

    if (!util::StartWith(std::to_string(response->status_code), "2")) {
        const std::string& errorCode = response->xml_body.FirstChildElement("Error")->FirstChildElement("Code")->GetText();
        const std::string& errorMessage = response->xml_body.FirstChildElement("Error")->FirstChildElement("Message")->GetText();
        util::OdpsThrow(errorCode, "Failed to delete volume: ", errorMessage);
    }
}


