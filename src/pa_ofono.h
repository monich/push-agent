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

#ifndef JOLLA_PUSH_AGENT_OFONO_H
#define JOLLA_PUSH_AGENT_OFONO_H

#include "pa.h"

typedef struct push_ofono_watcher PushOfonoWatcher;
typedef void
(*PushNotificationProc)(
    PushAgent* agent,
    const char* imsi,
    const guint8* data,
    gsize len);

PushOfonoWatcher*
push_ofono_watcher_new(
    PushNotificationProc proc,
    PushAgent* agent);

void
push_ofono_watcher_free(
    PushOfonoWatcher* watcher);

#endif /* JOLLA_PUSH_AGENT_OFONO_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
