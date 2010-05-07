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

#include "wocky-sasl-auth.h"
#include "wocky-xmpp-connection.h"
#include "wocky-stanza.h"

#include "wocky-tls.h"

G_BEGIN_DECLS

typedef struct _WockyConnector WockyConnector;
typedef struct _WockyConnectorClass WockyConnectorClass;
typedef struct _WockyConnectorPrivate WockyConnectorPrivate;

/**
 * WockyConnectorError:
 * @WOCKY_CONNECTOR_ERROR_UNKNOWN                  : Unexpected Error Condition
 * @WOCKY_CONNECTOR_ERROR_IN_PROGRESS              : Connection Already Underway
 * @WOCKY_CONNECTOR_ERROR_BAD_JID                  : JID is Invalid
 * @WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER       : XMPP Version < 1
 * @WOCKY_CONNECTOR_ERROR_BAD_FEATURES             : Feature Stanza Invalid
 * @WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE          : TLS Unavailable
 * @WOCKY_CONNECTOR_ERROR_TLS_REFUSED              : TLS Refused by Server
 * @WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED       : TLS Handshake Failed
 * @WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE         : Bind Not Available
 * @WOCKY_CONNECTOR_ERROR_BIND_FAILED              : Bind Failed
 * @WOCKY_CONNECTOR_ERROR_BIND_INVALID             : Bind Args Invalid
 * @WOCKY_CONNECTOR_ERROR_BIND_DENIED              : Bind Not Allowed
 * @WOCKY_CONNECTOR_ERROR_BIND_CONFLICT            : Bind Resource In Use
 * @WOCKY_CONNECTOR_ERROR_BIND_REJECTED            : Bind Error (Generic)
 * @WOCKY_CONNECTOR_ERROR_SESSION_FAILED           : Session Failed
 * @WOCKY_CONNECTOR_ERROR_SESSION_DENIED           : Session Refused by Server
 * @WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT         : Session Not Allowed
 * @WOCKY_CONNECTOR_ERROR_SESSION_REJECTED         : Session Error
 * @WOCKY_CONNECTOR_ERROR_JABBER_AUTH_UNAVAILABLE  : Jabber Auth Unavailable
 * @WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED       : Jabber Auth Failed
 * @WOCKY_CONNECTOR_ERROR_JABBER_AUTH_NO_MECHS     : Jabber Auth - No Mechanisms
 * @WOCKY_CONNECTOR_ERROR_JABBER_AUTH_REJECTED     : Jabber Auth - Unauthorised
 * @WOCKY_CONNECTOR_ERROR_JABBER_AUTH_INCOMPLETE   : Jabber Auth Args Incomplete
 * @WOCKY_CONNECTOR_ERROR_INSECURE                 : Insufficent Security for Requested Operation
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED      : Account Registration Error
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_UNAVAILABLE : Account Registration Not Available
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_UNSUPPORTED : Account Registration Not Implemented
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_EMPTY       : Account Registration Makes No Sense
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_CONFLICT    : Account Already Registered
 * @WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED    : Account Registration Rejected
 * @WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED        : Account Cancellation Failed
 * @WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED        : Account Cancellation Refused
 *
 * The #WockyConnector specific errors that can occur while connecting.
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
    GObjectClass parent_class;
};

struct _WockyConnector {
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
    GError **error,
    gchar **jid,
    gchar **sid);

WockyXmppConnection *wocky_connector_register_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error,
    gchar **jid,
    gchar **sid);

void wocky_connector_connect_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data);

WockyConnector *wocky_connector_new (const gchar *jid,
    const gchar *pass,
    const gchar *resource,
    WockyAuthRegistry *auth_registry);

void wocky_connector_register_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data);

void wocky_connector_unregister_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data);

gboolean wocky_connector_unregister_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error);

gboolean wocky_connector_add_crl (WockyConnector *self,
    const gchar *path);

gboolean wocky_connector_add_ca (WockyConnector *self,
    const gchar *path);

void wocky_connector_set_auth_registry (WockyConnector *self,
    WockyAuthRegistry *registry);

G_END_DECLS

#endif /* #ifndef __WOCKY_CONNECTOR_H__*/
