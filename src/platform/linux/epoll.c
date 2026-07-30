#include "boreline/boreline.h"
#include "boreline/platform.h"

#ifdef __linux__

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define BORELINE_EPOLL_MAX_EVENTS 256

struct boreline_platform {
    int epoll_fd;
    struct epoll_event events[BORELINE_EPOLL_MAX_EVENTS];
};

boreline_platform_t *boreline_platform_create(void)
{
    boreline_platform_t *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return NULL;
    p->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (p->epoll_fd < 0) {
        free(p);
        return NULL;
    }
    return p;
}

void boreline_platform_destroy(boreline_platform_t *p)
{
    if (p == NULL)
        return;
    if (p->epoll_fd >= 0)
        close(p->epoll_fd);
    free(p);
}

int boreline_platform_add_fd(boreline_platform_t *p, int fd, uint32_t events, void *ctx)
{
    if (p == NULL || fd < 0)
        return BORELINE_ERR_INVAL;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET; /* edge-triggered by default */
    if (events & BORELINE_POLLIN)
        ev.events |= EPOLLIN;
    if (events & BORELINE_POLLOUT)
        ev.events |= EPOLLOUT;
    ev.data.ptr = ctx;
    if (epoll_ctl(p->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        return BORELINE_ERR_GENERIC;
    return BORELINE_OK;
}

int boreline_platform_mod_fd(boreline_platform_t *p, int fd, uint32_t events, void *ctx)
{
    if (p == NULL || fd < 0)
        return BORELINE_ERR_INVAL;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET;
    if (events & BORELINE_POLLIN)
        ev.events |= EPOLLIN;
    if (events & BORELINE_POLLOUT)
        ev.events |= EPOLLOUT;
    ev.data.ptr = ctx;
    if (epoll_ctl(p->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
        return BORELINE_ERR_GENERIC;
    return BORELINE_OK;
}

int boreline_platform_del_fd(boreline_platform_t *p, int fd)
{
    if (p == NULL || fd < 0)
        return BORELINE_ERR_INVAL;
    if (epoll_ctl(p->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
        return BORELINE_ERR_GENERIC;
    return BORELINE_OK;
}

int boreline_platform_wait(boreline_platform_t *p, int timeout_ms,
                            boreline_event_t *events_out, int max_events)
{
    if (p == NULL || events_out == NULL || max_events <= 0)
        return BORELINE_ERR_INVAL;
    int n = epoll_wait(p->epoll_fd, p->events,
                       BORELINE_EPOLL_MAX_EVENTS, timeout_ms);
    if (n < 0) {
        if (errno == EINTR)
            return 0;
        return BORELINE_ERR_GENERIC;
    }
    int cnt = n > max_events ? max_events : n;
    for (int i = 0; i < cnt; i++) {
        events_out[i].ctx = p->events[i].data.ptr;
        events_out[i].events = 0;
        if (p->events[i].events & EPOLLIN)
            events_out[i].events |= BORELINE_POLLIN;
        if (p->events[i].events & EPOLLOUT)
            events_out[i].events |= BORELINE_POLLOUT;
        if (p->events[i].events & (EPOLLERR | EPOLLHUP))
            events_out[i].events |= BORELINE_POLLERR;
    }
    return cnt;
}

#endif /* __linux__ */