#ifndef APSARA_ODPS_TUNNEL_INTERNAL_CONNECTION_MANAGER_H
#define APSARA_ODPS_TUNNEL_INTERNAL_CONNECTION_MANAGER_H

#include "common/http_connection.h"
#include "odps_tunnel.h"

namespace apsara{ namespace odps{ namespace sdk { namespace internal{ namespace tunnel{

class ConnectionManager {
public:
    ConnectionManager(const RequestPtr& req, const Configuration& conf);
    ~ConnectionManager();

    HttpConnectionPtr OpenReaderConnection();
    HttpConnectionPtr OpenWriterConnection();
private:
    RequestPtr mRequestPtr;
    Configuration mConf;
};

typedef std::shared_ptr<ConnectionManager> ConnectionManagerPtr;

}}}}}

#endif
