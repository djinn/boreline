#include "boreline/boreline.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal TOML-subset configuration parser.
 *
 * Supports the documented subset: [section] headers, key = "value",
 * key = 123, key = true/false, and # comments. Keys are case-sensitive.
 * The schema is versioned via the "schema_version" field in the
 * [boreline] section. Unknown keys are rejected (fail closed).
 *
 * This intentionally avoids pulling in a full TOML dependency for the
 * small subset the project documents.
 */

static bool is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *trim(char *s)
{
    while (*s && is_ws(*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && is_ws(end[-1]))
        end--;
    *end = '\0';
    return s;
}

/* Parse a quoted string value. Returns 0 on success, -1 on error. */
static int parse_string(const char *in, char *out, size_t out_size)
{
    in = trim((char *)in);
    if (*in != '"')
        return -1;
    in++;
    size_t n = 0;
    while (*in && *in != '"') {
        if (n + 1 >= out_size)
            return -1;
        if (*in == '\\') {
            in++;
            switch (*in) {
            case 'n': out[n++] = '\n'; break;
            case 't': out[n++] = '\t'; break;
            case 'r': out[n++] = '\r'; break;
            case '"': out[n++] = '"';  break;
            case '\\': out[n++] = '\\'; break;
            default: return -1;
            }
            in++;
        } else {
            out[n++] = *in++;
        }
    }
    if (*in != '"')
        return -1;
    out[n] = '\0';
    return 0;
}

static int parse_u32(const char *in, uint32_t *out)
{
    char *end;
    unsigned long v = strtoul(in, &end, 10);
    if (end == in || *trim(end) != '\0')
        return -1;
    if (v > 0xFFFFFFFFUL)
        return -1;
    *out = (uint32_t)v;
    return 0;
}

static int parse_u64(const char *in, uint64_t *out)
{
    char *end;
    unsigned long long v = strtoull(in, &end, 10);
    if (end == in || *trim(end) != '\0')
        return -1;
    *out = v;
    return 0;
}

/* Parse a port number (16-bit). */
static int parse_port(const char *in, uint16_t *out)
{
    uint32_t v;
    if (parse_u32(in, &v) != 0 || v > 65535)
        return -1;
    *out = (uint16_t)v;
    return 0;
}

typedef enum {
    SECTION_NONE,
    SECTION_BORELINE,
    SECTION_SERVER,
    SECTION_CLIENT,
} section_t;

static int set_server_key(boreline_server_config_t *cfg,
                          const char *key, const char *value)
{
    if (strcmp(key, "public_bind") == 0)
        return parse_string(value, cfg->public_bind, sizeof(cfg->public_bind));
    if (strcmp(key, "public_port") == 0)
        return parse_port(value, &cfg->public_port);
    if (strcmp(key, "tunnel_bind") == 0)
        return parse_string(value, cfg->tunnel_bind, sizeof(cfg->tunnel_bind));
    if (strcmp(key, "tunnel_port") == 0)
        return parse_port(value, &cfg->tunnel_port);
    if (strcmp(key, "domain") == 0)
        return parse_string(value, cfg->domain, sizeof(cfg->domain));
    if (strcmp(key, "tls_certificate") == 0)
        return parse_string(value, cfg->tls_certificate, sizeof(cfg->tls_certificate));
    if (strcmp(key, "tls_key") == 0)
        return parse_string(value, cfg->tls_key, sizeof(cfg->tls_key));
    if (strcmp(key, "token_store") == 0)
        return parse_string(value, cfg->token_store, sizeof(cfg->token_store));
    if (strcmp(key, "allowed_public_tcp_ports") == 0)
        return parse_string(value, cfg->allowed_public_tcp_ports,
                            sizeof(cfg->allowed_public_tcp_ports));
    if (strcmp(key, "max_sessions") == 0)
        return parse_u32(value, &cfg->max_sessions);
    if (strcmp(key, "max_streams_per_session") == 0)
        return parse_u32(value, &cfg->max_streams_per_session);
    if (strcmp(key, "max_memory_per_session") == 0)
        return parse_u64(value, &cfg->max_memory_per_session);
    if (strcmp(key, "request_timeout_ms") == 0)
        return parse_u32(value, &cfg->request_timeout_ms);
    if (strcmp(key, "idle_timeout_ms") == 0)
        return parse_u32(value, &cfg->idle_timeout_ms);
    if (strcmp(key, "log_level") == 0)
        return parse_string(value, cfg->log_level, sizeof(cfg->log_level));
    if (strcmp(key, "metrics_bind") == 0)
        return parse_string(value, cfg->metrics_bind, sizeof(cfg->metrics_bind));
    if (strcmp(key, "metrics_port") == 0)
        return parse_port(value, &cfg->metrics_port);
    return BORELINE_ERR_INVAL; /* unknown key */
}

static int set_client_key(boreline_client_config_t *cfg,
                          const char *key, const char *value)
{
    if (strcmp(key, "server_address") == 0)
        return parse_string(value, cfg->server_address, sizeof(cfg->server_address));
    if (strcmp(key, "server_port") == 0)
        return parse_port(value, &cfg->server_port);
    if (strcmp(key, "certificate_trust") == 0)
        return parse_string(value, cfg->certificate_trust, sizeof(cfg->certificate_trust));
    if (strcmp(key, "token_source") == 0)
        return parse_string(value, cfg->token_source, sizeof(cfg->token_source));
    if (strcmp(key, "reconnect_base_ms") == 0)
        return parse_u32(value, &cfg->reconnect_base_ms);
    if (strcmp(key, "reconnect_max_ms") == 0)
        return parse_u32(value, &cfg->reconnect_max_ms);
    if (strcmp(key, "max_local_connections") == 0)
        return parse_u32(value, &cfg->max_local_connections);
    return BORELINE_ERR_INVAL;
}

void boreline_config_defaults_server(boreline_server_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->public_bind, "0.0.0.0");
    cfg->public_port = 443;
    strcpy(cfg->tunnel_bind, "0.0.0.0");
    cfg->tunnel_port = 8443;
    strcpy(cfg->domain, "tunnel.example.com");
    cfg->max_sessions = BORELINE_MAX_SESSIONS;
    cfg->max_streams_per_session = 256;
    cfg->max_memory_per_session = 64 * 1024 * 1024;
    cfg->request_timeout_ms = 30000;
    cfg->idle_timeout_ms = 60000;
    strcpy(cfg->log_level, "info");
    strcpy(cfg->metrics_bind, "127.0.0.1");
    cfg->metrics_port = 9090;
}

