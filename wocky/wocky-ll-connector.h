/*
 * wocky-ll-connector.h - Header for WockyLLConnector
 * Copyright (C) 2011 Collabora Ltd.
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_LL_CONNECTOR_H__
#define __WOCKY_LL_CONNECTOR_H__

#include <glib-object.h>

#include <gio/gio.h>

#include "wocky-xmpp-connection.h"

G_BEGIN_DECLS

typedef struct _WockyLLConnector WockyLLConnector;
typedef struct _WockyLLConnectorClass WockyLLConnectorClass;
typedef struct _WockyLLConnectorPrivate WockyLLConnectorPrivate;

typedef enum
{
  WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_SEND_STANZA,
  WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_RECEIVE_STANZA,
} WockyLLConnectorError;

GQuark wocky_ll_connector_error_quark (void);

#define WOCKY_LL_CONNECTOR_ERROR (wocky_ll_connector_error_quark ())

struct _WockyLLConnectorClass {
  GObjectClass parent_class;
};

struct _WockyLLConnector {
  GObject parent;
  WockyLLConnectorPrivate *priv;
};

GType wocky_ll_connector_get_type (void);

#define WOCKY_TYPE_LL_CONNECTOR \
  (wocky_ll_connector_get_type ())
#define WOCKY_LL_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_LL_CONNECTOR, \
   WockyLLConnector))
#define WOCKY_LL_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_LL_CONNECTOR, \
   WockyLLConnectorClass))
#define WOCKY_IS_LL_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_LL_CONNECTOR))
#define WOCKY_IS_LL_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_LL_CONNECTOR))
#define WOCKY_LL_CONNECTOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_LL_CONNECTOR, \
   WockyLLConnectorClass))

void wocky_ll_connector_incoming_async (
    GIOStream *stream,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

void wocky_ll_connector_outgoing_async (
    WockyXmppConnection *connection,
    const gchar *local_jid,
    const gchar *remote_jid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyXmppConnection * wocky_ll_connector_finish (
    WockyLLConnector *connector,
    GAsyncResult *result,
    gchar **from,
    GError **error);

G_END_DECLS

#endif /* #ifndef __WOCKY_LL_CONNECTOR_H__*/
