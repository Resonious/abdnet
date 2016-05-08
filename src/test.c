#ifdef ABD_TEST

#include "data.h"
#include "net.h"

int wsa_last_error = 0;
int last_errno = 0;

#define DATATEST_NEW_BUFFER() { .bytes = memory, .capacity = 2048, .pos = 0 }
#ifdef _WIN32
#define EXPECT_OR(cond, onfail) if (!(cond)) { printf("\"%s\" returned false\n", #cond); onfail; DebugBreak(); return false; }

#define EXPECT(cond) EXPECT_OR(cond, {})
#define NET_EXPECT(cond) EXPECT_OR(cond, wsa_last_error = WSAGetLastError())
#endif

#define str_eq(s1, s2) (strcmp((s1), (s2)) == 0)

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z, w; } vec4;

static bool test_can_write_and_read_an_unannotated_float(uint8_t* memory) {
    float to_write = 1.205000043f;

    AbdBuffer buffer = DATATEST_NEW_BUFFER();
    abd_transfer(ABD_WRITE, ABDT_FLOAT, &buffer, &to_write, NULL);
    buffer.pos = 0;

    float to_read;
    abd_transfer(ABD_READ, ABDT_FLOAT, &buffer, &to_read, NULL);

    EXPECT(to_write == to_read);
    return true;
}

static bool test_can_write_and_read_an_annotated_float(uint8_t* memory) {
    float to_write = 1.205000043f;

    AbdBuffer buffer = DATATEST_NEW_BUFFER();
    abd_transfer(ABD_WRITE, ABDT_FLOAT, &buffer, &to_write, "Here's my float");
    buffer.pos = 0;

    uint8_t read_type;
    char* read_annotation;
    abd_read_field(&buffer, &read_type, &read_annotation);

    EXPECT(read_type == ABDT_FLOAT);
    EXPECT(read_annotation != NULL);
    EXPECT(str_eq(read_annotation, "Here's my float"));
    return true;
}

static bool test_can_write_and_read_a_bunch_of_stuff(uint8_t* memory) {
    AbdBuffer buffer = DATATEST_NEW_BUFFER();
    float  f1 = 0.1f, f2 = 0.2f;
    int32_t i1 = 1,    i2 = 2;

    abd_transfer(ABD_WRITE, ABDT_FLOAT, &buffer, &f1, "First float");
    abd_transfer(ABD_WRITE, ABDT_FLOAT, &buffer, &f2, NULL);
    abd_transfer(ABD_WRITE, ABDT_S32,   &buffer, &i1, "Now int");
    abd_transfer(ABD_WRITE, ABDT_S32,   &buffer, &i2, "Now int");

    buffer.pos = 0;
    float rf1, rf2;
    int32_t ri1, ri2;
    abd_transfer(ABD_READ, ABDT_FLOAT, &buffer, &rf1, NULL);
    abd_transfer(ABD_READ, ABDT_FLOAT, &buffer, &rf2, NULL);
    abd_transfer(ABD_READ, ABDT_S32,   &buffer, &ri1, NULL);
    abd_transfer(ABD_READ, ABDT_S32,   &buffer, &ri2, NULL);

    EXPECT(f1 == rf1);
    EXPECT(f2 == rf2);
    EXPECT(i1 == ri1);
    EXPECT(i2 == ri2);

    return true;
}

static bool test_sections_work(uint8_t* memory) {
    float to_write = 1.205000043f;

    AbdBuffer buffer = DATATEST_NEW_BUFFER();
    abd_transfer(ABD_WRITE, ABDT_FLOAT, &buffer, &to_write, NULL);
    abd_section(ABD_WRITE, &buffer, "Test Section");
    buffer.pos = 0;

    float to_read;
    abd_transfer(ABD_READ, ABDT_FLOAT, &buffer, &to_read, NULL);

    uint8_t read_type;
    char* read_annotation;
    char read_sect[32];
    abd_read_field(&buffer, &read_type, &read_annotation);
    abd_read_string(&buffer, read_sect);

    EXPECT(str_eq(read_sect, "Test Section"));
    return true;
}

