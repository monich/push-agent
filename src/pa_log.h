/*
 * Copyright (C) 2013-2014 Jolla Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef JOLLA_PA_LOG_H
#define JOLLA_PA_LOG_H

#include <glib.h>
#include <stdarg.h>

/* Log levels */
#define PA_LOGLEVEL_NONE            (0)
#define PA_LOGLEVEL_ERR             (1)
#define PA_LOGLEVEL_WARN            (2)
#define PA_LOGLEVEL_INFO            (3)
#define PA_LOGLEVEL_DEBUG           (4)
#define PA_LOGLEVEL_VERBOSE         (5)

/* Allow these to be redefined */
#ifndef PA_LOGLEVEL_MAX
#  ifdef DEBUG
#    define PA_LOGLEVEL_MAX         PA_LOGLEVEL_VERBOSE
#  else
#    define PA_LOGLEVEL_MAX         PA_LOGLEVEL_DEBUG
#  endif
#endif /* PA_LOGLEVEL_MAX */

#ifndef PA_LOGLEVEL_DEFAULT
#  ifdef DEBUG
#    define PA_LOGLEVEL_DEFAULT     PA_LOGLEVEL_DEBUG
#  else
#    define PA_LOGLEVEL_DEFAULT     PA_LOGLEVEL_INFO
#  endif
#endif /* PA_LOGLEVEL_DEFAULT */

/* Do we need a separate log level for ASSERTs? */
#ifndef PA_LOGLEVEL_ASSERT
#  ifdef DEBUG
#    define PA_LOGLEVEL_ASSERT      PA_LOGLEVEL_ERR
#  else
     /* No asserts in release build */
#    define PA_LOGLEVEL_ASSERT      (PA_LOGLEVEL_MAX+1)
#  endif
#endif

/* Set log type by name ("syslog", "stdout" or "glib"). This is
 * primarily for parsing command line options */
gboolean
pa_log_set_type(
    const char* type);

/* Logging function */
void
pa_log(
    int level,                      /* Message log level */
    const char* format,             /* Message format */
    ...) G_GNUC_PRINTF(2,3);        /* Followed by arguments */

void
pa_log_assert(
    const char* expr,               /* Assert expression */
    const char* file,               /* File name */
    int line);                      /* Line number */

#ifdef unix
#  define PA_LOG_SYSLOG
#endif

/* Available log handlers */
#define PA_DEFINE_LOG_FN(fn) void fn(int level, const char* fmt, va_list va)
PA_DEFINE_LOG_FN(pa_log_stdout);
PA_DEFINE_LOG_FN(pa_log_glib);
#ifdef PA_LOG_SYSLOG
PA_DEFINE_LOG_FN(pa_log_syslog);
#endif

/* Log configuration */
typedef PA_DEFINE_LOG_FN((*PALogFunc));
extern PALogFunc pa_log_func;
extern const char* pa_log_name;
extern int pa_log_level;
extern gboolean pa_log_stdout_timestamp;

/* Logging macros */

#define PA_LOG_NOTHING ((void)0)
#define PA_ERRMSG(err) (((err) && (err)->message) ? (err)->message : \
  "Unknown error")

#define PA_LOG_ENABLED              (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_NONE)
#define PA_LOG_ERR                  (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_ERR)
#define PA_LOG_WARN                 (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_WARN)
#define PA_LOG_INFO                 (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_INFO)
#define PA_LOG_DEBUG                (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_DEBUG)
#define PA_LOG_VERBOSE              (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_VERBOSE)
#define PA_LOG_ASSERT               (PA_LOGLEVEL_MAX >= PA_LOGLEVEL_ASSERT)

#if PA_LOG_ASSERT
#  define PA_ASSERT(expr)           ((expr) ? PA_LOG_NOTHING : \
                                    pa_log_assert(#expr, __FILE__, __LINE__))
#  define PA_VERIFY(expr)           PA_ASSERT(expr)
#else
#  define PA_ASSERT(expr)
#  define PA_VERIFY(expr)           (expr)
#endif

#if PA_LOG_ERR
#  define PA_ERR(f,args...)         pa_log(PA_LOGLEVEL_ERR, f, ##args)
#  define PA_ERR_(f,args...)        pa_log(PA_LOGLEVEL_ERR, "%s() " f, \
                                    __FUNCTION__, ##args)
#else
#  define PA_ERR(f,args...)         PA_LOG_NOTHING
#  define PA_ERR_(f,args...)        PA_LOG_NOTHING
#endif /* PA_LOG_ERR */

#if PA_LOG_WARN
#  define PA_WARN(f,args...)        pa_log(PA_LOGLEVEL_WARN, f, ##args)
#  define PA_WARN_(f,args...)       pa_log(PA_LOGLEVEL_WARN, "%s() " f, \
                                    __FUNCTION__, ##args)
#else
#  define PA_WARN(f,args...)        PA_LOG_NOTHING
#  define PA_WARN_(f,args...)       PA_LOG_NOTHING
#endif /* PA_LOGL_WARN */

#if PA_LOG_INFO
#  define PA_INFO(f,args...)        pa_log(PA_LOGLEVEL_INFO, f, ##args)
#  define PA_INFO_(f,args...)       pa_log(PA_LOGLEVEL_INFO, "%s() " f, \
                                    __FUNCTION__, ##args)
#else
#  define PA_INFO(f,args...)        PA_LOG_NOTHING
#  define PA_INFO_(f,args...)       PA_LOG_NOTHING
#endif /* PA_LOG_INFO */

#if PA_LOG_DEBUG
#  define PA_DEBUG(f,args...)       pa_log(PA_LOGLEVEL_DEBUG, f, ##args)
#  define PA_DEBUG_(f,args...)      pa_log(PA_LOGLEVEL_DEBUG, "%s() " f, \
                                    __FUNCTION__, ##args)
#else
#  define PA_DEBUG(f,args...)       PA_LOG_NOTHING
#  define PA_DEBUG_(f,args...)      PA_LOG_NOTHING
#endif /* PA_LOG_DEBUG */

#if PA_LOG_VERBOSE
#  define PA_VERBOSE(f,args...)     pa_log(PA_LOGLEVEL_VERBOSE, f, ##args)
#  define PA_VERBOSE_(f,args...)    pa_log(PA_LOGLEVEL_VERBOSE, "%s() " f, \
                                    __FUNCTION__, ##args)
#else
#  define PA_VERBOSE(f,args...)     PA_LOG_NOTHING
#  define PA_VERBOSE_(f,args...)    PA_LOG_NOTHING
#endif /* PA_LOG_VERBOSE */

#endif /* JOLLA_PA_LOG_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
