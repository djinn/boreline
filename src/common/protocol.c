#include "boreline/boreline.h"

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

/* Library initialization */
static bool g_initialized = false;

int boreline_init(void)
{
    if (g_initialized)
        return BORELINE_OK;
    g_initialized = true;
    return BORELINE_OK;
}

void boreline_cleanup(void)
{
    g_initialized = false;
}

/* ============================ Version ============================ */

const char *boreline_version(void)
{
    return BORELINE_VERSION;
}

/* ============================ Errors ============================ */

static const struct {
    int code;
    const char *msg;
} error_table[] = {
    { BORELINE_OK,            "success"             },
    { BORELINE_ERR_GENERIC,   "generic error"       },
    { BORELINE_ERR_NOMEM,     "out of memory"       },
    { BORELINE_ERR_INVAL,     "invalid argument"    },
    { BORELINE_ERR_AGAIN,     "try again"           },
    { BORELINE_ERR_PROTOCOL,  "protocol violation"  },
    { BORELINE_ERR_AUTH,      "authentication failed" },
    { BORELINE_ERR_TIMEOUT,   "operation timed out" },
    { BORELINE_ERR_CLOSED,    "connection closed"   },
    { BORELINE_ERR_OVERFLOW,  "integer overflow"    },
    { BORELINE_ERR_EXISTS,    "already exists"      },
    { BORELINE_ERR_NOTFOUND,  "not found"           },
    { BORELINE_ERR_LIMIT,     "limit exceeded"      },
    { BORELINE_ERR_TLS,       "TLS error"           },
    { BORELINE_ERR_BUSY,      "resource busy"       },
};

const char *boreline_error_str(int err)
{
    for (size_t i = 0; i < sizeof(error_table) / sizeof(error_table[0]); i++) {
        if (error_table[i].code == err)
            return error_table[i].msg;
    }
    return "unknown error";
}

/* ============================ Frames ============================ */

static const char *frame_type_names[] = {
    [BORELINE_FRAME_HELLO]          = "HELLO",
    [BORELINE_FRAME_AUTH]           = "AUTH",
    [BORELINE_FRAME_AUTH_RESULT]    = "AUTH_RESULT",
    [BORELINE_FRAME_REGISTER]       = "REGISTER",
    [BORELINE_FRAME_REGISTER_RESULT] = "REGISTER_RESULT",
    [BORELINE_FRAME_OPEN]           = "OPEN",
    [BORELINE_FRAME_OPEN_RESULT]    = "OPEN_RESULT",
    [BORELINE_FRAME_DATA]           = "DATA",
    [BORELINE_FRAME_WINDOW_UPDATE]  = "WINDOW_UPDATE",
    [BORELINE_FRAME_HALF_CLOSE]     = "HALF_CLOSE",
    [BORELINE_FRAME_RESET]          = "RESET",
    [BORELINE_FRAME_PING]           = "PING",
    [BORELINE_FRAME_PONG]           = "PONG",
    [BORELINE_FRAME_ERROR]          = "ERROR",
    [BORELINE_FRAME_GOAWAY]         = "GOAWAY",
};

const char *boreline_frame_type_str(boreline_frame_type_t type)
{
    if (type < BORELINE_FRAME_HELLO || type > BORELINE_FRAME_GOAWAY)
        return "UNKNOWN";
    return frame_type_names[type];
}

