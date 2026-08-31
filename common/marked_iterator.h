#ifndef APSARA_ODPS_SDK_COMMON_MARKED_ITERATOR_H
#define APSARA_ODPS_SDK_COMMON_MARKED_ITERATOR_H

#include "odps_api.h"
#include "boost/optional.hpp"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{

template<typename T>
class MarkedIterator: public Iterator<T>
{
public:

    virtual ~MarkedIterator() {}

    virtual std::shared_ptr<T> Next() override
    {
        if (mReadIndex >= mBuffer.size())
        {
            mBuffer.clear();
            mReadIndex = 0;
            do
            {
                if (mMarker)
                {
                    if (mMarker->size() == 0)
                    {
                        return nullptr;
                    }
                }
                mMarker = Refill(mMarker, mBuffer);
            }
            while(mBuffer.size() == 0);
        }
        auto ptr = mBuffer.at(mReadIndex);
        mReadIndex++;
        return ptr;
    }

protected:
    // put read objects into container, return marker. if returned empty string, means EOF
    // if container is not filled, will retry refill() indefinitely until exception thrown or empty string returned.
    virtual std::string Refill(boost::optional<std::string> lastMarker, std::vector<std::shared_ptr<T>>& container) = 0;

private:
    size_t mReadIndex = 0;
    std::vector<std::shared_ptr<T>> mBuffer;
    boost::optional<std::string> mMarker = boost::none;
};

}}}}

#endif