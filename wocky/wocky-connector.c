/*
 * wocky-connector.c - Source for WockyConnector
 * Copyright © 2009 Collabora Ltd.
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

/**
 * SECTION: wocky-connector
 * @title: WockyConnector
 * @short_description: Low-level XMPP connection generator.
 * @include: wocky/wocky-connector.h
 *
 * See: RFC3920 XEP-0077
 *
 * Sends and receives #WockyStanza<!-- -->s from an underlying #GIOStream.
 * negotiating TLS if possible and completing authentication with the server
 * by the "most suitable" method available.
 * Returns a #WockyXmppConnection object to the user on successful completion.
 *
 * Can also be used to register or unregister an account: When unregistering
 * (cancelling) an account, a #WockyXmppConnection is NOT returned - a #gboolean
 * value indicating success or failure is returned instead.
 *
 * The WOCKY_DEBUG tag for this module is "connector".
 *
 * The flow of control during connection is roughly as follows:
 * (registration/cancellation flows are not represented with here)
 *
 * <informalexample>
 *  <programlisting>
 * tcp_srv_connected
 * │
 * ├→ tcp_host_connected
 * │  ↓
 * └→ maybe_old_ssl
 *    ↓
 *    xmpp_init ←─────────────────┬──┐
 *    ↓                           │  │
 *    xmpp_init_sent_cb           │  │
 *    ↓                           │  │
 *    xmpp_init_recv_cb           │  │
 *    │ ↓                         │  │
 *    │ xmpp_features_cb          │  │
 *    │ │ │ ↓                     │  │
 *    │ │ │ tls_module_secure_cb ─┘  │             ①
 *    │ │ ↓                      │             ↑
 *    │ │ sasl_request_auth      │             jabber_auth_done
 *    │ │ ↓                      │             ↑
 *    │ │ sasl_auth_done ────────┴─[no sasl]─→ jabber_request_auth
 *    │ ↓                                      ↑
 *    │ iq_bind_resource                       │
 *    │ ↓                                      │
 *    │ iq_bind_resource_sent_cb               │
 *    │ ↓                                      │
 *    │ iq_bind_resource_recv_cb               │
 *    │ ↓                                      │
 *    │ ①                                      │
 *    └──────────[old auth]────────────────────┘
 *
 *    ①
 *    ↓
 *    establish_session ─────────→ success
 *    ↓                              ↑
 *    establish_session_sent_cb      │
 *    ↓                              │
 *    establish_session_recv_cb ─────┘
 *  </programlisting>
 * </informalexample>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-connector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <gio/gio.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_CONNECTOR
#include "wocky-debug-internal.h"

#include "wocky-http-proxy.h"
#include "wocky-sasl-auth.h"
#include "wocky-tls-handler.h"
#include "wocky-tls-connector.h"
#include "wocky-jabber-auth.h"
#include "wocky-namespaces.h"
#include "wocky-xmpp-connection.h"
#include "wocky-xmpp-error.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

G_DEFINE_TYPE (WockyConnector, wocky_connector, G_TYPE_OBJECT);

enum {
  CONNECTION_ESTABLISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void wocky_connector_class_init (WockyConnectorClass *klass);

/* XMPP connect/auth/etc handlers */
static void tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector);
static void tcp_host_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector);

static void maybe_old_ssl (WockyConnector *self);

static void xmpp_init (WockyConnector *connector);
static void xmpp_init_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xmpp_init_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xmpp_features_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);

static void tls_connector_secure_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);

static void sasl_request_auth (WockyConnector *object,
    WockyStanza *stanza);
static void sasl_auth_done (GObject *source,
    GAsyncResult *result,
    gpointer data);

static void xep77_begin (WockyConnector *self);
static void xep77_begin_sent (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xep77_begin_recv (GObject *source,
    GAsyncResult *result,
    gpointer data);

static void xep77_cancel_send (WockyConnector *self);
static void xep77_cancel_sent (GObject *source,
    GAsyncResult *res,
    gpointer data);
static void xep77_cancel_recv (GObject *source,
    GAsyncResult *res,
    gpointer data);

static void xep77_signup_send (WockyConnector *self,
    WockyNode *req);
static void xep77_signup_sent (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xep77_signup_recv (GObject *source,
    GAsyncResult *result,
    gpointer data);

static void iq_bind_resource (WockyConnector *self);
static void iq_bind_resource_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void iq_bind_resource_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);

void establish_session (WockyConnector *self);
static void establish_session_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void establish_session_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);

/* old-style jabber auth handlers */
static void
jabber_request_auth (WockyConnector *self);

static void
jabber_auth_done (GObject *source,
    GAsyncResult *res,
    gpointer data);

/* private methods */
static void wocky_connector_dispose (GObject *object);
static void wocky_connector_finalize (GObject *object);

enum
{
  PROP_JID = 1,
  PROP_PASS,
  PROP_AUTH_INSECURE_OK,
  PROP_ENC_PLAIN_AUTH_OK,
  PROP_RESOURCE,
  PROP_TLS_REQUIRED,
  PROP_XMPP_PORT,
  PROP_XMPP_HOST,
  PROP_IDENTITY,
  PROP_FEATURES,
  PROP_LEGACY,
  PROP_LEGACY_SSL,
  PROP_SESSION_ID,
  PROP_EMAIL,
  PROP_AUTH_REGISTRY,
  PROP_TLS_HANDLER,
};

/* this tracks which XEP 0077 operation (register account, cancel account)  *
 * we are attempting (if any). There is at leats one other XEP77 operation, *
 * password change - but we don't deal with that here as it's not really a  *
 * connector operation:                                                     */
typedef enum
{
  XEP77_NONE,
  XEP77_SIGNUP,
  XEP77_CANCEL,
} WockyConnectorXEP77Op;

typedef enum
{
  WCON_DISCONNECTED,
  WCON_TCP_CONNECTING,
  WCON_TCP_CONNECTED,
  WCON_XMPP_AUTHED,
  WCON_XMPP_BOUND,
} WockyConnectorState;


struct _WockyConnectorPrivate
{
  /* caller's choices about what to allow/disallow */
  gboolean auth_insecure_ok; /* can we auth over non-ssl */
  gboolean encrypted_plain_auth_ok; /* plaintext auth over secure channel */

  /* xmpp account related properties */
  gboolean tls_required;
  guint xmpp_port;
  gchar *xmpp_host;
  gchar *pass;
  gchar *email;
  gchar *jid;
  gchar *resource; /* the /[...] part of the jid, if any */
  gchar *user;     /* the [...]@ part of the initial JID */
  gchar *domain;   /* the @[...]/ part of the initial JID */
  /* volatile/derived property: identity = jid, but may be updated by server: */
  gchar *identity; /* if the server hands us a new JID (not handled yet) */
  gboolean legacy_support;
  gboolean legacy_ssl;
  gchar *session_id;
  gchar *ca; /* file or dir containing x509 CA files */

  /* XMPP connection data */
  WockyStanza *features;

  /* misc internal state: */
  WockyConnectorState state;
  gboolean dispose_has_run;
  gboolean authed;
  gboolean encrypted;
  gboolean connected;
  /* register/cancel account, or normal login */
  WockyConnectorXEP77Op reg_op;
  GSimpleAsyncResult *result;
  GCancellable *cancellable;

  /* Used to hold the error from connecting to the result of an SRV lookup
   * while we fall back to connecting directly to the host.
   */
  GError *srv_connect_error /* jesus christ it's a lion */;

  /* socket/tls/etc structures */
  GSocketClient *client;
  GSocketConnection *sock;
  WockyXmppConnection *conn;
  WockyTLSHandler *tls_handler;

  WockyAuthRegistry *auth_registry;

  guint see_other_host_count;
};

/* choose an appropriate chunk of text describing our state for debug/error */
static const gchar *
state_message (WockyConnectorPrivate *priv)
{
  if (priv->authed)
    return "Authentication Completed";
  else if (priv->encrypted)
    {
      if (priv->legacy_ssl)
        return "SSL Negotiated";
      else
        return "TLS Negotiated";
    }
  else if (priv->connected)
    return "TCP Connection Established";
  else
    return "Connecting... ";
}

static void
complete_operation (WockyConnector *connector)
{
  WockyConnectorPrivate *priv = connector->priv;
  GSimpleAsyncResult *tmp;

  tmp = priv->result;
  priv->result = NULL;
  g_simple_async_result_complete (tmp);
  g_object_unref (tmp);
}


static void
abort_connect_error (WockyConnector *connector,
    GError **error,
    const char *fmt,
    ...)
{
  WockyConnectorPrivate *priv = NULL;
  va_list args;

  DEBUG ("connector: %p", connector);
  priv = connector->priv;

  g_assert (error != NULL);
  g_assert (*error != NULL);

  va_start (args, fmt);
  if ((fmt != NULL) && (*fmt != '\0'))
    {
      gchar *msg = g_strdup_vprintf (fmt, args);
      g_prefix_error (error, "%s: ", msg);
      g_free (msg);
    }
  va_end (args);

  if (priv->sock != NULL)
    {
      g_object_unref (priv->sock);
      priv->sock = NULL;
    }
  priv->state = WCON_DISCONNECTED;

  if (priv->cancellable != NULL)
    {
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  g_simple_async_result_set_from_error (priv->result, *error);
  complete_operation (connector);
}

static void
abort_connect (WockyConnector *connector,
    GError *error)
{
  WockyConnectorPrivate *priv = connector->priv;

  if (priv->sock != NULL)
    {
      g_object_unref (priv->sock);
      priv->sock = NULL;
    }
  priv->state = WCON_DISCONNECTED;

  if (priv->cancellable != NULL)
    {
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  g_simple_async_result_set_from_error (priv->result, error);
  complete_operation (connector);
}

static void
abort_connect_code (WockyConnector *connector,
    int code,
    const char *fmt,
    ...)
{
  GError *err = NULL;
  va_list args;

  va_start (args, fmt);
  err = g_error_new_valist (WOCKY_CONNECTOR_ERROR, code, fmt, args);
  va_end (args);

  abort_connect (connector, err);
  g_error_free (err);
}

GQuark
wocky_connector_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-connector-error");

  return quark;
}

static void
wocky_connector_init (WockyConnector *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_CONNECTOR,
      WockyConnectorPrivate);
}

static void
wocky_connector_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyConnector *connector = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = connector->priv;

  switch (property_id)
    {
      case PROP_TLS_REQUIRED:
        priv->tls_required = g_value_get_boolean (value);
        break;
      case PROP_AUTH_INSECURE_OK:
        priv->auth_insecure_ok = g_value_get_boolean (value);
        break;
      case PROP_ENC_PLAIN_AUTH_OK:
        priv->encrypted_plain_auth_ok = g_value_get_boolean (value);
        break;
      case PROP_JID:
        g_free (priv->jid);
        priv->jid = g_value_dup_string (value);
        break;
      case PROP_EMAIL:
        g_free (priv->email);
        priv->email = g_value_dup_string (value);
        break;
      case PROP_PASS:
        g_free (priv->pass);
        priv->pass = g_value_dup_string (value);
        break;
      case PROP_RESOURCE:
        g_free (priv->resource);
        if ((g_value_get_string (value) != NULL) &&
            *g_value_get_string (value) != '\0')
          priv->resource = g_value_dup_string (value);
        else
          priv->resource = NULL;
        break;
      case PROP_XMPP_PORT:
        priv->xmpp_port = g_value_get_uint (value);
        break;
      case PROP_XMPP_HOST:
        g_free (priv->xmpp_host);
        priv->xmpp_host = g_value_dup_string (value);
        break;
      case PROP_LEGACY:
        priv->legacy_support = g_value_get_boolean (value);
        break;
      case PROP_LEGACY_SSL:
        priv->legacy_ssl = g_value_get_boolean (value);
        break;
      case PROP_SESSION_ID:
        g_free (priv->session_id);
        priv->session_id = g_value_dup_string (value);
        break;
      case PROP_AUTH_REGISTRY:
        priv->auth_registry = g_value_dup_object (value);
        break;
      case PROP_TLS_HANDLER:
        priv->tls_handler = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_connector_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyConnector *connector = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = connector->priv;

  switch (property_id)
    {
      case PROP_TLS_REQUIRED:
        g_value_set_boolean (value, priv->tls_required);
        break;
      case PROP_AUTH_INSECURE_OK:
        g_value_set_boolean (value, priv->auth_insecure_ok);
        break;
      case PROP_ENC_PLAIN_AUTH_OK:
        g_value_set_boolean (value, priv->encrypted_plain_auth_ok);
        break;
      case PROP_JID:
        g_value_set_string (value, priv->jid);
        break;
      case PROP_PASS:
        g_value_set_string (value, priv->pass);
        break;
      case PROP_EMAIL:
        g_value_set_string (value, priv->email);
        break;
      case PROP_RESOURCE:
        g_value_set_string (value, priv->resource);
        break;
      case PROP_XMPP_PORT:
        g_value_set_uint (value, priv->xmpp_port);
        break;
      case PROP_XMPP_HOST:
        g_value_set_string (value, priv->xmpp_host);
        break;
      case PROP_IDENTITY:
        g_value_set_string (value, priv->identity);
        break;
      case PROP_FEATURES:
        g_value_set_object (value, priv->features);
        break;
      case PROP_LEGACY:
        g_value_set_boolean (value, priv->legacy_support);
        break;
      case PROP_LEGACY_SSL:
        g_value_set_boolean (value, priv->legacy_ssl);
        break;
      case PROP_SESSION_ID:
        g_value_set_string (value, priv->session_id);
        break;
      case PROP_AUTH_REGISTRY:
        g_value_set_object (value, priv->auth_registry);
        break;
      case PROP_TLS_HANDLER:
        g_value_set_object (value, priv->tls_handler);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_connector_class_init (WockyConnectorClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  g_type_class_add_private (klass, sizeof (WockyConnectorPrivate));

  oclass->set_property = wocky_connector_set_property;
  oclass->get_property = wocky_connector_get_property;
  oclass->dispose      = wocky_connector_dispose;
  oclass->finalize     = wocky_connector_finalize;

#if HAVE_GIO_PROXY
  /* Ensure that HTTP Proxy extension is registered */
  _wocky_http_proxy_get_type ();
#endif

  /**
   * WockyConnector:plaintext-auth-allowed:
   *
   * Whether auth info can be sent in the clear (eg PLAINTEXT auth).
   * This is independent of any encryption (TLS, SSL) that has been negotiated.
   */
  spec = g_param_spec_boolean ("plaintext-auth-allowed",
      "plaintext-auth-allowed",
      "Whether auth info can be sent in the clear", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_AUTH_INSECURE_OK, spec);

  /**
   * WockyConnector:encrypted-plain-auth-ok:
   *
   * Whether PLAINTEXT auth is ok when encrypted.
   */
  spec = g_param_spec_boolean ("encrypted-plain-auth-ok",
      "encrypted-plain-auth-ok",
      "Whether PLAIN auth can be used when encrypted", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ENC_PLAIN_AUTH_OK, spec);

  /**
   * WockyConnector:tls-required:
   *
   * Whether we require successful tls/ssl negotiation to continue.
   */
  spec = g_param_spec_boolean ("tls-required", "TLS required",
      "Whether SSL/TLS is required", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_TLS_REQUIRED, spec);

  /**
   * WockyConnector:jid:
   *
   * The XMPP account's JID (with or without a /resource).
   */
  spec = g_param_spec_string ("jid", "jid", "The XMPP jid", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_JID, spec);

  /**
   * WockyConnector:email:
   *
   * The XMPP account's email address (optional, MAY be required by the server
   * if we are registering an account, not required otherwise).
   */
  spec = g_param_spec_string ("email", "email", "user's email address", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_EMAIL, spec);

  /**
   * WockyConnector:password:
   *
   * XMPP Account password.
   */
  spec = g_param_spec_string ("password", "pass", "Password", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_PASS, spec);

  /**
   * WockyConnector:resource:
   *
   * The resource (sans '/') for this connection. If %NULL or the empty string,
   * Wocky will let the server decide. Even if you specify a particular
   * resource, the server may modify it.
   */
  spec = g_param_spec_string ("resource", "resource",
      "XMPP resource to append to the jid", NULL,
      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_RESOURCE, spec);

  /**
   * WockyConnector:identity:
   *
   * JID + resource (a AT b SLASH c) that is in effect _after_ a successful
   * resource binding operation. This is NOT guaranteed to be related to
   * the JID specified in the original #WockyConnector:jid property.
   * The resource, in particular, is often different, and with gtalk the
   * domain is often different.
   */
  spec = g_param_spec_string ("identity", "identity",
      "jid + resource (set by XMPP server)", NULL,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_IDENTITY, spec);

  /**
   * WockyConnector:xmpp-server:
   *
   * Optional XMPP connect server. Any DNS SRV record and the host specified
   * in #WockyConnector:jid will be ignored if this is set. May be a hostname
   * (fully qualified or otherwise), a dotted quad or an ipv6 address.
   */
  spec = g_param_spec_string ("xmpp-server", "XMPP server",
      "XMPP connect server hostname or address", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_XMPP_HOST, spec);

  /**
   * WockyConnector:xmpp-port:
   *
   * Optional XMPP connect port. Any DNS SRV record will be ignored if
   * this is set. (So the host will be either the WockyConnector:xmpp-server
   * property or the domain part of the JID, in descending order of preference)
   */
  spec = g_param_spec_uint ("xmpp-port", "XMPP port",
      "XMPP port", 0, 65535, 0,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_XMPP_PORT, spec);

  /**
   * WockyConnector:features:
   *
   * A #WockyStanza instance, the last WockyStanza instance received
   * by the connector during the connection procedure (there may be several,
   * the most recent one always being the one we should refer to).
   */
  spec = g_param_spec_object ("features", "XMPP Features",
      "Last XMPP Feature Stanza advertised by server", WOCKY_TYPE_STANZA,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_FEATURES, spec);

  /**
   * WockyConnector:legacy:
   *
   * Whether to attempt old-style (non-SASL) jabber auth.
   */
  spec = g_param_spec_boolean ("legacy", "Legacy Jabber Support",
      "Old style Jabber (Auth) support", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_LEGACY, spec);


  /**
   * WockyConnector:old-ssl:
   *
   * Whether to use old-style SSL-at-connect-time encryption rather than
   * the more modern STARTTLS approach.
   */
  spec = g_param_spec_boolean ("old-ssl", "Legacy SSL Support",
      "Old style SSL support", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_LEGACY_SSL, spec);

  /**
   * WockyConnector:session-id:
   *
   * The Session ID supplied by the server upon successfully connecting.
   * May be useful later on as some XEPs suggest this value should be used
   * at various stages as part of a hash or as an ID.
   */
  spec = g_param_spec_string ("session-id", "XMPP Session ID",
      "XMPP Session ID", NULL,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_SESSION_ID, spec);

  /**
   * WockyConnector:auth-registry
   *
   * An authentication registry that holds handlers for different
   * authentication mechanisms, arbitrates mechanism selection and relays
   * challenges and responses between the handlers and the connection.
   */
  spec = g_param_spec_object ("auth-registry", "Authentication Registry",
      "Authentication Registry", WOCKY_TYPE_AUTH_REGISTRY,
      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_AUTH_REGISTRY, spec);

  /**
   * WockyConnector:tls-handler
   *
   * A TLS handler that carries out the interactive verification of the
   * TLS certitificates provided by the server.
   */
  spec = g_param_spec_object ("tls-handler", "TLS Handler",
      "TLS Handler", WOCKY_TYPE_TLS_HANDLER,
      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_TLS_HANDLER, spec);

  /**
   * WockyConnector::connection-established:
   * @connection: the #GSocketConnection
   *
   * Emitted as soon as a connection to the remote server has been
   * established. This can be useful if you want to do something
   * unusual to the connection early in its lifetime not supported by
   * the #WockyConnector APIs.
   *
   * As the connection process has only just started and the stream
   * not even opened yet, no data must be sent over @connection. This
   * signal is merely intended to set esoteric socket options (such as
   * TCP_NODELAY) on the connection.
   */
  signals[CONNECTION_ESTABLISHED] = g_signal_new ("connection-established",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_SOCKET_CONNECTION);
}

#define GFREE_AND_FORGET(x) g_free (x); x = NULL;

static void
wocky_connector_dispose (GObject *object)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;
  g_clear_object (&priv->conn);
  g_clear_object (&priv->client);
  g_clear_object (&priv->sock);
  g_clear_object (&priv->features);
  g_clear_object (&priv->auth_registry);
  g_clear_object (&priv->tls_handler);

  if (G_OBJECT_CLASS (wocky_connector_parent_class )->dispose)
    G_OBJECT_CLASS (wocky_connector_parent_class)->dispose (object);
}

static void
wocky_connector_finalize (GObject *object)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = self->priv;

  GFREE_AND_FORGET (priv->jid);
  GFREE_AND_FORGET (priv->user);
  GFREE_AND_FORGET (priv->domain);
  GFREE_AND_FORGET (priv->resource);
  GFREE_AND_FORGET (priv->identity);
  GFREE_AND_FORGET (priv->xmpp_host);
  GFREE_AND_FORGET (priv->pass);
  GFREE_AND_FORGET (priv->session_id);
  GFREE_AND_FORGET (priv->email);

  if (priv->srv_connect_error != NULL)
    g_clear_error (&priv->srv_connect_error);

  G_OBJECT_CLASS (wocky_connector_parent_class)->finalize (object);
}

static void
connect_to_host_async (WockyConnector *connector,
    const gchar *host_and_port,
    guint default_port)
{
  WockyConnectorPrivate *priv = connector->priv;

#if HAVE_GIO_PROXY
  {
    const gchar *uri_format = "%s://%s";
    gchar *uri;

    /* If host_and_port is an ipv6 address we must ensure it has [] around it */
    if (host_and_port[0] != '[')
      {
        const gchar *p;

        /* if host_and_port contains 2 ':' chars, it must be an ipv6 address */
        p = g_strstr_len (host_and_port, -1, ":");
        if (p != NULL)
          p = g_strstr_len (p + 1, -1, ":");
        if (p != NULL)
          uri_format = "%s://[%s]";
      }

    /* Legacy SSL mode is just like doing HTTPS, so let's trigger HTTPS
     * proxy setting if any */
    uri = g_strdup_printf (uri_format,
        priv->legacy_ssl ? "https" : "xmpp-client", host_and_port);
    g_socket_client_connect_to_uri_async (priv->client,
        uri, default_port, NULL, tcp_host_connected, connector);
    g_free (uri);
  }
#else
  g_socket_client_connect_to_host_async (priv->client,
      host_and_port, default_port, NULL, tcp_host_connected, connector);
#endif
}

static void
tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = self->priv;

  priv->sock =
    g_socket_client_connect_to_service_finish (G_SOCKET_CLIENT (source),
        result, &error);

  /* if we didn't manage to connect via SRV records based on the JID
     (no SRV records or host unreachable/not listening) fall back to
     treating the domain part of the JID as a real host and try to
     talk to that */
  if (priv->sock == NULL)
    {
      gchar *node = NULL;      /* username   */ /* @ */
      gchar *host = NULL;      /* domain.tld */ /* / */
      guint port = (priv->xmpp_port == 0) ? 5222 : priv->xmpp_port;

      /* g_socket_client_connect_to_service_finish() should have set error if
       * it returned %NULL.
       */
      g_return_if_fail (error != NULL);

      DEBUG ("SRV connect failed: %s:%d %s", g_quark_to_string (error->domain),
          error->code, error->message);

      /* An IO error implies there IS a SRV record but we could not
       * connect. Stash the error, and fall back to connecting to the host
       * directly; if we also fail to connect to the host, we'll report the
       * error we stashed here rather than the later error. This is
       * predominantly to work around chat.facebook.com having a broken SRV
       * record.
       *
       * For any other kind of error, we assume this means there's no SRV
       * record, bin the GError and just fall back to the host completely.
       */
      if (error->domain == G_IO_ERROR)
        priv->srv_connect_error = error;
      else
        g_clear_error (&error);

      priv->state = WCON_TCP_CONNECTING;
      /* decode a hostname from the JID here: Don't check for an explicit *
       * connect host supplied by the user as we shouldn't even try a SRV *
       * connection in that case, and should therefore never get here     */
      wocky_decode_jid (priv->jid, &node, &host, NULL);

      if ((host != NULL) && (*host != '\0'))
        {
          DEBUG ("Falling back to HOST connection to %s port %u", host, port);
          connect_to_host_async (connector, host, port);
        }
      else
        {
          abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BAD_JID,
              "JID contains no domain: %s", priv->jid);
        }

      g_free (node);
      g_free (host);
    }
  else
    {
      DEBUG ("SRV connection succeeded");

      g_signal_emit (self, signals[CONNECTION_ESTABLISHED], 0, priv->sock);

      priv->connected = TRUE;
      priv->state = WCON_TCP_CONNECTED;
      maybe_old_ssl (self);
    }
}

static void
tcp_host_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = self->priv;
  GSocketClient *sock = G_SOCKET_CLIENT (source);

  priv->sock = g_socket_client_connect_to_host_finish (sock, result, &error);

  if (priv->sock == NULL)
    {
      DEBUG ("HOST connect failed: %s", error->message);

      if (priv->srv_connect_error != NULL)
        {
          DEBUG ("we previously hit a GIOError when connecting using SRV; "
              "reporting that error");
          abort_connect_error (connector, &priv->srv_connect_error,
              "couldn't connect to server specified by SRV record");
        }
      else
        {
          abort_connect_error (connector, &error,
              "couldn't connect to server");
        }

      g_error_free (error);
    }
  else
    {
      DEBUG ("HOST connection succeeded");

      g_signal_emit (self, signals[CONNECTION_ESTABLISHED], 0, priv->sock);

      priv->connected = TRUE;
      priv->state = WCON_TCP_CONNECTED;
      maybe_old_ssl (self);
    }
}

/* ************************************************************************* */
/* legacy jabber support                                                     */
static void
jabber_request_auth (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;
  WockyJabberAuth *jabber_auth;
  gboolean clear = FALSE;

  jabber_auth = wocky_jabber_auth_new (priv->session_id, priv->user,
      priv->resource, priv->pass, priv->conn, priv->auth_registry);

  if (priv->auth_insecure_ok ||
      (priv->encrypted && priv->encrypted_plain_auth_ok))
    clear = TRUE;

  DEBUG ("handing over control to WockyJabberAuth");
  wocky_jabber_auth_authenticate_async (jabber_auth, clear, priv->encrypted,
      priv->cancellable, jabber_auth_done, self);
}

static void
jabber_auth_done (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyJabberAuth *jabber_auth = WOCKY_JABBER_AUTH (source);

  if (!wocky_jabber_auth_authenticate_finish (jabber_auth, result, &error))
    {
      /* nothing to add, the SASL error should be informative enough */
      DEBUG ("Jabber auth complete (failure)");

      abort_connect_error (self, &error, "");

      g_error_free (error);
      goto out;
    }

  DEBUG ("Jabber auth complete (success)");
  priv->state = WCON_XMPP_AUTHED;
  priv->authed = TRUE;
  priv->identity = g_strdup_printf ("%s@%s/%s",
      priv->user, priv->domain, priv->resource);
  /* if there has been no features stanza, this will just finish up *
   * if there has been a feature stanza, we are in an XMPP 1.x      *
   * server that _only_ supports old style auth (no SASL). In this  *
   * bizarre situation, we would then proceed as if we were in a    *
   * normal XMPP server after a successful bind.                    */
  establish_session (self);
 out:
  g_object_unref (jabber_auth);
}

/* ************************************************************************* */
/* old-style SSL                                                             */

static const gchar *
get_peername (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;
  const gchar *peer;

  if (priv->legacy_ssl)
    peer = (priv->xmpp_host != NULL) ? priv->xmpp_host : priv->domain;
  else
    peer = priv->domain;

  return peer;
}

static void
maybe_old_ssl (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;

  g_assert (priv->conn == NULL);
  g_assert (priv->sock != NULL);

  priv->conn = wocky_xmpp_connection_new (G_IO_STREAM (priv->sock));

  if (priv->legacy_ssl && !priv->encrypted)
    {
      WockyTLSConnector *tls_connector;

      DEBUG ("Creating SSL connector");
      tls_connector = wocky_tls_connector_new (priv->tls_handler);

      DEBUG ("Beginning SSL handshake");
      wocky_tls_connector_secure_async (tls_connector,
          priv->conn, TRUE, get_peername (self), NULL,
          priv->cancellable, tls_connector_secure_cb, self);

      g_object_unref (tls_connector);
    }
  else
    {
      xmpp_init (self);
    }
}

/* ************************************************************************* */
/* standard XMPP stanza handling                                             */
static void
xmpp_init (WockyConnector *connector)
{
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = self->priv;

  DEBUG ("sending XMPP stream open to server");
  wocky_xmpp_connection_send_open_async (priv->conn, priv->domain, NULL,
      "1.0", NULL, NULL, priv->cancellable, xmpp_init_sent_cb, connector);
}

static void
xmpp_init_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;

  if (!wocky_xmpp_connection_send_open_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send open stanza");
      g_error_free (error);
      return;
    }

  DEBUG ("waiting for stream open from server");
  wocky_xmpp_connection_recv_open_async (priv->conn, priv->cancellable,
      xmpp_init_recv_cb, data);
}

static void
xmpp_init_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  gchar *version = NULL;
  gchar *from = NULL;
  gchar *id = NULL;
  gdouble ver = 0;

  if (!wocky_xmpp_connection_recv_open_finish (priv->conn, result, NULL,
          &from, &version, NULL, &id, &error))
    {
      abort_connect_error (self, &error, "%s: %s",
          state_message (priv), error->message);
      g_error_free (error);
      goto out;
    }

  g_free (priv->session_id);
  priv->session_id = g_strdup (id);

  DEBUG ("%s: received XMPP version=%s stream open from server",
      state_message (priv),
      version != NULL ? version : "(unspecified)");

  ver = (version != NULL) ? atof (version) : -1;

  if (ver < 1.0)
    {
      if (!priv->legacy_support)
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER,
            "Server not XMPP 1.0 Compliant");
      else if (priv->tls_required && !priv->encrypted)
        abort_connect_code (data, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE,
            "TLS requested but server is not XMPP 1.0 compliant (try using \"old SSL\")");
      else
        jabber_request_auth (self);
    }
  else
    {
      DEBUG ("waiting for feature stanza from server");
      wocky_xmpp_connection_recv_stanza_async (priv->conn, priv->cancellable,
          xmpp_features_cb, data);
    }

 out:
  g_free (version);
  g_free (from);
  g_free (id);
}

/* ************************************************************************* */
/* handle stream errors                                                      */
static gboolean
stream_error_abort (WockyConnector *self,
    WockyStanza *stanza)
{
  GError *error = NULL;

  if (!wocky_stanza_extract_stream_error (stanza, &error))
    return FALSE;

  if (g_error_matches (error, WOCKY_XMPP_STREAM_ERROR,
          WOCKY_XMPP_STREAM_ERROR_SEE_OTHER_HOST))
    {
      const gchar *other_host;

      other_host = wocky_node_get_content_from_child_ns (
          wocky_stanza_get_top_node (stanza),
          "see-other-host", WOCKY_XMPP_NS_STREAMS);

      if (other_host != NULL && self->priv->see_other_host_count < 5)
        {
          DEBUG ("Need to restart connection with host: %s", other_host);

          self->priv->see_other_host_count++;

          /* Reset to initial state */
          g_clear_object (&self->priv->features);
          g_clear_object (&self->priv->sock);
          g_clear_object (&self->priv->conn);
          self->priv->state = WCON_TCP_CONNECTING;
          self->priv->authed = FALSE;
          self->priv->encrypted = FALSE;
          self->priv->connected = FALSE;

          connect_to_host_async (self, other_host, 5222);

          goto out;
        }
    }

  DEBUG ("Received stream error: %s", error->message);
  abort_connect (self, error);

out:
  g_error_free (error);
  return TRUE;
}

/* ************************************************************************* */
static void
xmpp_features_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *stanza;
  WockyNode   *node;
  gboolean can_encrypt = FALSE;
  gboolean can_bind = FALSE;

  stanza =
    wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &error);

  if (stanza == NULL)
    {
      abort_connect_error (self, &error,
          "disconnected before XMPP features stanza");
      g_error_free (error);
      return;
    }

  if (stream_error_abort (self, stanza))
    goto out;

  if (!wocky_stanza_has_type (stanza, WOCKY_STANZA_TYPE_STREAM_FEATURES))
    {
      abort_connect_code (data, WOCKY_CONNECTOR_ERROR_BAD_FEATURES, "%s: %s",
          state_message (priv), "Malformed or missing feature stanza");
      goto out;
    }

  DEBUG ("received feature stanza from server");
  node = wocky_stanza_get_top_node (stanza);

  /* cache the current feature set: according to the RFC, we should forget
   * any previous feature set as soon as we open a new stream, so that
   * happens elsewhere */
  if (stanza != NULL)
    {
      if (priv->features != NULL)
        g_object_unref (priv->features);
      priv->features = g_object_ref (stanza);
    }

  can_encrypt =
    wocky_node_get_child_ns (node, "starttls", WOCKY_XMPP_NS_TLS) != NULL;
  can_bind =
    wocky_node_get_child_ns (node, "bind", WOCKY_XMPP_NS_BIND) != NULL;

  /* conditions:
   * not encrypted, not encryptable, require encryption → ABORT
   * !encrypted && encryptable                          → STARTTLS
   * !authed    && xep77_reg                            → XEP77 REGISTRATION
   * !authed                                            → AUTH
   * not bound && can bind                              → BIND
   */

  if (!priv->encrypted && !can_encrypt && priv->tls_required)
    {
      abort_connect_code (data, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE,
          "TLS requested but lack server support");
      goto out;
    }

  if (!priv->encrypted && can_encrypt)
    {
      WockyTLSConnector *tls_connector;

      tls_connector = wocky_tls_connector_new (priv->tls_handler);
      wocky_tls_connector_secure_async (tls_connector,
          priv->conn, FALSE, get_peername (self), NULL, priv->cancellable,
          tls_connector_secure_cb, self);

      g_object_unref (tls_connector);

      goto out;
    }

  if (!priv->authed && priv->reg_op == XEP77_SIGNUP)
    {
      xep77_begin (self);
      goto out;
    }

  if (!priv->authed)
    {
      sasl_request_auth (self, stanza);
      goto out;
    }

  /* we MUST bind here http://www.ietf.org/rfc/rfc3920.txt */
  if (can_bind)
    iq_bind_resource (self);
  else
    abort_connect_code (data, WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE,
        "XMPP Server does not support resource binding");

 out:
  if (stanza != NULL)
    g_object_unref (stanza);
}

static void
tls_connector_secure_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyTLSConnector *tls_connector = WOCKY_TLS_CONNECTOR (source);
  WockyConnector *self = user_data;
  GError *error = NULL;
  WockyXmppConnection *new_connection;

  new_connection = wocky_tls_connector_secure_finish (tls_connector,
      res, &error);

  if (error != NULL)
    {
      abort_connect (self, error);
      g_error_free (error);

      return;
    }

  if (self->priv->conn != NULL)
    g_object_unref (self->priv->conn);

  self->priv->conn = new_connection;

  self->priv->encrypted = TRUE;
  xmpp_init (self);
}

/* ************************************************************************* */
/* AUTH calls */

static void
sasl_request_auth (WockyConnector *object,
    WockyStanza *stanza)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = self->priv;
  WockySaslAuth *s;
  gboolean clear = FALSE;

  s = wocky_sasl_auth_new (priv->domain, priv->user, priv->pass, priv->conn,
      priv->auth_registry);

  if (priv->auth_insecure_ok ||
      (priv->encrypted && priv->encrypted_plain_auth_ok))
    clear = TRUE;

  DEBUG ("handing over control to SASL module");
  wocky_sasl_auth_authenticate_async (s, stanza, clear, priv->encrypted,
      priv->cancellable, sasl_auth_done, self);
}

static void
sasl_auth_done (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (source);

  if (!wocky_sasl_auth_authenticate_finish (sasl, result, &error))
    {
      /* nothing to add, the SASL error should be informative enough */
      DEBUG ("SASL complete (failure)");

      /* except: if there's no SASL and Jabber auth is available, we *
       * are allowed to attempt that instead                         */
      if ((error->domain == WOCKY_AUTH_ERROR) &&
          (error->code == WOCKY_AUTH_ERROR_NOT_SUPPORTED) &&
          (wocky_node_get_child_ns (
              wocky_stanza_get_top_node (priv->features), "auth",
              WOCKY_JABBER_NS_AUTH_FEATURE) != NULL))
        jabber_request_auth (self);
      else
        abort_connect_error (self, &error, "");

      g_error_free (error);
      goto out;
    }

  DEBUG ("SASL complete (success)");
  priv->state = WCON_XMPP_AUTHED;
  priv->authed = TRUE;
  wocky_xmpp_connection_reset (priv->conn);
  xmpp_init (self);
 out:
  g_object_unref (sasl);
}

/* ************************************************************************* */
/* XEP 0077 register/cancel calls                                            */
static void
xep77_cancel_send (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *iqs = NULL;
  gchar *iid = NULL;

  DEBUG ("");

  iid = wocky_xmpp_connection_new_id (priv->conn);
  iqs = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET,
      /* FIXME: It is debatable (XEP0077 section 3.2) whether we should *
       * include our JID here. The examples include it, the text states *
       * that we SHOULD NOT, at least in some use cases                 */
      NULL /* priv->identity */,
      priv->domain,
      '@', "id", iid,
      '(', "query", ':', WOCKY_XEP77_NS_REGISTER,
      '(', "remove", ')',
      ')',
      NULL);

  wocky_xmpp_connection_send_stanza_async (priv->conn, iqs, priv->cancellable,
      xep77_cancel_sent, self);

  g_free (iid);
  g_object_unref (iqs);
}

static void
xep77_cancel_sent (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, res, &error))
    {
      abort_connect_error (self, &error, "Failed to send unregister iq set");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, priv->cancellable,
      xep77_cancel_recv, self);
}

static void
xep77_cancel_recv (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *iq = NULL;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  DEBUG ("");
  iq = wocky_xmpp_connection_recv_stanza_finish (priv->conn, res, &error);

  if (iq == NULL)
    {
      g_simple_async_result_set_from_error (priv->result, error);
      g_error_free (error);
      goto out;
    }

  wocky_stanza_get_type_info (iq, &type, &sub_type);

  DEBUG ("type == %d; sub_type: %d", type, sub_type);

  if (wocky_stanza_extract_stream_error (iq, &error))
    {
      if (error->code != WOCKY_XMPP_STREAM_ERROR_NOT_AUTHORIZED)
        g_simple_async_result_set_from_error (priv->result, error);

      g_error_free (error);
      goto out;
    }

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      g_simple_async_result_set_error (priv->result,
          WOCKY_CONNECTOR_ERROR,
          WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED,
          "Unregister: Invalid response");
      goto out;
    }

  switch (sub_type)
    {
      int code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (iq, NULL, &error, NULL, NULL);

        switch (error->code)
          {
            case WOCKY_XMPP_ERROR_FORBIDDEN:
            case WOCKY_XMPP_ERROR_NOT_ALLOWED:
              code = WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED;
              break;
            default:
              code = WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED;
          }

        g_simple_async_result_set_error (priv->result,
            WOCKY_CONNECTOR_ERROR, code, "Unregister: %s", error->message);
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        /* Do nothing, we have already succeeded. */
        break;

      default:
        g_simple_async_result_set_error (priv->result,
            WOCKY_CONNECTOR_ERROR,
            WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED,
            "Unregister: Malformed Response");
        break;
    }

 out:
  if (iq != NULL)
    g_object_unref (iq);
  if (priv->sock != NULL)
    {
      g_object_unref (priv->sock);
      priv->sock = NULL;
    }
  if (priv->cancellable != NULL)
    {
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }
  complete_operation (self);
  priv->state = WCON_DISCONNECTED;
}

static void
xep77_begin (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *iqs = NULL;
  gchar *iid = NULL;
  gchar *jid = NULL;

  DEBUG ("");

  if (!priv->encrypted && !priv->auth_insecure_ok)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_INSECURE,
          "Cannot register account without encryption");
      return;
    }

  jid = g_strdup_printf ("%s@%s", priv->user, priv->domain);
  iid = wocky_xmpp_connection_new_id (priv->conn);
  iqs = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET,
      jid, priv->domain,
      '@', "id", iid,
      '(', "query",
      ':', WOCKY_XEP77_NS_REGISTER,
      ')',
      NULL);

  wocky_xmpp_connection_send_stanza_async (priv->conn, iqs, priv->cancellable,
      xep77_begin_sent, self);

  g_free (jid);
  g_free (iid);
  g_object_unref (iqs);
}

static void
xep77_begin_sent (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send register iq get");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, priv->cancellable,
      xep77_begin_recv, self);
}

static void
xep77_begin_recv (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *iq = NULL;
  WockyNode *query = NULL;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  DEBUG ("");

  iq = wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &error);

  if (iq == NULL)
    {
      abort_connect_error (self, &error, "Failed to receive register iq set");
      g_error_free (error);
      goto out;
    }

  wocky_stanza_get_type_info (iq, &type, &sub_type);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
          "Register: Response Invalid");
      goto out;
    }

  switch (sub_type)
    {
      int code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (iq, NULL, &error, NULL, NULL);

        if (error->code == WOCKY_XMPP_ERROR_SERVICE_UNAVAILABLE)
          code = WOCKY_CONNECTOR_ERROR_REGISTRATION_UNAVAILABLE;
        else
          code = WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED;

        abort_connect_code (self, code, "Registration: %s", error->message);
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_RESULT");
        query = wocky_node_get_child_ns (
          wocky_stanza_get_top_node (iq), "query",
            WOCKY_XEP77_NS_REGISTER);

        if (query == NULL)
          {
            abort_connect_code (self,
                WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
                "Malformed response to register iq");
            goto out;
          }

        /* already registered. woo hoo. proceed to auth stage */
        if (wocky_node_get_child (query, "registered") != NULL)
          {
            priv->reg_op = XEP77_NONE;
            sasl_request_auth (self, priv->features);
            goto out;
          }

        switch (priv->reg_op)
          {
            case XEP77_SIGNUP:
              xep77_signup_send (self, query);
              break;
            case XEP77_CANCEL:
              xep77_cancel_send (self);
              break;
            default:
              abort_connect_code (self, WOCKY_CONNECTOR_ERROR_UNKNOWN,
                  "This should never happen: broken logic in connctor");
          }
        break;

      default:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_*");
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
            "Register: Response Invalid");
        break;
    }

 out:
  if (iq != NULL)
    g_object_unref (iq);
}

static void
xep77_signup_send (WockyConnector *self,
    WockyNode *req)
{
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *riq = NULL;
  WockyNode *reg = NULL;
  GSList *arg = NULL;
  gchar *jid = g_strdup_printf ("%s@%s", priv->user, priv->domain);
  gchar *iid = wocky_xmpp_connection_new_id (priv->conn);
  guint args = 0;

  DEBUG ("");

  riq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET,
      jid, priv->domain,
      '@', "id", iid, NULL);
  reg = wocky_node_add_child_ns (wocky_stanza_get_top_node (riq),
      "query", WOCKY_XEP77_NS_REGISTER);

  for (arg = req->children; arg != NULL; arg = g_slist_next (arg))
    {
      gchar *value = NULL;
      WockyNode *a = (WockyNode *) arg->data;

      if (!wocky_strdiff ("instructions", a->name))
        continue;
      else if (!wocky_strdiff ("username", a->name))
        value = priv->user;
      else if (!wocky_strdiff ("password", a->name))
        value = priv->pass;
      else if (!wocky_strdiff ("email", a->name))
        if ((priv->email != NULL) && *(priv->email) != '0')
          value = priv->email;
        else
          {
            abort_connect_code (self,
                WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED,
                "Registration parameter %s missing", a->name);
            goto out;
          }
      else
        {
          abort_connect_code (self,
              WOCKY_CONNECTOR_ERROR_REGISTRATION_UNSUPPORTED,
              "Did not understand '%s' registration parameter", a->name);
          goto out;
        }
      DEBUG ("%s := %s", a->name, value);
      wocky_node_add_child_with_content (reg, a->name, value);
      args++;
    }

  /* we understood all args, and there was at least one of them: */
  if (args > 0)
    wocky_xmpp_connection_send_stanza_async (priv->conn, riq, priv->cancellable,
        xep77_signup_sent, self);
  else
    abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_EMPTY,
        "Registration without parameters makes no sense");

out:
  g_object_unref (riq);
  g_free (jid);
  g_free (iid);
}

static void
xep77_signup_sent (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send registration");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, priv->cancellable,
      xep77_signup_recv, self);
}

static void
xep77_signup_recv (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *iq = NULL;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  DEBUG ("");

  iq = wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &error);

  if (iq == NULL)
    {
      abort_connect_error (self, &error, "Failed to receive register result");
      g_error_free (error);
      return;
    }

  wocky_stanza_get_type_info (iq, &type, &sub_type);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
          "Register: Response Invalid");
      goto out;
    }

    switch (sub_type)
    {
      int code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (iq, NULL, &error, NULL, NULL);

        switch (error->code)
          {
            case WOCKY_XMPP_ERROR_CONFLICT:
              code = WOCKY_CONNECTOR_ERROR_REGISTRATION_CONFLICT;
              break;
            case WOCKY_XMPP_ERROR_NOT_ACCEPTABLE:
              code = WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED;
              break;
            default:
              code = WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED;
          }

        abort_connect_code (self, code, "Registration: %s %s",
            wocky_xmpp_error_string (error->code),
            error->message);
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_RESULT");
        /* successfully registered. woo hoo. proceed to auth stage */
        priv->reg_op = XEP77_NONE;
        sasl_request_auth (self, priv->features);
        break;

      default:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_*");
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
            "Register: Response Invalid");
        break;
    }

 out:
  g_object_unref (iq);
}

/* ************************************************************************* */
/* BIND calls */
static void
iq_bind_resource (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;
  gchar *id = wocky_xmpp_connection_new_id (priv->conn);
  WockyNode *bind;
  WockyStanza *iq =
    wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
        NULL, NULL,
        '@', "id", id,
        '(', "bind", ':', WOCKY_XMPP_NS_BIND,
          '*', &bind,
        ')',
        NULL);

  /* if we have a specific resource to ask for, ask for it: otherwise the
   * server will make one up for us */
  if ((priv->resource != NULL) && (*priv->resource != '\0'))
    wocky_node_add_child_with_content (bind, "resource", priv->resource);

  DEBUG ("sending bind iq set stanza");
  wocky_xmpp_connection_send_stanza_async (priv->conn, iq, priv->cancellable,
      iq_bind_resource_sent_cb, self);
  g_free (id);
  g_object_unref (iq);
}

static void
iq_bind_resource_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send bind iq set");
      g_error_free (error);
      return;
    }

  DEBUG ("bind iq set stanza sent");
  wocky_xmpp_connection_recv_stanza_async (priv->conn, priv->cancellable,
      iq_bind_resource_recv_cb, data);
}

static void
iq_bind_resource_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *reply = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType sub = WOCKY_STANZA_SUB_TYPE_NONE;

  reply = wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &error);
  DEBUG ("bind iq response stanza received");
  if (reply == NULL)
    {
      abort_connect_error (self, &error, "Failed to receive bind iq result");
      g_error_free (error);
      return;
    }

  if (stream_error_abort (self, reply))
    goto out;

  wocky_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
          "Bind iq response invalid");
      goto out;
    }

  switch (sub)
    {
      WockyNode *node = NULL;
      WockyConnectorError code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (reply, NULL, &error, NULL, NULL);

        switch (error->code)
          {
            case WOCKY_XMPP_ERROR_BAD_REQUEST:
              code = WOCKY_CONNECTOR_ERROR_BIND_INVALID;
              break;
            case WOCKY_XMPP_ERROR_NOT_ALLOWED:
              code = WOCKY_CONNECTOR_ERROR_BIND_DENIED;
              break;
            case WOCKY_XMPP_ERROR_CONFLICT:
              code = WOCKY_CONNECTOR_ERROR_BIND_CONFLICT;
              break;
            default:
              code = WOCKY_CONNECTOR_ERROR_BIND_REJECTED;
          }

        abort_connect_code (self, code, "resource binding: %s",
            wocky_xmpp_error_string (error->code));
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        node = wocky_node_get_child (
          wocky_stanza_get_top_node (reply), "bind");
        if (node != NULL)
          node = wocky_node_get_child (node, "jid");

        /* store the returned id (or the original if none came back)*/
        g_free (priv->identity);
        if ((node != NULL) && (node->content != NULL) && *(node->content))
          priv->identity = g_strdup (node->content);
        else
          priv->identity = g_strdup (priv->jid);

        priv->state = WCON_XMPP_BOUND;
        establish_session (self);
        break;

      default:
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
            "Bizarre response to bind iq set");
        break;
    }

 out:
  g_object_unref (reply);
}

/* ************************************************************************* */
/* final stage: establish a session, if so advertised: */
void
establish_session (WockyConnector *self)
{
  WockyConnectorPrivate *priv = self->priv;
  WockyNode *feat = (priv->features != NULL) ?
    wocky_stanza_get_top_node (priv->features) : NULL;

  /* _if_ session setup is advertised, a session _must_ be established to *
   * allow presence/messaging etc to work. If not, it is not important    */
  if ((feat != NULL) &&
      wocky_node_get_child_ns (feat, "session", WOCKY_XMPP_NS_SESSION))
    {
      WockyXmppConnection *conn = priv->conn;
      gchar *id = wocky_xmpp_connection_new_id (conn);
      WockyStanza *session =
        wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
            WOCKY_STANZA_SUB_TYPE_SET,
            NULL, NULL,
            '@', "id", id,
            '(', "session", ':', WOCKY_XMPP_NS_SESSION,
            ')',
            NULL);
      wocky_xmpp_connection_send_stanza_async (conn, session, priv->cancellable,
          establish_session_sent_cb, self);
      g_object_unref (session);
      g_free (id);
    }
  else if (priv->reg_op == XEP77_CANCEL)
    {
      /* sessions unavailable and we are cancelling our registration: *
       * enter the xep77 code instead of completing the _async call   */
      xep77_begin (self);
    }
  else
    {
      if (priv->cancellable != NULL)
        {
          g_object_unref (priv->cancellable);
          priv->cancellable = NULL;
        }

      complete_operation (self);
    }
}

static void
establish_session_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send session iq set");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, priv->cancellable,
      establish_session_recv_cb, data);
}

static void
establish_session_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = self->priv;
  WockyStanza *reply = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType sub = WOCKY_STANZA_SUB_TYPE_NONE;

  reply = wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &error);

  if (reply == NULL)
    {
      abort_connect_error (self, &error, "Failed to receive session iq result");
      g_error_free (error);
      return;
    }

  if (stream_error_abort (self, reply))
    goto out;

  wocky_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
          "Session iq response invalid");
      goto out;
    }

  switch (sub)
    {
      WockyConnectorError code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (reply, NULL, &error, NULL, NULL);

        switch (error->code)
          {
            case WOCKY_XMPP_ERROR_INTERNAL_SERVER_ERROR:
              code = WOCKY_CONNECTOR_ERROR_SESSION_FAILED;
              break;
            case WOCKY_XMPP_ERROR_FORBIDDEN:
              code = WOCKY_CONNECTOR_ERROR_SESSION_DENIED;
              break;
            case WOCKY_XMPP_ERROR_CONFLICT:
              code = WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT;
              break;
            default:
              code = WOCKY_CONNECTOR_ERROR_SESSION_REJECTED;
          }

        abort_connect_code (self, code, "establish session: %s",
            wocky_xmpp_error_string (error->code));
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        if (priv->reg_op == XEP77_CANCEL)
          {
            /* session initialised: if we were cancelling our account *
             * we can now start the xep77 cancellation process        */
            xep77_begin (self);
          }
        else
          {
            if (priv->cancellable != NULL)
              {
                g_object_unref (priv->cancellable);
                priv->cancellable = NULL;
              }

            complete_operation (self);
          }
        break;

      default:
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
            "Bizarre response to session iq set");
        break;
    }

 out:
  g_object_unref (reply);
}

static void
connector_propagate_jid_and_sid (WockyConnector *self,
    gchar **jid,
    gchar **sid)
{
  if (jid != NULL)
    {
      if (*jid != NULL)
        g_warning ("overwriting non-NULL gchar * pointer arg (JID)");
      *jid = g_strdup (self->priv->identity);
    }

  if (sid != NULL)
    {
      if (*sid != NULL)
        g_warning ("overwriting non-NULL gchar * pointer arg (Session ID)");
      *sid = g_strdup (self->priv->session_id);
    }
}

/* *************************************************************************
 * exposed methods
 * ************************************************************************* */

/**
 * wocky_connector_connect_finish:
 * @self: a #WockyConnector instance.
 * @res: a #GAsyncResult (from your wocky_connector_connect_async() callback).
 * @jid: (%NULL to ignore) the user JID from the server is stored here.
 * @sid: (%NULL to ignore) the Session ID is stored here.
 * @error: (%NULL to ignore) the #GError (if any) is sored here.
 *
 * Called by the callback passed to wocky_connector_connect_async().
 *
 * Returns: a #WockyXmppConnection instance (success), or %NULL (failure).
 */
WockyXmppConnection *
wocky_connector_connect_finish (WockyConnector *self,
    GAsyncResult *res,
    gchar **jid,
    gchar **sid,
    GError **error)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (result, error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (self),
      wocky_connector_connect_async), NULL);

  connector_propagate_jid_and_sid (self, jid, sid);
  return self->priv->conn;
}

/**
 * wocky_connector_register_finish:
 * @self: a #WockyConnector instance.
 * @res: a #GAsyncResult (from your wocky_connector_register_async() callback).
 * @jid: (%NULL to ignore) the JID in effect after connection is stored here.
 * @sid: (%NULL to ignore) the Session ID after connection is stored here.
 * @error: (%NULL to ignore) the #GError (if any) is stored here.
 *
 * Called by the callback passed to wocky_connector_register_async().
 *
 * Returns: a #WockyXmppConnection instance (success), or %NULL (failure).
 */
WockyXmppConnection *
wocky_connector_register_finish (WockyConnector *self,
    GAsyncResult *res,
    gchar **jid,
    gchar **sid,
    GError **error)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (result, error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (self),
      wocky_connector_register_async), NULL);

  connector_propagate_jid_and_sid (self, jid, sid);
  return self->priv->conn;
}

/**
 * wocky_connector_unregister_finish:
 * @self: a #WockyConnector instance.
 * @res: a #GAsyncResult (from the wocky_connector_unregister_async() callback).
 * @error: (%NULL to ignore) the #GError (if any) is stored here.
 *
 * Called by the callback passed to wocky_connector_unregister_async().
 *
 * Returns: a #gboolean value %TRUE (success), or %FALSE (failure).
 */
gboolean
wocky_connector_unregister_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);
  GObject *obj = G_OBJECT (self);

  if (g_simple_async_result_propagate_error (result, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (res, obj,
      wocky_connector_unregister_async), FALSE);

  return TRUE;
}

static void
connector_connect_async (WockyConnector *self,
    gpointer source_tag,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnectorPrivate *priv = self->priv;

  /* 'host' is (by default) the part of the jid after the @
   *  it must be non-empty (although this test may need to be changed
   *  for serverless XMPP (eg 'Bonjour')): if the xmpp_host property
   *  is set, it takes precedence for the purposes of finding a
   *  an XMPP server: Otherwise we look for a SRV record for 'host',
   *  falling back to a direct connection to 'host' if that fails.
   */
  gchar *node = NULL;  /* username   */ /* @ */
  gchar *host = NULL;  /* domain.tld */ /* / */
  gchar *uniq = NULL;  /* uniquifier */

  if (priv->result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), cb, user_data,
          WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_IN_PROGRESS,
          "Connection already established or in progress");
      return;
    }

  if (priv->cancellable != NULL)
    {
      g_warning ("Cancellable already present, but the async result is NULL; "
          "something's wrong with the state of the connector, please file a bug.");
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  priv->result = g_simple_async_result_new (G_OBJECT (self), cb, user_data,
      source_tag);

  if (cancellable != NULL)
    priv->cancellable = g_object_ref (cancellable);

  wocky_decode_jid (priv->jid, &node, &host, &uniq);

  if (host == NULL)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BAD_JID,
          "Invalid JID %s", priv->jid);
      goto abort;
    }

  if (*host == '\0')
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BAD_JID,
          "Missing Domain %s", priv->jid);
      goto abort;
    }

  if (priv->resource == NULL)
    priv->resource = uniq;
  else
    g_free (uniq);

  priv->user   = node;
  priv->domain = host;
  priv->client = g_socket_client_new ();
  priv->state  = WCON_TCP_CONNECTING;

  /* if the user supplied a specific HOST or PORT, use those:
     if just a HOST is supplied, HOST:5222,
     if just a port, set HOST from the JID and use JIDHOST:PORT
     otherwise attempt to find a SRV record  */
  if ((priv->xmpp_host != NULL) || (priv->xmpp_port != 0))
    {
      guint port = (priv->xmpp_port == 0) ? 5222 : priv->xmpp_port;
      const gchar *srv = (priv->xmpp_host == NULL) ? host : priv->xmpp_host;

      DEBUG ("host: %s; port: %d", priv->xmpp_host, priv->xmpp_port);
      connect_to_host_async (self, srv, port);
    }
  else
    {
      g_socket_client_connect_to_service_async (priv->client,
          host, "xmpp-client", priv->cancellable, tcp_srv_connected, self);
    }
  return;

 abort:
  g_free (host);
  g_free (node);
  g_free (uniq);
  return;
}

/**
 * wocky_connector_connect_async:
 * @self: a #WockyConnector instance.
 * @cancellable: an #GCancellable, or %NULL
 * @cb: a #GAsyncReadyCallback to call when the operation completes.
 * @user_data: a #gpointer to pass to the callback.
 *
 * Connect to the account/server specified by the @self.
 * @cb should invoke wocky_connector_connect_finish().
 */
void
wocky_connector_connect_async (WockyConnector *self,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  connector_connect_async (self, wocky_connector_connect_async,
      cancellable, cb, user_data);
}


/**
 * wocky_connector_unregister_async:
 * @self: a #WockyConnector instance.
 * @cancellable: an #GCancellable, or %NULL
 * @cb: a #GAsyncReadyCallback to call when the operation completes.
 * @user_data: a #gpointer to pass to the callback @cb.
 *
 * Connect to the account/server specified by @self, and unregister (cancel)
 * the account there.
 * @cb should invoke wocky_connector_unregister_finish().
 */
void
wocky_connector_unregister_async (WockyConnector *self,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnectorPrivate *priv = self->priv;

  priv->reg_op = XEP77_CANCEL;
  connector_connect_async (self, wocky_connector_unregister_async,
      cancellable, cb, user_data);
}

/**
 * wocky_connector_register_async:
 * @self: a #WockyConnector instance.
 * @cancellable: an #GCancellable, or %NULL
 * @cb: a #GAsyncReadyCallback to call when the operation completes.
 * @user_data: a #gpointer to pass to the callback @cb.
 *
 * Connect to the account/server specified by @self, register (set up)
 * the account there and then log in to it.
 * @cb should invoke wocky_connector_register_finish().
 */
void
wocky_connector_register_async (WockyConnector *self,
    GCancellable *cancellable,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnectorPrivate *priv = self->priv;

  priv->reg_op = XEP77_SIGNUP;
  connector_connect_async (self, wocky_connector_register_async,
      cancellable, cb, user_data);
}

/**
 * wocky_connector_new:
 * @jid: a JID (user AT domain).
 * @pass: the password.
 * @resource: the resource (sans '/'), or NULL to autogenerate one.
 * @auth_registry: a #WockyAuthRegistry, or %NULL
 * @tls_handler: a #WockyTLSHandler, or %NULL
 *
 * Connect to the account/server specified by @self.
 * To set other #WockyConnector properties, use g_object_new() instead.
 *
 * Returns: a #WockyConnector instance which can be used to connect to,
 * register or cancel an account
 */
WockyConnector *
wocky_connector_new (const gchar *jid,
    const gchar *pass,
    const gchar *resource,
    WockyAuthRegistry *auth_registry,
    WockyTLSHandler *tls_handler)
{
  return g_object_new (WOCKY_TYPE_CONNECTOR,
      "jid", jid,
      "password", pass,
      "resource", resource,
      "auth-registry", auth_registry,
      "tls-handler", tls_handler,
      NULL);
}
