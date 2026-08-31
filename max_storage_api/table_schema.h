#pragma once

#include "common/odps_column.h"
#include "common/json_serialize.h"

namespace apsara { namespace odps { namespace sdk { namespace max_storage_api {

class TableColumn: public internal::ODPSTableColumn
{
    friend void to_json(nlohmann::json& j, const TableColumn& c);
    friend void from_json(const nlohmann::json& j, TableColumn& c);

    virtual const std::string& GetName() const override
    {
        return mType.mMemberName;
    }

    virtual uint64_t GetColumnId() const override
    {
        return mType.mColumnId;
    }

    virtual IODPSTableColumn& SetName(const std::string& name) override
    {
        mName = name;
        mType.mMemberName = name;
        return *this;
    }

    virtual const std::string& GetDefaultValue() const override
    {
        return mType.mDefaultValue;
    }

    virtual bool HasDefaultValue() const override
    {
        return mType.mHasDefaultValue;
    }

    virtual IODPSTableColumn& SetNoDefaultValue() override
    {
        mHasDefaultValue = false;
        mDefaultValue.clear();
        mType.mHasDefaultValue = false;
        mType.mDefaultValue.clear();
        return *this;
    }

    virtual IODPSTableColumn& SetDefaultValue(const std::string& defaultValue) override
    {
        mHasDefaultValue = true;
        mDefaultValue = defaultValue;
        mType.mDefaultValue = defaultValue;
        mType.mHasDefaultValue = true;
        return *this;
    }

    virtual bool GetNullable() const override
    {
        return mType.mNullable;
    }

    virtual IODPSTableColumn& SetNullable(bool nullable) override
    {
        mType.mNullable = nullable;
        return *this;
    }
};

// storage api 表结构列的序列化格式(与 core/tunnel 格式均不同)
inline void to_json(nlohmann::json& j, const TableColumn& c)
{
    j = nlohmann::json{
        {"columnType", c.mType},
        {"comment", c.mComment},
        {"label", c.mLabel},
        {"extendedLabels", c.mExtendedLabels},
    };
}

inline void from_json(const nlohmann::json& j, TableColumn& c)
{
    c.mType = j.at("columnType").get<ODPSColumnTypeInfo>();
    c.mComment = j.value("comment", "");
    c.mLabel = j.value("label", "");
    c.mExtendedLabels = j.value("extendedLabels", std::vector<std::string>{});
}

using TableColumns = std::vector<TableColumn>;

struct TableSchema: IODPSTableSchema
{
    IODPSTableSchema* Clone() const override
    {
        return new TableSchema(*this);
    }

    uint32_t GetColumnCount() const override
    {
        return mDataColumns.size();
    }

    const IODPSTableColumn& GetTableColumn(uint32_t index) const override
    {
        return mDataColumns.at(index);
    }

    uint32_t GetPartitionLevels() const override
    {
        return mPartitionColumns.size();
    }

    const IODPSTableColumn& GetTablePartition(uint32_t index) const override
    {
        return mPartitionColumns.at(index);
    }

    IODPSTableSchema& AppendColumn(const IODPSTableColumn& column) override
    {
        const TableColumn& tableCol = dynamic_cast<const TableColumn&>(column);
        mDataColumns.push_back(tableCol);
        return *this;
    }

    IODPSTableSchema& AppendPartition(const IODPSTableColumn& partition) override
    {
        const TableColumn& tableCol = dynamic_cast<const TableColumn&>(partition);
        mPartitionColumns.push_back(tableCol);
        return *this;
    }

    std::string ToString() const override
    {
        return util::ToJsonCompactString(*this);
    }

    int64_t GetMaxFieldSize() const override { return mMaxFieldSize; }

    TableColumns mDataColumns;
    TableColumns mPartitionColumns;
    TableColumns mSystemColumns;

    int64_t mMaxFieldSize = 8 * 1024 * 1024; // default is 8M
};

inline void to_json(nlohmann::json& j, const TableSchema& s)
{
    j = nlohmann::json{
        {"DataColumns", s.mDataColumns},
        {"PartitionColumns", s.mPartitionColumns},
        {"SystemColumns", s.mSystemColumns},
        {"MaxFieldSize", s.mMaxFieldSize},
    };
}

inline void from_json(const nlohmann::json& j, TableSchema& s)
{
    j.at("DataColumns").get_to(s.mDataColumns);
    j.at("PartitionColumns").get_to(s.mPartitionColumns);
    j.at("SystemColumns").get_to(s.mSystemColumns);
    s.mMaxFieldSize = j.value("MaxFieldSize", static_cast<int64_t>(8 * 1024 * 1024));
}


}}}}