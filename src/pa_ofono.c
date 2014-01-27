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

#include "pa.h"
#include "pa_log.h"
#include "pa_ofono.h"

#include <string.h>

/* Generated headers */
#include "org.ofono.Manager.h"
#include "org.ofono.Modem.h"
#include "org.ofono.PushNotification.h"
#include "org.ofono.PushNotificationAgent.h"
#include "org.ofono.SimManager.h"

#define OFONO_BUS               G_BUS_TYPE_SYSTEM

#define OFONO_SERVICE           "org.ofono"
#define OFONO_SIM_INTERFACE     OFONO_SERVICE   ".SimManager"
#define OFONO_PUSH_INTERFACE    OFONO_SERVICE   ".PushNotification"

#define OFONO_MODEM_PROPERTY_INTERFACES         "Interfaces"
#define OFONO_SIM_PROPERTY_SUBSCRIBER_IDENTITY  "SubscriberIdentity"

typedef struct push_ofono PushOfono;
typedef struct push_modem PushModem;

struct push_modem {
    char* path;
    PushOfono* ofono;

    OrgOfonoModem* modem_proxy;
    gulong property_change_signal_id;

    char* imsi;
    OrgOfonoSimManager* sim_proxy;
    gulong sim_property_change_signal_id;

    OrgOfonoPushNotification* push_proxy;
    GDBusInterfaceSkeleton* push_agent_skeleton;
    gulong agent_receive_notification_signal_id;
    gulong agent_release_signal_id;
};

struct push_ofono {
    PushOfonoWatcher* watcher;
    GHashTable* modems;
    OrgOfonoManager* manager_proxy;
    gulong modem_added_signal_id;
    gulong modem_removed_signal_id;
};

struct push_ofono_watcher {
    GDBusConnection* bus;
    PushOfono* ofono;
    guint ofono_watch_id;
    PushNotificationProc notification_proc;
    PushAgent* agent;
};

static
gboolean /* org.ofono.PushNotificationAgent.ReceiveNotification */
push_notification_agent_receive_notification(
    OrgOfonoPushNotificationAgent* proxy,
    GDBusMethodInvocation* call,
    GVariant* data,
    GHashTable* dict,
    PushModem* modem)
{
    gsize len = 0;
    const guint8* bytes = g_variant_get_fixed_array(data, &len, 1);
    PushOfonoWatcher* watcher = modem->ofono->watcher;
    PA_VERBOSE_("%s %d bytes", modem->path, (int)len);
    if (watcher->notification_proc) {
        watcher->notification_proc(watcher->agent, modem->imsi, bytes, len);
    }
    org_ofono_push_notification_agent_complete_receive_notification(proxy,call);
    return TRUE;
}

static
gboolean /* org.ofono.PushNotificationAgent.Release */
push_notification_agent_release(
    OrgOfonoPushNotificationAgent* proxy,
    GDBusMethodInvocation* call,
    PushModem* modem)
{
    PA_INFO("Release %s", modem->path);
    org_ofono_push_notification_agent_complete_release(proxy, call);
    return TRUE;
}

static
void
push_modem_free(
    PushModem* modem)
{
    if (modem) {
        PA_VERBOSE_("%p '%s'", modem, modem->path);
        if (modem->sim_proxy) {
            g_signal_handler_disconnect(modem->sim_proxy,
                modem->sim_property_change_signal_id);
            g_object_unref(modem->sim_proxy);
        }
        if (modem->push_proxy) {
            GError* error = NULL;
            if (!org_ofono_push_notification_call_unregister_agent_sync(
                modem->push_proxy, modem->path, NULL, &error)) {
                PA_ERR("%s: %s", modem->path, PA_ERRMSG(error));
                g_error_free(error);
            }
            g_object_unref(modem->push_proxy);
        }

        g_signal_handler_disconnect(modem->modem_proxy,
            modem->property_change_signal_id);
        g_signal_handler_disconnect(modem->push_agent_skeleton,
            modem->agent_receive_notification_signal_id);
        g_object_unref(modem->modem_proxy);

        g_signal_handler_disconnect(modem->push_agent_skeleton,
            modem->agent_release_signal_id);
        g_dbus_interface_skeleton_unexport(modem->push_agent_skeleton);
        g_object_unref(modem->push_agent_skeleton);

        g_free(modem->path);
        g_free(modem->imsi);
        g_free(modem);
    }
}

