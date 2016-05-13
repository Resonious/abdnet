#ifndef ABD_NET_H
#define ABD_NET_H

#include "data.h"
#include <stdlib.h>
#include <stdarg.h>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <arpa/inet.h>
#define WSAGetLastError() (-1)
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define SOCKET int
#define closesocket close
#endif

#define ABD_NULL_CLIENT_ID (-2)
#define ABD_SERVER_ID (-1)

#ifndef ABD_NET_MAX_CLIENTS
#define ABD_NET_MAX_CLIENTS 10
#endif

#ifndef ABD_RECV_BUFFER_CAPACITY
#define ABD_RECV_BUFFER_CAPACITY 2048
#endif

#ifndef RPC_SEND_BUFFER_CAPACITY 2048
#define RPC_SEND_BUFFER_CAPACITY 2048
#endif

#ifndef ABD_USER_DATA_TYPE
#define ABD_USER_DATA_TYPE void*
#endif

// --- Errors and their accompanying messages ---
typedef enum AbdNetError {
    ABDE_NO_ERROR,
    ABDE_WINSOCK_INIT_FAIL,
    ABDE_COULDNT_CREATE_SOCKET,
    ABDE_FAILED_TO_BIND,
    ABDE_FAILED_TO_SEND,
    ABDE_FAILED_TO_RECEIVE,
    ABDE_UNKNOWN_OPCODE,
    ABDE_SERVER_WAS_FULL,
    ABDE_ALREADY_CONNECTED
} AbdNetError;

static const char* abd_error_message[] = {
    "Nothing is wrong",
    "Failed to initialize WinSock",
    "Couldn't create socket",
    "Failed to bind",
    "Failed to send",
    "recvfrom failed",
    "Unknown opcode received",
    "Tried to connect to a full server",
    "Tried to connect to a server that we are already connected to"
};

// --- Core Types ---

#define RPCF_EXECUTE_LOCALLY (1 << 0)

typedef struct RpcTarget {
    uint8_t raw_rpc_buffer[RPC_SEND_BUFFER_CAPACITY];
    AbdBuffer rpc_buf;
    uint16_t rpc_count;
} RpcTarget;

struct AbdConnection;
struct AbdJoinedClient;
typedef struct RpcInfo {
    RpcTarget* target;
    uint8_t rw;
    uint8_t flags;
    struct AbdConnection* con;
} RpcInfo;

typedef void(*RpcFunc)(RpcInfo, ...);

typedef struct AbdNetConfig {
    uint64_t performace_frequency;
    uint64_t(*get_performance_counter)();

    RpcFunc* rpc_list;
} AbdNetConfig;

enum AbdConnectionType {
    ABD_SERVER,
    ABD_CLIENT
};

#define ABD_CONNECTION_FIELDS                      \
    enum AbdConnectionType type;                   \
    AbdNetError error;                             \
    AbdNetConfig conf;                             \
    SOCKET socket;                                 \
    ABD_USER_DATA_TYPE ud;                         \
    uint8_t recv_buffer[ABD_RECV_BUFFER_CAPACITY]; \
    uint8_t send_buffer[ABD_RECV_BUFFER_CAPACITY]

typedef struct AbdConnection {
    ABD_CONNECTION_FIELDS;
} AbdConnection;

struct AbdServer;
typedef struct AbdJoinedClient {
    // Incoming from this client
    RpcTarget outgoing_rpc;
    // Outgoing to this client
    RpcTarget incoming_rpc;
    // index into AbdServer#clients
    int16_t id;
    struct sockaddr_in address;
    uint64_t last_received_at;
    struct AbdServer* server;
} AbdJoinedClient;

typedef struct AbdServer {
    ABD_CONNECTION_FIELDS;
    struct sockaddr_in address;
    AbdJoinedClient clients[ABD_NET_MAX_CLIENTS];
    // Outgoing to all clients
    RpcTarget outgoing_rpc;
} AbdServer;

typedef struct AbdRemoteClient {
    int16_t id;
} AbdRemoteClient;

typedef struct AbdClient {
    ABD_CONNECTION_FIELDS;
    struct sockaddr_in server_address;
    int16_t id;
    AbdRemoteClient clients[ABD_NET_MAX_CLIENTS];
    // Incoming from server
    RpcTarget incoming_rpc;
    // Outgoing to server
    RpcTarget outgoing_rpc;
} AbdClient;

#define AS_SERVER(connection) ((AbdServer*)(connection))
#define AS_CLIENT(connection) ((AbdClient*)(connection))
#define AS_CONNECTION(server_or_client) ((AbdConnection*)(server_or_client))

enum AbdOpcode {
    // First opcode sent to server by client should be this.
    AOP_HANDSHAKE,
    // An untimed RPC is executed as soon as it's received.
    AOP_UNTIMED_RPC
};

// --- handshake errors received by client ---
#define ABD_HANDSHAKE_NO_ROOM -1
#define ABD_HANDSHAKE_ALREADY_CONNECTED -2

bool abd_addr_eq(struct sockaddr_in* a1, struct sockaddr_in* a2);
void init_rpc_target(RpcTarget* rpc);

bool abd_start_server(AbdServer* out_server, AbdNetConfig* in_config, uint16_t port);
/* Receive one packet and send the response.
 * TODO this should re-send packets to clients we haven't heard back from.
 * Returns true if the server is ready for another tick, or false if it failed for some reason.
 */
bool abd_server_tick(AbdServer* server);

bool abd_connect_to_server(AbdClient* out_server, AbdNetConfig* in_config, const char* ip_address, uint16_t port);
/* Receive one packet and send the response.
 * TODO this should re-send packets if we haven't heard back.
 * Returns true if the client is ready for another tick, or false if it failed for some reason.
 */
bool abd_client_tick(AbdClient* client);

void abd_execute_rpcs(AbdConnection* connection, RpcTarget* rpc);
#define abd_execute_client_rpcs(client) abd_execute_rpcs(AS_CONNECTION(client), &(client)->incoming_rpc)
#define abd_execute_server_rpcs(server)                                                   \
    for (int i = 0; i < ABD_NET_MAX_CLIENTS; i++)                                         \
        if ((server)->clients[i].id != ABD_NULL_CLIENT_ID)                                \
            abd_execute_rpcs(AS_CONNECTION(server), &(server)->clients[i].incoming_rpc);

#endif //ABD_NET_H
