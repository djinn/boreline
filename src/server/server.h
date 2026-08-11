#ifndef BORELINE_SERVER_INTERNAL_H
#define BORELINE_SERVER_INTERNAL_H

#include "boreline/boreline.h"
#include <stdbool.h>
#include <stdint.h>

/* ---- listener.c ---- */
int boreline_listener_create(const char *bind_addr, uint16_t port, int backlog);

/* ---- session.c ---- */
typedef struct boreline_session boreline_session_t;
int boreline_session_create(uint32_t id, uint64_t memory_limit,
                             boreline_session_t **out);
void boreline_session_destroy(boreline_session_t *s);
bool boreline_session_is_authenticated(const boreline_session_t *s);
int boreline_session_authenticate(boreline_session_t *s, const char *token_hash);
uint32_t boreline_session_id(const boreline_session_t *s);

/* ---- routing.c ---- */
int boreline_routing_add(const char *hostname, uint32_t session_id,
                          const char *local_host, uint16_t local_port,
                          boreline_service_type_t type);
int boreline_routing_remove(const char *hostname);
int boreline_routing_lookup(const char *hostname, uint32_t *session_id,
                             char *local_host, uint16_t *local_port);
int boreline_routing_remove_session(uint32_t session_id);
void boreline_routing_clear(void);

/* ---- admin.c ---- */
void boreline_metrics_init(void);
void boreline_metrics_record_auth_failure(void);
void boreline_metrics_record_protocol_error(void);
void boreline_metrics_record_connection(void);
void boreline_metrics_disconnect(void);
char *boreline_metrics_json(char *buf, size_t len);

#endif /* BORELINE_SERVER_INTERNAL_H */