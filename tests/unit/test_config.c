#include "boreline/boreline.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Unit tests for configuration parser */

static int passed = 0, failed = 0, ran = 0;

#define ASSERT(cond, msg) do { \
    ran++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failed++; \
    } else \
        passed++; \
} while (0)

static void test_server_defaults(void)
{
    boreline_server_config_t cfg;
    boreline_config_defaults_server(&cfg);
    ASSERT(strcmp(cfg.public_bind, "0.0.0.0") == 0, "default public_bind");
    ASSERT(cfg.public_port == 443, "default public_port");
    ASSERT(strcmp(cfg.tunnel_bind, "0.0.0.0") == 0, "default tunnel_bind");
    ASSERT(cfg.tunnel_port == 8443, "default tunnel_port");
    ASSERT(cfg.max_sessions == BORELINE_MAX_SESSIONS, "default max_sessions");
    ASSERT(cfg.max_streams_per_session == 256, "default max_streams_per_session");
    ASSERT(cfg.request_timeout_ms == 30000, "default request_timeout");
    ASSERT(cfg.idle_timeout_ms == 60000, "default idle_timeout");
    ASSERT(strcmp(cfg.log_level, "info") == 0, "default log_level");
    ASSERT(strcmp(cfg.metrics_bind, "127.0.0.1") == 0, "default metrics_bind");
    ASSERT(cfg.metrics_port == 9090, "default metrics_port");
}

static void test_client_defaults(void)
{
    boreline_client_config_t cfg;
    boreline_config_defaults_client(&cfg);
    ASSERT(strcmp(cfg.server_address, "127.0.0.1") == 0, "default server_address");
    ASSERT(cfg.server_port == 8443, "default server_port");
    ASSERT(cfg.reconnect_base_ms == 500, "default reconnect_base");
    ASSERT(cfg.reconnect_max_ms == 30000, "default reconnect_max");
}

static void test_parse_server_config(void)
{
    /* Write a test config */
    const char *path = "/tmp/boreline_test_server.toml";
    FILE *f = fopen(path, "w");
    ASSERT(f != NULL, "open test config file");

    fprintf(f, "[boreline]\n");
    fprintf(f, "schema_version = 1\n");
    fprintf(f, "\n");
    fprintf(f, "[server]\n");
    fprintf(f, "public_bind = \"0.0.0.0\"\n");
    fprintf(f, "public_port = 8443\n");
    fprintf(f, "tunnel_bind = \"127.0.0.1\"\n");
    fprintf(f, "tunnel_port = 9443\n");
    fprintf(f, "domain = \"boreline.example.com\"\n");
    fprintf(f, "tls_certificate = \"/etc/boreline/cert.pem\"\n");
    fprintf(f, "tls_key = \"/etc/boreline/key.pem\"\n");
    fprintf(f, "token_store = \"/etc/boreline/tokens\"\n");
    fprintf(f, "max_sessions = 512\n");
    fprintf(f, "max_streams_per_session = 64\n");
    fprintf(f, "max_memory_per_session = 134217728\n");
    fprintf(f, "request_timeout_ms = 15000\n");
    fprintf(f, "idle_timeout_ms = 30000\n");
    fprintf(f, "log_level = \"debug\"\n");
    fprintf(f, "metrics_bind = \"127.0.0.1\"\n");
    fprintf(f, "metrics_port = 9091\n");
    fclose(f);

    boreline_server_config_t cfg;
    int rc = boreline_config_parse_server(path, &cfg);
    ASSERT(rc == BORELINE_OK, "parse server config");

    ASSERT(strcmp(cfg.public_bind, "0.0.0.0") == 0, "parsed public_bind");
    ASSERT(cfg.public_port == 8443, "parsed public_port");
    ASSERT(strcmp(cfg.tunnel_bind, "127.0.0.1") == 0, "parsed tunnel_bind");
    ASSERT(cfg.tunnel_port == 9443, "parsed tunnel_port");
    ASSERT(strcmp(cfg.domain, "boreline.example.com") == 0, "parsed domain");
    ASSERT(strcmp(cfg.tls_certificate, "/etc/boreline/cert.pem") == 0, "parsed cert");
    ASSERT(strcmp(cfg.tls_key, "/etc/boreline/key.pem") == 0, "parsed key");
    ASSERT(cfg.max_sessions == 512, "parsed max_sessions");
    ASSERT(cfg.max_streams_per_session == 64, "parsed max_streams");
    ASSERT(cfg.max_memory_per_session == 134217728, "parsed max_memory");
    ASSERT(cfg.request_timeout_ms == 15000, "parsed request_timeout");
    ASSERT(strcmp(cfg.log_level, "debug") == 0, "parsed log_level");

    remove(path);
}

