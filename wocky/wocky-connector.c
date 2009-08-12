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
 *
 * Sends and receives #WockyXmppStanzas from an underlying GIOStream.
 * negotiating TLS if possible and completing authentication with the server
 * by the "most suitable" method available.
 * Returns a WockyXmppConnection object to the user on successful completion.
 */

/*
 * tcp_srv_connected
 * │
 * ├→ tcp_host_connected                       ①
 * │  ↓                                        ↑
 * └→ maybe_old_ssl                            jabber_auth_reply
 *    ↓                                        ↑
 *    xmpp_init ←─────────────┬──┐             jabber_auth_query
 *    ↓                       │  │             ↑
 *    xmpp_init_sent_cb       │  │             ├──────────────────────┐
 *    ↓                       │  │             │                      │
 *    xmpp_init_recv_cb       │  │             │ jabber_auth_try_passwd
 *    │ ↓                     │  │             │                      ↑
 *    │ xmpp_features_cb      │  │             jabber_auth_try_digest │
 *    │ │ │ ↓                 │  │             ↑                      │
 *    │ │ │ starttls_sent_cb  │  │             ├──────────────────────┘
 *    │ │ │ ↓                 │  │             │
 *    │ │ │ starttls_recv_cb ─┘  │             jabber_auth_fields
 *    │ │ ↓                      │             ↑
 *    │ │ request-auth           │             jabber_auth_init_sent
 *    │ │ ↓                      │             ↑
 *    │ │ auth_done ─────────────┴─[no sasl]─→ jabber_auth_init
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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#define DEBUG_FLAG DEBUG_CONNECTOR
#include "wocky-debug.h"

#include "wocky-sasl-auth.h"
#include "wocky-namespaces.h"
#include "wocky-xmpp-connection.h"
#include "wocky-connector.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

G_DEFINE_TYPE (WockyConnector, wocky_connector, G_TYPE_OBJECT);

static void wocky_connector_class_init (WockyConnectorClass *klass);

/* XMPP connect/auth/etc handlers */
static void tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector);
static void tcp_host_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector);

static void maybe_old_ssl (WockyConnector *self);

static void xmpp_init (WockyConnector *connector, gboolean new_conn);
static void xmpp_init_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xmpp_init_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xmpp_features_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void starttls_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void starttls_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data);

static void request_auth (WockyConnector *object,
    WockyXmppStanza *stanza);
static void auth_done (GObject *source,
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
    WockyXmppNode *req);
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
jabber_auth_init (WockyConnector *connector);

static void
jabber_auth_init_sent (GObject *source,
    GAsyncResult *res,
    gpointer data);

static void
jabber_auth_fields (GObject *source,
    GAsyncResult *res,
    gpointer data);

static void
jabber_auth_try_digest (WockyConnector *self);

static void
jabber_auth_try_passwd (WockyConnector *self);

static void
jabber_auth_query (GObject *source,
    GAsyncResult *res,
    gpointer data);

static void
jabber_auth_reply (GObject *source,
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
  PROP_TLS_INSECURE_OK,
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


typedef struct _WockyConnectorPrivate WockyConnectorPrivate;

struct _WockyConnectorPrivate
{
  /* properties: */
  GIOStream *stream;

  /* caller's choices about what to allow/disallow */
  gboolean auth_insecure_ok; /* can we auth over non-ssl */
  gboolean cert_insecure_ok; /* care about bad certs etc? not handled yet */
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

  /* XMPP connection data */
  WockyXmppStanza *features;

  /* misc internal state: */
  WockyConnectorState state;
  gboolean dispose_has_run;
  gboolean authed;
  gboolean encrypted;
  gboolean connected;
  /* register/cancel account, or normal login */
  WockyConnectorXEP77Op reg_op;
  GSimpleAsyncResult *result;
  WockySaslAuthMechanism mech;

  /* socket/tls/etc structures */
  GSocketClient *client;
  GSocketConnection *sock;
  WockyTLSSession *tls_sess;
  WockyTLSConnection *tls;
  WockyXmppConnection *conn;
};

#define WOCKY_CONNECTOR_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE((o),WOCKY_TYPE_CONNECTOR,WockyConnectorPrivate))

/* choose an appropriate chunk of text describing our state for debug/error */
static char *
state_message (WockyConnectorPrivate *priv, const char *str)
{
  GString *msg = g_string_new ("");
  const char *state = NULL;

  if (priv->authed)
    state = "Authentication Completed";
  else if (priv->encrypted)
    {
      if (priv->legacy_ssl)
        state = "SSL Negotiated";
      else
        state = "TLS Negotiated";
    }
  else if (priv->connected)
    state = "TCP Connection Established";
  else
    state = "Connecting... ";

  g_string_printf (msg, "%s: %s", state, str);
  return g_string_free (msg, FALSE);
}

static void
abort_connect_error (WockyConnector *connector,
    GError **error,
    const char *fmt,
    ...)
{
  GSimpleAsyncResult *tmp = NULL;
  WockyConnectorPrivate *priv = NULL;
  va_list args;

  DEBUG ("connector: %p", connector);
  priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

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

  tmp = priv->result;
  priv->result = NULL;
  g_simple_async_result_set_from_error (tmp, *error);
  g_simple_async_result_complete (tmp);
  g_object_unref (tmp);
}

static void
abort_connect (WockyConnector *connector,
    GError *error)
{
  GSimpleAsyncResult *tmp = NULL;
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

  if (priv->sock != NULL)
    {
      g_object_unref (priv->sock);
      priv->sock = NULL;
    }
  priv->state = WCON_DISCONNECTED;

  tmp = priv->result;
  priv->result = NULL;
  g_simple_async_result_set_from_error (tmp, error);
  g_simple_async_result_complete (tmp);
  g_object_unref (tmp);
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
wocky_connector_init (WockyConnector *obj)
{
}

static void
wocky_connector_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyConnector *connector = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

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
      case PROP_TLS_INSECURE_OK:
        priv->cert_insecure_ok = g_value_get_boolean (value);
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
        if (g_value_get_string (value) != NULL)
          priv->pass = g_value_dup_string (value);
        else
          {
            g_warning ("%s property %s cannot be NULL",
                G_OBJECT_TYPE_NAME (object), pspec->name);
            priv->pass = g_strdup ("");
          }
        break;
      case PROP_RESOURCE:
        g_free (priv->resource);
        if ((g_value_get_string (value) != NULL) &&
            *g_value_get_string (value) != '\0')
          priv->resource = g_value_dup_string (value);
        else
          priv->resource = g_strdup_printf ("Wocky_%x", rand());
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

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
      case PROP_TLS_INSECURE_OK:
        g_value_set_boolean (value, priv->cert_insecure_ok);
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

  spec = g_param_spec_boolean ("ignore-ssl-errors", "ignore-ssl-errors",
      "Whether recoverable TLS errors should be ignored", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_TLS_INSECURE_OK, spec);

  spec = g_param_spec_boolean ("plaintext-auth-allowed",
      "plaintext-auth-allowed",
      "Whether auth info can be sent in the clear", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_AUTH_INSECURE_OK, spec);

  spec = g_param_spec_boolean ("encrypted-plain-auth-ok",
      "encrypted-plain-auth-ok",
      "Whether PLAIN auth can be used when encrypted", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ENC_PLAIN_AUTH_OK, spec);

  spec = g_param_spec_boolean ("tls-required", "TLS required",
      "Whether SSL/TLS is required", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_TLS_REQUIRED, spec);

  spec = g_param_spec_string ("jid", "jid", "The XMPP jid", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_JID, spec);

  spec = g_param_spec_string ("email", "email", "user's email adddress", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_EMAIL, spec);

  spec = g_param_spec_string ("password", "pass", "Password", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_PASS, spec);

  spec = g_param_spec_string ("resource", "resource",
      "XMPP resource to append to the jid", NULL,
      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_RESOURCE, spec);

  spec = g_param_spec_string ("identity", "identity",
      "jid + resource (set by XMPP server)", NULL,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_IDENTITY, spec);

  spec = g_param_spec_string ("xmpp-server", "XMPP server",
      "XMPP connect server hostname or address", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_XMPP_HOST, spec);

  spec = g_param_spec_uint ("xmpp-port", "XMPP port",
      "XMPP port", 0, 65535, 0,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_XMPP_PORT, spec);

  spec = g_param_spec_object ("features", "XMPP Features",
      "Last XMPP Feature Stanza advertised by server", WOCKY_TYPE_XMPP_STANZA,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_FEATURES, spec);

  spec = g_param_spec_boolean ("legacy", "Legacy Jabber Support",
      "Old style Jabber (Auth) support", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_LEGACY, spec);

  spec = g_param_spec_boolean ("old-ssl", "Legacy SSL Support",
      "Old style SSL support", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_LEGACY_SSL, spec);

  spec = g_param_spec_string ("session-id", "XMPP Session ID",
      "XMPP Session ID", NULL,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_SESSION_ID, spec);
}

#define UNREF_AND_FORGET(x) if (x != NULL) { g_object_unref (x); x = NULL; }
#define GFREE_AND_FORGET(x) g_free (x); x = NULL;

static void
wocky_connector_dispose (GObject *object)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;
  UNREF_AND_FORGET (priv->conn);
  UNREF_AND_FORGET (priv->client);
  UNREF_AND_FORGET (priv->sock);
  UNREF_AND_FORGET (priv->tls_sess);
  UNREF_AND_FORGET (priv->tls);
  UNREF_AND_FORGET (priv->features);

  if (G_OBJECT_CLASS (wocky_connector_parent_class )->dispose)
    G_OBJECT_CLASS (wocky_connector_parent_class)->dispose (object);
}

static void
wocky_connector_finalize (GObject *object)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  GFREE_AND_FORGET (priv->jid);
  GFREE_AND_FORGET (priv->user);
  GFREE_AND_FORGET (priv->domain);
  GFREE_AND_FORGET (priv->resource);
  GFREE_AND_FORGET (priv->identity);
  GFREE_AND_FORGET (priv->xmpp_host);
  GFREE_AND_FORGET (priv->pass);
  GFREE_AND_FORGET (priv->session_id);
  GFREE_AND_FORGET (priv->email);

  G_OBJECT_CLASS (wocky_connector_parent_class)->finalize (object);
}

