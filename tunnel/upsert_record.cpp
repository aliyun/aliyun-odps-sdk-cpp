#include "odps_table.h"
#include "upsert.h"

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal::tunnel;

UpsertRecord::UpsertRecord(const std::shared_ptr<IODPSTableSchema>& schema) : ODPSTableRecord(schema)
{
    uint32_t colCount = schema->GetColumnCount() - 5;
    for(uint32_t i = 0; i < colCount; i++)
    {
        std::string columnName = schema->GetTableColumn(i).GetName();
        mColName2Id[columnName] = i;
    }
}

const std::map<std::string, uint32_t> UpsertRecord::GetColName2Id() const
{
    return mColName2Id;
}

void UpsertRecord::SetOperation(uint32_t operation)
{
    SetTinyIntValue(mColCount - 3, operation);
}

void UpsertRecord::SetValueCols(std::shared_ptr<ODPSArray> value)
{
    SetArrayValue(mColCount - 1, value);
}