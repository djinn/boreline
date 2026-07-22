#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "boreline/boreline.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Portable timer abstraction based on clock_gettime.
 *
 * Provides relative and absolute timers with nanosecond precision.
 * This is the shared portable implementation; platform event loops
 * provide integrated timer_expiry checking.
 */

struct boreline_timer {
    struct timespec deadline;
    bool active;
};

boreline_timer_t *boreline_timer_create(void)
{
    boreline_timer_t *t = calloc(1, sizeof(*t));
    if (t)
        memset(t, 0, sizeof(*t));
    return t;
}

void boreline_timer_destroy(boreline_timer_t *t)
{
    free(t);
}

int boreline_timer_start(boreline_timer_t *t, uint32_t ms)
{
    if (!t)
        return BORELINE_ERR_INVAL;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    t->deadline.tv_sec  = now.tv_sec + (time_t)(ms / 1000);
    t->deadline.tv_nsec = now.tv_nsec + (long)(ms % 1000) * 1000000L;
    if (t->deadline.tv_nsec >= 1000000000L) {
        t->deadline.tv_sec++;
        t->deadline.tv_nsec -= 1000000000L;
    }
    t->active = true;
    return BORELINE_OK;
}

void boreline_timer_stop(boreline_timer_t *t)
{
    if (t)
        t->active = false;
}

bool boreline_timer_expired(const boreline_timer_t *t)
{
    if (!t || !t->active)
        return false;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > t->deadline.tv_sec)
        return true;
    if (now.tv_sec == t->deadline.tv_sec &&
        now.tv_nsec > t->deadline.tv_nsec)
        return true;
    return false;
}

uint64_t boreline_timer_remaining_ms(const boreline_timer_t *t)
{
    if (!t || !t->active)
        return 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > t->deadline.tv_sec)
        return 0;
    uint64_t ms = (uint64_t)(t->deadline.tv_sec - now.tv_sec) * 1000UL;
    if (t->deadline.tv_nsec < now.tv_nsec)
        ms -= (now.tv_nsec - t->deadline.tv_nsec) / 1000000;
    else
        ms += (t->deadline.tv_nsec - now.tv_nsec) / 1000000;
    return ms;
}

int boreline_timer_reset(boreline_timer_t *t, uint32_t ms)
{
    return boreline_timer_start(t, ms);
}