int boreline_encode_header(boreline_frame_header_t *hdr,
                            uint8_t buf[BORELINE_FRAME_HEADER_SIZE])
{
    if (hdr == NULL || buf == NULL)
        return BORELINE_ERR_INVAL;
    if (hdr->reserved != 0)
        return BORELINE_ERR_INVAL;
    if (hdr->magic != BORELINE_MAGIC)
        return BORELINE_ERR_INVAL;
    if (hdr->version != BORELINE_VERSION_PROTO)
        return BORELINE_ERR_INVAL;
    if (hdr->type < BORELINE_FRAME_HELLO || hdr->type > BORELINE_FRAME_GOAWAY)
        return BORELINE_ERR_INVAL;
    if (hdr->payload_length > BORELINE_MAX_PAYLOAD)
        return BORELINE_ERR_INVAL;

    /* Network byte order (big endian) */
    buf[0] = (uint8_t)(hdr->magic >> 8);
    buf[1] = (uint8_t)(hdr->magic & 0xFF);
    buf[2] = hdr->version;
    buf[3] = hdr->type;
    buf[4] = (uint8_t)(hdr->flags >> 8);
    buf[5] = (uint8_t)(hdr->flags & 0xFF);
    buf[6] = (uint8_t)(hdr->reserved >> 8);
    buf[7] = (uint8_t)(hdr->reserved & 0xFF);
    buf[8]  = (uint8_t)(hdr->stream_id >> 24);
    buf[9]  = (uint8_t)(hdr->stream_id >> 16);
    buf[10] = (uint8_t)(hdr->stream_id >> 8);
    buf[11] = (uint8_t)(hdr->stream_id & 0xFF);
    buf[12] = (uint8_t)(hdr->payload_length >> 24);
    buf[13] = (uint8_t)(hdr->payload_length >> 16);
    buf[14] = (uint8_t)(hdr->payload_length >> 8);
    buf[15] = (uint8_t)(hdr->payload_length & 0xFF);

    return BORELINE_OK;
}

int boreline_decode_header(const uint8_t buf[BORELINE_FRAME_HEADER_SIZE],
                            boreline_frame_header_t *hdr)
{
    if (buf == NULL || hdr == NULL)
        return BORELINE_ERR_INVAL;

    hdr->magic     = (uint16_t)((buf[0] << 8) | buf[1]);
    hdr->version   = buf[2];
    hdr->type      = buf[3];
    hdr->flags     = (uint16_t)((buf[4] << 8) | buf[5]);
    hdr->reserved  = (uint16_t)((buf[6] << 8) | buf[7]);
    hdr->stream_id = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) |
                     ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];
    hdr->payload_length = ((uint32_t)buf[12] << 24) | ((uint32_t)buf[13] << 16) |
                          ((uint32_t)buf[14] << 8) | (uint32_t)buf[15];

    return BORELINE_OK;
}

/* ============================ Parser ============================ */

struct boreline_parser {
    uint8_t  buffer[BORELINE_MAX_PAYLOAD + BORELINE_FRAME_HEADER_SIZE];
    size_t   used;
    bool     have_header;
};

boreline_parser_t *boreline_parser_create(void)
{
    boreline_parser_t *p = calloc(1, sizeof(*p));
    return p;
}

void boreline_parser_destroy(boreline_parser_t *p)
{
    free(p);
}

int boreline_parser_feed(boreline_parser_t *p,
                          const uint8_t *data, size_t len)
{
    if (p == NULL || (data == NULL && len > 0))
        return BORELINE_ERR_INVAL;
    if (p->used + len > sizeof(p->buffer))
        return BORELINE_ERR_OVERFLOW;
    if (len > 0)
        memcpy(p->buffer + p->used, data, len);
    p->used += len;
    return BORELINE_OK;
}

