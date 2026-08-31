#ifndef APSARA_ODPS_SDK_COLUMN_H
#define APSARA_ODPS_SDK_COLUMN_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "odps_table.h"
#include "util/utils.h"
#include "common/json_serialize.h"


namespace apsara { namespace odps { namespace sdk { namespace internal {

class ODPSTableColumn;
class ODPSTunnelTableColumn;

void to_json(nlohmann::json& j, const ODPSTableColumn& c);
void from_json(const nlohmann::json& j, ODPSTableColumn& c);

class ODPSTableColumn: public IODPSTableColumn
{
public:
    ODPSTableColumn() = default;
    ODPSTableColumn(const std::string& name, const std::string& type):
    mName(name), mType(ODPSColumnTypeInfo::ParseTypeInfoString(type))
    {}
    ODPSTableColumn(const std::string& name, ODPSColumnTypeInfo& type):
    mName(name), mType(type)
    {}
    virtual ~ODPSTableColumn() {}
    virtual IODPSTableColumn* Clone() const override
    {
        return new ODPSTableColumn(*this);
    }

    virtual const ODPSColumnTypeInfo& GetTypeInfo() const override
    {
        return mType;
    }

    virtual IODPSTableColumn& SetTypeInfo(const ODPSColumnTypeInfo& typeinfo) override
    {
        mType = typeinfo;
        return *this;
    }

    virtual const std::string& GetName() const override
    {
        return mName;
    }

    virtual uint64_t GetColumnId() const override
    {
        return std::stoul(mColumnId);
    }

    virtual IODPSTableColumn& SetName(const std::string& name) override
    {
        mName = name;
        return *this;
    }

    virtual const std::string& GetDefaultValue() const override
    {
        return mDefaultValue;
    }

    virtual bool HasDefaultValue() const override
    {
        return mHasDefaultValue;
    }

    virtual IODPSTableColumn& SetNoDefaultValue() override
    {
        mHasDefaultValue = false;
        mDefaultValue.clear();
        return *this;
    }

    virtual IODPSTableColumn& SetDefaultValue(const std::string& defaultValue) override
    {
        mHasDefaultValue = true;
        mDefaultValue = defaultValue;
        return *this;
    }

    virtual const std::string& GetComment() const override
    {
        return mComment;
    }

    virtual IODPSTableColumn& SetComment(const std::string& comment) override
    {
        mComment = comment;
        return *this;
    }

    virtual const std::string& GetLabel() const override
    {
        return mLabel;
    }

    virtual IODPSTableColumn& SetLabel(const std::string& comment) override
    {
        mLabel = comment;
        return *this;
    }

    virtual const std::vector<std::string>& GetExtendedLabels() const override
    {
        return mExtendedLabels;
    }

    virtual IODPSTableColumn& SetExtendedLabels(const std::vector<std::string> &extendedLabels) override
    {
        mExtendedLabels = extendedLabels;
        return *this;
    }

    virtual bool GetNullable() const override { return mIsNullable; }
    virtual IODPSTableColumn& SetNullable(bool nullable) override { mIsNullable = nullable; return *this; }

    virtual std::string ToString() const override
    {
        return util::ToJsonCompactString(*this);
    }

    friend void to_json(nlohmann::json& j, const ODPSTableColumn& c);
    friend void from_json(const nlohmann::json& j, ODPSTableColumn& c);
    friend void to_json(nlohmann::json& j, const ODPSTunnelTableColumn& c);
    friend void from_json(const nlohmann::json& j, ODPSTunnelTableColumn& c);

protected:
    std::string mName;
    ODPSColumnTypeInfo mType;
    std::string mComment;
    std::string mDefaultValue;
    std::vector<std::string> mExtendedLabels;
    bool mHasDefaultValue = false;
    bool mIsNullable = true;
    std::string mLabel;
    std::string mColumnId;
};

// to make problem worse, tunnel have different serialization.
// if you are going to use this serialization method, cast the object to this type.
// the "downcast" is safe because there's no extra data members in this class.
class ODPSTunnelTableColumn: public ODPSTableColumn
{
public:
    ODPSTunnelTableColumn() = default;
    ODPSTunnelTableColumn(const std::string& name, const std::string& type) : ODPSTableColumn(name, type)
    {
    }
    ODPSTunnelTableColumn(const std::string& name, ODPSColumnTypeInfo type) : ODPSTableColumn(name, type)
    {
    }
    virtual ~ODPSTunnelTableColumn() {}

    // 原实现中 Jsonize 是虚函数,经基类 ToString() 序列化 tunnel 列对象时
    // 会走到 tunnel 格式;改为自由函数后需显式覆写 ToString() 保持该行为。
    virtual std::string ToString() const override
    {
        return util::ToJsonCompactString(*this);
    }
};

// ODPSTableColumn 的序列化(core 表元数据格式)
inline void to_json(nlohmann::json& j, const ODPSTableColumn& c)
{
    j = nlohmann::json{
        {"name", c.mName},
        {"type", c.mType.ToTypeString()},
        {"comment", c.mComment},
        {"hasDefaultValue", c.mHasDefaultValue},
        {"label", c.mLabel},
        {"isNullable", c.mIsNullable},
        {"extendedLabels", c.mExtendedLabels},
    };
}

inline void from_json(const nlohmann::json& j, ODPSTableColumn& c)
{
    c.mName = j.at("name").get<std::string>();
    c.mType = ODPSColumnTypeInfo::ParseTypeInfoString(j.at("type").get<std::string>());
    c.mComment = j.value("comment", "");
    c.mHasDefaultValue = j.value("hasDefaultValue", false);
    c.mLabel = j.value("label", "");
    c.mIsNullable = j.value("isNullable", true);
    c.mExtendedLabels = j.value("extendedLabels", std::vector<std::string>{});
}

// tunnel 使用不同的序列化格式
inline void to_json(nlohmann::json& j, const ODPSTunnelTableColumn& c)
{
    j = nlohmann::json{
        {"name", c.mName},
        {"type", c.mType.ToTypeString()},
        {"default_value", c.mDefaultValue},
        {"nullable", c.mIsNullable},
        {"column_id", c.mColumnId},
    };
}

inline void from_json(const nlohmann::json& j, ODPSTunnelTableColumn& c)
{
    c.mName = j.at("name").get<std::string>();
    c.mType = ODPSColumnTypeInfo::ParseTypeInfoString(j.at("type").get<std::string>());
    c.mDefaultValue = j.value("default_value", "");
    c.mIsNullable = j.value("nullable", true);
    c.mColumnId = j.value("column_id", "");
    c.mHasDefaultValue = (c.mDefaultValue.size() != 0);
}

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif