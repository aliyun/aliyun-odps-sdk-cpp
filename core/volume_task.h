#ifndef APSARA_ODPS_SDK_ODPS_VOLUME_H
#define APSARA_ODPS_SDK_ODPS_VOLUME_H

#include "odps_api.h"
#include "configuration.h"
#include "tinyxml2.h"
#include "rest_client.h"

namespace apsara { namespace odps { namespace sdk {

namespace internal {
/**
 * @brief Volume 类型枚举
 */
enum class VolumeType {
    OLD,        // 原有Volume
    NEW,        // 新VolumeFS功能的volume
    EXTERNAL    // 外部Volume
};

/**
 * @brief Volume 模型类
 */
class VolumeModel {
public:
    std::string mName;
    std::string mComment;
    std::string mType;
    int64_t mLifecycle;
    std::map<std::string, std::string> mProperties;

    VolumeModel() : mLifecycle(0) {}

    // XML 序列化方法
    std::string ToXml() const;
    static VolumeModel FromXml(const std::string& xml);
};

/**
 * @brief Volume 类
 */
class Volume {
public:
    Volume(const VolumeModel& model, const std::string& projectName);
    ~Volume();

private:
    VolumeModel mModel;
    std::string mProjectName;
};

typedef std::shared_ptr<Volume> VolumePtr;

/**
 * @brief Volume 构建器类
 */
class VolumeBuilder {
public:
    VolumeBuilder();

    VolumeBuilder& SetProject(const std::string& projectName);
    VolumeBuilder& SetVolumeName(const std::string& volumeName);
    VolumeBuilder& SetType(VolumeType type);
    VolumeBuilder& SetComment(const std::string& comment);
    VolumeBuilder& SetLifecycle(int64_t lifecycle);
    VolumeBuilder& SetExtLocation(const std::string& location);
    VolumeBuilder& SetProperties(const std::map<std::string, std::string>& props);
    VolumeBuilder& SetAutoMkDir(bool autoMkdir);
    VolumeBuilder& SetAccelerate(bool accelerate);
    VolumeBuilder& SetProperty(const std::string& key, const std::string& value);

    VolumeModel GetModel() const { return mModel; }
    std::string GetProjectName() const { return mProjectName; }

private:
    VolumeModel mModel;
    std::string mProjectName;
    VolumeType mType;
    std::string mExtLocation;
    bool mAutoMkdir;
    bool mAccelerate;
};

/**
 * @brief Volume 管理接口
 */
class IVolumeManager {
public:
    virtual ~IVolumeManager() {}

    virtual void SetProject(const std::string& project) = 0;
    virtual const std::string& GetProject() const = 0;

    virtual void SetProperty(const std::string& key, const std::string& value) = 0;
    virtual std::string GetProperty(const std::string& key) const = 0;

    virtual void SetConfiguration(const Configuration& config) = 0;
    virtual const Configuration& GetConfiguration() const = 0;

    virtual VolumePtr CreateVolume(const VolumeBuilder& builder) = 0;
    virtual VolumePtr CreateVolume(const std::string& volumeName, const std::string& comment) = 0;
    virtual VolumePtr CreateVolume(const std::string& volumeName, const std::string& comment, VolumeType type) = 0;
    virtual VolumePtr CreateVolume(const std::string& volumeName, const std::string& comment, VolumeType type, int64_t lifecycle) = 0;

    virtual void DeleteVolume(const std::string& volumeName) = 0;

    static std::shared_ptr<IVolumeManager> Create(const std::map<std::string, std::string>& props = std::map<std::string, std::string>());
};

typedef std::shared_ptr<IVolumeManager> IVolumeManagerPtr;

class VolumeManager : public IVolumeManager {
public:
    VolumeManager(const std::map<std::string, std::string>& props);
    virtual ~VolumeManager();

    virtual void SetProject(const std::string& project) override;
    virtual const std::string& GetProject() const override;

    virtual void SetProperty(const std::string& key, const std::string& value) override;
    virtual std::string GetProperty(const std::string& key) const override;

    virtual void SetConfiguration(const Configuration& config) override;
    virtual const Configuration& GetConfiguration() const override;

    virtual VolumePtr CreateVolume(const VolumeBuilder& builder) override;
    virtual VolumePtr CreateVolume(const std::string& volumeName, const std::string& comment) override;
    virtual VolumePtr CreateVolume(const std::string& volumeName, const std::string& comment, VolumeType type) override;
    virtual VolumePtr CreateVolume(const std::string& volumeName, const std::string& comment, VolumeType type, int64_t lifecycle) override;

    virtual void DeleteVolume(const std::string& volumeName) override;

private:
    std::string mProject;
    std::map<std::string, std::string> mProperties;
    std::shared_ptr<RestClient> mRestClient;
    Configuration mConfig;

    std::string BuildVolumeResource(const std::string& projectName, const std::string& volumeName) const;
    std::string BuildVolumesResource(const std::string& projectName) const;
    void SendCreateRequest(const VolumeModel& model, const std::string& projectName);
    void SendDeleteRequest(const std::string& projectName, const std::string& volumeName);
};

typedef std::shared_ptr<VolumeManager> VolumeManagerPtr;

inline std::shared_ptr<IVolumeManager> IVolumeManager::Create(const std::map<std::string, std::string>& props) {
    return std::make_shared<VolumeManager>(props);
}

} // namespace internal
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif // APSARA_ODPS_SDK_ODPS_VOLUME_H