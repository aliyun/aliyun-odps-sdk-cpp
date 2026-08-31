#ifndef APSARA_ODPS_MAX_STORAGE_API_INSTANCE_READ_STREAM_H
#define APSARA_ODPS_MAX_STORAGE_API_INSTANCE_READ_STREAM_H
#ifdef ODPS_SDK_ENABLE_ARROW

#include "include/max_storage_api.h"
#include "max_storage_api/arrow_read_stream.h"
#include "max_storage_api/common_macro_define.h"
#include "max_storage_api/split.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

IMPLEMENT_BUILDER(InstanceReadStreamBuilderImpl, IInstanceReadStreamBuilder, IArrowReadStreamPtr,
    DEFINE_BUILDER_PARAM(InstanceReadStreamBuilderImpl, Project, std::string);
    DEFINE_BUILDER_PARAM(InstanceReadStreamBuilderImpl, Instance, std::string);
    DEFINE_BUILDER_PARAM(InstanceReadStreamBuilderImpl, SessionId, std::string);
    DEFINE_BUILDER_PARAM(IInstanceReadStreamBuilder, Split, ISplitPtr);
    DEFINE_BUILDER_PARAM(IInstanceReadStreamBuilder, Columns, std::vector<std::string>);
);

IMPLEMENT_BUILDER(InstanceDirectReadStreamBuilderImpl, IInstanceDirectReadStreamBuilder, IArrowReadStreamPtr,
    DEFINE_BUILDER_PARAM(IInstanceDirectReadStreamBuilder, Project, std::string);
    DEFINE_BUILDER_PARAM(IInstanceDirectReadStreamBuilder, Instance, std::string);
    DEFINE_BUILDER_PARAM(IInstanceDirectReadStreamBuilder, TaskName, std::string);
    DEFINE_BUILDER_PARAM(IInstanceDirectReadStreamBuilder, QueryId, int64_t);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IInstanceDirectReadStreamBuilder, Offset, int64_t, 0);
    DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(IInstanceDirectReadStreamBuilder, Count, int64_t, 0);
    DEFINE_BUILDER_PARAM(IInstanceDirectReadStreamBuilder, EnableLimit, bool);
);

class InstanceReadStreamImpl : public ArrowReadStreamImpl, public ISplitVisitor
{
public:
    InstanceReadStreamImpl(const InstanceReadStreamBuilderImpl& builder);
    InstanceReadStreamImpl(const InstanceDirectReadStreamBuilderImpl& builder);
    virtual ~InstanceReadStreamImpl() {};

public:
    virtual void Close() override;

public:
    virtual void Visit(const SplitContext& context) override
    {
        mSplitContext = context;
    }

private:
    virtual std::shared_ptr<ArrowBatchReader> OpenReader() override;

private:
    Configuration mConf;
    std::string mProject;
    std::string mInstance;
    std::string mSessionId;
    int64_t mOffset;
    int64_t mCount;
    std::vector<std::string> mColumns;
    std::string mTaskName;
    int64_t mQueryId = 0;
    bool mEnableLimit = false;
    SplitContext mSplitContext;
};

typedef std::shared_ptr<InstanceReadStreamImpl> InstanceReadStreamImplPtr;

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif  // ODPS_SDK_ENABLE_ARROW
#endif