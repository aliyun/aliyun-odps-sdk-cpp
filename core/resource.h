#ifndef APSARA_ODPS_SDK_RESOURCE_H
#define APSARA_ODPS_SDK_RESOURCE_H

#include "rest_client.h"
#include "rest_path.h"
#include "odps.h"



namespace apsara { namespace odps { namespace sdk { namespace internal {

class Resource : public IODPSResource
{
public:
    Resource(OdpsPtr odps, const std::string& name, const ODPSResourceType& type, std::stringstream &content) : odps(odps), content(content)
    {
        this->project = odps->GetProject();
        this->name = name;
        this->type = type;
    }

    Resource(OdpsPtr odps, const std::string& name, std::stringstream &content) : odps(odps), content(content)
    {
        this->project = odps->GetProject();
        this->name = name;
    }

    virtual void SetProject(const std::string& project) override
    {
        this->project = project;
    }

    virtual const std::string& GetProject() const override
    {
        return this->project;
    }

    virtual void SetName(const std::string& name) override
    {
        this->name = name;
    }

    virtual const std::string& GetName() const override
    {
        return this->name;
    }

    virtual void SetType(const ODPSResourceType& type) override
    {
        this->type = type;
    }

    virtual const ODPSResourceType& GetType() const override
    {
        return this->type;
    }

    virtual void SetIsTempResource(const bool& is_temp) override
    {
        this->is_temp_resource = is_temp;
    }

    virtual bool IsTempResource() override
    {
        return this->is_temp_resource;
    }

    virtual void SetComment(const std::string& comment) override
    {
        this->comment = comment;
    }

    virtual const std::string& GetComment() const override
    {
        return this->comment;
    }

    /**
     *
     * @brief 判断资源是否存在
     *
     * @return 资源存在与否
     */
    virtual bool Exists() override;

    /**
     *
     * @brief 创建资源
     *
     * @param overwrite 是否更新
     * @return 创建资源是否成功
     */
    virtual bool Create(bool overwrite) override;

    /**
     *
     * @brief 删除资源
     *
     * @return 删除资源是否成功
     */
    virtual bool Delete() override;

private:
    OdpsPtr odps;
    std::string project;
    std::string name;
    std::stringstream &content;
    ODPSResourceType type = FILE;
    bool is_temp_resource = false;
    std::string comment = "";
};

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif