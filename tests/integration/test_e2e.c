#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include "boreline/boreline.h"
#include "server/server.h"

#define _XOPEN_SOURCE 500
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* End-to-end loopback tests.
 *
 * These tests exercise the routing table, session management and client
 * CLI entry points without requiring real DNS or production TLS
 * credentials. They bind to loopback ports only and use test certificates
 * where TLS is involved (covered by the M2 gate).
 */

static int passed = 0, failed = 0, ran = 0;

#define ASSERT(cond, msg) do { \
    ran++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failed++; \
    } else \
        passed++; \
} while (0)

/* ---- routing table tests ---- */

static void test_routing_add_lookup(void)
{
    /* Clear global routing state */
    boreline_routing_clear();

    int rc = boreline_routing_add("demo.boreline.test", 42,
                                   "127.0.0.1", 3000,
                                   BORELINE_SERVICE_HTTP);
    ASSERT(rc == BORELINE_OK, "add route");

    uint32_t session_id = 0;
    char local_host[128] = {0};
    uint16_t local_port = 0;
    rc = boreline_routing_lookup("demo.boreline.test", &session_id,
                                  local_host, &local_port);
    ASSERT(rc == BORELINE_OK, "lookup route");
    ASSERT(session_id == 42, "lookup session id");
    ASSERT(strcmp(local_host, "127.0.0.1") == 0, "lookup local host");
    ASSERT(local_port == 3000, "lookup local port");
}

static void test_routing_conflict(void)
{
    boreline_routing_clear();
    boreline_routing_add("a.boreline.test", 1, "127.0.0.1", 3000,
                          BORELINE_SERVICE_HTTP);
    int rc = boreline_routing_add("a.boreline.test", 2, "127.0.0.1", 3001,
                                   BORELINE_SERVICE_HTTP);
    ASSERT(rc == BORELINE_ERR_EXISTS, "conflicting hostname rejected");
}

static void test_routing_lookup_missing(void)
{
    boreline_routing_clear();
    uint32_t session_id = 0;
    int rc = boreline_routing_lookup("nope.boreline.test", &session_id,
                                      NULL, NULL);
    ASSERT(rc == BORELINE_ERR_NOTFOUND, "missing route returns NOTFOUND");
}

static void test_routing_remove(void)
{
    boreline_routing_clear();
    boreline_routing_add("b.boreline.test", 1, "127.0.0.1", 3000,
                          BORELINE_SERVICE_HTTP);
    int rc = boreline_routing_remove("b.boreline.test");
    ASSERT(rc == BORELINE_OK, "remove route");

    uint32_t session_id = 0;
    rc = boreline_routing_lookup("b.boreline.test", &session_id, NULL, NULL);
    ASSERT(rc == BORELINE_ERR_NOTFOUND, "removed route gone");
}

static void test_routing_remove_session(void)
{
    boreline_routing_clear();
    boreline_routing_add("c1.boreline.test", 100, "127.0.0.1", 3000,
                          BORELINE_SERVICE_HTTP);
    boreline_routing_add("c2.boreline.test", 100, "127.0.0.1", 3001,
                          BORELINE_SERVICE_HTTP);
    int rc = boreline_routing_remove_session(100);
    ASSERT(rc == BORELINE_OK, "remove session routes");

    uint32_t session_id = 0;
    rc = boreline_routing_lookup("c1.boreline.test", &session_id, NULL, NULL);
    ASSERT(rc == BORELINE_ERR_NOTFOUND, "session route c1 gone");
    rc = boreline_routing_lookup("c2.boreline.test", &session_id, NULL, NULL);
    ASSERT(rc == BORELINE_ERR_NOTFOUND, "session route c2 gone");
}

static void test_routing_invalid_input(void)
{
    boreline_routing_clear();
    int rc = boreline_routing_add(NULL, 1, "127.0.0.1", 3000,
                                   BORELINE_SERVICE_HTTP);
    ASSERT(rc == BORELINE_ERR_INVAL, "null hostname rejected");

    rc = boreline_routing_add("", 1, "127.0.0.1", 3000,
                               BORELINE_SERVICE_HTTP);
    ASSERT(rc == BORELINE_ERR_INVAL, "empty hostname rejected");

    uint32_t session_id;
    rc = boreline_routing_lookup(NULL, &session_id, NULL, NULL);
    ASSERT(rc == BORELINE_ERR_INVAL, "null lookup rejected");
}

