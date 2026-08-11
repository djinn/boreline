#include "boreline/boreline.h"
#include "server.h"

/* Routing table maps hostnames to service registrations.
 *
 * This ensures that inbound HTTP Host and SNI are resolved to an
 * authenticated client tunnel and its registered origin.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROUTES 4096

typedef struct {
    char hostname[256];
    uint32_t session_id;
    char local_host[128];
    uint16_t local_port;
    boreline_service_type_t type;
    bool active;
} boreline_route_t;

static boreline_route_t routes[MAX_ROUTES];
static int route_count = 0;

int boreline_routing_add(const char *hostname, uint32_t session_id,
                          const char *local_host, uint16_t local_port,
                          boreline_service_type_t type)
{
    if (hostname == NULL || *hostname == '\0')
        return BORELINE_ERR_INVAL;
    if (route_count >= MAX_ROUTES)
        return BORELINE_ERR_LIMIT;

    /* Check for conflict */
    for (int i = 0; i < route_count; i++) {
        if (strcmp(routes[i].hostname, hostname) == 0 && routes[i].active)
            return BORELINE_ERR_EXISTS;
    }

    routes[route_count].active = true;
    routes[route_count].session_id = session_id;
    routes[route_count].type = type;
    strncpy(routes[route_count].hostname, hostname,
            sizeof(routes[route_count].hostname) - 1);
    strncpy(routes[route_count].local_host, local_host,
            sizeof(routes[route_count].local_host) - 1);
    routes[route_count].local_port = local_port;
    route_count++;
    return BORELINE_OK;
}

int boreline_routing_remove(const char *hostname)
{
    for (int i = 0; i < route_count; i++) {
        if (strcmp(routes[i].hostname, hostname) == 0) {
            routes[i].active = false;
            return BORELINE_OK;
        }
    }
    return BORELINE_ERR_NOTFOUND;
}

int boreline_routing_lookup(const char *hostname,
                             uint32_t *session_id,
                             char *local_host,
                             uint16_t *local_port)
{
    if (hostname == NULL || session_id == NULL)
        return BORELINE_ERR_INVAL;

    for (int i = 0; i < route_count; i++) {
        if (strcmp(routes[i].hostname, hostname) == 0 && routes[i].active) {
            *session_id = routes[i].session_id;
            if (local_host)
                strncpy(local_host, routes[i].local_host, 127);
            if (local_port)
                *local_port = routes[i].local_port;
            return BORELINE_OK;
        }
    }
    return BORELINE_ERR_NOTFOUND;
}

int boreline_routing_remove_session(uint32_t session_id)
{
    for (int i = 0; i < route_count; i++) {
        if (routes[i].session_id == session_id)
            routes[i].active = false;
    }
    return BORELINE_OK;
}

void boreline_routing_clear(void)
{
    for (int i = 0; i < route_count; i++)
        routes[i].active = false;
    route_count = 0;
}