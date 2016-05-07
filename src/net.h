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

typedef struct AbdNetConfig {
    uint64_t performace_frequency;
    uint64_t(*get_performance_counter)();
} AbdNetConfig;

typedef struct AbdJoinedClient {
    bool active;
    struct sockaddr_in address;
} AbdJoinedClient;

typedef struct AbdServer {
    AbdNetConfig conf;
    SOCKET socket;
    struct sockaddr_in address;
    AbdJoinedClient clients[ABD_NET_MAX_CLIENTS];
} AbdServer;

typedef struct AbdClient {
    AbdNetConfig conf;
    SOCKET socket;
    struct sockaddr_in server_address;
} AbdClient;

enum AbdOpcode {
    AOP_HANDSHAKE
};

bool abd_start_server(AbdServer* out_server, AbdNetConfig* in_config, uint16_t port);
bool abd_server_tick(AbdServer* server);

bool abd_connect_to_server(AbdClient* out_server, AbdNetConfig* in_config, const char* ip_address, uint16_t port);
bool abd_client_tick(AbdClient* client);

#endif //ABD_NET_H
