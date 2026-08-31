#ifndef APSARA_ODPS_SDK_JSON_SERIALIZE_H
#define APSARA_ODPS_SDK_JSON_SERIALIZE_H

/** @file json_serialize.h
 * 公开类型的 nlohmann::json 序列化接入(原 INJECT_JSONIZABLE 宏注入的替代品)。
 * 这些类型定义在 apsara::odps::sdk 命名空间内,因此把 to_json/from_json
 * 自由函数定义在同一命名空间即可被 ADL 找到。
 *
 * 字段缺省语义沿用原实现:
 *  - 两参形式(无缺省值)对应 j.at(key),缺失时抛异常;
 *  - 三参形式(有缺省值)对应 j.value(key, def)。
 */

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "include/odps_table.h"
#include "include/odps_tunnel.h"

namespace apsara
{
namespace odps
{
namespace sdk
{

inline void to_json(nlohmann::json& j, const SortColumn& c)
{
    j = nlohmann::json{{"col", c.mColumn}, {"order", c.mOrder}};
}

inline void from_json(const nlohmann::json& j, SortColumn& c)
{
    c.mColumn = j.value("col", "");
    c.mOrder = j.value("order", "");
}

inline void to_json(nlohmann::json& j, const PartitionExtendedReservedInfo& r)
{
    j = nlohmann::json{
        {"ClusterType", r.mClusterType},
        {"BucketNum", r.mBucketNum},
        {"ClusterCols", r.mClusterCols},
        {"SortCols", r.mSortCols},
    };
}

inline void from_json(const nlohmann::json& j, PartitionExtendedReservedInfo& r)
{
    r.mClusterType = j.value("ClusterType", "");
    r.mBucketNum = j.value("BucketNum", static_cast<int64_t>(-1));
    r.mClusterCols = j.value("ClusterCols", std::vector<std::string>{});
    if (j.contains("SortCols"))
    {
        j.at("SortCols").get_to(r.mSortCols);
    }
    else
    {
        r.mSortCols.clear();
    }
}

inline void to_json(nlohmann::json& j, const TableExtendedReservedInfo& r)
{
    // BOOL_AS_STRING:服务端以字符串 "true"/"false" 传输布尔值
    j = nlohmann::json{
        {"HasRowAccessPolicy", r.mHasRowAccessPolicy ? "true" : "false"},
        {"Transactional", r.mIsTransactional ? "true" : "false"},
        {"ClusterType", r.mClusterType},
        {"BucketNum", r.mBucketNum},
        {"ClusterCols", r.mClusterCols},
        {"SortCols", r.mSortCols},
        {"PrimaryKey", r.mPrimaryKey},
    };
}

inline void from_json(const nlohmann::json& j, TableExtendedReservedInfo& r)
{
    r.mHasRowAccessPolicy = (j.value("HasRowAccessPolicy", std::string("false")) == "true");
    r.mIsTransactional = (j.value("Transactional", std::string("false")) == "true");
    r.mClusterType = j.value("ClusterType", "");
    r.mBucketNum = j.value("BucketNum", static_cast<int64_t>(-1));
    r.mClusterCols = j.value("ClusterCols", std::vector<std::string>{});
    if (j.contains("SortCols"))
    {
        j.at("SortCols").get_to(r.mSortCols);
    }
    else
    {
        r.mSortCols.clear();
    }
    r.mPrimaryKey = j.value("PrimaryKey", std::vector<std::string>{});
}

inline void to_json(nlohmann::json& j, const ODPSPartitionExtendedInfo& info)
{
    j = nlohmann::json{
        {"IsArchived", info.mIsArchived},
        {"IsExstore", info.mIsExstore},
        {"LifeCycle", info.mLifeCycle},
        {"PhysicalSize", info.mPhysicalSize},
        {"FileNum", info.mFileNum},
        // Reserved 字段本身是一个内嵌的 JSON 字符串
        {"Reserved", nlohmann::json(info.mReserved).dump()},
    };
}

inline void from_json(const nlohmann::json& j, ODPSPartitionExtendedInfo& info)
{
    info.mIsArchived = j.value("IsArchived", false);
    info.mIsExstore = j.value("IsExstore", false);
    info.mLifeCycle = j.value("LifeCycle", -1);
    info.mPhysicalSize = j.value("PhysicalSize", static_cast<int64_t>(-1));
    info.mFileNum = j.value("FileNum", static_cast<int64_t>(-1));
    std::string reserved = j.value("Reserved", std::string("{}"));
    nlohmann::json::parse(reserved).get_to(info.mReserved);
}

inline void to_json(nlohmann::json& j, const ODPSTableExtendedInfo& info)
{
    j = nlohmann::json{
        {"IsArchived", info.mIsArchived},
        {"PhysicalSize", info.mPhysicalSize},
        {"FileNum", info.mFileNum},
        {"Reserved", nlohmann::json(info.mReserved).dump()},
    };
}

inline void from_json(const nlohmann::json& j, ODPSTableExtendedInfo& info)
{
    info.mIsArchived = j.value("IsArchived", false);
    info.mPhysicalSize = j.value("PhysicalSize", static_cast<int64_t>(-1));
    info.mFileNum = j.value("FileNum", static_cast<int64_t>(-1));
    std::string reserved = j.value("Reserved", std::string("{}"));
    nlohmann::json::parse(reserved).get_to(info.mReserved);
}

inline void to_json(nlohmann::json& j, const ODPSColumnTypeInfo& t)
{
    j = nlohmann::json{
        {"Type", static_cast<int>(t.mType)},
        {"Precision", t.mPrecision},
        {"Scale", t.mScale},
        {"SpecifiedLength", t.mSpecifiedLength},
        {"MemberName", t.mMemberName},
        {"SubTypes", t.mSubTypes},
        {"Nullable", t.mNullable},
        {"ColumnId", t.mColumnId},
        {"DefaultValue", t.mDefaultValue},
        {"HasDefaultValue", t.mHasDefaultValue},
    };
}

inline void from_json(const nlohmann::json& j, ODPSColumnTypeInfo& t)
{
    t.mType = static_cast<ODPSColumnType>(j.at("Type").get<int>());
    t.mPrecision = j.value("Precision", 0);
    t.mScale = j.value("Scale", 0);
    t.mSpecifiedLength = j.value("SpecifiedLength", 0);
    t.mMemberName = j.value("MemberName", "");
    if (j.contains("SubTypes"))
    {
        j.at("SubTypes").get_to(t.mSubTypes);
    }
    else
    {
        t.mSubTypes.clear();
    }
    t.mNullable = j.value("Nullable", false);
    t.mColumnId = j.value("ColumnId", static_cast<uint64_t>(0));
    t.mDefaultValue = j.value("DefaultValue", "");
    t.mHasDefaultValue = j.value("HasDefaultValue", false);
}

#ifdef ODPS_SDK_ENABLE_ARROW
inline void to_json(nlohmann::json& j, const EOSInfo& info)
{
    j = nlohmann::json{
        {"sequence_id", info.mSequenceId},
        {"sequence_off", info.mSequenceOffset},
    };
}

inline void from_json(const nlohmann::json& j, EOSInfo& info)
{
    info.mSequenceId = j.at("sequence_id").get<int64_t>();
    info.mSequenceOffset = j.at("sequence_off").get<int64_t>();
}
#endif

}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
