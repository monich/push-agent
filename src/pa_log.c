/* Copyright (C) 2013-2014 Jolla Ltd.
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pa_log.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#  define vsnprintf     _vsnprintf
#  define strcasecmp    _stricmp
#  define strncasecmp   _strnicmp
#endif

gboolean pa_log_stdout_timestamp = TRUE;
int pa_log_level = PA_LOGLEVEL_DEFAULT;
PALogFunc pa_log_func = pa_log_stdout;
const char* pa_log_name = NULL;

#ifdef PA_LOG_SYSLOG
static const char PA_LOG_TYPE_SYSLOG[] = "syslog";
#endif /* PA_LOG_SYSLOG */
static const char PA_LOG_TYPE_STDOUT[] = "stdout";
static const char PA_LOG_TYPE_GLIB[]   = "glib";

/* Forwards output to stdout */
void
pa_log_stdout(
    int level,
    const char* format,
    va_list va)
{
    char t[32];
    char buf[512];
    const char* prefix = "";
    if (pa_log_stdout_timestamp) {
        time_t now;
        time(&now);
        strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S ", localtime(&now));
    } else {
        t[0] = 0;
    }
    switch (level) {
    case PA_LOGLEVEL_WARN: prefix = "WARNING: "; break;
    case PA_LOGLEVEL_ERR:  prefix = "ERROR: ";   break;
    default:                break;
    }
    vsnprintf(buf, sizeof(buf), format, va);
    if (pa_log_name) {
        printf("%s[%s] %s%s\n", t, pa_log_name, prefix, buf);
    } else {
        printf("%s%s%s\n", t, prefix, buf);
    }
}

/* Formards output to syslog */
#ifdef PA_LOG_SYSLOG
#include <syslog.h>
void
pa_log_syslog(
    int level,
    const char* format,
    va_list va)
{
    int priority;
    const char* prefix = NULL;
    switch (level) {
    default:
    case PA_LOGLEVEL_VERBOSE:
        priority = LOG_DEBUG;
        break;
    case PA_LOGLEVEL_DEBUG:
        priority = LOG_INFO;
        break;
    case PA_LOGLEVEL_INFO:
        priority = LOG_NOTICE;
        break;
    case PA_LOGLEVEL_WARN:
        priority = LOG_WARNING;
        prefix = "WARNING! ";
        break;
    case PA_LOGLEVEL_ERR:
        priority = LOG_ERR;
        prefix = "ERROR! ";
        break;
    }
    if (prefix && prefix[0]) {
        char buf[512];
        vsnprintf(buf, sizeof(buf), format, va);
        syslog(priority, "%s%s", prefix, buf);
    } else {
        vsyslog(priority, format, va);
    }
}
#endif /* PA_LOG_SYSLOG */

/* Forwards output to g_logv */
void
pa_log_glib(
    int level,
    const char* format,
    va_list va)
{
    GLogLevelFlags flags;
    switch (level) {
    default:
    case PA_LOGLEVEL_VERBOSE: flags = G_LOG_LEVEL_DEBUG;    break;
    case PA_LOGLEVEL_DEBUG:   flags = G_LOG_LEVEL_INFO;     break;
    case PA_LOGLEVEL_INFO:    flags = G_LOG_LEVEL_MESSAGE;  break;
    case PA_LOGLEVEL_WARN:    flags = G_LOG_LEVEL_WARNING;  break;
    case PA_LOGLEVEL_ERR:     flags = G_LOG_LEVEL_CRITICAL; break;
    }
    g_logv(pa_log_name, flags, format, va);
}

/* Logging functions */
static
void
pa_logv(
    int level,
    const char* format,
    va_list va)
{
    if (level != PA_LOGLEVEL_NONE) {
        PALogFunc log = pa_log_func;
        if (log && level <= pa_log_level) {
            log(level, format, va);
        }
    }
}

void
pa_log(
    int level,
    const char* format,
    ...)
{
    va_list va;
    va_start(va, format);
    pa_logv(level, format, va);
    va_end(va);
}

void
pa_log_assert(
    const char* expr,
    const char* file,
    int line)
{
    pa_log(PA_LOGLEVEL_ASSERT, "Assert %s at %s:%d\r", expr, file, line);
}

gboolean
pa_log_set_type(
    const char* type)
{
#ifdef PA_LOG_SYSLOG
    if (!strcasecmp(type, PA_LOG_TYPE_SYSLOG)) {
        if (pa_log_func != pa_log_syslog) {
            openlog(NULL, LOG_PID | LOG_CONS, LOG_USER);
        }
        pa_log_func = pa_log_syslog;
        return TRUE;
    }
    if (pa_log_func == pa_log_syslog) {
        closelog();
    }
#endif /* PA_LOG_SYSLOG */
    if (!strcasecmp(type, PA_LOG_TYPE_STDOUT)) {
        pa_log_func = pa_log_stdout;
        return TRUE;
    } else if (!strcasecmp(type, PA_LOG_TYPE_GLIB)) {
        pa_log_func = pa_log_glib;
        return TRUE;
    }
    return FALSE;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
