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

#ifndef JOLLA_PUSH_AGENT_DIR_H
#define JOLLA_PUSH_AGENT_DIR_H

#include "pa.h"

typedef struct push_dir_watcher PushDirWatcher;
typedef void
(*PushDirWatchProc)(
    PushAgent* agent,
    const char* files[],
    unsigned int count);

PushDirWatcher*
push_dir_watcher_new(
    const char* dir,
    PushDirWatchProc proc,
    PushAgent* agent);

void
push_dir_watcher_free(
    PushDirWatcher* watcher);

#endif /* JOLLA_PUSH_AGENT_DIR_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
