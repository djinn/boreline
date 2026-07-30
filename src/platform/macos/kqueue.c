#include "boreline/platform.h"

#ifdef __APPLE__

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#define BORELINE_KQUEUE_MAX_EVENTS 256

struct boreline_platform {
    int kq;
    struct kevent events[BORELINE_KQUEUE_MAX_EVENTS];
};

boreline_platform_t *boreline_platform_create(void)
{
    boreline_platform_t *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return NULL;
    p->kq = kqueue();
    if (p->kq < 0) {
        free(p);
        return NULL;
    }
    return p;
}

void boreline_platform_destroy(boreline_platform_t *p)
{
    if (p == NULL)
        return;
    if (p->kq >= 0)
        close(p->kq);
    free(p);
}

int boreline_platform_add_fd(boreline_platform_t *p, int fd,
                              uint32_t events, void *ctx)
{
    if (p == NULL || fd < 0)
        return BORELINE_ERR_INVAL;

    struct kevent ev[2];
    int n = 0;
    memset(ev, 0, sizeof(ev));

    if (events & BORELINE_POLLIN) {
        EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ctx);
    }
    if (events & BORELINE_POLLOUT) {
        EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ctx);
    }
    if (n > 0 && kevent(p->kq, ev, n, NULL, 0, NULL) < 0)
        return BORELINE_ERR_GENERIC;
    return BORELINE_OK;
}

int boreline_platform_mod_fd(boreline_platform_t *p, int fd,
                              uint32_t events, void *ctx)
{
    /* kqueue requires deleting and re-adding to change filters */
    int rc = boreline_platform_del_fd(p, fd);
    if (rc != BORELINE_OK && rc != BORELINE_ERR_GENERIC)
        return rc;
    return boreline_platform_add_fd(p, fd, events, ctx);
}

int boreline_platform_del_fd(boreline_platform_t *p, int fd)
{
    if (p == NULL || fd < 0)
        return BORELINE_ERR_INVAL;

    struct kevent ev[2];
    /* Disable both read and write without modifying context */
    EV_SET(&ev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&ev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    (void)kevent(p->kq, ev, 2, NULL, 0, NULL);
    return BORELINE_OK;
}

int boreline_platform_wait(boreline_platform_t *p, int timeout_ms,
                            boreline_event_t *events_out, int max_events)
{
    if (p == NULL || events_out == NULL || max_events <= 0)
        return BORELINE_ERR_INVAL;

    struct timespec ts;
    struct timespec *pts = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        pts = &ts;
    }

    int n = kevent(p->kq, NULL, 0, p->events,
                   BORELINE_KQUEUE_MAX_EVENTS, pts);
    if (n < 0) {
        if (errno == EINTR)
            return 0;
        return BORELINE_ERR_GENERIC;
    }
    int cnt = n > max_events ? max_events : n;
    for (int i = 0; i < cnt; i++) {
        events_out[i].ctx = p->events[i].udata;
        events_out[i].events = 0;
        if (p->events[i].filter == EVFILT_READ)
            events_out[i].events |= BORELINE_POLLIN;
        if (p->events[i].filter == EVFILT_WRITE)
            events_out[i].events |= BORELINE_POLLOUT;
        if (p->events[i].flags & (EV_EOF | EV_ERROR))
            events_out[i].events |= BORELINE_POLLERR;
    }
    return cnt;
}

#endif /* __APPLE__ */