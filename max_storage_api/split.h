#ifndef APSARA_ODPS_MAX_STORAGE_SPLIT_H
#define APSARA_ODPS_MAX_STORAGE_SPLIT_H

#include "include/max_storage_api.h"
#include "util/utils.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

struct SplitContext
{
    SplitMode mSplitMode = SplitMode::ROW_OFFSET; // Split模式
    int64_t mOffset = 0;     // RowOffset模式下使用
    int64_t mCount = 0;      // RowOffset模式下使用
    int32_t mIndex = 0;      // Size/Bucket/Parallelism模式下使用
    int32_t mBucketId = 0;   // Bucket模式下使用
};

class ISplitVisitor
{
public:
    virtual void Visit(const SplitContext& context) = 0;
};

class Split : public ISplit
{
public:
    Split(const SplitContext& context);
    ~Split() = default;

public:
    void Accept(ISplitVisitor& visitor) override;

private:
    SplitContext mContext;
};

class Splits : public ISplits
{
public:
    Splits(SplitMode mode, int64_t records, const std::vector<SplitContext>& splits);
    ~Splits() = default;

public:
    virtual SplitMode GetSplitMode() const override;
    virtual int64_t GetRecordCount() const override;
    virtual int32_t GetSplitCount() const override;
    virtual std::shared_ptr<ISplit> GetSplit(int32_t index) const override;
    virtual std::shared_ptr<ISplit> GetSplit(int64_t offset, int64_t count) const override;

private:
    SplitMode mSplitMode;
    int64_t mRecordCount;
    std::vector<SplitContext> mSplits;
};

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif
