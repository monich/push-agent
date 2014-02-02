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

#include "pa_dir.h"
#include "pa_log.h"

#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

struct push_dir_watcher {
    PushDirWatchProc proc;
    PushAgent* agent;
    char* dir;
    int inotify_fd;
    int watch_fd;
    guint watch_source_id;
    GIOChannel* watch_chan;
};

static
gboolean
push_dir_watcher_notify(
    GIOChannel* gio,
    GIOCondition condition,
    gpointer data)
{
    gboolean ok = FALSE;
    PushDirWatcher* watcher = data;
    int avail = 0;
    if (ioctl(watcher->inotify_fd, FIONREAD, &avail) >= 0) {
        gsize len = 0;
        GError* error = NULL;
        void* buf = g_malloc(avail);
        if (g_io_channel_read_chars(gio, buf, avail, &len, &error) ==
            G_IO_STATUS_NORMAL) {
            GPtrArray* dir = g_ptr_array_new();
            const guint8* ptr = buf;
            const struct inotify_event* event = (void*)ptr;
            while (len >= sizeof(*event)) {
                gsize eventlen = sizeof(*event) + event->len;
                PA_ASSERT(len >= eventlen);
                if (len >= eventlen) {
                    PA_VERBOSE("Notify: %s/%s", watcher->dir, event->name);
                    g_ptr_array_add(dir, (char*)event->name);
                    len -= eventlen;
                    event = (void*)(ptr += eventlen);
                } else {
                    break;
                }
            }
            watcher->proc(watcher->agent, (const char**)dir->pdata, dir->len);
            g_ptr_array_free(dir, TRUE);
            ok = TRUE;
        } else {
            PA_ERR("%s", PA_ERRMSG(error));
            g_error_free(error);
        }
        g_free(buf);
    } else {
        PA_ERR("Directory watch error: %s", strerror(errno));
    }
    return ok;
}

PushDirWatcher*
push_dir_watcher_new(
    const char* dir,
    PushDirWatchProc proc,
    PushAgent* agent)
{
    PushDirWatcher* watcher = g_new0(PushDirWatcher, 1);
    watcher->inotify_fd = inotify_init();
    if (watcher->inotify_fd >= 0) {
        watcher->watch_fd = inotify_add_watch(watcher->inotify_fd, dir,
            IN_CLOSE_WRITE | IN_DELETE | IN_MOVE);
        if (watcher->watch_fd >= 0) {
            watcher->watch_chan = g_io_channel_unix_new(watcher->inotify_fd);
            if (watcher->watch_chan) {
                g_io_channel_set_encoding(watcher->watch_chan, NULL, NULL);
                g_io_channel_set_buffered(watcher->watch_chan, FALSE);
                watcher->watch_source_id = g_io_add_watch(watcher->watch_chan,
                    G_IO_IN, push_dir_watcher_notify, watcher);
                if (watcher->watch_source_id) {
                    watcher->proc = proc;
                    watcher->agent = agent;
                    watcher->dir = g_strdup(dir);
                    return watcher;
                }
                g_io_channel_unref(watcher->watch_chan);
            }
            inotify_rm_watch(watcher->inotify_fd, watcher->watch_fd);
        }
        close(watcher->inotify_fd);
    }
    g_free(watcher);
    return NULL;
}

void
push_dir_watcher_free(
    PushDirWatcher* watcher)
{
    if (watcher) {
        if (watcher->inotify_fd >= 0) {
            if (watcher->watch_fd >= 0) {
                if (watcher->watch_chan) {
                    if (watcher->watch_source_id) {
                        g_source_remove(watcher->watch_source_id);
                    }
                    g_io_channel_unref(watcher->watch_chan);
                }
                inotify_rm_watch(watcher->inotify_fd, watcher->watch_fd);
            }
            close(watcher->inotify_fd);
        }
        g_free(watcher->dir);
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