/* ---- session tests ---- */

static void test_session_lifecycle(void)
{
    boreline_session_t *s = NULL;
    int rc = boreline_session_create(1, 64 * 1024 * 1024, &s);
    ASSERT(rc == BORELINE_OK, "create session");
    ASSERT(s != NULL, "session non-null");
    ASSERT(boreline_session_id(s) == 1, "session id");
    ASSERT(!boreline_session_is_authenticated(s), "not authenticated");

    rc = boreline_session_authenticate(s, "abcdef0123456789");
    ASSERT(rc == BORELINE_OK, "authenticate session");
    ASSERT(boreline_session_is_authenticated(s), "authenticated");

    boreline_session_destroy(s);
}

/* ---- metrics tests ---- */

static void test_metrics(void)
{
    boreline_metrics_init();
    char buf[512];

    boreline_metrics_record_connection();
    boreline_metrics_record_connection();
    boreline_metrics_disconnect();
    boreline_metrics_record_auth_failure();
    boreline_metrics_record_protocol_error();

    boreline_metrics_json(buf, sizeof(buf));
    ASSERT(strstr(buf, "\"active_sessions\":1") != NULL,
           "metrics active sessions");
    ASSERT(strstr(buf, "\"total_sessions\":2") != NULL,
           "metrics total sessions");
    ASSERT(strstr(buf, "\"auth_failures\":1") != NULL,
           "metrics auth failures");
    ASSERT(strstr(buf, "\"protocol_errors\":1") != NULL,
           "metrics protocol errors");
}

/* ---- loopback TCP echo pipeline ---- */

static volatile int echo_running = 1;

static void *echo_server(void *arg)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return NULL;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0); /* ephemeral */

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listen_fd);
        return NULL;
    }
    listen(listen_fd, 16);

    /* Client will read the port from the shared argument */
    socklen_t alen = sizeof(addr);
    getsockname(listen_fd, (struct sockaddr *)&addr, &alen);
    *(uint16_t *)arg = ntohs(addr.sin_port);

    while (echo_running) {
        int cfd = accept(listen_fd, NULL, NULL);
        if (cfd < 0) continue;

        char buf[4096];
        ssize_t n;
        while ((n = read(cfd, buf, sizeof(buf))) > 0) {
            ssize_t off = 0;
            while (off < n) {
                ssize_t w = write(cfd, buf + off, (size_t)(n - off));
                if (w <= 0) break;
                off += w;
            }
        }
        close(cfd);
    }
    close(listen_fd);
    return NULL;
}

static int connect_tcp(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void test_loopback_echo(void)
{
    uint16_t port = 0;
    pthread_t tid;
    if (pthread_create(&tid, NULL, echo_server, &port) != 0) {
        ASSERT(0, "create echo thread");
        return;
    }

    /* Wait for the port to be assigned */
    int attempts = 0;
    while (port == 0 && attempts++ < 100)
        usleep(10000);
    ASSERT(port != 0, "echo server got a port");

    int fd = connect_tcp(port);
    ASSERT(fd >= 0, "connect to echo server");

    const char *msg = "hello via tunnel pipeline";
    ssize_t n = write(fd, msg, strlen(msg));
    ASSERT(n == (ssize_t)strlen(msg), "write to echo");

    char buf[128] = {0};
    n = read(fd, buf, sizeof(buf) - 1);
    ASSERT(n == (ssize_t)strlen(msg), "read echo length");
    ASSERT(strcmp(buf, msg) == 0, "echo content matches");

    close(fd);
    echo_running = 0;
    pthread_join(tid, NULL);
    echo_running = 1;
}

int main(void)
{
    boreline_init();

    test_routing_add_lookup();
    test_routing_conflict();
    test_routing_lookup_missing();
    test_routing_remove();
    test_routing_remove_session();
    test_routing_invalid_input();
    test_session_lifecycle();
    test_metrics();
    test_loopback_echo();

    boreline_cleanup();

    fprintf(stdout, "\nE2E tests: %d passed, %d failed, %d total\n",
            passed, failed, ran);
    return failed > 0 ? 1 : 0;
}