static void
tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

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

      DEBUG ("SRV connect failed: %s", error->message);

      if (error != NULL)
        {
          /* io-error-quark => there IS a SRV record but we could not
             connect: we do not fall through in this case: */
          if (error->domain == g_io_error_quark ())
            {
              abort_connect_error (self, &error, "Bad SRV record");
              g_error_free (error);
              return;
            }
          else
            {
              const gchar *domain = g_quark_to_string (error->domain);
              DEBUG ("SRV error is: %s:%d", domain, error->code);
            }
        }
      DEBUG ("Falling back to HOST connection\n");

      g_error_free (error);
      priv->state = WCON_TCP_CONNECTING;

      /* decode a hostname from the JID here: Don't check for an explicit *
       * connect host supplied by the user as we shouldn't even try a SRV *
       * connection in that case, and should therefore never get here     */
      wocky_decode_jid (priv->jid, &node, &host, NULL);

      if ((host != NULL) && (*host != '\0'))
        g_socket_client_connect_to_host_async (priv->client,
            host, port, NULL, tcp_host_connected, connector);
      else
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BAD_JID,
            "JID contains no domain: %s", priv->jid);

      g_free (node);
      g_free (host);
    }
  else
    {
      DEBUG ("SRV connection succeeded");
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  GSocketClient *sock = G_SOCKET_CLIENT (source);

  priv->sock = g_socket_client_connect_to_host_finish (sock, result, &error);

  if (priv->sock == NULL)
    {
      DEBUG ("HOST connect failed: %s\n", error->message);
      abort_connect_error (connector, &error, "connection failed");
      g_error_free (error);
    }
  else
    {
      DEBUG ("HOST connection succeeded");
      priv->connected = TRUE;
      priv->state = WCON_TCP_CONNECTED;
      maybe_old_ssl (self);
    }
}

/* ************************************************************************* */
/* legacy jabber support                                                     */
static void
jabber_auth_init (WockyConnector *connector)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);
  WockyXmppConnection *conn = priv->conn;
  gchar *id = wocky_xmpp_connection_new_id (priv->conn);
  WockyXmppStanza *iq = NULL;

  DEBUG ("");
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      NULL, priv->domain,
      WOCKY_NODE_ATTRIBUTE, "id", id,
      WOCKY_NODE, "query", WOCKY_NODE_XMLNS, WOCKY_JABBER_NS_AUTH,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_connection_send_stanza_async (conn, iq, NULL,
      jabber_auth_init_sent, connector);

  g_free (id);
  g_object_unref (iq);
}

static void
jabber_auth_init_sent (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection *conn = priv->conn;
  GError *error = NULL;

  DEBUG ("");
  if (!wocky_xmpp_connection_send_stanza_finish (conn, res, &error))
    {
      abort_connect_error (self, &error, "Jabber Auth Init");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (conn, NULL,
      jabber_auth_fields, data);
}

static void
jabber_auth_fields (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection *conn = priv->conn;
  GError *error = NULL;
  WockyXmppStanza *fields = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType sub = WOCKY_STANZA_SUB_TYPE_NONE;

  DEBUG ("");
  fields = wocky_xmpp_connection_recv_stanza_finish (conn, res, &error);

  if (fields == NULL)
    {
      abort_connect_error (self, &error, "Jabber Auth Fields");
      g_error_free (error);
      return;
    }

  wocky_xmpp_stanza_get_type_info (fields, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED,
          "Jabber Auth Init: Response Invalid");
      goto out;
    }

  switch (sub)
    {
      WockyXmppNode *node = NULL;
      WockyXmppNode *text = NULL;
      const gchar *tag = NULL;
      const gchar *msg = NULL;
      WockyConnectorError code;
      gboolean passwd;
      gboolean digest;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        node = fields->node;
        tag = wocky_xmpp_node_unpack_error (node, NULL, &text, NULL, NULL);
        if (tag == NULL)
          tag = "unknown-error";
        msg = (text != NULL) ? text->content : "";

        if (!wocky_strdiff ("service-unavailable", tag))
          code = WOCKY_CONNECTOR_ERROR_JABBER_AUTH_UNAVAILABLE;
        else
          code = WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED;

        abort_connect_code (self, code, "Jabber Auth: %s %s", tag, msg);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        passwd = FALSE;
        digest = FALSE;
        node = fields->node;
        node = wocky_xmpp_node_get_child_ns (node, "query",
            WOCKY_JABBER_NS_AUTH);
        if ((node != NULL) &&
            (wocky_xmpp_node_get_child (node, "resource") != NULL) &&
            (wocky_xmpp_node_get_child (node, "username") != NULL))
          {
            passwd = wocky_xmpp_node_get_child (node, "password") != NULL;
            digest = wocky_xmpp_node_get_child (node, "digest") != NULL;
          }

        if (digest)
          jabber_auth_try_digest (self);
        else if (passwd)
          jabber_auth_try_passwd (self);
        else
          abort_connect_code (self, WOCKY_CONNECTOR_ERROR_JABBER_AUTH_NO_MECHS,
              "Jabber Auth: No Known Mechanisms");
        break;

      default:
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED,
            "Bizarre response to Jabber Auth request");
        break;
    }

 out:
  g_object_unref (fields);
}

static void
jabber_auth_try_digest (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection *conn = priv->conn;
  gchar *hsrc = g_strconcat (priv->session_id, priv->pass, NULL);
  gchar *sha1 = g_compute_checksum_for_string (G_CHECKSUM_SHA1, hsrc, -1);
  gchar *iqid = wocky_xmpp_connection_new_id (priv->conn);
  WockyXmppStanza *iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", iqid,
      WOCKY_NODE, "query", WOCKY_NODE_XMLNS, WOCKY_JABBER_NS_AUTH,
      WOCKY_NODE, "username", WOCKY_NODE_TEXT, priv->user, WOCKY_NODE_END,
      WOCKY_NODE, "digest", WOCKY_NODE_TEXT, sha1, WOCKY_NODE_END,
      WOCKY_NODE, "resource", WOCKY_NODE_TEXT, priv->resource, WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  DEBUG ("checksum: %s", sha1);
  wocky_xmpp_connection_send_stanza_async (conn, iq, NULL,
      jabber_auth_query, self);

  g_object_unref (iq);
  g_free (iqid);
  g_free (hsrc);
  g_free (sha1);
}

