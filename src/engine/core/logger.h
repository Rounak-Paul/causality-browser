// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Severity levels, ordered low to high. Messages below the active
    threshold are dropped before formatting. */
typedef enum Eng_LogLevel {
    ENG_LOG_TRACE = 0,
    ENG_LOG_DEBUG,
    ENG_LOG_INFO,
    ENG_LOG_WARN,
    ENG_LOG_ERROR,
    ENG_LOG_FATAL,
} Eng_LogLevel;

/**
 * Optional sink invoked for every message that passes the active level
 * filter, in addition to the built-in stdout/stderr output. Called with
 * the log mutex held — must not call back into the logger.
 *
 * level      Severity of the message.
 * tag        Subsystem tag (e.g. "job", "net"); never NULL.
 * message    Fully formatted message text; never NULL.
 * user_data  Context pointer passed to eng_log_set_sink.
 */
typedef void (*Eng_LogSinkFn)(Eng_LogLevel level, const char *tag,
                               const char *message, void *user_data);

/**
 * Initializes the logger. Must be called once before any eng_log_* call,
 * typically first in engine startup.
 */
void eng_log_init(void);

/** Shuts down the logger and releases its internal mutex. */
void eng_log_shutdown(void);

/**
 * Sets the minimum severity that is formatted and emitted. Messages below
 * this level are dropped cheaply (level check only, no formatting).
 *
 * level  New minimum severity; defaults to ENG_LOG_INFO at init.
 */
void eng_log_set_level(Eng_LogLevel level);

/** Returns the current minimum severity. */
Eng_LogLevel eng_log_get_level(void);

/**
 * Installs an additional sink for formatted messages (e.g. an in-app
 * console panel). Pass fn = NULL to remove the current sink.
 *
 * fn         Sink callback, or NULL to clear.
 * user_data  Passed to fn on every call.
 */
void eng_log_set_sink(Eng_LogSinkFn fn, void *user_data);

/**
 * Formats and emits a log message; thread-safe. Prefer the ENG_LOG_*
 * macros below over calling this directly.
 *
 * level   Message severity.
 * tag     Subsystem tag, e.g. "job", "net", "ui".
 * file    Source file (__FILE__).
 * line    Source line (__LINE__).
 * fmt     printf-style format string.
 */
void eng_log_write(Eng_LogLevel level, const char *tag,
                    const char *file, int line, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;

#define ENG_LOG_TRACE(tag, fmt, ...) \
    eng_log_write(ENG_LOG_TRACE, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ENG_LOG_DEBUG(tag, fmt, ...) \
    eng_log_write(ENG_LOG_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ENG_LOG_INFO(tag, fmt, ...) \
    eng_log_write(ENG_LOG_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ENG_LOG_WARN(tag, fmt, ...) \
    eng_log_write(ENG_LOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ENG_LOG_ERROR(tag, fmt, ...) \
    eng_log_write(ENG_LOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ENG_LOG_FATAL(tag, fmt, ...) \
    eng_log_write(ENG_LOG_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