static
char*
push_modem_query_imsi(
    OrgOfonoSimManager* proxy)
{
    char* imsi = NULL;
    GError* error = NULL;
    GVariant* properties = NULL;
    if (org_ofono_sim_manager_call_get_properties_sync(proxy, &properties,
        NULL, &error)) {
        GVariant* imsi_value = g_variant_lookup_value(properties,
            OFONO_SIM_PROPERTY_SUBSCRIBER_IDENTITY, G_VARIANT_TYPE_STRING);
        if (imsi_value) {
            imsi = g_strdup(g_variant_get_string(imsi_value, NULL));
            g_variant_unref(imsi_value);
            PA_VERBOSE("IMSI: %s", imsi);
        }
        g_variant_unref(properties);
    } else {
        PA_ERR("%s", PA_ERRMSG(error));
        g_error_free(error);
    }
    return imsi;
}

static
void /* org.ofono.SimManager.PropertyChanged */
push_modem_sim_property_changed(
    OrgOfonoSimManager* proxy,
    const char* key,
    GVariant* value,
    PushModem* modem)
{
    PA_VERBOSE_("%p %s", modem->path, key);
    PA_ASSERT(proxy == modem->sim_proxy);
    if (!strcmp(key, OFONO_SIM_PROPERTY_SUBSCRIBER_IDENTITY)) {
        GVariant* variant = g_variant_get_variant(value);
        g_free(modem->imsi);
        modem->imsi = g_strdup(g_variant_get_string(variant, NULL));
        g_variant_unref(variant);
        PA_VERBOSE("IMSI: %s", modem->imsi);
    }
}

static
void
push_modem_scan_interfaces(
    PushModem* modem,
    GVariant* ifs)
{
    GError* error = NULL;
    gboolean sim_interface = FALSE;
    gboolean push_interface = FALSE;

    if (ifs) {
        GVariantIter iter;
        GVariant* child;
        for (g_variant_iter_init(&iter, ifs);
             (child = g_variant_iter_next_value(&iter)) &&
             (!sim_interface || !push_interface);
             g_variant_unref(child)) {
            const char* ifname = NULL;
            g_variant_get(child, "&s", &ifname);
            if (ifname) {
                if (!strcmp(ifname, OFONO_SIM_INTERFACE)) {
                    PA_VERBOSE("  Found %s", ifname);
                    sim_interface = TRUE;
                } else if (!strcmp(ifname, OFONO_PUSH_INTERFACE)) {
                    PA_VERBOSE("  Found %s", ifname);
                    push_interface = TRUE;
                }
            }
        }
    }

    /* org.ofono.SimManager */
    if (modem->imsi) {
        g_free(modem->imsi);
        modem->imsi = NULL;
    }
    if (sim_interface) {
        if (!modem->sim_proxy) {
            modem->sim_proxy = org_ofono_sim_manager_proxy_new_sync(
                modem->ofono->watcher->bus, G_DBUS_PROXY_FLAGS_NONE,
                OFONO_SERVICE, modem->path, NULL, &error);
            if (modem->sim_proxy) {
                /* Subscribe for PropertyChanged notifications */
                modem->sim_property_change_signal_id = g_signal_connect(
                    modem->sim_proxy, "property-changed",
                    G_CALLBACK(push_modem_sim_property_changed),
                    modem);
            } else {
                PA_ERR("%s: %s", modem->path, PA_ERRMSG(error));
                g_error_free(error);
                error = NULL;
            }
        }
        if (modem->sim_proxy) {
            modem->imsi = push_modem_query_imsi(modem->sim_proxy);
        }
    } else if (modem->sim_proxy) {
        g_signal_handler_disconnect(modem->sim_proxy,
            modem->sim_property_change_signal_id);
        g_object_unref(modem->sim_proxy);
        modem->sim_proxy = NULL;
    }

    /* org.ofono.PushNotification */
    if (push_interface) {
        if (!modem->push_proxy) {
            modem->push_proxy = org_ofono_push_notification_proxy_new_sync(
                modem->ofono->watcher->bus, G_DBUS_PROXY_FLAGS_NONE,
                OFONO_SERVICE, modem->path, NULL, &error);
            if (modem->push_proxy) {
                if (org_ofono_push_notification_call_register_agent_sync(
                    modem->push_proxy, modem->path, NULL, &error)) {
                    PA_DEBUG("Registered with %s", modem->path);
                } else {
                    PA_ERR("%s: %s", modem->path, PA_ERRMSG(error));
                    g_error_free(error);
                    error = NULL;
                    g_object_unref(modem->push_proxy);
                    modem->push_proxy = NULL;
                }
            } else {
                PA_ERR("%s: %s", modem->path, PA_ERRMSG(error));
                g_error_free(error);
                error = NULL;
            }
        }
    } else if (modem->push_proxy) {
        g_object_unref(modem->push_proxy);
        modem->push_proxy = NULL;
    }
}

