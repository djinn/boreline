#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "boreline/boreline.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Structured logging with a pluggable callback.
 *
 * The default backend writes to stderr with a timestamp, level tag and
 * source location. Secrets are never logged: the wrapping convenience
 * macros are used with messages that must not contain tokens or bodies.
 */

static boreline_log_fn g_log_fn = NULL;
static void *g_log_userdata = NULL;
static boreline_log_level_t g_min_level = BORELINE_LOG_INFO;

static const char *level_tag(boreline_log_level_t level)
{
    switch (level) {
    case BORELINE_LOG_ERROR: return "ERROR";
    case BORELINE_LOG_WARN:  return "WARN";
    case BORELINE_LOG_INFO:  return "INFO";
    case BORELINE_LOG_DEBUG: return "DEBUG";
    case BORELINE_LOG_TRACE: return "TRACE";
    }
    return "?";
}

static void default_log_fn(boreline_log_level_t level, const char *file,
                           int line, const char *msg, void *userdata)
{
    (void)userdata;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_buf;
    localtime_r(&ts.tv_sec, &tm_buf);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    fprintf(stderr, "%s.%03ldZ %-5s %s:%d: %s\n",
            date, ts.tv_nsec / 1000000, level_tag(level), file, line, msg);
}

void boreline_set_log_fn(boreline_log_fn fn, void *userdata)
{
    g_log_fn = fn;
    g_log_userdata = userdata;
}

void boreline_set_log_level(boreline_log_level_t level)
{
    g_min_level = level;
}

void boreline_log(boreline_log_level_t level, const char *file, int line,
                   const char *fmt, ...)
{
    if (level > g_min_level)
        return;

    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (g_log_fn) {
        g_log_fn(level, file, line, msg, g_log_userdata);
    } else {
        default_log_fn(level, file, line, msg, NULL);
    }
}