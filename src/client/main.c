#include "boreline/boreline.h"

/* boreline client - main entry point
 *
 * Supported commands:
 *   boreline server --config <path>
 *   boreline http <port> --name <name>
 *   boreline tcp <port> --name <name>
 *   boreline status
 *   boreline version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <command> [options]\n"
            "\n"
            "Commands:\n"
            "  server --config <path>     Connect to tunnel server\n"
            "  http <port> [--name <name>] Expose a local HTTP service\n"
            "  tcp <port> [--name <name>]  Expose a local TCP service\n"
            "  status                      Show tunnel status\n"
            "  version                     Print version and exit\n",
            prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "version") == 0) {
        printf("boreline %s\n", BORELINE_VERSION);
        return 0;
    }

    if (strcmp(cmd, "status") == 0) {
        printf("{\n");
        printf("  \"version\": \"%s\",\n", BORELINE_VERSION);
        printf("  \"connected\": false,\n");
        printf("  \"services\": []\n");
        printf("}\n");
        return 0;
    }

    if (strcmp(cmd, "server") == 0) {
        const char *config_path = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
                config_path = argv[++i];
        }

        boreline_client_config_t cfg;
        if (config_path) {
            int rc = boreline_config_parse_client(config_path, &cfg);
            if (rc != BORELINE_OK) {
                fprintf(stderr, "Configuration error: %s\n",
                        boreline_error_str(rc));
                return 1;
            }
        } else {
            boreline_config_defaults_client(&cfg);
        }

        BORELINE_LOG_INFO("boreline client %s starting", BORELINE_VERSION);
        BORELINE_LOG_INFO("Connecting to %s:%u",
                          cfg.server_address, cfg.server_port);

        fprintf(stderr, "Server mode not fully implemented in this build.\n");
        fprintf(stderr, "Would connect to %s:%u\n",
                cfg.server_address, cfg.server_port);
        return 0;
    }

    if (strcmp(cmd, "http") == 0 || strcmp(cmd, "tcp") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: boreline %s <port> [--name <name>]\n", cmd);
            return 1;
        }

        uint16_t port = (uint16_t)atoi(argv[2]);
        const char *name = NULL;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--name") == 0 && i + 1 < argc)
                name = argv[++i];
        }

        printf("Registered %s service on port %u", cmd, port);
        if (name)
            printf(" as '%s'\n", name);
        printf("\n");
        printf("Would forward https://%s.tunnel.example.com -> 127.0.0.1:%u\n",
               name ? name : "service", port);
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage(argv[0]);
    return 1;
}