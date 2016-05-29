#include "rpcdsl.h"

// Server --> Client; called when a client successfully joins the server.
DEFINE_RPC(corerpc_client_joined, A(ABDT_U16, c_id)) {
    abd_assert(connection->type == ABD_CLIENT);
    AS_CLIENT(connection)->clients[c_id].id = c_id;
}
END_RPC

// If received on client, it means the given client disconnected.
// If received on server, it means the client is gracefully disconnecting.
DEFINE_RPC(corerpc_disconnect, A(ABDT_U16, c_id)) {
    if (connection->type == ABD_CLIENT) {
        if (c_id == ABD_SERVER_ID)
            abd_assert(!"TODO figure out how to handle server disconnection. A callback perhaps, or just end up returning false from next tick.");

        AS_CLIENT(connection)->clients[c_id].id = ABD_NULL_CLIENT_ID;
    }
    else {
        AS_SERVER(connection)->clients[c_id].id = ABD_NULL_CLIENT_ID;
    }
}
END_RPC
