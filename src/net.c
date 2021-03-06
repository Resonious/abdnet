#include "net.h"
#include "core_rpcs.h"
#include <string.h>

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

void init_rpc_target(RpcTarget* rpc) {
    rpc->rpc_count = 0;
    rpc->rpc_buf.bytes = rpc->raw_rpc_buffer;
    rpc->rpc_buf.capacity = RPC_SEND_BUFFER_CAPACITY;
    rpc->rpc_buf.pos = 0;
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

// =========================== CREATE SERVER =========================

bool abd_start_server(AbdServer* out_server, AbdNetConfig* in_config, uint16_t port) {
    abd_init_sockets(&out_server->error);

    memset(out_server, 0, sizeof(AbdServer));
    out_server->type = ABD_SERVER;
    out_server->conf = *in_config;
    init_core_rpcs(&out_server->conf);

    for (int i = 0; i < ABD_NET_MAX_CLIENTS; i++) {
        out_server->clients[i].id = ABD_NULL_CLIENT_ID;
        init_rpc_target(&out_server->clients[i].incoming_rpc);
        init_rpc_target(&out_server->clients[i].outgoing_rpc);
    }

    init_rpc_target(&out_server->outgoing_rpc);

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

static bool x2x_rpcs(AbdConnection* connection, struct sockaddr_in* target_addr, RpcTarget* rpc, bool consume) {
    connection->send_buffer[0] = AOP_RPC;

    int16_t from_id;
    if (connection->type == ABD_SERVER)
        from_id = ABD_SERVER_ID;
    else
        from_id = AS_CLIENT(connection)->id;

    AbdBuffer send_buf = {.bytes = connection->send_buffer, .pos = 1, .capacity = RPC_SEND_BUFFER_CAPACITY};

    abd_data_write[ABDT_S16](&send_buf, &from_id);
    abd_data_write[ABDT_U16](&send_buf, &rpc->rpc_count);
    if (rpc->rpc_count == 0)
        goto Send;
    abd_data_write[ABDT_U16](&send_buf, &(uint16_t)rpc->rpc_buf.pos);

    memcpy(send_buf.bytes + send_buf.pos, rpc->rpc_buf.bytes, rpc->rpc_buf.pos);
    send_buf.pos += rpc->rpc_buf.pos;

    if (consume) {
        rpc->rpc_buf.pos = 0;
        rpc->rpc_count = 0;
    }

Send:;
    int send_result = sendto(connection->socket, send_buf.bytes, send_buf.pos, 0, target_addr, sizeof(struct sockaddr_in));

    if (send_result == SOCKET_ERROR) {
        connection->error = ABDE_FAILED_TO_SEND;
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
    AbdBuffer recv_buf = {.bytes = server->recv_buffer, .pos = 1, .capacity = ABD_RECV_BUFFER_CAPACITY};

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

            if (client->id == ABD_NULL_CLIENT_ID) {
                client->id = i;
                client->address = other_address;

                for (int j = 0; j < ABD_NET_MAX_CLIENTS; j++)
                    if (j != i)
                        server->conf.core_rpcs[CRPC_CLIENT_JOINED](CALL_ON_CLIENT_ID(server, j), i);

                return s2c_handshake_msg(server, &other_address, other_address_len, i);
            }
        }

        // No room, sorry guy.
        return s2c_handshake_msg(server, &other_address, other_address_len, ABD_HANDSHAKE_NO_ROOM);
    } break;

        // ===================== SERVER =====================
    case AOP_RPC: {
        AbdJoinedClient* client;

        int16_t client_id;
        abd_data_read[ABDT_S16](&recv_buf, &client_id);

        // TODO don't just assert... Actually HANDLE bogus data.
        abd_assert(client_id >= 0); abd_assert(client_id < ABD_NET_MAX_CLIENTS);
        client = &server->clients[client_id];

        uint16_t rpc_count;
        abd_data_read[ABDT_U16](&recv_buf, &rpc_count);
        if (rpc_count == 0)
            goto SendRpcs;
        uint16_t rpc_byte_count;
        abd_data_read[ABDT_U16](&recv_buf, &rpc_byte_count);

        AbdBuffer* rpc_dest = &client->incoming_rpc.rpc_buf;
        abd_assert(rpc_dest->pos + rpc_byte_count <= RPC_SEND_BUFFER_CAPACITY);

        memcpy(rpc_dest->bytes + rpc_dest->pos, recv_buf.bytes + recv_buf.pos, rpc_byte_count);
        client->incoming_rpc.rpc_count += rpc_count;
        rpc_dest->pos += rpc_byte_count;

    SendRpcs:
        return x2x_rpcs(AS_CONNECTION(server), &client->address, &client->outgoing_rpc, true);
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

// =========================== CREATE CLIENT ==========================

bool abd_connect_to_server(AbdClient* out_client, AbdNetConfig* in_config, const char* ip_address, uint16_t port) {
    abd_init_sockets(&out_client->error);

    memset(out_client, 0, sizeof(AbdClient));
    out_client->type = ABD_CLIENT;
    out_client->conf = *in_config;
    init_core_rpcs(&out_client->conf);

    for (int i = 0; i < ABD_NET_MAX_CLIENTS; i++)
        out_client->clients[i].id = ABD_NULL_CLIENT_ID;

    init_rpc_target(&out_client->incoming_rpc);
    init_rpc_target(&out_client->outgoing_rpc);

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
    AbdBuffer recv_buf = {.bytes = client->recv_buffer, .pos = 1, .capacity = ABD_RECV_BUFFER_CAPACITY};

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
            client->clients[client->id].id = client->id;
            return x2x_rpcs(AS_CONNECTION(client), &client->server_address, &client->outgoing_rpc, true);
        }
    } break;

        // ===================== CLIENT =====================
    case AOP_RPC: {
        int16_t client_id;
        abd_data_read[ABDT_S16](&recv_buf, &client_id);
        uint16_t rpc_count;
        abd_data_read[ABDT_U16](&recv_buf, &rpc_count);
        if (rpc_count == 0)
            goto SendRpcs;
        uint16_t rpc_byte_count;
        abd_data_read[ABDT_U16](&recv_buf, &rpc_byte_count);

        // TODO don't just assert... Actually bogus data.
        abd_assert(client_id == ABD_SERVER_ID);

        AbdBuffer* rpc_dest = &client->incoming_rpc.rpc_buf;
        abd_assert(rpc_dest->pos + rpc_byte_count <= RPC_SEND_BUFFER_CAPACITY);

        memcpy(rpc_dest->bytes + rpc_dest->pos, recv_buf.bytes + recv_buf.pos, rpc_byte_count);
        client->incoming_rpc.rpc_count += rpc_count;
        rpc_dest->pos += rpc_byte_count;

    SendRpcs:
        return x2x_rpcs(AS_CONNECTION(client), &client->server_address, &client->outgoing_rpc, true);
    } break;

    default:
        printf("Unknown opcode %i\n", op);
        client->error = ABDE_UNKNOWN_OPCODE;
        return false;
    }

    return true;
}

