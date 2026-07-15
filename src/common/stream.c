#include "boreline/boreline.h"

#include <stdlib.h>
#include <string.h>

/* Stream state machine.
 *
 * A stream has a well-defined lifecycle:
 *
 *   IDLE -> OPEN -> HALF_CLOSED_LOCAL -> CLOSED
 *        \-> OPEN -> HALF_CLOSED_REMOTE -> CLOSED
 *        \-> OPEN -> CLOSED (both halves closed)
 *        \-> RESET (any state, with FIN or RST)
 *
 * Transitions are validated by boreline_stream_transition(). Invalid
 * transitions return BORELINE_ERR_PROTOCOL, which the caller must treat
 * as a connection-level protocol violation.
 */

struct boreline_stream {
    uint32_t id;
    boreline_stream_state_t state;
    bool     local_closed;
    bool     remote_closed;
    uint32_t send_window;   /* credit we may send */
    uint32_t recv_window;   /* credit remote may send to us */
    uint32_t recv_consumed; /* bytes consumed since last update */
    uint64_t bytes_sent;
    uint64_t bytes_received;
    void    *userdata;
};

boreline_stream_t *boreline_stream_create(uint32_t id,
                                           uint32_t initial_window)
{
    boreline_stream_t *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return NULL;

    s->id = id;
    s->state = BORELINE_STREAM_IDLE;
    s->send_window = initial_window;
    s->recv_window = initial_window;
    return s;
}

void boreline_stream_destroy(boreline_stream_t *s)
{
    free(s);
}

uint32_t boreline_stream_id(const boreline_stream_t *s)
{
    return s ? s->id : 0;
}

boreline_stream_state_t boreline_stream_state(const boreline_stream_t *s)
{
    return s ? s->state : BORELINE_STREAM_CLOSED;
}

int boreline_stream_transition(boreline_stream_t *s,
                                boreline_stream_state_t next)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;

    const boreline_stream_state_t cur = s->state;

    /* Terminal states */
    if (cur == BORELINE_STREAM_CLOSED || cur == BORELINE_STREAM_RESET) {
        if (cur == next)
            return BORELINE_OK;
        return BORELINE_ERR_PROTOCOL;
    }

    switch (cur) {
    case BORELINE_STREAM_IDLE:
        if (next == BORELINE_STREAM_OPEN)
            break;
        return BORELINE_ERR_PROTOCOL;
    case BORELINE_STREAM_OPEN:
        if (next == BORELINE_STREAM_HALF_CLOSED_LOCAL ||
            next == BORELINE_STREAM_HALF_CLOSED_REMOTE ||
            next == BORELINE_STREAM_CLOSED ||
            next == BORELINE_STREAM_RESET)
            break;
        return BORELINE_ERR_PROTOCOL;
    case BORELINE_STREAM_HALF_CLOSED_LOCAL:
        if (next == BORELINE_STREAM_HALF_CLOSED_REMOTE ||
            next == BORELINE_STREAM_CLOSED ||
            next == BORELINE_STREAM_RESET)
            break;
        return BORELINE_ERR_PROTOCOL;
    case BORELINE_STREAM_HALF_CLOSED_REMOTE:
        if (next == BORELINE_STREAM_HALF_CLOSED_LOCAL ||
            next == BORELINE_STREAM_CLOSED ||
            next == BORELINE_STREAM_RESET)
            break;
        return BORELINE_ERR_PROTOCOL;
    default:
        return BORELINE_ERR_INVAL;
    }

    s->state = next;
    return BORELINE_OK;
}

/* Mark the local sending half closed. */
int boreline_stream_half_close_local(boreline_stream_t *s)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;
    if (s->local_closed)
        return BORELINE_ERR_PROTOCOL;

    s->local_closed = true;
    if (s->remote_closed)
        return boreline_stream_transition(s, BORELINE_STREAM_CLOSED);
    if (s->state == BORELINE_STREAM_OPEN)
        return boreline_stream_transition(s, BORELINE_STREAM_HALF_CLOSED_LOCAL);
    if (s->state == BORELINE_STREAM_HALF_CLOSED_REMOTE)
        return boreline_stream_transition(s, BORELINE_STREAM_CLOSED);
    return BORELINE_OK;
}

