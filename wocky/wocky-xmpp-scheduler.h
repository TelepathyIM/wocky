/*
 * wocky-xmpp-scheduler.h - Header for WockyXmppScheduler
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __WOCKY_XMPP_SCHEDULER_H__
#define __WOCKY_XMPP_SCHEDULER_H__

#include <glib-object.h>

#include "wocky-xmpp-connection.h"
#include "wocky-xmpp-stanza.h"

G_BEGIN_DECLS

typedef struct _WockyXmppScheduler WockyXmppScheduler;
typedef struct _WockyXmppSchedulerClass WockyXmppSchedulerClass;

struct _WockyXmppSchedulerClass {
    GObjectClass parent_class;
};

struct _WockyXmppScheduler {
    GObject parent;
};

GType wocky_xmpp_scheduler_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_SCHEDULER \
  (wocky_xmpp_scheduler_get_type ())
#define WOCKY_XMPP_SCHEDULER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_SCHEDULER, \
   WockyXmppScheduler))
#define WOCKY_XMPP_SCHEDULER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_SCHEDULER, \
   WockyXmppSchedulerClass))
#define WOCKY_IS_XMPP_SCHEDULER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_SCHEDULER))
#define WOCKY_IS_XMPP_SCHEDULER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_SCHEDULER))
#define WOCKY_XMPP_SCHEDULER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_SCHEDULER, \
   WockyXmppSchedulerClass))

WockyXmppScheduler * wocky_xmpp_scheduler_new (WockyXmppConnection *connection);

void wocky_xmpp_scheduler_send_full (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_scheduler_send_full_finish (
    WockyXmppScheduler *scheduler,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_scheduler_send (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza);

void wocky_xmpp_scheduler_start (WockyXmppScheduler *scheduler);

typedef gboolean (* WockyXmppSchedulerStanzaFilterFunc) (
    WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data);

typedef void (* WockyXmppSchedulerStanzaCallbackFunc) (
    WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data);

void wocky_xmpp_scheduler_add_stanza_filter (WockyXmppScheduler *scheduler,
    WockyXmppSchedulerStanzaFilterFunc filter,
    WockyXmppSchedulerStanzaCallbackFunc callback,
    gpointer user_data);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_SCHEDULER_H__*/