static void
jabber_auth_try_passwd (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection *conn = priv->conn;
  gchar *iqid = wocky_xmpp_connection_new_id (priv->conn);
  WockyXmppStanza *iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      WOCKY_NODE_ATTRIBUTE, "id", iqid,
      WOCKY_NODE, "query", WOCKY_NODE_XMLNS, WOCKY_JABBER_NS_AUTH,
      WOCKY_NODE, "username", WOCKY_NODE_TEXT, priv->user, WOCKY_NODE_END,
      WOCKY_NODE, "password", WOCKY_NODE_TEXT, priv->pass, WOCKY_NODE_END,
      WOCKY_NODE, "resource", WOCKY_NODE_TEXT, priv->resource, WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  DEBUG ("");
  wocky_xmpp_connection_send_stanza_async (conn, iq, NULL,
      jabber_auth_query, self);

  g_object_unref (iq);
  g_free (iqid);
}

static void
jabber_auth_query (GObject *source, GAsyncResult *res, gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection *conn = priv->conn;
  GError *error = NULL;

  DEBUG ("");
  if (!wocky_xmpp_connection_send_stanza_finish (conn, res, &error))
    {
      abort_connect_error (self, &error, "Jabber Auth IQ Set");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (conn, NULL,
      jabber_auth_reply, data);
}

static void
jabber_auth_reply (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection *conn = priv->conn;
  GError *error = NULL;
  WockyXmppStanza *reply = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType sub = WOCKY_STANZA_SUB_TYPE_NONE;

  DEBUG ("");
  reply = wocky_xmpp_connection_recv_stanza_finish (conn, res, &error);

  if (reply == NULL)
    {
      abort_connect_error (self, &error, "Jabber Auth Reply");
      g_error_free (error);
      return;
    }

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED,
          "Jabber Auth Reply: Response Invalid");
      goto out;
    }

  switch (sub)
    {
      WockyXmppNode *node = NULL;
      WockyXmppNode *text = NULL;
      const gchar *tag = NULL;
      const gchar *msg = NULL;
      WockyConnectorError code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        node = reply->node;
        tag = wocky_xmpp_node_unpack_error (node, NULL, &text, NULL, NULL);
        if (tag == NULL)
          tag = "unknown-error";
        msg = (text != NULL) ? text->content : "";

        if (!wocky_strdiff ("not-authorized", tag))
          code = WOCKY_CONNECTOR_ERROR_JABBER_AUTH_REJECTED;
        else if (!wocky_strdiff ("conflict", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_CONFLICT;
        else if (!wocky_strdiff ("not-acceptable", tag))
          code = WOCKY_CONNECTOR_ERROR_JABBER_AUTH_INCOMPLETE;
        else
          code = WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED;

        abort_connect_code (self, code, "Jabber Auth: %s %s", tag, msg);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        g_free (priv->identity);
        priv->state = WCON_XMPP_BOUND;
        priv->authed = TRUE;
        priv->mech = WOCKY_SASL_AUTH_NR_MECHANISMS;
        priv->identity = g_strdup_printf ("%s@%s/%s",
            priv->user, priv->domain, priv->resource);
        /* if there has been no features stanza, this will just finish up *
         * if there has been a feature stanza, we are in an XMPP 1.x      *
         * server that _only_ supports old style auth (no SASL). In this  *
         * bizarre situation, we would then proceed as if we were in a    *
         * normal XMPP server after a successful bind.                    */
        establish_session (self);
        break;

      default:
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_JABBER_AUTH_FAILED,
            "Bizarre response to Jabber Auth request");
        break;
    }

  out:
    g_object_unref (reply);

}

/* ************************************************************************* */
/* old-style SSL                                                             */
static void
maybe_old_ssl (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (priv->legacy_ssl && !priv->encrypted)
    {
      GError *error = NULL;

      g_assert (priv->conn == NULL);
      g_assert (priv->sock != NULL);

      DEBUG ("creating SSL session");
      priv->tls_sess = wocky_tls_session_new (G_IO_STREAM (priv->sock));
      if (priv->tls_sess == NULL)
        {
          abort_connect_code (self, WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
              "SSL Session Failed");
          return;
        }

      DEBUG ("beginning SSL handshake");
      priv->tls = wocky_tls_session_handshake (priv->tls_sess, NULL, &error);
      DEBUG ("completed SSL handshake");

      if (priv->tls == NULL)
        {
          abort_connect_error (self, &error, "SSL Handshake Error");
          g_error_free (error);
          return;
        }

      priv->encrypted = TRUE;
      priv->conn = wocky_xmpp_connection_new (G_IO_STREAM (priv->tls));

      xmpp_init (self, FALSE);
    }
  else
    {
      xmpp_init (self, TRUE);
    }
}

/* ************************************************************************* */
/* standard XMPP stanza handling                                             */
static void
xmpp_init (WockyConnector *connector, gboolean new_conn)
{
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (new_conn)
    {
      g_assert (priv->conn == NULL);
      priv->conn = wocky_xmpp_connection_new (G_IO_STREAM(priv->sock));
    }

  DEBUG ("sending XMPP stream open to server");
  wocky_xmpp_connection_send_open_async (priv->conn, priv->domain, NULL,
      "1.0", NULL, NULL, NULL, xmpp_init_sent_cb, connector);
}

