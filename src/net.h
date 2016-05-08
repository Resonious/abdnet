#ifndef ABD_NET_H
#define ABD_NET_H

#include "data.h"
#include <stdlib.h>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#endif

#ifndef ABD_NET_MAX_CLIENTS
#define ABD_NET_MAX_CLIENTS 10
#endif
#ifndef ABD_BUFFER_CAPACITY
#define ABD_BUFFER_CAPACITY 2048
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

// --- Core Structures ---

typedef struct AbdNetConfig {
    uint64_t performace_frequency;
    uint64_t(*get_performance_counter)();
} AbdNetConfig;

typedef struct AbdJoinedClient {
    // index into AbdServer#clients
    int16_t id;
    struct sockaddr_in address;
} AbdJoinedClient;

typedef struct AbdServer {
    AbdNetError error;
    AbdNetConfig conf;
    SOCKET socket;
    struct sockaddr_in address;
    AbdJoinedClient clients[ABD_NET_MAX_CLIENTS];
    uint8_t recv_buffer[ABD_BUFFER_CAPACITY];
} AbdServer;

typedef struct AbdClient {
    AbdNetError error;
    AbdNetConfig conf;
    SOCKET socket;
    struct sockaddr_in server_address;
    int16_t id;
    uint8_t recv_buffer[ABD_BUFFER_CAPACITY];
} AbdClient;

enum AbdOpcode {
    AOP_HANDSHAKE
};

// --- handshake errors received by client ---
#define ABD_HANDSHAKE_NO_ROOM -1
#define ABD_HANDSHAKE_ALREADY_CONNECTED -2

bool abd_addr_eq(struct sockaddr_in* a1, struct sockaddr_in* a2);

/* Receive one packet and send the response.
 * TODO this should re-send packets to clients we haven't heard back from.
 * Returns true if the server is ready for another tick, or false if it failed for some reason.
 */
bool abd_start_server(AbdServer* out_server, AbdNetConfig* in_config, uint16_t port);
/* Receive one packet and send the response.
 * TODO this should re-send packets if we haven't heard back.
 * Returns true if the client is ready for another tick, or false if it failed for some reason.
 */
bool abd_server_tick(AbdServer* server);

bool abd_connect_to_server(AbdClient* out_server, AbdNetConfig* in_config, const char* ip_address, uint16_t port);
bool abd_client_tick(AbdClient* client);

#endif //ABD_NET_H