int boreline_parser_next(boreline_parser_t *p,
                          boreline_frame_header_t *hdr,
                          const uint8_t **payload)
{
    if (p == NULL || hdr == NULL || payload == NULL)
        return BORELINE_ERR_INVAL;

    /* Need at least a full header */
    if (p->used < BORELINE_FRAME_HEADER_SIZE) {
        *payload = NULL;
        return BORELINE_ERR_AGAIN;
    }

    /* If header already processed, require full payload delivered */
    if (p->have_header) {
        /* Peek at buffered header */
        boreline_frame_header_t tmp;
        int rc = boreline_decode_header(p->buffer, &tmp);

        if (rc != BORELINE_OK)
            return rc;
        if (tmp.payload_length > BORELINE_MAX_PAYLOAD)
            return BORELINE_ERR_OVERFLOW;
        if (p->used < BORELINE_FRAME_HEADER_SIZE + tmp.payload_length) {
            *payload = NULL;
            return BORELINE_ERR_AGAIN;
        }
        /* Check validation rules at first header parse */
        if (hdr->reserved != 0)
            return BORELINE_ERR_PROTOCOL;
        if (hdr->magic != BORELINE_MAGIC)
            return BORELINE_ERR_PROTOCOL;
        if (hdr->version != BORELINE_VERSION_PROTO)
            return BORELINE_ERR_PROTOCOL;

        *payload = p->buffer + BORELINE_FRAME_HEADER_SIZE;
        size_t total = BORELINE_FRAME_HEADER_SIZE + tmp.payload_length;
        memmove(p->buffer, p->buffer + total, p->used - total);
        p->used -= total;
        p->have_header = false;
        *hdr = tmp;
        return BORELINE_OK;
    }

    /* Parse header */
    int rc = boreline_decode_header(p->buffer, hdr);
    if (rc != BORELINE_OK)
        return rc;

    /* Validate header fields before allocation/processing */
    if (hdr->magic != BORELINE_MAGIC)
        return BORELINE_ERR_PROTOCOL;
    if (hdr->version != BORELINE_VERSION_PROTO)
        return BORELINE_ERR_PROTOCOL;
    if (hdr->reserved != 0)
        return BORELINE_ERR_PROTOCOL;
    if (hdr->type < BORELINE_FRAME_HELLO || hdr->type > BORELINE_FRAME_GOAWAY)
        return BORELINE_ERR_PROTOCOL;
    if (hdr->payload_length > BORELINE_MAX_PAYLOAD)
        return BORELINE_ERR_PROTOCOL;

    /* Control-frame payload limit (4 KiB) */
    if (hdr->type != BORELINE_FRAME_DATA &&
        hdr->payload_length > BORELINE_MAX_CONTROL_PAYLOAD)
        return BORELINE_ERR_PROTOCOL;

    /* Check for invalid flags per type */
    switch (hdr->type) {
    case BORELINE_FRAME_PING:
    case BORELINE_FRAME_PONG:
        if (hdr->flags & ~(BORELINE_FLAG_ACK))
            return BORELINE_ERR_PROTOCOL;
        break;
    case BORELINE_FRAME_DATA:
        if (hdr->flags & ~(BORELINE_FLAG_FIN | BORELINE_FLAG_PRIORITY))
            return BORELINE_ERR_PROTOCOL;
        break;
    default:
        if (hdr->flags & BORELINE_FLAG_PRIORITY && hdr->type != BORELINE_FRAME_DATA)
            return BORELINE_ERR_PROTOCOL;
        break;
    }

    /* Stream 0 is connection control only; non-control frames on stream 0
     * other than HELLO/PING/PONG/GOAWAY are violations. */
    if (hdr->stream_id == 0) {
        switch (hdr->type) {
        case BORELINE_FRAME_HELLO:
        case BORELINE_FRAME_AUTH:
        case BORELINE_FRAME_AUTH_RESULT:
        case BORELINE_FRAME_PING:
        case BORELINE_FRAME_PONG:
        case BORELINE_FRAME_ERROR:
        case BORELINE_FRAME_GOAWAY:
            break;
        default:
            return BORELINE_ERR_PROTOCOL;
        }
    }

    /* If payload is zero-length, complete immediately */
    p->have_header = true;

    if (hdr->payload_length == 0) {
        *payload = NULL;
        /* Reuse path: consume header */
        memmove(p->buffer, p->buffer + BORELINE_FRAME_HEADER_SIZE,
                p->used - BORELINE_FRAME_HEADER_SIZE);
        p->used -= BORELINE_FRAME_HEADER_SIZE;
        p->have_header = false;
        return BORELINE_OK;
    }

    if (p->used >= BORELINE_FRAME_HEADER_SIZE + hdr->payload_length) {
        *payload = p->buffer + BORELINE_FRAME_HEADER_SIZE;
        size_t total = BORELINE_FRAME_HEADER_SIZE + hdr->payload_length;
        memmove(p->buffer, p->buffer + total, p->used - total);
        p->used -= total;
        p->have_header = false;
        return BORELINE_OK;
    }

    *payload = NULL;
    return BORELINE_ERR_AGAIN;
}

size_t boreline_parser_buffered(const boreline_parser_t *p)
{
    return p ? p->used : 0;
}

/* ============================ Constant time compare ============================ */

int boreline_constant_time_eq(const uint8_t *a, const uint8_t *b, size_t n)
{
    if (a == NULL || b == NULL)
        return 0;
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++)
        diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}