void abd_execute_rpcs(AbdConnection* connection, RpcTarget* rpc, AbdConnection* sender) {
    RpcInfo info;
    info.target = rpc;
    info.con    = connection;
    info.flags  = RPCF_EXECUTE_LOCALLY;
    info.rw     = ABD_READ;
    info.from   = sender;
    rpc->rpc_buf.pos = 0;

    while (rpc->rpc_count > 0) {
        uint16_t rpc_id;
        abd_transfer(ABD_READ, ABDT_U16, &rpc->rpc_buf, &rpc_id, NULL);

        // Negative RPC IDs indicate core RPCs. (for client disconnecting, joining, etc)
        if (rpc_id < 0)
            connection->conf.core_rpcs[-rpc_id](info);
        else
            connection->conf.rpc_list[rpc_id](info);

        rpc->rpc_count -= 1;
    }
    rpc->rpc_buf.pos = 0;
}

void init_core_rpcs(AbdNetConfig* conf) {
    if (conf->core_rpcs[CRPC_CLIENT_JOINED] == NULL)
        SET_CORE_RPC(conf->core_rpcs, corerpc_client_joined, CRPC_CLIENT_JOINED);

    if (conf->core_rpcs[CRPC_DISCONNECT] == NULL)
        SET_CORE_RPC(conf->core_rpcs, corerpc_disconnect, CRPC_DISCONNECT);
}
