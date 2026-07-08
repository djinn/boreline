#include "boreline/boreline.h"

#include <stdio.h>
#include <string.h>

/* Unit tests for the ring buffer */

static int passed = 0, failed = 0, ran = 0;

#define ASSERT(cond, msg) do { \
    ran++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failed++; \
    } else \
        passed++; \
} while (0)

static void test_create_destroy(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(1024);
    ASSERT(rb != NULL, "create ring buf");
    ASSERT(boreline_ringbuf_capacity(rb) == 1024, "capacity matches");
    ASSERT(boreline_ringbuf_available(rb) == 0, "initially empty");
    boreline_ringbuf_destroy(rb);

    /* NULL destroy should not crash */
    boreline_ringbuf_destroy(NULL);
    ASSERT(1, "null destroy no crash");
}

static void test_write_read(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(256);
    ASSERT(rb != NULL, "create");

    uint8_t in[] = "hello world";
    size_t n = boreline_ringbuf_write(rb, in, 11);
    ASSERT(n == 11, "write 11 bytes");
    ASSERT(boreline_ringbuf_available(rb) == 11, "available matches");

    uint8_t out[32] = {0};
    n = boreline_ringbuf_read(rb, out, 11);
    ASSERT(n == 11, "read 11 bytes");
    ASSERT(memcmp(in, out, 11) == 0, "content matches");
    ASSERT(boreline_ringbuf_available(rb) == 0, "empty after read");
    boreline_ringbuf_destroy(rb);
}

static void test_wrap_around(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(16);

    /* Fill to capacity */
    uint8_t data[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    size_t n = boreline_ringbuf_write(rb, data, 16);
    ASSERT(n == 16, "write to capacity");

    /* Read 5 bytes */
    uint8_t out[5];
    n = boreline_ringbuf_read(rb, out, 5);
    ASSERT(n == 5, "read 5");
    ASSERT(out[0] == 0 && out[4] == 4, "first 5 bytes correct");

    /* Write more bytes - should wrap */
    uint8_t more[10] = {16,17,18,19,20,21,22,23,24,25};
    n = boreline_ringbuf_write(rb, more, 10);
    ASSERT(n == 5, "write wraps and fills to capacity");

    /* Read all */
    uint8_t all[32] = {0};
    n = boreline_ringbuf_read(rb, all, 32);
    ASSERT(n >= 11, "read remaining");
    boreline_ringbuf_destroy(rb);
}

static void test_read_empty(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(64);
    uint8_t buf[4];
    size_t n = boreline_ringbuf_read(rb, buf, 4);
    ASSERT(n == 0, "read from empty returns 0");
    boreline_ringbuf_destroy(rb);
}

static void test_write_full(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(4);
    uint8_t data[8] = {1,2,3,4,5,6,7,8};
    size_t n = boreline_ringbuf_write(rb, data, 8);
    ASSERT(n == 4, "write beyond capacity truncated to 4");
    boreline_ringbuf_destroy(rb);
}

static void test_reset(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(64);
    uint8_t data[8] = {1,2,3,4,5,6,7,8};
    boreline_ringbuf_write(rb, data, 8);
    ASSERT(boreline_ringbuf_available(rb) == 8, "has data");

    boreline_ringbuf_reset(rb);
    ASSERT(boreline_ringbuf_available(rb) == 0, "empty after reset");
    boreline_ringbuf_destroy(rb);
}

static void test_null_operations(void)
{
    ASSERT(boreline_ringbuf_write(NULL, NULL, 0) == 0, "null write");
    ASSERT(boreline_ringbuf_read(NULL, NULL, 0) == 0, "null read");
    ASSERT(boreline_ringbuf_available(NULL) == 0, "null available");
    ASSERT(boreline_ringbuf_capacity(NULL) == 0, "null capacity");
}

static void test_zero_capacity(void)
{
    boreline_ringbuf_t *rb = boreline_ringbuf_create(0);
    ASSERT(rb == NULL, "zero capacity returns NULL");

    rb = boreline_ringbuf_create((size_t)1 << 33);
    ASSERT(rb == NULL, "huge capacity returns NULL");
}

int main(void)
{
    test_create_destroy();
    test_write_read();
    test_wrap_around();
    test_read_empty();
    test_write_full();
    test_reset();
    test_null_operations();
    test_zero_capacity();

    fprintf(stdout, "\nRingbuf tests: %d passed, %d failed, %d total\n",
            passed, failed, ran);
    return failed > 0 ? 1 : 0;
}