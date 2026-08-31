#ifdef ODPS_SDK_ENABLE_ARROW
#include "table_preview_stream.h"

#include <arrow/array.h>
#include <arrow/ipc/reader.h>
#include <arrow/record_batch.h>

#include <sstream>

#include "common/http_connection.h"
#include "common/http_flags.h"
#include "common/http_message.h"
#include "commons.h"
#include "include/odps_exception.h"
#include "tunnel/arrow_stream.h"
#include "tunnel/serialize.h"
#include "util/utils.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;
using namespace apsara::odps::sdk::max_storage_api;
using namespace apsara::odps::tunnel::serialize;
using namespace apsara::odps::sdk::util;

class TablePreviewRequest
{
private:
    int64_t mLimit;
    std::string mPartition;
    std::vector<std::string> mColumns;

public:
    TablePreviewRequest() : mLimit(0) {}

    void SetLimit(int64_t limit) { mLimit = limit; }
    void SetPartition(const std::string& partition) { mPartition = partition; }
    void SetColumns(const std::vector<std::string>& columns)
    {
        mColumns = columns;
    }

    friend void to_json(nlohmann::json& j, const TablePreviewRequest& r);
    friend void from_json(const nlohmann::json& j, TablePreviewRequest& r);
};

inline void to_json(nlohmann::json& j, const TablePreviewRequest& r)
{
    j = nlohmann::json{
        {REQUEST_LIMIT, r.mLimit},
        {REQUEST_PARTITION, r.mPartition},
        {REQUEST_COLUMNS, r.mColumns},
    };
}

inline void from_json(const nlohmann::json& j, TablePreviewRequest& r)
{
    r.mLimit = j.at(REQUEST_LIMIT).get<int64_t>();
    r.mPartition = j.at(REQUEST_PARTITION).get<std::string>();
    r.mColumns = j.at(REQUEST_COLUMNS).get<std::vector<std::string>>();
}

IArrowReadStreamPtr TablePreviewStreamBuilderImpl::Build()
{
    return std::make_shared<TablePreviewStreamImpl>(mConf, *this);
}

TablePreviewStreamImpl::TablePreviewStreamImpl(const Configuration& conf,
                                     const TablePreviewStreamBuilderImpl& builder)
    : ArrowReadStreamImpl(ACTION_TABLE_PREVIEW),
      mConf(conf),
      mProject(builder.GetProject()),
      mTable(builder.GetTable()),
      mSchema(builder.GetSchema()),
      mPartition(builder.GetPartition()),
      mColumns(builder.GetColumns()),
      mLimit(builder.GetLimit())
{
}

void TablePreviewStreamImpl::Close() { mClosed = true; }

std::shared_ptr<ArrowBatchReader> TablePreviewStreamImpl::OpenReader()
{
    if (mConf.tunnelEndpoint.empty())
    {
        mConf.tunnelEndpoint = GetRouterServer(mConf, mProject);
    }

    RequestPtr req = util::CreateMaxStorageApiTableRequest(
        mConf, ACTION_TABLE_PREVIEW, mProject, mSchema, mTable);

    TablePreviewRequest request;
    if (mLimit > 0)
    {
        request.SetLimit(mLimit);
    }
    if (!mPartition.empty())
    {
        request.SetPartition(mPartition);
    }
    if (!mColumns.empty())
    {
        request.SetColumns(mColumns);
    }

    std::string requestBody = ToJsonString(request);
    req->SetHeader(CONTENT_TYPE, HEADER_APPLICATION_JSON);
    req->SetHeader(CONTENT_LENGTH, std::to_string(requestBody.size()));
    req->SetHeader(ACCEPT_ENCODING, COMPRESS_LZ4_FRAME + "," + COMPRESS_ZSTD);
    req->SetBody(requestBody);

    return std::make_shared<ArrowBatchReader>(req, mConf);
}
#endif  // ODPS_SDK_ENABLE_ARROW
