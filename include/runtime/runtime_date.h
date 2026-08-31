#ifndef APSARA_ODPS_RUNTIME_DATE_H
#define APSARA_ODPS_RUNTIME_DATE_H

#include <stdint.h>

namespace apsara { namespace odps {

struct RuntimeDate {

    RuntimeDate() :mSecond(0) { }

    RuntimeDate(int64_t second): mSecond(second) {
    }

    bool operator == (const RuntimeDate& other) const {
        return (other.mSecond == mSecond);
    }

    bool operator != (const RuntimeDate& other) const {
        return (other.mSecond != mSecond);
    }

    bool operator > (const RuntimeDate& other) const {
        return mSecond > other.mSecond;
    }

    bool operator >= (const RuntimeDate& other) const {
        return mSecond >= other.mSecond;
    }

    bool operator < (const RuntimeDate& other) const {
        return mSecond < other.mSecond;
    }

    bool operator <= (const RuntimeDate& other) const {
        return mSecond <= other.mSecond;
    }

    int64_t mSecond;
};

}}

#endif
