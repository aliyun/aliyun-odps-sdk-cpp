#include <memory>
#include "max_storage_api/split.h"
#include "util/utils.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

Split::Split(const SplitContext& context)
    : mContext(context)
{
}

void Split::Accept(ISplitVisitor& visitor)
{
    visitor.Visit(mContext);
}

Splits::Splits(SplitMode mode, int64_t records, const std::vector<SplitContext>& splits)
    : mSplitMode(mode)
    , mRecordCount(records)
    , mSplits(splits)
{
}

SplitMode Splits::GetSplitMode() const
{
    return mSplitMode;
}

int64_t Splits::GetRecordCount() const
{
    if (mSplitMode != SplitMode::ROW_OFFSET)
    {
        util::TunnelThrow("only ROW_OFFSET mode support get record count");
    }
    return mRecordCount;
}

int32_t Splits::GetSplitCount() const
{
    if (mSplitMode == SplitMode::ROW_OFFSET)
    {
        util::TunnelThrow("ROW_OFFSET mode DO NOT support get split count");
    }
    return mSplits.size();
}

std::shared_ptr<ISplit> Splits::GetSplit(int32_t index) const
{
    if (index < 0 || index >= static_cast<int32_t>(mSplits.size())) {
        util::TunnelThrow("Invalid index");
    }

    if (mSplitMode == SplitMode::ROW_OFFSET)
    {
        util::TunnelThrow("ROW_OFFSET mode DO NOT support get split by index");
    }

    return std::make_shared<Split>(mSplits[index]);
}

std::shared_ptr<ISplit> Splits::GetSplit(int64_t offset, int64_t count) const
{
    if (mSplitMode != SplitMode::ROW_OFFSET)
    {
        util::TunnelThrow("only ROW_OFFSET mode support get split by offset and count");
    }

    SplitContext context;
    context.mSplitMode = mSplitMode;
    context.mOffset = offset;
    context.mCount = count;

    return std::make_shared<Split>(context);
}

}
}
}
}