static
void /* org.ofono.Modem.PropertyChanged */
push_modem_property_changed(
    OrgOfonoModem* proxy,
    const char* key,
    GVariant* variant,
    PushModem* modem)
{
    PA_VERBOSE_("%s %s", modem->path, key);
    PA_ASSERT(proxy == modem->modem_proxy);
    if (!strcmp(key, OFONO_MODEM_PROPERTY_INTERFACES)) {
        GVariant* value = g_variant_get_variant(variant);
        push_modem_scan_interfaces(modem, value);
        g_variant_unref(value);
    }
}

static
PushModem*
push_modem_new(
    PushOfono* ofono,
    const char* path,
    GVariant* properties)
{
    GError* error = NULL;
    PushModem* modem = g_new0(PushModem, 1);
    PA_DEBUG("Modem path %s", path);
    modem->ofono = ofono;
    modem->modem_proxy = org_ofono_modem_proxy_new_sync(
        modem->ofono->watcher->bus, G_DBUS_PROXY_FLAGS_NONE,
        OFONO_SERVICE, path, NULL, &error);
    if (modem->modem_proxy) {
        modem->push_agent_skeleton = G_DBUS_INTERFACE_SKELETON(
            org_ofono_push_notification_agent_skeleton_new());

        /* Make interface available */
        if (g_dbus_interface_skeleton_export(modem->push_agent_skeleton,
            modem->ofono->watcher->bus, path, &error)) {

            GVariant* interfaces = g_variant_lookup_value(properties,
                OFONO_MODEM_PROPERTY_INTERFACES, G_VARIANT_TYPE_STRING_ARRAY);

            modem->path = g_strdup(path);

            /* Check what we currently have */
            push_modem_scan_interfaces(modem, interfaces);
            g_variant_unref(interfaces);

            /* Register to receive PropertyChanged notifications */
            modem->property_change_signal_id = g_signal_connect(
                modem->modem_proxy, "property-changed",
                G_CALLBACK(push_modem_property_changed), modem);

            /* Connect the signals */
            modem->agent_receive_notification_signal_id = g_signal_connect(
                modem->push_agent_skeleton, "handle-receive-notification",
                G_CALLBACK(push_notification_agent_receive_notification),
                modem);

            modem->agent_release_signal_id = g_signal_connect(
                modem->push_agent_skeleton, "handle-release",
                G_CALLBACK(push_notification_agent_release),
                modem);

            return modem;
        } else {
            PA_ERR("%s: %s", path, PA_ERRMSG(error));
            g_error_free(error);
        }
        g_object_unref(modem->modem_proxy);
    } else {
        PA_ERR("%s: %s", path, PA_ERRMSG(error));
        g_error_free(error);
    }
    g_free(modem);
    return NULL;
}

static
void /* org.ofono.Manager.ModemAdded */
push_ofono_modem_added(
    OrgOfonoManager* proxy,
    const char* path,
    GVariant* properties,
    PushOfono* ofono)
{
    PushModem* modem;
    PA_VERBOSE_("%s", path);
    PA_ASSERT(proxy == ofono->manager_proxy);
    g_hash_table_remove(ofono->modems, path);
    modem = push_modem_new(ofono, path, properties);
    if (modem) g_hash_table_replace(ofono->modems, modem->path, modem);
}

static
void /* org.ofono.Manager.ModemRemoved */
push_ofono_modem_removed(
    OrgOfonoManager* proxy,
    const char* path,
    PushOfono* ofono)
{
    PA_VERBOSE_("%s", path);
    PA_ASSERT(proxy == ofono->manager_proxy);
    PushModem* modem = g_hash_table_lookup(ofono->modems, path);
    PA_ASSERT(modem);
    if (modem) {
        if (modem->push_proxy) {
            /* So that push_modem_free does't try to unregister */
            g_object_unref(modem->push_proxy);
            modem->push_proxy = NULL;
        }
        g_hash_table_remove(ofono->modems, path);
    }
}

static
void
push_ofono_modem_free_proc(
    gpointer data)
{
    push_modem_free(data);
}

