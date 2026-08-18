#include "boreline/boreline.h"

/* Tunnel service handling:
 * For each OPEN frame, connect to the configured loopback origin
 * and relay data bidirectionally. Never listens publicly.
 */

#include <stdio.h>
#include <string.h>

struct boreline_service_reg {
    char name[256];
    boreline_service_type_t type;
    char local_host[128];
    uint16_t local_port;
};

int boreline_service_register(boreline_service_reg_t *reg,
                               const char *name,
                               boreline_service_type_t type,
                               const char *local_host,
                               uint16_t local_port)
{
    if (reg == NULL || name == NULL || local_host == NULL)
        return BORELINE_ERR_INVAL;
    if (strlen(name) >= sizeof(reg->name))
        return BORELINE_ERR_OVERFLOW;
    if (strlen(local_host) >= sizeof(reg->local_host))
        return BORELINE_ERR_OVERFLOW;

    strcpy(reg->name, name);
    reg->type = type;
    strcpy(reg->local_host, local_host);
    reg->local_port = local_port;
    return BORELINE_OK;
}