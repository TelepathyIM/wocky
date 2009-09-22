/*
 * wocky-pubsub-service.h - Header of WockyPubsubService
 * Copyright (C) 2009 Collabora Ltd.
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

#ifndef __WOCKY_PUBSUB_SERVICE_H__
#define __WOCKY_PUBSUB_SERVICE_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-session.h>

G_BEGIN_DECLS

typedef struct _WockyPubsubService WockyPubsubService;
typedef struct _WockyPubsubServiceClass WockyPubsubServiceClass;

struct _WockyPubsubServiceClass {
  GObjectClass parent_class;
};

struct _WockyPubsubService {
  GObject parent;
};

GType wocky_pubsub_service_get_type (void);

#define WOCKY_TYPE_PUBSUB_SERVICE \
  (wocky_pubsub_service_get_type ())
#define WOCKY_PUBSUB_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PUBSUB_SERVICE, \
   WockyPubsubService))
#define WOCKY_PUBSUB_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PUBSUB_SERVICE, \
   WockyPubsubServiceClass))
#define WOCKY_IS_PUBSUB_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PUBSUB_SERVICE))
#define WOCKY_IS_PUBSUB_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PUBSUB_SERVICE))
#define WOCKY_PUBSUB_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PUBSUB_SERVICE, \
   WockyPubsubServiceClass))

WockyPubsubService * wocky_pubsub_service_new (WockySession *session,
    const gchar *jid);

G_END_DECLS

#endif /* __WOCKY_PUBSUB_SERVICE_H__ */
