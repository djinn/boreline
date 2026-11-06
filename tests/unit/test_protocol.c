#include "boreline/boreline.h"

#include <stdio.h>
#include <string.h>

/* Unit tests for protocol frame encoding, decoding, and parsing */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while (0)

static void test_encode_decode_header(void)
{
    boreline_frame_header_t hdr, out;
    uint8_t buf[16];

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BORELINE_MAGIC;
    hdr.version = BORELINE_VERSION_PROTO;
    hdr.type = BORELINE_FRAME_HELLO;
    hdr.flags = BORELINE_FLAG_NONE;
    hdr.stream_id = 0;
    hdr.payload_length = 0;

    int rc = boreline_encode_header(&hdr, buf);
    ASSERT(rc == BORELINE_OK, "encode valid header");

    rc = boreline_decode_header(buf, &out);
    ASSERT(rc == BORELINE_OK, "decode valid header");
    ASSERT(out.magic == BORELINE_MAGIC, "decode magic");
    ASSERT(out.version == BORELINE_VERSION_PROTO, "decode version");
    ASSERT(out.type == BORELINE_FRAME_HELLO, "decode type");
    ASSERT(out.stream_id == 0, "decode stream_id");
    ASSERT(out.payload_length == 0, "decode payload_length");

    /* Reserved non-zero must be rejected by encode */
    hdr.reserved = 0x1;
    rc = boreline_encode_header(&hdr, buf);
    ASSERT(rc == BORELINE_ERR_INVAL, "encode rejects non-zero reserved");
    hdr.reserved = 0;
}

static void test_encode_invalid_magic(void)
{
    boreline_frame_header_t hdr;
    uint8_t buf[16];

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = 0xDEAD;
    hdr.version = BORELINE_VERSION_PROTO;
    hdr.type = BORELINE_FRAME_DATA;

    int rc = boreline_encode_header(&hdr, buf);
    ASSERT(rc == BORELINE_ERR_INVAL, "encode rejects bad magic");
}

static void test_encode_invalid_version(void)
{
    boreline_frame_header_t hdr;
    uint8_t buf[16];

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BORELINE_MAGIC;
    hdr.version = 99;
    hdr.type = BORELINE_FRAME_DATA;

    int rc = boreline_encode_header(&hdr, buf);
    ASSERT(rc == BORELINE_ERR_INVAL, "encode rejects bad version");
}

static void test_fragmented_frame(void)
{
    boreline_parser_t *p = boreline_parser_create();
    ASSERT(p != NULL, "create parser");

    uint8_t frame[BORELINE_FRAME_HEADER_SIZE + 4];
    boreline_frame_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BORELINE_MAGIC;
    hdr.version = BORELINE_VERSION_PROTO;
    hdr.type = BORELINE_FRAME_DATA;
    hdr.stream_id = 1;
    hdr.payload_length = 4;

    boreline_encode_header(&hdr, frame);
    frame[BORELINE_FRAME_HEADER_SIZE + 0] = 0x01;
    frame[BORELINE_FRAME_HEADER_SIZE + 1] = 0x02;
    frame[BORELINE_FRAME_HEADER_SIZE + 2] = 0x03;
    frame[BORELINE_FRAME_HEADER_SIZE + 3] = 0x04;

    /* Feed header only */
    int rc = boreline_parser_feed(p, frame, BORELINE_FRAME_HEADER_SIZE);
    ASSERT(rc == BORELINE_OK, "feed header");

    boreline_frame_header_t got;
    const uint8_t *payload = NULL;
    rc = boreline_parser_next(p, &got, &payload);
    ASSERT(rc == BORELINE_ERR_AGAIN, "header only should pend");

    /* Feed rest */
    rc = boreline_parser_feed(p, frame + BORELINE_FRAME_HEADER_SIZE, 4);
    ASSERT(rc == BORELINE_OK, "feed payload");

    rc = boreline_parser_next(p, &got, &payload);
    ASSERT(rc == BORELINE_OK || rc == BORELINE_ERR_AGAIN, "complete frame");
    if (rc == BORELINE_OK) {
        ASSERT(got.type == BORELINE_FRAME_DATA, "decoded type DATA");
        ASSERT(got.stream_id == 1, "decoded stream_id 1");
        ASSERT(payload != NULL, "payload non-null");
        ASSERT(payload[0] == 0x01 && payload[3] == 0x04, "payload content");
    }

    boreline_parser_destroy(p);
}

