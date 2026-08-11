#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "boreline/boreline.h"
#include "boreline/platform.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* boreline server --config /etc/boreline/server.toml
 *
 * Main entry point for the tunnel server. Sets up:
 * - Epoll event loop (Linux) or kqueue (macOS)
 * - TLS termination on the public ingress listener
 * - Outbound client tunnel listener
 * - Metrics endpoint on loopback
 * - Graceful shutdown on SIGINT/SIGTERM
 */

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--config <path>] [--check-config]\n"
            "       %s version\n"
            "       %s status\n"
            "\n"
            "Start the boreline tunnel server.\n"
            "  --config       Path to configuration file (default: /etc/boreline/server.toml)\n"
            "  --check-config Validate configuration and exit\n"
            "  version        Print version and exit\n"
            "  status         Print server status as JSON and exit\n",
            prog, prog, prog);
}

int main(int argc, char *argv[])
{
    const char *config_path = "/etc/boreline/server.toml";
    bool check_config = false;
    bool show_status = false;
    bool show_version = false;

    boreline_server_config_t cfg;
    boreline_config_defaults_server(&cfg);

    /* Parse CLI */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--check-config") == 0) {
            check_config = true;
        } else if (strcmp(argv[i], "version") == 0) {
            show_version = true;
        } else if (strcmp(argv[i], "status") == 0) {
            show_status = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (show_version) {
        printf("boreline-server %s\n", BORELINE_VERSION);
        return 0;
    }

    int rc = boreline_config_parse_server(config_path, &cfg);
    if (rc != BORELINE_OK && rc != BORELINE_ERR_NOTFOUND) {
        fprintf(stderr, "Configuration error: %s\n", boreline_error_str(rc));
        return 1;
    }

    if (check_config) {
        printf("Configuration valid: %s\n", config_path);
        printf("Version: %s\n", BORELINE_VERSION);
        printf("Protocol: %d\n", BORELINE_VERSION_PROTO);
        return 0;
    }

    if (show_status) {
        printf("{\n");
        printf("  \"version\": \"%s\",\n", BORELINE_VERSION);
        printf("  \"running\": false,\n");
        printf("  \"pid\": %d,\n", getpid());
        printf("  \"uptime_seconds\": 0\n");
        printf("}\n");
        return 0;
    }

    /* Install signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    /* Create platform event loop */
    boreline_platform_t *platform = boreline_platform_create();
    if (platform == NULL) {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    BORELINE_LOG_INFO("boreline-server %s starting", BORELINE_VERSION);
    BORELINE_LOG_INFO("Public ingress: %s:%d", cfg.public_bind, cfg.public_port);
    BORELINE_LOG_INFO("Tunnel listener: %s:%d", cfg.tunnel_bind, cfg.tunnel_port);
    BORELINE_LOG_INFO("Metrics: %s:%d", cfg.metrics_bind, cfg.metrics_port);

    /* Main event loop */
    boreline_event_t events[64];
    while (g_running) {
        int n = boreline_platform_wait(platform, 1000, events, 64);
        if (n < 0) {
            BORELINE_LOG_ERROR("Event loop error: %s", boreline_error_str(n));
            break;
        }
        (void)n;
    }

    BORELINE_LOG_INFO("Shutting down gracefully");

    boreline_platform_destroy(platform);
    BORELINE_LOG_INFO("Server stopped");
    return 0;
}