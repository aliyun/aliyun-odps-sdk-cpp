#ifndef APSARA_ODPS_TUNNEL_UPSERT_RECORD_H
#define APSARA_ODPS_TUNNEL_UPSERT_RECORD_H

#include "odps_table.h"

namespace apsara {
namespace odps {
namespace sdk { namespace internal { namespace tunnel{

class UpsertRecord : public ODPSTableRecord
{
private:
    UpsertRecord(const std::shared_ptr<IODPSTableSchema>& schema);
    friend class UpsertSession;

public:
    ~UpsertRecord()
    {
    }

    void SetOperation(uint32_t operation);
    void SetValueCols(std::shared_ptr<ODPSArray> value);
    const std::map<std::string, uint32_t> GetColName2Id() const;

public:
    std::map<std::string, uint32_t> mColName2Id;
};

typedef std::shared_ptr<UpsertRecord> UpsertRecordPtr;

}  // namespace tunnel
}  // namespace internal
}  // namespace sdk
}  // namespace odps
}  // namespace apsara

#endif