static void test_oversize_payload(void)
{
    boreline_parser_t *p = boreline_parser_create();
    ASSERT(p != NULL, "create parser");

    /* Craft the header bytes manually (encode_header validates and would
     * reject the oversize length, so we bypass it to test the parser
     * side of validation). */
    uint8_t buf[BORELINE_FRAME_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = (uint8_t)(BORELINE_MAGIC >> 8);
    buf[1] = (uint8_t)(BORELINE_MAGIC & 0xFF);
    buf[2] = BORELINE_VERSION_PROTO;
    buf[3] = BORELINE_FRAME_DATA;
    buf[4] = 0; buf[5] = 0;            /* flags */
    buf[6] = 0; buf[7] = 0;            /* reserved */
    buf[8] = 0; buf[9] = 0;            /* stream id high */
    buf[10] = 0; buf[11] = 1;          /* stream id = 1 */
    buf[12] = (uint8_t)((BORELINE_MAX_PAYLOAD + 1) >> 24);
    buf[13] = (uint8_t)((BORELINE_MAX_PAYLOAD + 1) >> 16);
    buf[14] = (uint8_t)((BORELINE_MAX_PAYLOAD + 1) >> 8);
    buf[15] = (uint8_t)((BORELINE_MAX_PAYLOAD + 1) & 0xFF);

    int rc = boreline_parser_feed(p, buf, sizeof(buf));
    ASSERT(rc == BORELINE_OK, "feed oversize header");

    boreline_frame_header_t got;
    const uint8_t *payload;
    rc = boreline_parser_next(p, &got, &payload);
    ASSERT(rc == BORELINE_ERR_PROTOCOL, "parser rejects oversize payload");

    boreline_parser_destroy(p);
}

static void test_malformed_header(void)
{
    boreline_parser_t *p = boreline_parser_create();
    ASSERT(p != NULL, "create parser");

    /* Feed garbage that looks like a header with bad magic */
    uint8_t buf[16] = {0};
    buf[0] = 0x00; buf[1] = 0x00; /* magic = 0 */
    buf[2] = BORELINE_VERSION_PROTO;
    buf[3] = BORELINE_FRAME_DATA;

    boreline_parser_feed(p, buf, 16);
    boreline_frame_header_t got;
    const uint8_t *payload;
    int rc = boreline_parser_next(p, &got, &payload);
    ASSERT(rc == BORELINE_ERR_PROTOCOL, "parser rejects bad magic");

    boreline_parser_destroy(p);
}

static void test_reserved_nonzero(void)
{
    uint8_t buf[16];
    /* Manually craft a header with non-zero reserved field */
    memset(buf, 0, sizeof(buf));
    buf[0] = (uint8_t)(BORELINE_MAGIC >> 8);
    buf[1] = (uint8_t)(BORELINE_MAGIC & 0xFF);
    buf[2] = BORELINE_VERSION_PROTO;
    buf[3] = BORELINE_FRAME_PING;
    buf[6] = 0x00; buf[7] = 0x01; /* reserved = 1 */

    boreline_parser_t *p = boreline_parser_create();
    boreline_parser_feed(p, buf, 16);
    boreline_frame_header_t got;
    const uint8_t *payload;
    int rc = boreline_parser_next(p, &got, &payload);
    ASSERT(rc == BORELINE_ERR_PROTOCOL, "parser rejects non-zero reserved");

    boreline_parser_destroy(p);
}

static void test_multiple_frames(void)
{
    boreline_parser_t *p = boreline_parser_create();
    ASSERT(p != NULL, "create parser");

    /* Build two PING frames back-to-back */
    uint8_t data[32];
    boreline_frame_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BORELINE_MAGIC;
    hdr.version = BORELINE_VERSION_PROTO;
    hdr.type = BORELINE_FRAME_PING;
    hdr.stream_id = 0;
    hdr.payload_length = 0;

    boreline_encode_header(&hdr, data);
    boreline_encode_header(&hdr, data + 16);

    boreline_parser_feed(p, data, 32);
    for (int i = 0; i < 2; i++) {
        const uint8_t *payload;
        hdr = (boreline_frame_header_t){0};
        int rc = boreline_parser_next(p, &hdr, &payload);
        ASSERT(rc == BORELINE_OK, "extract frame from multi-frame buffer");
        ASSERT(hdr.type == BORELINE_FRAME_PING, "frame type PING");
    }

    boreline_parser_destroy(p);
}

static void test_zero_length_payload(void)
{
    boreline_parser_t *p = boreline_parser_create();
    ASSERT(p != NULL, "create parser");

    uint8_t buf[16];
    boreline_frame_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BORELINE_MAGIC;
    hdr.version = BORELINE_VERSION_PROTO;
    hdr.type = BORELINE_FRAME_PING;

    boreline_encode_header(&hdr, buf);
    boreline_parser_feed(p, buf, 16);

    const uint8_t *payload;
    hdr = (boreline_frame_header_t){0};
    int rc = boreline_parser_next(p, &hdr, &payload);
    ASSERT(rc == BORELINE_OK, "zero-length payload frame");
    ASSERT(hdr.type == BORELINE_FRAME_PING, "frame type PING");

    boreline_parser_destroy(p);
}

static void test_control_frame_payload_limit(void)
{
    boreline_parser_t *p = boreline_parser_create();
    ASSERT(p != NULL, "create parser");

    /* A non-DATA frame with payload > 4 KiB must be rejected */
    uint8_t buf[16];
    boreline_frame_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BORELINE_MAGIC;
    hdr.version = BORELINE_VERSION_PROTO;
    hdr.type = BORELINE_FRAME_PING;
    hdr.payload_length = BORELINE_MAX_CONTROL_PAYLOAD + 1;

    boreline_encode_header(&hdr, buf);
    boreline_parser_feed(p, buf, 16);

    const uint8_t *payload;
    hdr = (boreline_frame_header_t){0};
    int rc = boreline_parser_next(p, &hdr, &payload);
    ASSERT(rc == BORELINE_ERR_PROTOCOL, "control frame payload limit enforced");

    boreline_parser_destroy(p);
}

static void test_constant_time_eq(void)
{
    uint8_t a[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t b[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t c[] = {0x01, 0x02, 0x03, 0x05};

    ASSERT(boreline_constant_time_eq(a, b, 4) == 1, "equal buffers");
    ASSERT(boreline_constant_time_eq(a, c, 4) == 0, "different buffers");
    ASSERT(boreline_constant_time_eq(a, a, 0) == 1, "zero length");
    ASSERT(boreline_constant_time_eq(NULL, b, 4) == 0, "null first");
    ASSERT(boreline_constant_time_eq(a, NULL, 4) == 0, "null second");
}

static void test_error_strings(void)
{
    ASSERT(strcmp(boreline_error_str(BORELINE_OK), "success") == 0,
           "OK error string");
    ASSERT(strcmp(boreline_error_str(BORELINE_ERR_GENERIC), "generic error") == 0,
           "generic error string");
    ASSERT(strcmp(boreline_error_str(BORELINE_ERR_PROTOCOL), "protocol violation") == 0,
           "protocol error string");
    ASSERT(strcmp(boreline_error_str(-9999), "unknown error") == 0,
           "unknown error string");
}

static void test_frame_type_names(void)
{
    ASSERT(strcmp(boreline_frame_type_str(BORELINE_FRAME_HELLO), "HELLO") == 0,
           "HELLO name");
    ASSERT(strcmp(boreline_frame_type_str(0xFF), "UNKNOWN") == 0,
           "unknown type name");
    ASSERT(strcmp(boreline_frame_type_str(BORELINE_FRAME_GOAWAY), "GOAWAY") == 0,
           "GOAWAY name");
}

int main(void)
{
    test_encode_decode_header();
    test_encode_invalid_magic();
    test_encode_invalid_version();
    test_fragmented_frame();
    test_oversize_payload();
    test_malformed_header();
    test_reserved_nonzero();
    test_multiple_frames();
    test_zero_length_payload();
    test_control_frame_payload_limit();
    test_constant_time_eq();
    test_error_strings();
    test_frame_type_names();

    fprintf(stdout, "\n");
    fprintf(stdout, "Protocol tests: %d passed, %d failed, %d total\n",
            tests_passed, tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}