#include "net.h"

#ifdef _WIN32
bool abd_winsock_initialized = false;
WSADATA abd_wsadata;

static void abd_clean_up_winsock() {
    if (!abd_winsock_initialized)
        return;
    WSACleanup();
}
#endif

bool abd_start_server(AbdServer* out_server, AbdNetConfig* in_config, uint16_t port) {
#ifdef _WIN32
    if (!abd_winsock_initialized) {
        if (WSAStartup(MAKEWORD(2,2),&abd_wsadata) != 0)
        {
            printf("Failed to initialize winsock. Error Code : %d", WSAGetLastError());
            return false;
        }
        abd_winsock_initialized = true;
        atexit(abd_clean_up_winsock);
    }
#endif
    memset(out_server, 0, sizeof(AbdServer));
    out_server->conf = *in_config;

    out_server->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_server->socket == INVALID_SOCKET) {
        return false;
    }

    out_server->address.sin_family = AF_INET;
    out_server->address.sin_addr.s_addr = INADDR_ANY;
    out_server->address.sin_port = htons(port);

    int bind_result = bind(out_server->socket, (struct sockaddr*)&out_server->address, sizeof(struct sockaddr_in));
    if (bind_result == SOCKET_ERROR) {
        return false;
    }

    return true;
}