static void
xmpp_init_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (!wocky_xmpp_connection_send_open_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send open stanza");
      g_error_free (error);
      return;
    }

  DEBUG ("waiting for stream open from server");
  wocky_xmpp_connection_recv_open_async (priv->conn, NULL,
      xmpp_init_recv_cb, data);
}

static void
xmpp_init_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  gchar *debug = NULL;
  gchar *version = NULL;
  gchar *from = NULL;
  gchar *id = NULL;
  gdouble ver = 0;

  if (!wocky_xmpp_connection_recv_open_finish (priv->conn, result, NULL,
          &from, &version, NULL, &id, &error))
    {
      char *msg = state_message (priv, error->message);
      abort_connect_error (self, &error, msg);
      g_free (msg);
      g_error_free (error);
      goto out;
    }

  g_free (priv->session_id);
  priv->session_id = g_strdup (id);

  debug = state_message (priv, "");
  DEBUG ("%s: received XMPP v%s stream open from server", debug, version);
  g_free (debug);

  ver = (version != NULL) ? atof (version) : -1;

  if (ver < 1.0)
    {
      if (!priv->legacy_support)
        abort_connect_code (self, WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER,
            "Server not XMPP 1.0 Compliant");
      else
        jabber_auth_init (self);
      goto out;
    }

  DEBUG ("waiting for feature stanza from server");
  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      xmpp_features_cb, data);

 out:
  g_free (version);
  g_free (from);
  g_free (id);
}

/* ************************************************************************* */
/* handle stream errors                                                      */
static gboolean
stream_error_abort (WockyConnector *connector,
    WockyXmppStanza *stanza)
{
  GError *error = NULL;

  error = wocky_xmpp_stanza_to_gerror (stanza);
  if (error == NULL)
    return FALSE;

  DEBUG ("Received stream error: %s", error->message);

  abort_connect (connector, error);

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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  WockyXmppNode   *node;
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

  DEBUG ("received feature stanza from server");
  node = stanza->node;

  if (wocky_strdiff (node->name, "features") ||
      wocky_strdiff (wocky_xmpp_node_get_ns (node), WOCKY_XMPP_NS_STREAM))
    {
      char *msg = state_message (priv, "Malformed or missing feature stanza");
      abort_connect_code (data, WOCKY_CONNECTOR_ERROR_BAD_FEATURES, msg);
      g_free (msg);
      goto out;
    }

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
    wocky_xmpp_node_get_child_ns (node, "starttls", WOCKY_XMPP_NS_TLS) != NULL;
  can_bind =
    wocky_xmpp_node_get_child_ns (node, "bind", WOCKY_XMPP_NS_BIND) != NULL;

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
      WockyXmppStanza *starttls = wocky_xmpp_stanza_new ("starttls");
      wocky_xmpp_node_set_ns (starttls->node, WOCKY_XMPP_NS_TLS);
      DEBUG ("sending TLS request");
      wocky_xmpp_connection_send_stanza_async (priv->conn, starttls,
          NULL, starttls_sent_cb, data);
      g_object_unref (starttls);
      goto out;
    }

  if (!priv->authed && priv->reg_op == XEP77_SIGNUP)
    {
      xep77_begin (self);
      goto out;
    }

  if (!priv->authed)
    {
      request_auth (self, stanza);
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
starttls_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  GError *error = NULL;

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result,
          &error))
    {
      abort_connect_error (data, &error, "Failed to send STARTTLS stanza");
      g_error_free (error);
      return;
    }

  DEBUG ("sent TLS request");
  wocky_xmpp_connection_recv_stanza_async (priv->conn,
      NULL, starttls_recv_cb, data);
}

static void
starttls_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  WockyXmppStanza *stanza;
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppNode *node;

  stanza =
    wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &error);

  if (stanza == NULL)
    {
      abort_connect_error (data, &error, "STARTTLS reply not received");
      g_error_free (error);
      goto out;
    }

  if (stream_error_abort (self, stanza))
    goto out;

  DEBUG ("received TLS response");
  node = stanza->node;

  if (wocky_strdiff (node->name, "proceed") ||
      wocky_strdiff (wocky_xmpp_node_get_ns (node), WOCKY_XMPP_NS_TLS))
    {
      abort_connect_code (data, WOCKY_CONNECTOR_ERROR_TLS_REFUSED,
          "STARTTLS refused by server");
      goto out;
    }
  else
    {
      DEBUG ("starting client TLS handshake");
      priv->tls_sess = wocky_tls_session_new (G_IO_STREAM (priv->sock));
      priv->tls = wocky_tls_session_handshake (priv->tls_sess, NULL, &error);
      DEBUG ("completed TLS handshake");

      if (priv->tls == NULL)
        {
          abort_connect_error (data, &error, "TLS Handshake Error");
          g_error_free (error);
          goto out;
        }

      priv->encrypted = TRUE;
      /* throw away the old connection object, we're in TLS land now */
      g_object_unref (priv->conn);
      priv->conn = wocky_xmpp_connection_new (G_IO_STREAM (priv->tls));
      xmpp_init (self, FALSE);
    }

 out:
  if (stanza != NULL)
    g_object_unref (stanza);
}

/* ************************************************************************* */
/* AUTH calls */

static void
request_auth (WockyConnector *object,
    WockyXmppStanza *stanza)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockySaslAuth *s =
    wocky_sasl_auth_new (priv->domain, priv->user, priv->pass, priv->conn);
  gboolean clear = FALSE;

  if (priv->auth_insecure_ok ||
      (priv->encrypted && priv->encrypted_plain_auth_ok))
    clear = TRUE;

  DEBUG ("handing over control to SASL module");
  wocky_sasl_auth_authenticate_async (s, stanza, clear, NULL, auth_done, self);
}

static void
auth_done (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (source);

  if (!wocky_sasl_auth_authenticate_finish (sasl, result, &error))
    {
      /* nothing to add, the SASL error should be informative enough */
      DEBUG ("SASL complete (failure)");

      /* except: if there's no SASL and Jabber auth is available, we *
       * are allowed to attempt that instead                         */
      if ((error->domain == WOCKY_SASL_AUTH_ERROR) &&
          (error->code == WOCKY_SASL_AUTH_ERROR_SASL_NOT_SUPPORTED) &&
          (wocky_xmpp_node_get_child_ns (priv->features->node, "auth",
              WOCKY_JABBER_NS_AUTH_FEATURE) != NULL))
        jabber_auth_init (self);
      else
        abort_connect_error (self, &error, "");

      g_error_free (error);
      goto out;
    }

  DEBUG ("SASL complete (success)");
  priv->state = WCON_XMPP_AUTHED;
  priv->authed = TRUE;
  priv->mech = wocky_sasl_auth_mechanism_used (sasl);
  wocky_xmpp_connection_reset (priv->conn);
  xmpp_init (self, FALSE);
 out:
  g_object_unref (sasl);
}

/* ************************************************************************* */
/* XEP 0077 register/cancel calls                                            */
static void
xep77_cancel_send (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *iqs = NULL;
  gchar *iid = NULL;

  DEBUG ("");

  iid = wocky_xmpp_connection_new_id (priv->conn);
  iqs = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET,
      /* FIXME: It is debatable (XEP0077 section 3.2) whether we should *
       * include our JID here. The examples include it, the text states *
       * that we SHOULD NOT, at least in some use cases                 */
      NULL /* priv->identity */,
      priv->domain,
      WOCKY_NODE_ATTRIBUTE, "id", iid,
      WOCKY_NODE, "query", WOCKY_NODE_XMLNS, WOCKY_XEP77_NS_REGISTER,
      WOCKY_NODE, "remove", WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_connection_send_stanza_async (priv->conn, iqs, NULL,
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, res, &error))
    {
      abort_connect_error (self, &error, "Failed to send unregister iq set");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      xep77_cancel_recv, self);
}

static void
xep77_cancel_recv (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *iq = NULL;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  DEBUG ("");
  iq = wocky_xmpp_connection_recv_stanza_finish (priv->conn, res, &error);
  g_simple_async_result_set_op_res_gboolean (priv->result, FALSE);

  if (iq == NULL)
    {
      g_simple_async_result_set_from_error (priv->result, error);
      g_error_free (error);
      goto out;
    }

  wocky_xmpp_stanza_get_type_info (iq, &type, &sub_type);

  DEBUG ("type == %d; sub_type: %d", type, sub_type);

  if (type == WOCKY_STANZA_TYPE_STREAM_ERROR)
    {
      error = wocky_xmpp_stanza_to_gerror (iq);

      if ((error != NULL) &&
          (error->code == WOCKY_XMPP_STREAM_ERROR_NOT_AUTHORIZED))
        g_simple_async_result_set_op_res_gboolean (priv->result, TRUE);
      else
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
      WockyXmppNode *txt;
      const gchar *err;
      const gchar *msg;
      int code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        err = wocky_xmpp_node_unpack_error (iq->node, NULL, &txt, NULL, NULL);

        if (err == NULL)
          err = "unknown-error";
        msg = (txt != NULL) ? txt->content : "";

        if (!wocky_strdiff ("forbidden", err) ||
            !wocky_strdiff ("not-allowed", err))
          code = WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED;
        else
          code = WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED;

        g_simple_async_result_set_error (priv->result,
            WOCKY_CONNECTOR_ERROR, code, "Unregister: %s", msg);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        g_simple_async_result_set_op_res_gboolean (priv->result, TRUE);
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
  g_simple_async_result_complete (priv->result);
  priv->state = WCON_DISCONNECTED;
}

static void
xep77_begin (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *iqs = NULL;
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
  iqs = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET,
      jid, priv->domain,
      WOCKY_NODE_ATTRIBUTE, "id", iid,
      WOCKY_NODE, "query",
      WOCKY_NODE_XMLNS, WOCKY_XEP77_NS_REGISTER,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_connection_send_stanza_async (priv->conn, iqs, NULL,
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send register iq get");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      xep77_begin_recv, self);
}

static void
xep77_begin_recv (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *iq = NULL;
  WockyXmppNode *query = NULL;
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

  wocky_xmpp_stanza_get_type_info (iq, &type, &sub_type);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
          "Register: Response Invalid");
      goto out;
    }

  switch (sub_type)
    {
      WockyXmppNode *txt;
      const gchar *err;
      const gchar *msg;
      int code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_ERROR");
        err = wocky_xmpp_node_unpack_error (iq->node, NULL, &txt, NULL, NULL);

        if (err == NULL)
          err = "unknown-error";
        msg = (txt != NULL) ? txt->content : "";

        if (!wocky_strdiff ("service-unavailable", err))
          code = WOCKY_CONNECTOR_ERROR_REGISTRATION_UNAVAILABLE;
        else
          code = WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED;

        abort_connect_code (self, code, "Registration: %s", msg);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_RESULT");
        query = wocky_xmpp_node_get_child_ns (iq->node, "query",
            WOCKY_XEP77_NS_REGISTER);

        if (query == NULL)
          {
            abort_connect_code (self,
                WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
                "Malformed response to register iq");
            goto out;
          }

        /* already registered. woo hoo. proceed to auth stage */
        if (wocky_xmpp_node_get_child (query, "registered") != NULL)
          {
            priv->reg_op = XEP77_NONE;
            request_auth (self, priv->features);
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
    WockyXmppNode *req)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *riq = NULL;
  WockyXmppNode *reg = NULL;
  GSList *arg = NULL;
  gchar *jid = g_strdup_printf ("%s@%s", priv->user, priv->domain);
  gchar *iid = wocky_xmpp_connection_new_id (priv->conn);
  guint args = 0;

  DEBUG ("");

  riq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET,
      jid, priv->domain,
      WOCKY_NODE_ATTRIBUTE, "id", iid, WOCKY_STANZA_END);
  reg = wocky_xmpp_node_add_child_ns (riq->node, "query",
      WOCKY_XEP77_NS_REGISTER);

  for (arg = req->children; arg != NULL; arg = g_slist_next (arg))
    {
      gchar *value = NULL;
      WockyXmppNode *a = (WockyXmppNode *) arg->data;

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
      wocky_xmpp_node_add_child_with_content (reg, a->name, value);
      args++;
    }

  /* we understood all args, and there was at least one of them: */
  if (args > 0)
    wocky_xmpp_connection_send_stanza_async (priv->conn, riq, NULL,
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send registration");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      xep77_signup_recv, self);
}

static void
xep77_signup_recv (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *iq = NULL;
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

  wocky_xmpp_stanza_get_type_info (iq, &type, &sub_type);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED,
          "Register: Response Invalid");
      goto out;
    }

    switch (sub_type)
    {
      WockyXmppNode *txt;
      const gchar *err;
      const gchar *msg;
      int code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_ERROR");
        err = wocky_xmpp_node_unpack_error (iq->node, NULL, &txt, NULL, NULL);

        if (err == NULL)
          err = "unknown-error";
        msg = (txt != NULL) ? txt->content : "";

        if (!wocky_strdiff ("conflict", err))
            code = WOCKY_CONNECTOR_ERROR_REGISTRATION_CONFLICT;
        else if (!wocky_strdiff ("not-acceptable", err))
          code = WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED;
        else
          code = WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED;

        abort_connect_code (self, code, "Registration: %s %s", err, msg);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        DEBUG ("WOCKY_STANZA_SUB_TYPE_RESULT");
        /* successfully registered. woo hoo. proceed to auth stage */
        priv->reg_op = XEP77_NONE;
        request_auth (self, priv->features);
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  gchar *id = wocky_xmpp_connection_new_id (priv->conn);
  WockyXmppStanza *iq =
    wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
        NULL, NULL,
        WOCKY_NODE_ATTRIBUTE, "id", id,
        WOCKY_NODE, "bind", WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_BIND,
        WOCKY_NODE_END,
        WOCKY_STANZA_END);

  /* if we have a specific resource to ask for, ask for it: otherwise the
   * server will make one up for us */
  if ((priv->resource != NULL) && (*priv->resource != '\0'))
    {
      WockyXmppNode *bind = wocky_xmpp_node_get_child (iq->node, "bind");
      wocky_xmpp_node_add_child_with_content (bind, "resource", priv->resource);
    }

  DEBUG ("sending bind iq set stanza");
  wocky_xmpp_connection_send_stanza_async (priv->conn, iq, NULL,
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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send bind iq set");
      g_error_free (error);
      return;
    }

  DEBUG ("bind iq set stanza sent");
  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      iq_bind_resource_recv_cb, data);
}

