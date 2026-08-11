#include "server.h"

/* Session manager tracks authenticated client connections.
 *
 * Each session has:
 * - A unique session ID
 * - Authentication token reference
 * - Registered services (hostname -> local origin mapping)
 * - Active streams
 * - Memory and stream budgets
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct boreline_session {
    uint32_t id;
    bool     authenticated;
    char     token_hash[64];
    uint32_t stream_count;
    uint64_t memory_used;
    uint64_t memory_limit;
};

int boreline_session_create(uint32_t id, uint64_t memory_limit,
                             boreline_session_t **out)
{
    if (out == NULL)
        return BORELINE_ERR_INVAL;
    boreline_session_t *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return BORELINE_ERR_NOMEM;
    s->id = id;
    s->memory_limit = memory_limit;
    *out = s;
    return BORELINE_OK;
}

void boreline_session_destroy(boreline_session_t *s)
{
    free(s);
}

bool boreline_session_is_authenticated(const boreline_session_t *s)
{
    return s && s->authenticated;
}

int boreline_session_authenticate(boreline_session_t *s, const char *token_hash)
{
    if (s == NULL || token_hash == NULL)
        return BORELINE_ERR_INVAL;
    s->authenticated = true;
    strncpy(s->token_hash, token_hash, sizeof(s->token_hash) - 1);
    return BORELINE_OK;
}

uint32_t boreline_session_id(const boreline_session_t *s)
{
    return s ? s->id : 0;
}