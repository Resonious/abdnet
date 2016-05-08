#include "net.h"

#ifdef _WIN32
bool abd_winsock_initialized = false;
WSADATA abd_wsadata;

void abd_clean_up_winsock() {
    if (!abd_winsock_initialized)
        return;
    WSACleanup();
}
#endif

bool abd_addr_eq(struct sockaddr_in* a1, struct sockaddr_in* a2) {
    return a1->sin_addr.s_addr == a2->sin_addr.s_addr && a1->sin_port == a2->sin_port;
}

#ifdef _WIN32
static void abd_init_sockets(AbdNetError* err) {
    *err = ABDE_NO_ERROR;
    if (!abd_winsock_initialized) {
        if (WSAStartup(MAKEWORD(2,2),&abd_wsadata) != 0)
        {
            printf("Failed to initialize winsock. Error Code : %d", WSAGetLastError());
            *err = ABDE_WINSOCK_INIT_FAIL;
            return;
        }
        abd_winsock_initialized = true;
        atexit(abd_clean_up_winsock);
    }
}
#else
#define abd_init_sockets(x) do{*(x)=ABDE_NO_ERROR;}while(0)
#endif

bool abd_start_server(AbdServer* out_server, AbdNetConfig* in_config, uint16_t port) {
    abd_init_sockets(&out_server->error);

    memset(out_server, 0, sizeof(AbdServer));
    out_server->conf = *in_config;

    for (int i = 0; i < ABD_NET_MAX_CLIENTS; i++)
        out_server->clients[i].id = -1;

    out_server->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_server->socket == INVALID_SOCKET) {
        out_server->error = ABDE_COULDNT_CREATE_SOCKET;
        return false;
    }

    out_server->address.sin_family = AF_INET;
    out_server->address.sin_addr.s_addr = INADDR_ANY;
    out_server->address.sin_port = htons(port);

    int bind_result = bind(out_server->socket, (struct sockaddr*)&out_server->address, sizeof(struct sockaddr_in));
    if (bind_result == SOCKET_ERROR) {
        out_server->error = ABDE_FAILED_TO_BIND;
        return false;
    }

    return true;
}

static bool s2c_handshake_msg(AbdServer* server, struct sockaddr_in* to_addr, int tolen, int16_t id_or_error) {
    uint8_t payload[3];
    payload[0] = AOP_HANDSHAKE;
    memcpy(payload + 1, &id_or_error, 2);

    int send_result = sendto(server->socket, payload, 3, 0, (struct sockaddr*)to_addr, tolen);

    if (send_result == SOCKET_ERROR) {
        server->error = ABDE_FAILED_TO_SEND;
        return false;
    }
    else
        return true;
}

bool abd_server_tick(AbdServer* server) {
    struct sockaddr_in other_address;
    int other_address_len = sizeof(other_address);

    int recv_size = recvfrom(
        server->socket,
        server->recv_buffer,
        sizeof(server->recv_buffer),
        0,
        (struct sockaddr*)&other_address,
        &other_address_len
    );

    if (recv_size == SOCKET_ERROR) {
        server->error = ABDE_FAILED_TO_RECEIVE;
        return false;
    }
    if (recv_size == 0) {
        // TODO do timeout checks or whatever
        return true;
    }

    uint8_t op = server->recv_buffer[0];

    switch (op) {
    case AOP_HANDSHAKE: {
        AbdJoinedClient* client;

        // Check if this client has already connected:
        for (int i = 0; i < ABD_NET_MAX_CLIENTS; i++) {
            client = &server->clients[i];

            if (abd_addr_eq(&client->address, &other_address))
                return s2c_handshake_msg(server, &other_address, other_address_len, ABD_HANDSHAKE_ALREADY_CONNECTED);
        }

        // Find an unused slot (id) for them:
        for (int i = 0; i < ABD_NET_MAX_CLIENTS; i++) {
            client = &server->clients[i];

            if (client->id == -1) {
                client->id = i;
                client->address = other_address;
                return s2c_handshake_msg(server, &other_address, other_address_len, i);
            }
        }

        // No room, sorry guy.
        return s2c_handshake_msg(server, &other_address, other_address_len, ABD_HANDSHAKE_NO_ROOM);
    } break;

    default:
        // TODO this is a per-client issue... we should first make sure the address even belongs to a client.
        server->error = ABDE_UNKNOWN_OPCODE;
        return false;
    }

    return true;
}

// literally just the handshake opcode
static bool c2s_handshake_msg(AbdClient* client) {
    uint8_t payload[1] = { AOP_HANDSHAKE };
    int send_result = sendto(client->socket, payload, 1, 0, (struct sockaddr*)&client->server_address, sizeof(client->server_address));

    if (send_result == SOCKET_ERROR) {
        client->error = ABDE_FAILED_TO_SEND;
        return false;
    }
    else
        return true;
}

bool abd_connect_to_server(AbdClient* out_client, AbdNetConfig* in_config, const char* ip_address, uint16_t port) {
    abd_init_sockets(&out_client->error);

    memset(out_client, 0, sizeof(AbdClient));
    out_client->conf = *in_config;

    out_client->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (out_client->socket == INVALID_SOCKET) {
        out_client->error = ABDE_COULDNT_CREATE_SOCKET;
        return false;
    }

    out_client->server_address.sin_family = AF_INET;
    out_client->server_address.sin_addr.s_addr = inet_addr(ip_address);
    out_client->server_address.sin_port = htons(port);

    return c2s_handshake_msg(out_client);
}

bool abd_client_tick(AbdClient* client) {
    struct sockaddr_in other_address;
    int other_address_len = sizeof(other_address);

    int recv_size = recvfrom(
        client->socket,
        client->recv_buffer,
        sizeof(client->recv_buffer),
        0,
        (struct sockaddr*)&other_address,
        &other_address_len
    );

    if (recv_size == SOCKET_ERROR) {
        client->error = ABDE_FAILED_TO_RECEIVE;
        return false;
    }
    if (recv_size == 0) {
        // TODO do timeout checks or whatever
        return true;
    }

    uint8_t op = client->recv_buffer[0];

    switch (op) {
    case AOP_HANDSHAKE: {
        int16_t id_or_error;
        memcpy(&id_or_error, client->recv_buffer + 1, 2);

        switch (id_or_error) {
        case ABD_HANDSHAKE_NO_ROOM:
            printf("Handshake error: SERVER FULL\n");
            client->error = ABDE_SERVER_WAS_FULL;
            return false;
        case ABD_HANDSHAKE_ALREADY_CONNECTED:
            printf("Handshake error: ALREADY CONNECTED\n");
            client->error = ABDE_ALREADY_CONNECTED;
            return false;

        default:
            client->id = id_or_error;
        }
    } break;

    default:
        printf("Unknown opcode %i\n", op);
        client->error = ABDE_UNKNOWN_OPCODE;
        return false;
    }

    return true;
}