/* Mark the remote sending half closed. */
int boreline_stream_half_close_remote(boreline_stream_t *s)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;
    if (s->remote_closed)
        return BORELINE_ERR_PROTOCOL;

    s->remote_closed = true;
    if (s->local_closed)
        return boreline_stream_transition(s, BORELINE_STREAM_CLOSED);
    if (s->state == BORELINE_STREAM_OPEN)
        return boreline_stream_transition(s, BORELINE_STREAM_HALF_CLOSED_REMOTE);
    if (s->state == BORELINE_STREAM_HALF_CLOSED_LOCAL)
        return boreline_stream_transition(s, BORELINE_STREAM_CLOSED);
    return BORELINE_OK;
}

/* Tear down the stream (RESET frame or local abort). */
int boreline_stream_reset(boreline_stream_t *s)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;
    s->local_closed = true;
    s->remote_closed = true;
    return boreline_stream_transition(s, BORELINE_STREAM_RESET);
}

bool boreline_stream_is_closed(const boreline_stream_t *s)
{
    if (s == NULL)
        return true;
    return s->state == BORELINE_STREAM_CLOSED ||
           s->state == BORELINE_STREAM_RESET;
}

bool boreline_stream_can_send(const boreline_stream_t *s)
{
    if (s == NULL)
        return false;
    if (s->local_closed)
        return false;
    return s->state == BORELINE_STREAM_OPEN ||
           s->state == BORELINE_STREAM_HALF_CLOSED_REMOTE;
}

bool boreline_stream_can_receive(const boreline_stream_t *s)
{
    if (s == NULL)
        return false;
    if (s->remote_closed)
        return false;
    return s->state == BORELINE_STREAM_OPEN ||
           s->state == BORELINE_STREAM_HALF_CLOSED_LOCAL;
}

/* Flow control */

int boreline_stream_consume_send_credit(boreline_stream_t *s, uint32_t n)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;
    if (n > s->send_window)
        return BORELINE_ERR_FLOW_CONTROL;
    s->send_window -= n;
    s->bytes_sent += n;
    return BORELINE_OK;
}

int boreline_stream_restore_send_credit(boreline_stream_t *s, uint32_t n)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;
    if (UINT32_MAX - s->send_window < n)
        return BORELINE_ERR_OVERFLOW;
    s->send_window += n;
    return BORELINE_OK;
}

int boreline_stream_received(boreline_stream_t *s, uint32_t n)
{
    if (s == NULL)
        return BORELINE_ERR_INVAL;
    if (n > s->recv_window)
        return BORELINE_ERR_FLOW_CONTROL;
    s->recv_window -= n;
    s->recv_consumed += n;
    s->bytes_received += n;
    return BORELINE_OK;
}

int boreline_stream_update_recv_window(boreline_stream_t *s, uint32_t base,
                                        uint32_t *update)
{
    if (s == NULL || update == NULL)
        return BORELINE_ERR_INVAL;
    /* Restore credit up to the base window size */
    uint32_t restore = base - s->recv_window;
    if (restore == 0) {
        *update = 0;
        return BORELINE_OK;
    }
    if (s->recv_window > base)
        return BORELINE_ERR_OVERFLOW;
    s->recv_window += restore;
    *update = restore;
    return BORELINE_OK;
}

uint32_t boreline_stream_send_window(const boreline_stream_t *s)
{
    return s ? s->send_window : 0;
}

uint32_t boreline_stream_recv_window(const boreline_stream_t *s)
{
    return s ? s->recv_window : 0;
}

uint64_t boreline_stream_bytes_sent(const boreline_stream_t *s)
{
    return s ? s->bytes_sent : 0;
}

uint64_t boreline_stream_bytes_received(const boreline_stream_t *s)
{
    return s ? s->bytes_received : 0;
}

void boreline_stream_set_userdata(boreline_stream_t *s, void *ud)
{
    if (s)
        s->userdata = ud;
}

void *boreline_stream_userdata(const boreline_stream_t *s)
{
    return s ? s->userdata : NULL;
}