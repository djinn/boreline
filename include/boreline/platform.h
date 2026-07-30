#ifndef BORELINE_PLATFORM_H
#define BORELINE_PLATFORM_H

#include <stdint.h>

/* Platform event polling abstraction */

#define BORELINE_POLLIN  0x001
#define BORELINE_POLLOUT 0x002
#define BORELINE_POLLERR 0x004

typedef struct boreline_platform boreline_platform_t;

typedef struct {
    void *ctx;
    uint32_t events;
} boreline_event_t;

boreline_platform_t *boreline_platform_create(void);
void boreline_platform_destroy(boreline_platform_t *p);
int boreline_platform_add_fd(boreline_platform_t *p, int fd,
                              uint32_t events, void *ctx);
int boreline_platform_mod_fd(boreline_platform_t *p, int fd,
                              uint32_t events, void *ctx);
int boreline_platform_del_fd(boreline_platform_t *p, int fd);
int boreline_platform_wait(boreline_platform_t *p, int timeout_ms,
                            boreline_event_t *events_out, int max_events);

#endif /* BORELINE_PLATFORM_H */