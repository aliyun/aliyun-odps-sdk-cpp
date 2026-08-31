#ifndef APSARA_ODPS_SDK_REST_UTILS_H
#define APSARA_ODPS_SDK_REST_UTILS_H

#include "common/http_message.h"
#include "common/http_connection.h"
#include "rest_response.h"



namespace apsara { namespace odps { namespace sdk { namespace internal {

class RestUtils
{
public:
    static RestResponsePtr ToRestResponse(
        Response &resp, HttpConnection &conn, bool can_parse);

    static RestResponsePtr ToRestResponse(
        Response &resp, HttpConnection &conn);

    static void HandleOdpsFailures(RestResponsePtr resp);
};

}
} // namespace sdk
} // namespace odps
} // namespace apsara

#endif