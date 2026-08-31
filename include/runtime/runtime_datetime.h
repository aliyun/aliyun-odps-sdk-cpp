#ifndef APSARA_ODPS_RUNTIME_DATETIME_H
#define APSARA_ODPS_RUNTIME_DATETIME_H

#include <stdint.h>

namespace apsara { namespace odps {

struct RuntimeDateTime {

    RuntimeDateTime() : mMilliSecond(0) { }

    RuntimeDateTime(int64_t ms): mMilliSecond(ms) {
    }

    int64_t seconds() const {
        int64_t second = mMilliSecond / 1000;
        int milliPart = mMilliSecond % 1000;
        if (milliPart < 0) {
            second -= 1;
        }
        return second;
    }

    bool operator == (const RuntimeDateTime& other) const {
        return (other.mMilliSecond == mMilliSecond);
    }

    bool operator != (const RuntimeDateTime& other) const {
        return (other.mMilliSecond != mMilliSecond);
    }

    bool operator > (const RuntimeDateTime& other) const {
        return mMilliSecond > other.mMilliSecond;
    }

    bool operator >= (const RuntimeDateTime& other) const {
        return mMilliSecond >= other.mMilliSecond;
    }

    bool operator < (const RuntimeDateTime& other) const {
        return mMilliSecond < other.mMilliSecond;
    }

    bool operator <= (const RuntimeDateTime& other) const {
        return mMilliSecond <= other.mMilliSecond;
    }

    int64_t mMilliSecond;
};

}}

#endif
