#ifndef APSARA_ODPS_MAX_STORAGE_API_TABLE_READ_SESSION_H
#define APSARA_ODPS_MAX_STORAGE_API_TABLE_READ_SESSION_H

#include "common/odps_table_schema.h"
#include "include/max_storage_api.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/split.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include "max_storage_api/table_read_stream.h"
#endif
#include "max_storage_api/table_schema.h"

namespace apsara
{
namespace odps
{
namespace sdk
{
namespace max_storage_api
{

// 实现表读取会话构建器
IMPLEMENT_BUILDER(TableReadSessionBuilderImpl, ITableReadSessionBuilder, ITableReadSessionPtr,
    // 定义构建器参数
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, Table, std::string);
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, Schema, std::string);
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, SessionId, std::string);

    // 结构化选项
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, SplitOptions, SplitOptions);
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, ArrowOptions, ArrowOptions);
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, IncrementalReadOptions, IncrementalReadOptions);
    DEFINE_BUILDER_PARAM(ITableReadSessionBuilder, FilterOptions, FilterOptions);
);

/**
 * @brief 表读取会话实现类
 */
class TableReadSessionImpl : public ITableReadSession
{
public:
    /**
     * @brief 构造函数
     * @param builder 构建器对象
     */
    TableReadSessionImpl(const TableReadSessionBuilderImpl& builder);
    ~TableReadSessionImpl() = default;

public:
    /**
     * @brief 构建表读取流
     * @return 表读取流构建器指针
     */
    ITableReadStreamBuilderPtr BuildTableReadStream() override;

    /**
     * @brief 获取会话ID
     * @return 会话ID字符串
     */
    std::string GetSessionId() override;

    /**
     * @brief 获取分片信息
     * @return 分片信息指针
     */
    ISplitsPtr GetSplits() override;

    /**
     * @brief 获取增量信息
     * @return 增量信息
     */
    IncrementalInfo GetIncrementalInfo() override;

private:
    /**
     * @brief 创建读取会话
     */
    void CreateReadSession();

    /**
     * @brief 获取读取会话
     */
    void GetReadSession();

    void LoadSession(const std::string& json);

private:
    Configuration mConf;                    // 配置信息
    std::string mProject;                   // 项目名称
    std::string mTable;                     // 表名
    std::string mSchemaName;                // 模式名称
    std::string mSessionId;                 // 会话ID
    TableSchema mTableSchema;               // Schema
    SplitOptions mSplitOptions;             // 分片选项
    ArrowOptions mArrowOptions;             // Arrow选项
    FilterOptions mFilterOptions;           // 过滤选项
    std::shared_ptr<Splits> mSplits;        // 分片信息
    IncrementalInfo mIncrementalInfo;       // 增量信息
    std::string mStatus;
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif