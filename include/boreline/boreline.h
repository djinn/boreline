#ifndef BORELINE_H
#define BORELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Version information */
#define BORELINE_VERSION_MAJOR 0
#define BORELINE_VERSION_MINOR 1
#define BORELINE_VERSION_PATCH 0
#define BORELINE_VERSION "0.1.0"

/* Protocol constants */
#define BORELINE_MAGIC         0x424C
#define BORELINE_VERSION_PROTO 1
#define BORELINE_FRAME_HEADER_SIZE 16
#define BORELINE_MAX_PAYLOAD   (64 * 1024)
#define BORELINE_DEFAULT_DATA_TARGET (16 * 1024)
#define BORELINE_MAX_CONTROL_PAYLOAD (4 * 1024)
#define BORELINE_DEFAULT_WINDOW (256 * 1024)
#define BORELINE_MIN_WINDOW     (64 * 1024)
#define BORELINE_MAX_WINDOW     (4 * 1024 * 1024)
#define BORELINE_MAX_STREAMS    65536
#define BORELINE_MAX_SESSIONS   1024

/* Error codes */
typedef enum {
    BORELINE_OK                  = 0,
    BORELINE_ERR_GENERIC        = -1,
    BORELINE_ERR_NOMEM          = -2,
    BORELINE_ERR_INVAL          = -3,
    BORELINE_ERR_AGAIN          = -4,
    BORELINE_ERR_PROTOCOL       = -5,
    BORELINE_ERR_AUTH           = -6,
    BORELINE_ERR_TIMEOUT        = -7,
    BORELINE_ERR_CLOSED         = -8,
    BORELINE_ERR_OVERFLOW       = -9,
    BORELINE_ERR_EXISTS         = -10,
    BORELINE_ERR_NOTFOUND       = -11,
    BORELINE_ERR_LIMIT          = -12,
    BORELINE_ERR_TLS            = -13,
    BORELINE_ERR_BUSY           = -14,
    BORELINE_ERR_FLOW_CONTROL   = -15,
} boreline_error_t;

/* Frame types */
typedef enum {
    BORELINE_FRAME_HELLO         = 0x01,
    BORELINE_FRAME_AUTH          = 0x02,
    BORELINE_FRAME_AUTH_RESULT   = 0x03,
    BORELINE_FRAME_REGISTER      = 0x04,
    BORELINE_FRAME_REGISTER_RESULT = 0x05,
    BORELINE_FRAME_OPEN          = 0x06,
    BORELINE_FRAME_OPEN_RESULT   = 0x07,
    BORELINE_FRAME_DATA          = 0x08,
    BORELINE_FRAME_WINDOW_UPDATE = 0x09,
    BORELINE_FRAME_HALF_CLOSE    = 0x0A,
    BORELINE_FRAME_RESET         = 0x0B,
    BORELINE_FRAME_PING          = 0x0C,
    BORELINE_FRAME_PONG          = 0x0D,
    BORELINE_FRAME_ERROR         = 0x0E,
    BORELINE_FRAME_GOAWAY        = 0x0F,
} boreline_frame_type_t;

/* Frame flags */
typedef enum {
    BORELINE_FLAG_NONE       = 0x0000,
    BORELINE_FLAG_FIN        = 0x0001,
    BORELINE_FLAG_ACK        = 0x0002,
    BORELINE_FLAG_SYNC       = 0x0004,
    BORELINE_FLAG_PRIORITY   = 0x0008,
} boreline_frame_flags_t;

/* Stream states */
typedef enum {
    BORELINE_STREAM_IDLE,
    BORELINE_STREAM_OPEN,
    BORELINE_STREAM_HALF_CLOSED_LOCAL,
    BORELINE_STREAM_HALF_CLOSED_REMOTE,
    BORELINE_STREAM_CLOSED,
    BORELINE_STREAM_RESET,
} boreline_stream_state_t;

/* Connection states */
typedef enum {
    BORELINE_CONN_INIT,
    BORELINE_CONN_HELLO_SENT,
    BORELINE_CONN_AUTH_PENDING,
    BORELINE_CONN_AUTHENTICATED,
    BORELINE_CONN_ACTIVE,
    BORELINE_CONN_DRAINING,
    BORELINE_CONN_CLOSED,
    BORELINE_CONN_FAILED,
} boreline_conn_state_t;

