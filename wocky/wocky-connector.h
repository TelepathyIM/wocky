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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_CONNECTOR_H__
#define __WOCKY_CONNECTOR_H__

#include <glib-object.h>

#include "wocky-enumtypes.h"
#include "wocky-sasl-auth.h"
#include "wocky-xmpp-connection.h"
#include "wocky-stanza.h"

#include "wocky-tls.h"
#include "wocky-tls-handler.h"

G_BEGIN_DECLS

typedef struct _WockyConnector WockyConnector;

/**
 * WockyConnectorClass:
 *
 * The class of a #WockyConnector.
 */
typedef struct _WockyConnectorClass WockyConnectorClass;
typedef struct _WockyConnectorPrivate WockyConnectorPrivate;

/**
 * WockyConnectorError:
 * @WOCKY_CONNECTOR_ERROR_UNKNOWN: Unexpected error condition
 * @WOCKY_CONNECTOR_ERROR_IN_PROGRESS: Connection already underway
 * @WOCKY_CONNECTOR_ERROR_BAD_JID: JID is invalid
 * @WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER: XMPP version < 1
 * @WOCKY_CONNECTOR_ERROR_BAD_FEATURES: Feature stanza invalid
 * @WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE: TLS unavailable
 * @WOCKY_CONNECTOR_ERROR_TLS_REFUSED: TLS refused by server
 * @WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED: TLS handshake failed
 * @WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE: Bind not available
 * @WOCKY_CONNECTOR_ERROR_BIND_FAILED: Bind failed
 * @WOCKY_CONNECTOR_ERROR_BIND_INVALID: Bind args invalid
 * @WOCKY_CONNECTOR_ERROR_BIND_DENIED: Bind not allowed
 * @WOCKY_CONNECTOR_ERROR_BIND_CONFLICT: Bind resource in use
 * @WOCKY_CONNECTOR_ERROR_BIND_REJECTED: Bind error (generic)
 * @WOCKY_CONNECTOR_ERROR_SESSION_FAILED: Session failed
 * @WOCKY_CONNECTOR_ERROR_SESSION_DENIED: Session refused by server
 * @WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT: Session not allowed
 * @WOCKY_CONNECTOR_ERROR_SESSION_REJECTED: Session error
 * @WOCKY_CONNECTOR_ERROR_INSECURE: Insufficent security for requested
 *   operation
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED: Account registration
 *   error
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_UNAVAILABLE: Account
 *   registration not available
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_UNSUPPORTED: Account
 *   registration not implemented
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_EMPTY: Account registration
 *   makes no sense
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_CONFLICT: Account already
 *   registered
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED: Account registration
 *   rejected
 * @WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED: Account cancellation
 *   failed
 * @WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED: Account cancellation
 *   refused
 *
 * The #WockyConnector specific errors that can occur while
 * connecting.
 */
typedef enum {
  WOCKY_CONNECTOR_ERROR_UNKNOWN,
  WOCKY_CONNECTOR_ERROR_IN_PROGRESS,
  WOCKY_CONNECTOR_ERROR_BAD_JID,
  WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER,
  WOCKY_CONNECTOR_ERROR_BAD_FEATURES,
  WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE,
  WOCKY_CONNECTOR_ERROR_TLS_REFUSED,
  WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
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
  WOCKY_CONNECTOR_ERROR_INSECURE,
  WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
  WOCKY_CONNECTOR_ERROR_REGISTRATION_UNAVAILABLE,
  WOCKY_CONNECTOR_ERROR_REGISTRATION_UNSUPPORTED,
  WOCKY_CONNECTOR_ERROR_REGISTRATION_EMPTY,
  WOCKY_CONNECTOR_ERROR_REGISTRATION_CONFLICT,
  WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED,
  WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED,
  WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED,
} WockyConnectorError;

GQuark wocky_connector_error_quark (void);

/**
 * WOCKY_CONNECTOR_ERROR:
 *
 * Get access to the error quark of the connector.
 */
#define WOCKY_CONNECTOR_ERROR (wocky_connector_error_quark ())

struct _WockyConnectorClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _WockyConnector {
    /*<private>*/
    GObject parent;
    WockyConnectorPrivate *priv;
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

WockyXmppConnection *wocky_connector_connect_finish (WockyConnector *self,
    GAsyncResult *res,
    gchar **jid,
    gchar **sid,
    GError **error);

WockyXmppConnection *wocky_connector_register_finish (WockyConnector *self,
    GAsyncResult *res,
    gchar **jid,
    gchar **sid,
    GError **error);

void wocky_connector_connect_async (WockyConnector *self,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data);

WockyConnector *wocky_connector_new (const gchar *jid,
    const gchar *pass,
    const gchar *resource,
    WockyAuthRegistry *auth_registry,
    WockyTLSHandler *tls_handler);

void wocky_connector_register_async (WockyConnector *self,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data);

void wocky_connector_unregister_async (WockyConnector *self,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data);

gboolean wocky_connector_unregister_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error);

void wocky_connector_set_auth_registry (WockyConnector *self,
    WockyAuthRegistry *registry);

G_END_DECLS

#endif /* #ifndef __WOCKY_CONNECTOR_H__*/
