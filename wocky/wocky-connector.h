/*
 * wocky-connector.h - Header for WockyConnector
 * Copyright (C) 2009 Collabora Ltd.
 * @author Vivek Dasmohapatra <vivek@collabora.co.uk>
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

#ifndef __WOCKY_CONNECTOR_H__
#define __WOCKY_CONNECTOR_H__

#include <glib-object.h>
#include <gio/gnio.h>

#include "wocky-xmpp-connection.h"
#include "wocky-xmpp-stanza.h"

G_BEGIN_DECLS

typedef struct _WockyConnector WockyConnector;
typedef struct _WockyConnectorClass WockyConnectorClass;

/**
 * WockyConnectorError:
 *
 * @WOCKY_CONNECTOR_ERROR_UNKNOWN        : Generic Bad Things happened
 * @WOCKY_CONNECTOR_ERROR_IS_CONNECTED   : Already connected
 * @WOCKY_CONNECTOR_ERROR_INVALID_JID    : JID is malformed in some obvious way
 * @WOCKY_CONNECTOR_ERROR_DISCONNECTED   : Connection closed
 * @WOCKY_CONNECTOR_ERROR_NON_XMPP_SERVER: Malformed XMPP response
 * @WOCKY_CONNECTOR_ERROR_NOT_SUPPORTED  : Requested feature not available
 * @WOCKY_CONNECTOR_ERROR_REFUSED        : Feature request refused (eg starttls)
 * @WOCKY_CONNECTOR_ERROR_AUTH_FAILED    : Auth failed in some way (eg bad pass)
 * The #WockyConnector specific errors that can occur while connecting.
 */
typedef enum {
  WOCKY_CONNECTOR_ERROR_UNKNOWN,
  WOCKY_CONNECTOR_ERROR_IN_PROGRESS,
  WOCKY_CONNECTOR_ERROR_BAD_JID,
  WOCKY_CONNECTOR_ERROR_DISCONNECTED,
  WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER,
  WOCKY_CONNECTOR_ERROR_NOT_SUPPORTED,
  WOCKY_CONNECTOR_ERROR_BAD_FEATURES,
  WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE,
  WOCKY_CONNECTOR_ERROR_TLS_REFUSED,
  WOCKY_CONNECTOR_ERROR_TLS_FAILED,
  WOCKY_CONNECTOR_ERROR_REFUSED,
  WOCKY_CONNECTOR_ERROR_AUTH_FAILED,
  WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE,
  WOCKY_CONNECTOR_ERROR_BIND_FAILED,
  WOCKY_CONNECTOR_ERROR_BIND_INVALID,
  WOCKY_CONNECTOR_ERROR_BIND_DENIED,
  WOCKY_CONNECTOR_ERROR_BIND_CONFLICT,
  WOCKY_CONNECTOR_ERROR_BIND_REJECTED,
  WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
  WOCKY_CONNECTOR_ERROR_SESSION_DENIED,
  WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT,
  WOCKY_CONNECTOR_ERROR_SESSION_REJECTED,
} WockyConnectorError;

GQuark wocky_connector_error_quark (void);

/**
 * WOCKY_CONNECTOR_ERROR:
 *
 * Get access to the error quark of the connector.
 */
#define WOCKY_CONNECTOR_ERROR (wocky_connector_error_quark ())

struct _WockyConnectorClass {
    GObjectClass parent_class;
};

struct _WockyConnector {
    GObject parent;
};

GType wocky_connector_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_CONNECTOR (wocky_connector_get_type ())

#define WOCKY_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_CONNECTOR, WockyConnector))

#define WOCKY_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_CONNECTOR, WockyXmppConnector))

#define WOCKY_IS_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_CONNECTOR))

#define WOCKY_IS_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_CONNECTOR))

#define WOCKY_CONNECTOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_CONNECTOR, WockyConnectorClass))

WockyXmppConnection *
wocky_connector_connect_finish (WockyConnector *connector,
    GAsyncResult *res,
    GError **error,
    gchar **jid);

void wocky_connector_connect_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data);

WockyConnector *wocky_connector_new (const gchar *jid,
    const gchar *pass);

WockyConnector *wocky_connector_new_full (const gchar *jid,
    const gchar *pass,
    const gchar *resource,
    const gchar *host,
    guint port,
    gboolean tls_required,
    gboolean insecure_tls_ok,
    gboolean insecure_auth_ok,
    gboolean encrypted_plain_auth_ok);

G_END_DECLS

#endif /* #ifndef __WOCKY_CONNECTOR_H__*/
