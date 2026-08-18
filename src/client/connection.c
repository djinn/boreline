#include "boreline/boreline.h"

/* Tunnel client connection state:
 * Managed the outbound TLS 1.3 connection to the server,
 * authentication and reconnection with exponential backoff + jitter.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fd;
    bool connected;
    uint32_t reconnect_attempts;
    char server_address[256];
    uint16_t server_port;
} boreline_client_conn_t;

/* Exponential backoff with full jitter.
 * Returns the delay in milliseconds to wait before the next reconnect.
 */
uint32_t boreline_reconnect_delay(uint32_t attempt,
                                   uint32_t base_ms,
                                   uint32_t max_ms,
                                   const uint8_t *random_bytes, size_t n)
{
    if (attempt == 0)
        return 0;

    uint32_t exp = base_ms;
    for (uint32_t i = 1; i < attempt; i++) {
        if (exp > max_ms / 2) {
            exp = max_ms;
            break;
        }
        exp *= 2;
    }
    if (exp > max_ms)
        exp = max_ms;

    if (exp <= 1)
        return 1;

    /* Full jitter: pick uniformly in [0, exp) */
    uint32_t jitter = 0;
    if (random_bytes && n > 0) {
        for (size_t i = 0; i < n; i++)
            jitter = (jitter << 8) | random_bytes[i];
    } else {
        jitter = (uint32_t)(rand() ^ (rand() << 16));
    }
    return 1 + (jitter % exp);
}