// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "logger.h"

#include <causality.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static Ca_Mutex     *g_log_mutex = NULL;
static Eng_LogLevel   g_log_level = ENG_LOG_INFO;
static Eng_LogSinkFn  g_sink_fn   = NULL;
static void          *g_sink_data = NULL;

static const char *level_name(Eng_LogLevel level)
{
    switch (level) {
        case ENG_LOG_TRACE: return "TRACE";
        case ENG_LOG_DEBUG: return "DEBUG";
        case ENG_LOG_INFO:  return "INFO";
        case ENG_LOG_WARN:  return "WARN";
        case ENG_LOG_ERROR: return "ERROR";
        case ENG_LOG_FATAL: return "FATAL";
        default:            return "?";
    }
}

/* ANSI color per level; stderr/stdout in a typical dev terminal both
   support this, and it is harmless (raw escape bytes) when redirected
   to a file or a non-ANSI console. */
static const char *level_color(Eng_LogLevel level)
{
    switch (level) {
        case ENG_LOG_TRACE: return "\x1b[90m";  /* bright black */
        case ENG_LOG_DEBUG: return "\x1b[36m";  /* cyan */
        case ENG_LOG_INFO:  return "\x1b[32m";  /* green */
        case ENG_LOG_WARN:  return "\x1b[33m";  /* yellow */
        case ENG_LOG_ERROR: return "\x1b[31m";  /* red */
        case ENG_LOG_FATAL: return "\x1b[97;41m"; /* white on red */
        default:            return "\x1b[0m";
    }
}

void eng_log_init(void)
{
    if (g_log_mutex) return;
    g_log_mutex = ca_mutex_create();
    g_log_level = ENG_LOG_INFO;
}

void eng_log_shutdown(void)
{
    if (!g_log_mutex) return;
    ca_mutex_destroy(g_log_mutex);
    g_log_mutex = NULL;
    g_sink_fn   = NULL;
    g_sink_data = NULL;
}

void eng_log_set_level(Eng_LogLevel level)
{
    g_log_level = level;
}

Eng_LogLevel eng_log_get_level(void)
{
    return g_log_level;
}

void eng_log_set_sink(Eng_LogSinkFn fn, void *user_data)
{
    if (!g_log_mutex) return;
    ca_mutex_lock(g_log_mutex);
    g_sink_fn   = fn;
    g_sink_data = user_data;
    ca_mutex_unlock(g_log_mutex);
}

void eng_log_write(Eng_LogLevel level, const char *tag,
                    const char *file, int line, const char *fmt, ...)
{
    if (level < g_log_level) return;

    /* Trim to the basename so lines stay short; file is always a
       compile-time __FILE__ literal, never NULL. */
    const char *slash = strrchr(file, '/');
    const char *base   = slash ? slash + 1 : file;

    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    struct tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &ts.tv_sec);
#else
    localtime_r(&ts.tv_sec, &tm_buf);
#endif
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_buf);

    if (g_log_mutex) ca_mutex_lock(g_log_mutex);

    FILE *stream = (level >= ENG_LOG_WARN) ? stderr : stdout;
    fprintf(stream, "%s%s.%03ld [%-5s] %-8s %s:%d: %s\x1b[0m\n",
            level_color(level), time_buf, ts.tv_nsec / 1000000,
            level_name(level), tag, base, line, message);

    if (g_sink_fn) g_sink_fn(level, tag, message, g_sink_data);

    if (g_log_mutex) ca_mutex_unlock(g_log_mutex);
}
