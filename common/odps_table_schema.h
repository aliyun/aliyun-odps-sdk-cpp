#ifndef APSARA_TUNNLE_ODPS_TABLE_SCHEMA
#define APSARA_TUNNLE_ODPS_TABLE_SCHEMA

#include <stdint.h>
#include <vector>
#include "odps_tunnel.h"
#include "common/odps_column.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal {

class ODPSTableSchema : public IODPSTableSchema
{
public:
    ODPSTableSchema() {};
    ODPSTableSchema(const ODPSTableSchema& other);
    virtual ODPSTableSchema* Clone() const override;
    virtual uint32_t GetColumnCount() const override;

    virtual const IODPSTableColumn& GetTableColumn(uint32_t index) const override;

    virtual uint32_t GetPartitionLevels() const override;

    virtual const IODPSTableColumn& GetTablePartition(uint32_t index) const override;

    virtual IODPSTableSchema& AppendColumn(const IODPSTableColumn& column) override;
    ODPSTableSchema& AppendColumns(const std::vector<ODPSTableColumn>& column);

    virtual IODPSTableSchema& AppendPartition(const IODPSTableColumn& partition) override;
    ODPSTableSchema& AppendPartitions(const std::vector<ODPSTableColumn>& partition);

    const std::vector<uint32_t>& GetValidIndexSet() const { return mValidIndex; }

    virtual std::string ToString() const override;

    virtual int64_t GetMaxFieldSize() const override { return mMaxFieldSize; }
    void SetMaxFieldSize(int64_t maxFieldSize) { mMaxFieldSize = maxFieldSize; }
private:
    std::vector<ODPSTableColumn> mColumns;
    std::vector<ODPSTableColumn> mPartitions;
    std::vector<uint32_t> mValidIndex;
    int64_t mMaxFieldSize = 8 * 1024 * 1024; // default is 8M

};

using ODPSTableSchemaPtr = std::shared_ptr<ODPSTableSchema>;

}}}}
#endif