/* Protocol error codes */
typedef enum {
    BORELINE_PROTO_ERR_NONE              = 0,
    BORELINE_PROTO_ERR_PROTOCOL_VIOLATION = 1,
    BORELINE_PROTO_ERR_INTERNAL          = 2,
    BORELINE_PROTO_ERR_FLOW_CONTROL      = 3,
    BORELINE_PROTO_ERR_STREAM_NOT_FOUND  = 4,
    BORELINE_PROTO_ERR_STREAM_CLOSED     = 5,
    BORELINE_PROTO_ERR_STREAM_LIMIT      = 6,
    BORELINE_PROTO_ERR_AUTH_FAILED       = 7,
    BORELINE_PROTO_ERR_TOKEN_REJECTED    = 8,
    BORELINE_PROTO_ERR_REGISTRATION      = 9,
    BORELINE_PROTO_ERR_RESOURCE_EXHAUSTED = 10,
    BORELINE_PROTO_ERR_TIMEOUT           = 11,
    BORELINE_PROTO_ERR_GOAWAY            = 12,
} boreline_proto_error_t;

/* Frame header structure */
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  type;
    uint16_t flags;
    uint16_t reserved;
    uint32_t stream_id;
    uint32_t payload_length;
} boreline_frame_header_t;

_Static_assert(sizeof(boreline_frame_header_t) == 16,
               "Frame header must be exactly 16 bytes");

/* Log levels */
typedef enum {
    BORELINE_LOG_ERROR,
    BORELINE_LOG_WARN,
    BORELINE_LOG_INFO,
    BORELINE_LOG_DEBUG,
    BORELINE_LOG_TRACE,
} boreline_log_level_t;

/* Log callback */
typedef void (*boreline_log_fn)(boreline_log_level_t level,
                                const char *file, int line,
                                const char *msg, void *userdata);

/* Service type */
typedef enum {
    BORELINE_SERVICE_HTTP,
    BORELINE_SERVICE_TCP,
} boreline_service_type_t;

/* Service registration */
typedef struct {
    char name[256];
    boreline_service_type_t type;
    char local_host[128];
    uint16_t local_port;
    char public_hostname[256];
    uint16_t public_port;
} boreline_service_t;

/* Ring buffer */
typedef struct boreline_ringbuf boreline_ringbuf_t;

/* Stream */
typedef struct boreline_stream boreline_stream_t;

/* Stream API */
boreline_stream_t *boreline_stream_create(uint32_t id, uint32_t initial_window);
void boreline_stream_destroy(boreline_stream_t *s);
uint32_t boreline_stream_id(const boreline_stream_t *s);
boreline_stream_state_t boreline_stream_state(const boreline_stream_t *s);
int boreline_stream_transition(boreline_stream_t *s, boreline_stream_state_t next);
int boreline_stream_half_close_local(boreline_stream_t *s);
int boreline_stream_half_close_remote(boreline_stream_t *s);
int boreline_stream_reset(boreline_stream_t *s);
bool boreline_stream_is_closed(const boreline_stream_t *s);
bool boreline_stream_can_send(const boreline_stream_t *s);
bool boreline_stream_can_receive(const boreline_stream_t *s);
int boreline_stream_consume_send_credit(boreline_stream_t *s, uint32_t n);
int boreline_stream_restore_send_credit(boreline_stream_t *s, uint32_t n);
int boreline_stream_received(boreline_stream_t *s, uint32_t n);
int boreline_stream_update_recv_window(boreline_stream_t *s, uint32_t base, uint32_t *update);
uint32_t boreline_stream_send_window(const boreline_stream_t *s);
uint32_t boreline_stream_recv_window(const boreline_stream_t *s);
uint64_t boreline_stream_bytes_sent(const boreline_stream_t *s);
uint64_t boreline_stream_bytes_received(const boreline_stream_t *s);
void boreline_stream_set_userdata(boreline_stream_t *s, void *ud);
void *boreline_stream_userdata(const boreline_stream_t *s);

/* Protocol parser */
typedef struct boreline_parser boreline_parser_t;

/* Timer */
typedef struct boreline_timer boreline_timer_t;

/* Connection */
typedef struct boreline_connection boreline_connection_t;

/* Configuration structures */
typedef struct {
    char public_bind[64];
    uint16_t public_port;
    char tunnel_bind[64];
    uint16_t tunnel_port;
    char domain[256];
    char tls_certificate[512];
    char tls_key[512];
    char token_store[512];
    char allowed_public_tcp_ports[256];
    uint32_t max_sessions;
    uint32_t max_streams_per_session;
    uint64_t max_memory_per_session;
    uint32_t request_timeout_ms;
    uint32_t idle_timeout_ms;
    char log_level[16];
    char metrics_bind[64];
    uint16_t metrics_port;
} boreline_server_config_t;