static void
iq_bind_resource_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *reply = NULL;
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

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
          "Bind iq response invalid");
      goto out;
    }

  switch (sub)
    {
      WockyXmppNode *node = NULL;
      const char *tag = NULL;
      WockyConnectorError code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        node = wocky_xmpp_node_get_child (reply->node, "error");
        if (node != NULL)
            node = wocky_xmpp_node_get_first_child (node);

        if ((node != NULL) && (node->name != NULL) && (*node->name != '\0'))
          tag = node->name;
        else
          tag = "unknown-error";

        if (!wocky_strdiff ("bad-request", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_INVALID;
        else if (!wocky_strdiff ("not-allowed", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_DENIED;
        else if (!wocky_strdiff ("conflict", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_CONFLICT;
        else
          code = WOCKY_CONNECTOR_ERROR_BIND_REJECTED;

        abort_connect_code (self, code, "resource binding: %s", tag);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        node = wocky_xmpp_node_get_child (reply->node, "bind");
        if (node != NULL)
          node = wocky_xmpp_node_get_child (node, "jid");

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
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppNode *feat = (priv->features != NULL) ? priv->features->node : NULL;

  /* _if_ session setup is advertised, a session _must_ be established to *
   * allow presence/messaging etc to work. If not, it is not important    */
  if ((feat != NULL) &&
      wocky_xmpp_node_get_child_ns (feat, "session", WOCKY_XMPP_NS_SESSION))
    {
      WockyXmppConnection *conn = priv->conn;
      gchar *id = wocky_xmpp_connection_new_id (conn);
      WockyXmppStanza *session =
        wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
            WOCKY_STANZA_SUB_TYPE_SET,
            NULL, NULL,
            WOCKY_NODE_ATTRIBUTE, "id", id,
            WOCKY_NODE, "session", WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_SESSION,
            WOCKY_NODE_END,
            WOCKY_STANZA_END);
      wocky_xmpp_connection_send_stanza_async (conn, session, NULL,
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
      GSimpleAsyncResult *tmp = priv->result;
      priv->result = NULL;
      g_simple_async_result_complete (tmp);
      g_object_unref (tmp);
    }
}

static void
establish_session_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result, &error))
    {
      abort_connect_error (self, &error, "Failed to send session iq set");
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      establish_session_recv_cb, data);
}

static void
establish_session_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *reply = NULL;
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

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect_code (self, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
          "Session iq response invalid");
      goto out;
    }

  switch (sub)
    {
      WockyXmppNode *node = NULL;
      const char *tag = NULL;
      WockyConnectorError code;
      GSimpleAsyncResult *tmp;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        node = wocky_xmpp_node_get_child (reply->node, "error");
        if (node != NULL)
          node = wocky_xmpp_node_get_first_child (node);
        tag = ((node != NULL) && (node->name != NULL) && (*(node->name))) ?
          node->name : "unknown-error";

        if (!wocky_strdiff ("internal-server-error", tag))
          code = WOCKY_CONNECTOR_ERROR_SESSION_FAILED;
        else if (!wocky_strdiff ("forbidden", tag))
          code = WOCKY_CONNECTOR_ERROR_SESSION_DENIED;
        else if (!wocky_strdiff ("conflict" , tag))
          code = WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT;
        else
          code = WOCKY_CONNECTOR_ERROR_SESSION_REJECTED;

        abort_connect_code (self, code, "establish session: %s", tag);
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
            tmp = priv->result;
            g_simple_async_result_complete (tmp);
            g_object_unref (tmp);
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

/* *************************************************************************
 * exposed methods
 * ************************************************************************* */
WockyXmppConnection *
wocky_connector_connect_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error,
    gchar **jid,
    gchar **sid)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);
  GObject *obj = G_OBJECT (self);
  gboolean ok = FALSE;

  if (g_simple_async_result_propagate_error (result, error))
    return NULL;

  ok =
    g_simple_async_result_is_valid (res, obj, wocky_connector_connect_finish);
  g_return_val_if_fail (ok, NULL);

  if (jid != NULL)
    {
      if (*jid != NULL)
        g_warning ("overwriting non-NULL gchar * pointer arg (JID)");
      *jid = g_strdup (priv->identity);
    }

  if (sid != NULL)
    {
      if (*sid != NULL)
        g_warning ("overwriting non-NULL gchar * pointer arg (Session ID)");
      *sid = g_strdup (priv->session_id);
    }

  return priv->conn;
}

WockyXmppConnection *
wocky_connector_register_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error,
    gchar **jid,
    gchar **sid)
{
  return wocky_connector_connect_finish (self, res, error, jid, sid);
}


gboolean
wocky_connector_unregister_finish (WockyConnector *self,
    GAsyncResult *res,
    GError **error)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);
  GObject *obj = G_OBJECT (self);
  gboolean ok = FALSE;
  gpointer tag = wocky_connector_unregister_finish;

  g_simple_async_result_propagate_error (result, error);

  if (g_simple_async_result_is_valid (res, obj, tag))
    ok = g_simple_async_result_get_op_res_gboolean (result);

  return ok;
}

WockySaslAuthMechanism
wocky_connector_auth_mechanism (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  return priv->mech;
}

void
wocky_connector_connect_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

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
  gpointer rc = wocky_connector_connect_finish;

  if (priv->result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), cb, user_data,
          WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_IN_PROGRESS,
          "Connection already established or in progress");
      return;
    }

  /* *********************************************************************** *
   * setting up the async result with callback held in rc:                   *
   * wocky_connector_register_finish is a thin wrapper around                *
   * wocky_connector_connect_finish, so we don't actually want               *
   * to change rc for XEP77_SIGNUP, only for XEP77_CANCEL                    */
  if (priv->reg_op == XEP77_CANCEL)
    rc = wocky_connector_unregister_finish;

  priv->result = g_simple_async_result_new (G_OBJECT (self), cb, user_data, rc);
  /* *********************************************************************** */

  if (priv->reg_op == XEP77_CANCEL)
    g_simple_async_result_set_op_res_gboolean (priv->result, FALSE);

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

      DEBUG ("host: %s; port: %d\n", priv->xmpp_host, priv->xmpp_port);
      g_socket_client_connect_to_host_async (priv->client, srv, port, NULL,
          tcp_host_connected, self);
    }
  else
    {
      g_socket_client_connect_to_service_async (priv->client,
          host, "xmpp-client", NULL, tcp_srv_connected, self);
    }
  return;

 abort:
  g_free (host);
  g_free (node);
  g_free (uniq);
  return;
}

void
wocky_connector_unregister_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  priv->reg_op = XEP77_CANCEL;
  wocky_connector_connect_async (self, cb, user_data);
}

void
wocky_connector_register_async (WockyConnector *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  priv->reg_op = XEP77_SIGNUP;
  wocky_connector_connect_async (self, cb, user_data);
}

WockyConnector *
wocky_connector_new (const gchar *jid,
    const gchar *pass,
    const gchar *resource)
{
  return g_object_new (WOCKY_TYPE_CONNECTOR,
      "jid", jid,
      "password", pass,
      "resource", resource,
      NULL);
}

