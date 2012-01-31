/*
 * wocky-tls-connector.h - Header for WockyTLSConnector
 * Copyright (C) 2010 Collabora Ltd.
 * @author Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
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

#ifndef __WOCKY_TLS_CONNECTOR_H__
#define __WOCKY_TLS_CONNECTOR_H__

#include <glib-object.h>

#include "wocky-tls-handler.h"
#include "wocky-xmpp-connection.h"

G_BEGIN_DECLS

typedef struct _WockyTLSConnector WockyTLSConnector;

/**
 * WockyTLSConnectorClass:
 *
 * The class of a #WockyTLSConnector.
 */
typedef struct _WockyTLSConnectorClass WockyTLSConnectorClass;
typedef struct _WockyTLSConnectorPrivate WockyTLSConnectorPrivate;

struct _WockyTLSConnectorClass {
  /*<private>*/
  GObjectClass parent_class;
};

struct _WockyTLSConnector {
  /*<private>*/
  GObject parent;
  WockyTLSConnectorPrivate *priv;
};

GType wocky_tls_connector_get_type (void);

#define WOCKY_TYPE_TLS_CONNECTOR \
  (wocky_tls_connector_get_type ())
#define WOCKY_TLS_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_TLS_CONNECTOR, \
      WockyTLSConnector))
#define WOCKY_TLS_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_TLS_CONNECTOR, \
      WockyTLSConnectorClass))
#define WOCKY_IS_TLS_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_TLS_CONNECTOR))
#define WOCKY_IS_TLS_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_TLS_CONNECTOR))
#define WOCKY_TLS_CONNECTOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_TLS_CONNECTOR, \
      WockyTLSConnectorClass))

WockyTLSConnector * wocky_tls_connector_new (WockyTLSHandler *handler);

void wocky_tls_connector_secure_async (WockyTLSConnector *self,
    WockyXmppConnection *connection,
    gboolean old_style_ssl,
    const gchar *peername,
    GStrv extra_identities,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyXmppConnection *
wocky_tls_connector_secure_finish (WockyTLSConnector *self,
    GAsyncResult *res,
    GError **error);

G_END_DECLS

#endif /* #ifndef __WOCKY_TLS_CONNECTOR_H__*/
