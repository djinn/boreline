#include "boreline/boreline.h"
#include "server.h"

/* Minimal admin/metrics endpoint (loopback only by default).
 * Exposes structured JSON status, session counts, and error counters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t active_sessions;
    uint64_t total_sessions;
    uint64_t active_streams;
    uint64_t bytes_forwarded;
    uint64_t auth_failures;
    uint64_t protocol_errors;
    uint64_t start_time;
} boreline_metrics_t;

static boreline_metrics_t metrics;

void boreline_metrics_init(void)
{
    memset(&metrics, 0, sizeof(metrics));
}

void boreline_metrics_record_auth_failure(void)
{
    metrics.auth_failures++;
}

void boreline_metrics_record_protocol_error(void)
{
    metrics.protocol_errors++;
}

void boreline_metrics_record_connection(void)
{
    metrics.active_sessions++;
    metrics.total_sessions++;
}

void boreline_metrics_disconnect(void)
{
    if (metrics.active_sessions > 0)
        metrics.active_sessions--;
}

char *boreline_metrics_json(char *buf, size_t len)
{
    snprintf(buf, len,
             "{\"active_sessions\":%lu,\"total_sessions\":%lu,"
             "\"active_streams\":%lu,\"bytes_forwarded\":%lu,"
             "\"auth_failures\":%lu,\"protocol_errors\":%lu}",
             metrics.active_sessions, metrics.total_sessions,
             metrics.active_streams, metrics.bytes_forwarded,
             metrics.auth_failures, metrics.protocol_errors);
    return buf;
}