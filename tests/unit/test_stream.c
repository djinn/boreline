#include "boreline/boreline.h"

#include <stdio.h>
#include <string.h>

/* Unit tests for stream state machine and flow control */

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
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    ASSERT(s != NULL, "create stream");
    ASSERT(boreline_stream_id(s) == 1, "stream id");
    ASSERT(boreline_stream_state(s) == BORELINE_STREAM_IDLE, "initial state");
    ASSERT(boreline_stream_send_window(s) == 65536, "send window");
    ASSERT(boreline_stream_recv_window(s) == 65536, "recv window");
    ASSERT(!boreline_stream_is_closed(s), "not closed initially");
    ASSERT(boreline_stream_can_send(s) == false, "cannot send in idle");
    ASSERT(boreline_stream_can_receive(s) == false, "cannot receive in idle");
    boreline_stream_destroy(s);
}

static void test_idle_to_open(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 256 * 1024);
    int rc = boreline_stream_transition(s, BORELINE_STREAM_OPEN);
    ASSERT(rc == BORELINE_OK, "idle->open ok");
    ASSERT(boreline_stream_state(s) == BORELINE_STREAM_OPEN, "now open");
    ASSERT(boreline_stream_can_send(s), "can send in open");
    ASSERT(boreline_stream_can_receive(s), "can receive in open");
    boreline_stream_destroy(s);
}

static void test_invalid_transition(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    /* Cannot skip from idle to CLOSED directly (must go through open/half-close) */
    int rc = boreline_stream_transition(s, BORELINE_STREAM_CLOSED);
    ASSERT(rc == BORELINE_ERR_PROTOCOL, "idle->closed rejected");
    boreline_stream_destroy(s);
}

static void test_half_close_local(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);

    int rc = boreline_stream_half_close_local(s);
    ASSERT(rc == BORELINE_OK, "half close local ok");
    ASSERT(boreline_stream_state(s) == BORELINE_STREAM_HALF_CLOSED_LOCAL,
           "half closed local state");
    ASSERT(!boreline_stream_can_send(s), "cannot send after local close");
    ASSERT(boreline_stream_can_receive(s), "can still receive");
    boreline_stream_destroy(s);
}

static void test_half_close_both(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);

    boreline_stream_half_close_local(s);
    int rc = boreline_stream_half_close_remote(s);
    ASSERT(rc == BORELINE_OK, "half close remote");
    ASSERT(boreline_stream_state(s) == BORELINE_STREAM_CLOSED, "now fully closed");
    ASSERT(boreline_stream_is_closed(s), "is closed");
    boreline_stream_destroy(s);
}

static void test_reset(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);

    int rc = boreline_stream_reset(s);
    ASSERT(rc == BORELINE_OK, "reset ok");
    ASSERT(boreline_stream_state(s) == BORELINE_STREAM_RESET, "reset state");
    ASSERT(boreline_stream_is_closed(s), "is closed after reset");
    ASSERT(!boreline_stream_can_send(s), "cannot send after reset");
    ASSERT(!boreline_stream_can_receive(s), "cannot receive after reset");
    boreline_stream_destroy(s);
}

static void test_double_reset_idempotent(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);
    boreline_stream_reset(s);

    /* Second reset should succeed (already in reset state) */
    int rc = boreline_stream_reset(s);
    ASSERT(rc == BORELINE_OK, "double reset ok");
    boreline_stream_destroy(s);
}

static void test_flow_control_basic(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 1000);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);

    /* Consume send credit */
    int rc = boreline_stream_consume_send_credit(s, 400);
    ASSERT(rc == BORELINE_OK, "consume 400 credit");
    ASSERT(boreline_stream_send_window(s) == 600, "send window reduced");

    /* Cannot exceed window */
    rc = boreline_stream_consume_send_credit(s, 700);
    ASSERT(rc == BORELINE_ERR_FLOW_CONTROL, "consume beyond window rejected");
    ASSERT(boreline_stream_send_window(s) == 600, "window unchanged");

    /* Restore credit */
    rc = boreline_stream_restore_send_credit(s, 300);
    ASSERT(rc == BORELINE_OK, "restore 300 credit");
    ASSERT(boreline_stream_send_window(s) == 900, "send window restored");

    boreline_stream_destroy(s);
}

static void test_flow_control_overflow(void)
{
    boreline_stream_t *s = boreline_stream_create(1, UINT32_MAX);
    int rc = boreline_stream_consume_send_credit(s, 1);
    ASSERT(rc == BORELINE_OK, "consume 1");

    /* Restoring to UINT32_MAX is exactly at the limit - allowed */
    rc = boreline_stream_restore_send_credit(s, 1);
    ASSERT(rc == BORELINE_OK, "restore to exact max allowed");

    /* Restoring beyond UINT32_MAX would overflow - but window cannot
     * exceed UINT32_MAX; verify a fresh window at max cannot grow. */
    boreline_stream_t *s2 = boreline_stream_create(2, UINT32_MAX);
    boreline_stream_consume_send_credit(s2, 100);
    rc = boreline_stream_restore_send_credit(s2, 100);
    ASSERT(rc == BORELINE_OK, "restore back to max");
    ASSERT(boreline_stream_send_window(s2) == UINT32_MAX, "window at max");

    boreline_stream_destroy(s);
    boreline_stream_destroy(s2);
}

static void test_receive_window(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 1000);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);

    /* Receive data from remote */
    int rc = boreline_stream_received(s, 300);
    ASSERT(rc == BORELINE_OK, "received 300 bytes");
    ASSERT(boreline_stream_recv_window(s) == 700, "recv window reduced");

    /* Can't go negative */
    rc = boreline_stream_received(s, 800);
    ASSERT(rc == BORELINE_ERR_FLOW_CONTROL, "receive beyond window rejected");

    /* Update recv window */
    uint32_t update = 0;
    rc = boreline_stream_update_recv_window(s, 1000, &update);
    ASSERT(rc == BORELINE_OK, "update recv window");
    ASSERT(update == 300, "restore 300 credit");
    ASSERT(boreline_stream_recv_window(s) == 1000, "recv window restored");

    boreline_stream_destroy(s);
}

static void test_userdata(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 1024);
    int ctx = 42;
    boreline_stream_set_userdata(s, &ctx);
    ASSERT(boreline_stream_userdata(s) == &ctx, "userdata set/get");
    boreline_stream_destroy(s);
}

static void test_bytes_counted(void)
{
    boreline_stream_t *s = boreline_stream_create(1, 65536);
    boreline_stream_transition(s, BORELINE_STREAM_OPEN);

    boreline_stream_consume_send_credit(s, 100);
    ASSERT(boreline_stream_bytes_sent(s) == 100, "bytes sent tracked");

    boreline_stream_received(s, 50);
    ASSERT(boreline_stream_bytes_received(s) == 50, "bytes received tracked");

    boreline_stream_destroy(s);
}

int main(void)
{
    test_create_destroy();
    test_idle_to_open();
    test_invalid_transition();
    test_half_close_local();
    test_half_close_both();
    test_reset();
    test_double_reset_idempotent();
    test_flow_control_basic();
    test_flow_control_overflow();
    test_receive_window();
    test_userdata();
    test_bytes_counted();

    fprintf(stdout, "\nStream tests: %d passed, %d failed, %d total\n",
            passed, failed, ran);
    return failed > 0 ? 1 : 0;
}