static void test_parse_client_config(void)
{
    const char *path = "/tmp/boreline_test_client.toml";
    FILE *f = fopen(path, "w");
    fprintf(f, "[boreline]\n");
    fprintf(f, "schema_version = 1\n");
    fprintf(f, "\n");
    fprintf(f, "[client]\n");
    fprintf(f, "server_address = \"tunnel.example.com\"\n");
    fprintf(f, "server_port = 8443\n");
    fprintf(f, "certificate_trust = \"/etc/ssl/certs/ca-certificates.crt\"\n");
    fprintf(f, "token_source = \"/etc/boreline/client.token\"\n");
    fprintf(f, "reconnect_base_ms = 1000\n");
    fprintf(f, "reconnect_max_ms = 60000\n");
    fprintf(f, "max_local_connections = 512\n");
    fclose(f);

    boreline_client_config_t cfg;
    int rc = boreline_config_parse_client(path, &cfg);
    ASSERT(rc == BORELINE_OK, "parse client config");

    ASSERT(strcmp(cfg.server_address, "tunnel.example.com") == 0, "parsed server_address");
    ASSERT(cfg.server_port == 8443, "parsed server_port");
    ASSERT(strcmp(cfg.certificate_trust, "/etc/ssl/certs/ca-certificates.crt") == 0,
           "parsed cert trust");
    ASSERT(cfg.reconnect_base_ms == 1000, "parsed reconnect_base");
    ASSERT(cfg.reconnect_max_ms == 60000, "parsed reconnect_max");
    ASSERT(cfg.max_local_connections == 512, "parsed max_local_connections");

    remove(path);
}

static void test_config_file_not_found(void)
{
    boreline_server_config_t cfg;
    int rc = boreline_config_parse_server("/nonexistent/path.toml", &cfg);
    ASSERT(rc == BORELINE_ERR_NOTFOUND, "missing config file returns NOTFOUND");
}

static void test_config_unknown_key(void)
{
    const char *path = "/tmp/boreline_bad_key.toml";
    FILE *f = fopen(path, "w");
    fprintf(f, "[boreline]\n");
    fprintf(f, "schema_version = 1\n");
    fprintf(f, "[server]\n");
    fprintf(f, "nonexistent_key = \"value\"\n");
    fclose(f);

    boreline_server_config_t cfg;
    int rc = boreline_config_parse_server(path, &cfg);
    ASSERT(rc == BORELINE_ERR_INVAL, "unknown key rejected");

    remove(path);
}

static void test_config_missing_schema_version(void)
{
    const char *path = "/tmp/boreline_no_schema.toml";
    FILE *f = fopen(path, "w");
    fprintf(f, "[server]\n");
    fprintf(f, "public_bind = \"0.0.0.0\"\n");
    fclose(f);

    boreline_server_config_t cfg;
    int rc = boreline_config_parse_server(path, &cfg);
    /* Missing [boreline] section before [server] should fail */
    ASSERT(rc == BORELINE_ERR_INVAL, "missing schema version rejected");

    remove(path);
}

static void test_config_string_escape(void)
{
    boreline_server_config_t cfg;
    boreline_config_defaults_server(&cfg);

    const char *path = "/tmp/boreline_esc.toml";
    FILE *f = fopen(path, "w");
    fprintf(f, "[boreline]\n");
    fprintf(f, "schema_version = 1\n");
    fprintf(f, "[server]\n");
    fprintf(f, "domain = \"boreline.\\\"test\\\".com\"\n");
    fclose(f);

    int rc = boreline_config_parse_server(path, &cfg);
    ASSERT(rc == BORELINE_OK, "parse with escaped quotes");
    ASSERT(strcmp(cfg.domain, "boreline.\"test\".com") == 0,
           "domain with escaped quotes");

    remove(path);
}

int main(void)
{
    test_server_defaults();
    test_client_defaults();
    test_parse_server_config();
    test_parse_client_config();
    test_config_file_not_found();
    test_config_unknown_key();
    test_config_missing_schema_version();
    test_config_string_escape();

    fprintf(stdout, "\nConfig tests: %d passed, %d failed, %d total\n",
            passed, failed, ran);
    return failed > 0 ? 1 : 0;
}