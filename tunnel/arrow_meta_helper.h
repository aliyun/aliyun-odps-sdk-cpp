#ifndef APSARA_ODPS_TUNNEL_ODPS_ARROW_META_HELPER_H
#define APSARA_ODPS_TUNNEL_ODPS_ARROW_META_HELPER_H

#ifdef ODPS_SDK_ENABLE_ARROW

#include <stdint.h>
#include <memory>
#include <arrow/record_batch.h>
#include <arrow/api.h>
#include "odps_meta.h"
#include "odps_tunnel.h"
#include "common/odps_table_schema.h"
#include "odps_exception.h"

namespace apsara {
namespace odps {
namespace sdk {
namespace internal {
namespace tunnel{

inline std::shared_ptr<arrow::DataType> TransformToArrowType(ODPSColumnTypeInfo typeInfo)
{
    switch (typeInfo.mType)
    {
        case ODPS_TINYINT:               return arrow::int8();
        case ODPS_SMALLINT:              return arrow::int16();
        case ODPS_INTEGER:               return arrow::int32();
        case ODPS_BIGINT:                return arrow::int64();
        case ODPS_BOOLEAN:               return arrow::boolean();
        case ODPS_FLOAT:                 return arrow::float32();
        case ODPS_DOUBLE:                return arrow::float64();
        case ODPS_VARCHAR:               return arrow::utf8();
        case ODPS_CHAR:                  return arrow::utf8();
        case ODPS_STRING:                return arrow::utf8();
        case ODPS_JSON:                  return arrow::utf8();
        case ODPS_DATE:                  return arrow::date32();
        case ODPS_TIMESTAMP:             return arrow::timestamp(arrow::TimeUnit::NANO);
        case ODPS_TIMESTAMP_NTZ:         return arrow::timestamp(arrow::TimeUnit::NANO);
        case ODPS_BINARY:                return arrow::binary();
        case ODPS_DATETIME:              return arrow::timestamp(arrow::TimeUnit::MILLI);
        case ODPS_INTERVAL_YEAR_MONTH:   return arrow::int64();
        case ODPS_INTERVAL_DAY_TIME:     return arrow::timestamp(arrow::TimeUnit::NANO);
        case ODPS_DECIMAL:
            // force precision 38 and scale 18 for legacy decimal instead of (54,18)
            // compatible with arrow Decimal128Type
            return typeInfo.IsLegacyDecimal()? arrow::decimal(38, 18) : arrow::decimal(typeInfo.mPrecision, typeInfo.mScale);
        case ODPS_ARRAY:
        {
            return arrow::list(TransformToArrowType(typeInfo.mSubTypes.at(0)));
        }
        case ODPS_MAP:
        {
            return arrow::map(TransformToArrowType(typeInfo.mSubTypes.at(0)), TransformToArrowType(typeInfo.mSubTypes.at(1)));
        }
        case ODPS_STRUCT:
        {
            std::vector<std::shared_ptr<arrow::Field>> aFields;
            for (auto i: typeInfo.mSubTypes)
            {
                aFields.push_back(arrow::field(i.mMemberName, TransformToArrowType(i)));
            }
            return arrow::struct_(aFields);
        }
        default:
            throw OdpsTunnelException(std::string("Unsupported type: ") + typeInfo.ToTypeString());
    }
}

inline std::shared_ptr<arrow::Schema> SqlSchemaToArrowSchema(const IODPSTableSchema& schema)
{
    std::vector<std::shared_ptr<arrow::Field>> fields;
    for (uint32_t i = 0; i != schema.GetColumnCount(); ++i)
    {
        const std::string& fieldName = schema.GetTableColumn(i).GetName();
        ODPSColumnTypeInfo fieldType = schema.GetTableColumn(i).GetTypeInfo();
        bool nullable = schema.GetTableColumn(i).GetNullable();
        std::shared_ptr<arrow::DataType> arrowType = TransformToArrowType(fieldType);
        fields.push_back(arrow::field(fieldName, arrowType, nullable));
    }
    std::shared_ptr<arrow::Schema> arrowSchema = std::make_shared<arrow::Schema>(fields);
    return arrowSchema;
}

}}}}}

#endif

#endif