static bool test_inspect_works(uint8_t* memory) {
    float fl = 1.205000043f;
    int in = 4;
    vec2 v2 = {10.5f, 0.2f};
    vec4 v4 = {1.4, 2.3, 3.2, 4.1};
    uint8_t col[4] = {255, 255, 0, 255};
    bool boo = true;
    unsigned int uin = 120000;

    AbdBuffer buffer = DATATEST_NEW_BUFFER();
    abd_transfer(ABD_WRITE, ABDT_FLOAT, &buffer, &fl, "the float");
    abd_transfer(ABD_WRITE, ABDT_S32, &buffer, &in, NULL);
    abd_section(ABD_WRITE, &buffer, "Test Section");
    abd_transfer(ABD_WRITE, ABDT_S32, &buffer, &in, "This is the same int");
    abd_transfer(ABD_WRITE, ABDT_VEC2, &buffer, &v2, "a vec2");
    abd_transfer(ABD_WRITE, ABDT_VEC4, &buffer, &v4, NULL);
    abd_transfer(ABD_WRITE, ABDT_BOOL, &buffer, &boo, "True (I think)");
    abd_transfer(ABD_WRITE, ABDT_VEC4, &buffer, &v4, NULL);
    abd_section(ABD_WRITE, &buffer, "New Types");
    abd_transfer(ABD_WRITE, ABDT_COLOR, &buffer, &col, NULL);
    abd_transfer(ABD_WRITE, ABDT_BOOL, &buffer, &boo, "True"); boo = false;
    abd_transfer(ABD_WRITE, ABDT_BOOL, &buffer, &boo, "False");
    abd_transfer(ABD_WRITE, ABDT_STRING, &buffer, "A string value", "that is a string");
    abd_transfer(ABD_WRITE, ABDT_U32, &buffer, &uin, NULL);
    buffer.capacity = buffer.pos;
    buffer.pos = 0;

    printf("\n----------------------- BEGIN INSPECTION --------------------------\n\n");
    bool r = abd_inspect(&buffer, stdout);
    printf("\n------------------------ END INSPECTION ---------------------------\n");
    EXPECT(r);
    EXPECT(buffer.pos == 0);

    return true;
}

typedef struct TestMemory {
    AbdNetConfig config;
    AbdServer server;
    AbdClient client;
} TestMemory;

static uint64_t qpf() {
#ifdef _WIN32
    uint64_t f;
    if (QueryPerformanceFrequency(&f))
        return f;
    else
        exit(1);
#endif
}

static bool test_server_can_start(uint8_t* pmemory) {
    TestMemory* mem = (TestMemory*)pmemory;

#ifdef _WIN32
    QueryPerformanceFrequency(&mem->config.performace_frequency);
#endif
    mem->config.get_performance_counter = qpf;

    EXPECT(abd_start_server(&mem->server, &mem->config, 7778));

    return true;
}

static bool test_client_can_join(uint8_t* pmemory) {
    TestMemory* mem = (TestMemory*)pmemory;

    // This should send out the handshake
    NET_EXPECT(abd_connect_to_server(&mem->client, &mem->config, "127.0.0.1", 7778));
    // This should receive said handshake
    NET_EXPECT(abd_server_tick(&mem->server));

    NET_EXPECT(mem->client.id == 0);
    NET_EXPECT(mem->server.clients[0].id == 0);

    return true;
}

static bool close_sockets(uint8_t* pmemory) {
    TestMemory* mem = (TestMemory*)pmemory;
    closesocket(mem->server.socket);
    closesocket(mem->client.socket);
    return true;
}

#define RUN_TEST(name) if (name(memory)) printf(#name" passed!\n"); else printf(#name" FAILED.\n");

int main() {
    uint8_t* memory = malloc(1024 * 10);

    RUN_TEST(test_can_write_and_read_an_unannotated_float);
    RUN_TEST(test_can_write_and_read_an_annotated_float);
    RUN_TEST(test_can_write_and_read_a_bunch_of_stuff);
    RUN_TEST(test_sections_work);
    RUN_TEST(test_inspect_works);

    RUN_TEST(test_server_can_start);
    RUN_TEST(test_client_can_join);
    RUN_TEST(close_sockets);

    system("pause");
    free(memory);
    return 0;
}

#endif

