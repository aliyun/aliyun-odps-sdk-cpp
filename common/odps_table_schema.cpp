#include <sstream>
#include "common/odps_table_schema.h"
#include "error_code.h"
#include "util/utils.h"

using namespace apsara::odps::sdk::util;

namespace apsara { namespace odps{ namespace sdk { namespace internal {

ODPSTableSchema::ODPSTableSchema(const ODPSTableSchema& other)
    :mColumns(other.mColumns),
     mPartitions(other.mPartitions),
     mValidIndex(other.mValidIndex)
{
}

ODPSTableSchema* ODPSTableSchema::Clone() const
{
    return new ODPSTableSchema(*this);
}

uint32_t ODPSTableSchema::GetColumnCount() const
{
    return mColumns.size();
}

uint32_t ODPSTableSchema::GetPartitionLevels() const
{
    return mPartitions.size();
}

const IODPSTableColumn& ODPSTableSchema::GetTableColumn(uint32_t index) const
{
    return mColumns.at(index);
}

const IODPSTableColumn& ODPSTableSchema::GetTablePartition(uint32_t index) const
{
    return mPartitions.at(index);
}

IODPSTableSchema& ODPSTableSchema::AppendColumn(const IODPSTableColumn& column)
{
    mColumns.emplace_back(dynamic_cast<const ODPSTableColumn&>(column));
    return *this;
}

IODPSTableSchema& ODPSTableSchema::AppendPartition(const IODPSTableColumn& partition)
{
    mPartitions.emplace_back(dynamic_cast<const ODPSTableColumn&>(partition));
    return *this;
}

ODPSTableSchema& ODPSTableSchema::AppendColumns(const std::vector<ODPSTableColumn>& columns)
{
    for(const auto& c: columns)
    {
        mColumns.emplace_back(c);
    }
    return *this;
}

ODPSTableSchema& ODPSTableSchema::AppendPartitions(const std::vector<ODPSTableColumn>& partitions)
{
    for(const auto& c: partitions)
    {
        mPartitions.emplace_back(c);
    }
    return *this;
}

std::string ODPSTableSchema::ToString() const
{
    std::ostringstream oss;
    oss << "Schema Columns:" << std::endl;
    for (const auto& column : mColumns)
    {
        oss << column.ToString() << std::endl;
    }
    oss << "Partition Columns:" << std::endl;
    for (const auto& column : mPartitions)
    {
        oss << column.ToString() << std::endl;
    }
    return oss.str();
}

}}}}