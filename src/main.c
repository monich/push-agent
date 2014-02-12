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

#include "pa.h"
#include "pa_log.h"

#include <glib-object.h>
#include <glib-unix.h>

#include <stdio.h>

#define RET_OK (0)
#define RET_ERR (1)

static
gboolean
pa_signal_handler(
    gpointer arg)
{
    PA_INFO("Caught signal, shutting down...");
    if (arg) {
        push_agent_stop(arg);
    } else {
        exit(0);
    }
    return FALSE;
}

static
gboolean
pa_option_logtype(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    if (pa_log_set_type(value)) {
        return TRUE;
    } else {
        if (error) {
            *error = g_error_new(G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "Invalid log type \'%s\'", value);
        }
        return FALSE;
    }
}

static
gboolean
pa_parse_options(
    PushAgentConfig* config,
    int argc,
    char* argv[])
{
    gboolean ok;
    GError* error = NULL;
    gboolean verbose = FALSE;
    char* config_dir_help = g_strdup_printf(
        "Configuration directory [%s]",
        config->config_dir);
    GOptionContext* options;
    GOptionEntry entries[] = {
        { "config-dir", 'c', 0, G_OPTION_ARG_FILENAME,
          (void*)&config->config_dir, config_dir_help, "DIR" },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE,
           &verbose, "Enable verbose output", NULL },
        { "log-output", 'o', 0, G_OPTION_ARG_CALLBACK, pa_option_logtype,
          "Log output [stdout]", "<stdout|syslog|glib>" },
        { NULL }
    };

    options = g_option_context_new("- oFono push agent");
    g_option_context_add_main_entries(options, entries, NULL);
    ok = g_option_context_parse(options, &argc, &argv, &error);
    g_option_context_free(options);
    g_free(config_dir_help);
    if (verbose) pa_log_level = PA_LOGLEVEL_VERBOSE;

    if (ok) {
        PA_INFO("Starting");
        return TRUE;
    } else {
        fprintf(stderr, "%s\n", PA_ERRMSG(error));
        g_error_free(error);
        return FALSE;
    }
}

int main(int argc, char* argv[])
{
    int ret = RET_ERR;
    PushAgentConfig config;
    config.config_dir = "/etc/push-agent";
    config.dbus_timeout = 5000;
    pa_log_name = "push-agent";

#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    /* g_type_init has been deprecated since version 2.36
     * the type system is initialised automagically since then */
    g_type_init();
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

    if (pa_parse_options(&config, argc, argv)) {
        PushAgent* agent = push_agent_new(&config);
        if (agent) {
            GMainLoop* loop = g_main_loop_new(NULL, FALSE);
            g_unix_signal_add(SIGTERM, pa_signal_handler, agent);
            g_unix_signal_add(SIGINT, pa_signal_handler, agent);
            push_agent_run(agent, loop);
            g_main_loop_unref(loop);
            push_agent_free(agent);
            ret = RET_OK;
        }
    }
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
