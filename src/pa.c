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
#include "pa_dir.h"
#include "pa_log.h"
#include "pa_ofono.h"

#include <gio/gio.h>
#include <wspcodec.h>
#include <string.h>

struct push_agent {
    const PushAgentConfig* config;
    PushOfonoWatcher* ofono;
    PushDirWatcher* config_watch;
    GSList* handlers;
    GMainLoop* loop;
};

typedef struct push_handler {
    char* name;
    char* content_type;
    char* interface;
    char* service;
    char* method;
    char* path;
} PushHandler;

static
void
push_handler_notify(
    PushAgent* agent,
    PushHandler* handler,
    const char* imsi,
    const char* content_type,
    const void* data,
    unsigned int len)
{
    GError* error = NULL;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (bus) {
        GVariant* result;
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("(ssay)"));
        g_variant_builder_add(&b, "s", imsi);
        g_variant_builder_add(&b, "s", content_type);
        g_variant_builder_add_value(&b, g_variant_new_from_data(
            G_VARIANT_TYPE_BYTESTRING, data, len, TRUE, NULL, NULL));
        result = g_dbus_connection_call_sync(bus, handler->service,
            handler->path, handler->interface, handler->method,
            g_variant_builder_end(&b), NULL, G_DBUS_CALL_FLAGS_NONE,
            agent->config->dbus_timeout, NULL, &error);
        if (result) {
            g_variant_unref(result);
        } else {
            PA_ERR("%s", PA_ERRMSG(error));
            g_error_free(error);
        }
        g_object_unref(bus);
    } else {
        PA_ERR("%s", PA_ERRMSG(error));
        g_error_free(error);
    }
}

static
void
push_handler_free(
    void* data)
{
    PushHandler* handler = data;
    g_free(handler->content_type);
    g_free(handler->interface);
    g_free(handler->service);
    g_free(handler->method);
    g_free(handler->path);
    g_free(handler->name);
    g_free(handler);
}

static
void
push_agent_parse_handler(
    PushAgent* agent,
    GKeyFile* conf,
    const char* g)
{
    /* These are required */
    char* interface = g_key_file_get_string(conf, g, "Interface", NULL);
    char* service = g_key_file_get_string(conf, g, "Service", NULL);
    char* method = g_key_file_get_string(conf, g, "Method", NULL);
    char* path = g_key_file_get_string(conf, g, "Path", NULL);
    if (interface && service && method && path) {
        PushHandler* h = g_new0(PushHandler, 1);
        h->name = g_strdup(g);
        h->interface = interface;
        h->service = service;
        h->method = method;
        h->path = path;

        /* Content type is optional */
        h->content_type = g_key_file_get_string(conf, g, "ContentType", NULL);

        PA_INFO("Registered %s", h->name);
        if (h->content_type) PA_DEBUG("  ContentType: %s", h->content_type);
        PA_DEBUG("  Interface: %s", interface);
        PA_DEBUG("  Service: %s", service);
        PA_DEBUG("  Method: %s", method);
        PA_DEBUG("  Path: %s", path);

        agent->handlers = g_slist_append(agent->handlers, h);
    } else {
        g_free(interface);
        g_free(service);
        g_free(method);
        g_free(path);
    }
}

static
gboolean
push_agent_config_file_match(
    const char* file)
{
    return g_str_has_suffix(file, ".conf");
}

static
void
push_agent_parse_config(
    PushAgent* agent)
{
    const char* config_dir = agent->config->config_dir;
    GDir* dir = g_dir_open(config_dir, 0, NULL);
    if (agent->handlers) {
        g_slist_free_full(agent->handlers, push_handler_free);
        agent->handlers = NULL;
    }
    if (dir) {
        const gchar* file;
        while ((file = g_dir_read_name(dir)) != NULL) {
            if (push_agent_config_file_match(file)) {
                GError* error = NULL;
                GKeyFile* conf = g_key_file_new();
                char* path = g_strconcat(config_dir, "/", file, NULL);
                PA_DEBUG("Reading %s", file);
                if (g_key_file_load_from_file(conf, path, 0, &error)) {
                    gsize i, n = 0;
                    char** names = g_key_file_get_groups(conf, &n);
                    for (i=0; i<n; i++) {
                        push_agent_parse_handler(agent, conf, names[i]);
                    }
                    g_strfreev(names);
                } else {
                    PA_WARN("%s", error->message);
                    g_error_free(error);
                }
                g_key_file_free(conf);
                g_free(path);
            }
        }
        g_dir_close(dir);
    } else {
        PA_WARN("%s directory not found", config_dir);
    }
}

static
void
push_agent_config_changed(
    PushAgent* agent,
    const char* files[],
    unsigned int count)
{
    unsigned int i;
    for (i=0; i<count; i++) {
        if (push_agent_config_file_match(files[i])) {
            PA_INFO("Reloading configuration");
            push_agent_parse_config(agent);
            break;
        }
    }
}

static
gboolean
push_handler_match(
    PushHandler* handler,
    const char* type)
{
    return (!handler->content_type || !strcmp(handler->content_type, type));
}

static
void
push_agent_notification(
    PushAgent* agent,
    const char* imsi,
    const guint8* pdu,
    gsize len)
{
    PA_INFO("Received %d bytes from %s", (int)len, imsi);
    /* First two bytes are Transaction ID and PDU Type */
    if (imsi && len >= 3 && pdu[1] == 6 /* Push PDU */) {
        guint remain = len - 2;
        const guint8* data = pdu + 2;
        unsigned int hdrlen = 0;
        unsigned int off = 0;
        if (wsp_decode_uintvar(data, remain, &hdrlen, &off) &&
            (off + hdrlen) <= remain) {
            const void* ct = NULL;
            data += off;
            remain -= off;
            PA_DEBUG("WAP header %u bytes", hdrlen);
            if (wsp_decode_content_type(data, hdrlen, &ct, &off, NULL)) {
                GSList* link = agent->handlers;
                remain -= hdrlen;
                data += hdrlen;
                PA_DEBUG("WSP payload %u bytes", remain);
                PA_DEBUG("Content type %s", (char*)ct);
                while (link) {
                    PushHandler* h = link->data;
                    if (push_handler_match(h, ct)) {
                        PA_INFO("Notifying %s", h->name);
                        push_handler_notify(agent, h, imsi, ct, data, remain);
                    }
                    link = link->next;
                }
            }
        }
    }
}

PushAgent*
push_agent_new(
    const PushAgentConfig* config)
{
    PushAgent* agent = g_new0(PushAgent, 1);
    agent->config = config;
    agent->ofono = push_ofono_watcher_new(push_agent_notification, agent);
    if (agent->ofono) {
        agent->config_watch = push_dir_watcher_new(config->config_dir,
            push_agent_config_changed, agent);
        PA_INFO("Loading configuration from %s", config->config_dir);
        push_agent_parse_config(agent);
        return agent;
    } else {
        g_free(agent);
        return NULL;
    }
}

void
push_agent_free(
    PushAgent* agent)
{
    if (agent) {
        PA_ASSERT(!agent->loop);
        push_dir_watcher_free(agent->config_watch);
        push_ofono_watcher_free(agent->ofono);
        g_slist_free_full(agent->handlers, push_handler_free);
        g_free(agent);
    }
}

void
push_agent_run(
    PushAgent* agent,
    GMainLoop* loop)
{
    PA_ASSERT(!agent->loop);
    agent->loop = loop;
    g_main_loop_run(loop);
    agent->loop = NULL;
}

void
push_agent_stop(
    PushAgent* agent)
{
    if (agent && agent->loop) {
        g_main_loop_quit(agent->loop);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