static
PushOfono*
push_ofono_new(
    PushOfonoWatcher* watcher)
{
    GError* error = NULL;
    PushOfono* ofono = g_new0(PushOfono, 1);
    ofono->watcher = watcher;
    ofono->manager_proxy = org_ofono_manager_proxy_new_sync(watcher->bus,
        G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, "/", NULL, &error);
    if (ofono->manager_proxy) {

        /* Fetch current list of modems */
        GVariant* modems = NULL;
        if (org_ofono_manager_call_get_modems_sync(ofono->manager_proxy,
            &modems, NULL, &error)) {

            GVariantIter iter;
            GVariant* child;

            ofono->modems = g_hash_table_new_full(g_str_hash, g_str_equal,
                NULL, push_ofono_modem_free_proc);

            PA_DEBUG("%d modem(s) found", (int)g_variant_n_children(modems));
            for (g_variant_iter_init(&iter, modems);
                 (child = g_variant_iter_next_value(&iter)) != NULL;
                 g_variant_unref(child)) {

                PushModem* modem;
                const char* path = NULL;
                GVariant* properties = NULL;

                g_variant_get(child, "(&o@a{sv})", &path, &properties);
                PA_ASSERT(path);
                PA_ASSERT(properties);

                modem = push_modem_new(ofono, path, properties);
                g_hash_table_replace(ofono->modems, modem->path, modem);
                g_variant_unref(properties);
            }

            /* Subscribe for ModemAdded/Removed notifications */
            ofono->modem_added_signal_id = g_signal_connect(
                ofono->manager_proxy, "modem-added",
                G_CALLBACK(push_ofono_modem_added), ofono);
            ofono->modem_removed_signal_id = g_signal_connect(
                ofono->manager_proxy, "modem-removed",
                G_CALLBACK(push_ofono_modem_removed), ofono);

            return ofono;
        } else {
            PA_ERR("%s", PA_ERRMSG(error));
            g_error_free(error);
        }

        g_object_unref(ofono->manager_proxy);
    } else {
        PA_ERR("%s", PA_ERRMSG(error));
        g_error_free(error);
    }
    g_free(ofono);
    return NULL;
}

static
void
push_ofono_free(
    PushOfono* ofono)
{
    if (ofono) {
        g_hash_table_destroy(ofono->modems);
        g_signal_handler_disconnect(ofono->manager_proxy,
            ofono->modem_added_signal_id);
        g_signal_handler_disconnect(ofono->manager_proxy,
            ofono->modem_removed_signal_id);
        g_object_unref(ofono->manager_proxy);
        g_free(ofono);
    }
}

static
void
push_ofono_gone(
    gpointer key,
    gpointer value,
    gpointer user_data)
{
    PushModem* modem = value;
    if (modem->push_proxy) {
        /* So that push_modem_free does't try to unregister */
        g_object_unref(modem->push_proxy);
        modem->push_proxy = NULL;
    }
}

static
void
push_ofono_vanished(
    GDBusConnection* bus,
    const char* name,
    gpointer self)
{
    PushOfonoWatcher* watcher = self;
    PA_DEBUG("Name '%s' has disappeared", name);
    if (watcher->ofono) {
        g_hash_table_foreach(watcher->ofono->modems, push_ofono_gone, watcher);
        push_ofono_free(watcher->ofono);
        watcher->ofono = NULL;
    }
}

static
void
push_ofono_appeared(
    GDBusConnection* bus,
    const char* name,
    const char* owner,
    gpointer self)
{
    PushOfonoWatcher* watcher = self;
    PA_DEBUG("Name '%s' is owned by %s", name, owner);
    PA_ASSERT(!watcher->ofono);
    push_ofono_free(watcher->ofono);
    watcher->ofono = push_ofono_new(watcher);
}

PushOfonoWatcher*
push_ofono_watcher_new(
    PushNotificationProc proc,
    PushAgent* agent)
{
    GError* error = NULL;
    PushOfonoWatcher* watcher = g_new0(PushOfonoWatcher, 1);
    watcher->bus = g_bus_get_sync(OFONO_BUS, NULL, &error);
    if (watcher->bus) {
        watcher->ofono_watch_id = g_bus_watch_name_on_connection(watcher->bus,
            OFONO_SERVICE, G_BUS_NAME_WATCHER_FLAGS_NONE, push_ofono_appeared,
            push_ofono_vanished, watcher, NULL);
        PA_ASSERT(watcher->ofono_watch_id);
        watcher->notification_proc = proc;
        watcher->agent = agent;
        return watcher;
    } else {
        PA_ERR("%s", PA_ERRMSG(error));
        g_error_free(error);
    }
    g_free(watcher);
    return NULL;
}

void
push_ofono_watcher_free(
    PushOfonoWatcher* watcher)
{
    if (watcher) {
        g_bus_unwatch_name(watcher->ofono_watch_id);
        push_ofono_free(watcher->ofono);
        g_object_unref(watcher->bus);
        g_free(watcher);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
