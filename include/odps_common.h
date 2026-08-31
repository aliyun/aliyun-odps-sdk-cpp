#ifndef APSARA_ODPS_COMMON_H
#define APSARA_ODPS_COMMON_H
#include <memory>

namespace apsara{ namespace odps{ namespace sdk{

template <typename T>
class Iterator
{
public:
    // return null if no more elements.
    // every call of Next() will invalidate previous returned pointer.
    virtual std::shared_ptr<T> Next() = 0;
};

}}}
#endif