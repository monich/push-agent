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

#ifndef JOLLA_PUSH_AGENT_H
#define JOLLA_PUSH_AGENT_H

#include <glib.h>

typedef struct push_agent PushAgent;
typedef struct push_agent_config {
    const char* config_dir;
    int dbus_timeout;
} PushAgentConfig;

PushAgent*
push_agent_new(
    const PushAgentConfig* config);

void
push_agent_free(
    PushAgent* agent);

void
push_agent_run(
    PushAgent* agent,
    GMainLoop* loop);

void
push_agent_stop(
    PushAgent* agent);

#endif /* JOLLA_PUSH_AGENT_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