typedef struct {
    char server_address[256];
    uint16_t server_port;
    char certificate_trust[512];
    char token_source[512];
    uint32_t reconnect_base_ms;
    uint32_t reconnect_max_ms;
    uint32_t max_local_connections;
} boreline_client_config_t;

/* Public API functions - client */

/* Initialize the boreline library */
int boreline_init(void);

/* Cleanup the boreline library */
void boreline_cleanup(void);

/* Set log callback */
void boreline_set_log_fn(boreline_log_fn fn, void *userdata);

/* Set log level */
void boreline_set_log_level(boreline_log_level_t level);

const char *boreline_version(void);

/* Create a ring buffer */
boreline_ringbuf_t *boreline_ringbuf_create(size_t capacity);
void boreline_ringbuf_destroy(boreline_ringbuf_t *rb);
size_t boreline_ringbuf_write(boreline_ringbuf_t *rb,
                               const uint8_t *data, size_t len);
size_t boreline_ringbuf_read(boreline_ringbuf_t *rb,
                              uint8_t *data, size_t len);
size_t boreline_ringbuf_available(const boreline_ringbuf_t *rb);
size_t boreline_ringbuf_capacity(const boreline_ringbuf_t *rb);
void boreline_ringbuf_reset(boreline_ringbuf_t *rb);

/* Frame encoding/decoding */
int boreline_encode_header(boreline_frame_header_t *hdr,
                            uint8_t buf[BORELINE_FRAME_HEADER_SIZE]);
int boreline_decode_header(const uint8_t buf[BORELINE_FRAME_HEADER_SIZE],
                            boreline_frame_header_t *hdr);
const char *boreline_frame_type_str(boreline_frame_type_t type);
const char *boreline_error_str(int err);

/* Protocol parser */
boreline_parser_t *boreline_parser_create(void);
void boreline_parser_destroy(boreline_parser_t *p);
int boreline_parser_feed(boreline_parser_t *p,
                          const uint8_t *data, size_t len);
int boreline_parser_next(boreline_parser_t *p,
                          boreline_frame_header_t *hdr,
                          const uint8_t **payload);
size_t boreline_parser_buffered(const boreline_parser_t *p);

/* Timer API */
boreline_timer_t *boreline_timer_create(void);
void boreline_timer_destroy(boreline_timer_t *t);
int boreline_timer_start(boreline_timer_t *t, uint32_t ms);
void boreline_timer_stop(boreline_timer_t *t);
bool boreline_timer_expired(const boreline_timer_t *t);
uint64_t boreline_timer_remaining_ms(const boreline_timer_t *t);
int boreline_timer_reset(boreline_timer_t *t, uint32_t ms);

/* Configuration parsing */
int boreline_config_parse_server(const char *path,
                                  boreline_server_config_t *cfg);
int boreline_config_parse_client(const char *path,
                                  boreline_client_config_t *cfg);
void boreline_config_defaults_server(boreline_server_config_t *cfg);
void boreline_config_defaults_client(boreline_client_config_t *cfg);

/* Utility: constant-time memory comparison */
int boreline_constant_time_eq(const uint8_t *a, const uint8_t *b, size_t n);

/* Logging convenience macros */
#define BORELINE_LOG_ERROR(fmt, ...) \
    boreline_log(BORELINE_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BORELINE_LOG_WARN(fmt, ...) \
    boreline_log(BORELINE_LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BORELINE_LOG_INFO(fmt, ...) \
    boreline_log(BORELINE_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BORELINE_LOG_DEBUG(fmt, ...) \
    boreline_log(BORELINE_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BORELINE_LOG_TRACE(fmt, ...) \
    boreline_log(BORELINE_LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

void boreline_log(boreline_log_level_t level, const char *file, int line,
                   const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#endif /* BORELINE_H */
/* Client connection utilities */
uint32_t boreline_reconnect_delay(uint32_t attempt,
                                   uint32_t base_ms,
                                   uint32_t max_ms,
                                   const uint8_t *random_bytes, size_t n);

/* Service registration */
typedef struct boreline_service_reg boreline_service_reg_t;
int boreline_service_register(boreline_service_reg_t *reg,
                               const char *name,
                               boreline_service_type_t type,
                               const char *local_host,
                               uint16_t local_port);