void boreline_config_defaults_client(boreline_client_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->server_address, "127.0.0.1");
    cfg->server_port = 8443;
    cfg->reconnect_base_ms = 500;
    cfg->reconnect_max_ms = 30000;
    cfg->max_local_connections = 1024;
}

/* Generic TOML-subset file parser shared by server and client. */
static int parse_file(const char *path, section_t *section,
                      int (*set_key)(void *cfg, const char *, const char *),
                      void *cfg)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return BORELINE_ERR_NOTFOUND;

    char line[1024];
    int rc = BORELINE_OK;
    int lineno = 0;
    bool saw_boreline_section = false;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *s = trim(line);

        /* Strip comments (outside strings). */
        {
            bool in_str = false;
            for (char *p = s; *p; p++) {
                if (*p == '"')
                    in_str = !in_str;
                else if (*p == '#' && !in_str) {
                    *p = '\0';
                    break;
                }
            }
            s = trim(s);
        }

        if (*s == '\0')
            continue;

        /* Section header */
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end == NULL || trim(end + 1)[0] != '\0') {
                rc = BORELINE_ERR_INVAL;
                break;
            }
            *end = '\0';
            if (strcmp(s + 1, "boreline") == 0) {
                *section = SECTION_BORELINE;
                saw_boreline_section = true;
            } else if (strcmp(s + 1, "server") == 0) {
                if (!saw_boreline_section) {
                    rc = BORELINE_ERR_INVAL;
                    break;
                }
                *section = SECTION_SERVER;
            } else if (strcmp(s + 1, "client") == 0) {
                if (!saw_boreline_section) {
                    rc = BORELINE_ERR_INVAL;
                    break;
                }
                *section = SECTION_CLIENT;
            } else {
                rc = BORELINE_ERR_INVAL;
                break;
            }
            continue;
        }

        /* key = value */
        char *eq = strchr(s, '=');
        if (eq == NULL) {
            rc = BORELINE_ERR_INVAL;
            break;
        }
        *eq = '\0';
        char *key = trim(s);
        char *value = trim(eq + 1);
        if (*key == '\0' || *value == '\0') {
            rc = BORELINE_ERR_INVAL;
            break;
        }

        switch (*section) {
        case SECTION_BORELINE:
            if (strcmp(key, "schema_version") == 0) {
                uint32_t v;
                if (parse_u32(value, &v) != 0 || v != 1) {
                    rc = BORELINE_ERR_INVAL;
                }
            } else {
                rc = BORELINE_ERR_INVAL;
            }
            break;
        case SECTION_SERVER:
            rc = set_key(cfg, key, value);
            break;
        case SECTION_CLIENT:
            rc = set_key(cfg, key, value);
            break;
        default:
            rc = BORELINE_ERR_INVAL;
            break;
        }
        if (rc != BORELINE_OK)
            break;

        (void)lineno;
    }

    fclose(f);
    return rc;
}

int boreline_config_parse_server(const char *path,
                                  boreline_server_config_t *cfg)
{
    if (path == NULL || cfg == NULL)
        return BORELINE_ERR_INVAL;
    boreline_config_defaults_server(cfg);
    section_t section = SECTION_NONE;
    return parse_file(path, &section, (int (*)(void *, const char *, const char *))set_server_key, cfg);
}

int boreline_config_parse_client(const char *path,
                                  boreline_client_config_t *cfg)
{
    if (path == NULL || cfg == NULL)
        return BORELINE_ERR_INVAL;
    boreline_config_defaults_client(cfg);
    section_t section = SECTION_NONE;
    return parse_file(path, &section, (int (*)(void *, const char *, const char *))set_client_key, cfg);
}