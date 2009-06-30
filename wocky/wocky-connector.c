/*
 * wocky-connector.c - Source for WockyConnector
 * Copyright (C) 2006-2009 Collabora Ltd.
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
 * Returns a WockyXmppConnection objetc to the user on successful completion.
 */

/*
 * tcp_srv_connected
 * ├→ tcp_host_connected
 * │  ↓
 * └→ xmpp_init ←───────────┬──┐
 *    ↓                     │  │
 *    xmpp_init_sent_cb     │  │
 *    ↓                     │  │
 *    xmpp_init_recv_cb     │  │
 *    ↓                     │  │
 *    xmpp_features_cb      │  │
 *    │ │ ↓                 │  │
 *    │ │ starttls_sent_cb  │  │
 *    │ │ ↓                 │  │
 *    │ │ starttls_recv_cb ─┘  │
 *    │ ↓                      │
 *    │ request-auth           │
 *    │ ↓                      │
 *    │ auth_done ─────────────┘
 *    ↓
 *    iq_bind_resource
 *    ↓
 *    iq_bind_resource_sent_cb
 *    ↓
 *    iq_bind_resource_recv_cb → success
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
static void tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector);
static void tcp_host_connected (GObject *source,
    GAsyncResult *result,
    gpointer connector);
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
};

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
  gchar *jid;
  gchar *resource; /* the /[...] part of the jid, if any */
  gchar *user;     /* the [...]@ part of the initial JID */
  gchar *domain;   /* the @[...]/ part of the initial JID */
  /* volatile/derived property: identity = jid, but may be updated by server: */
  gchar *identity; /* if the server hands us a new JID (not handled yet) */

  /* XMPP connection data */
  WockyXmppStanza *features;

  /* misc internal state: */
  WockyConnectorState state;
  gboolean dispose_has_run;
  gboolean authed;
  gboolean encrypted;
  gboolean connected;
  GSimpleAsyncResult *result;

  /* socket/tls/etc structures */
  GSocketClient *client;
  GSocketConnection *sock;
  GTLSSession *tls_sess;
  GTLSConnection *tls;
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
    state = "TLS Negotiated";
  else if (priv->connected)
    state = "TCP Connection Established";
  else
    state = "Connecting... ";

  g_string_printf (msg, "%s: %s", state, str);
  return g_string_free (msg, FALSE);
}

static void
abort_connect (WockyConnector *connector,
    GError *error,
    int code,
    const char *fmt,
    ...)
{
  GError *err = NULL;
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);
  va_list args;

  va_start (args, fmt);
  if (error != NULL)
    {
      err = g_error_new (WOCKY_CONNECTOR_ERROR, code, "%s", error->message);
      if ((fmt != NULL) && *fmt)
        {
          GString *msg = g_string_new ("");
          g_string_vprintf (msg, fmt, args);
          g_prefix_error (&err, "%s: ", msg->str);
          g_string_free (msg, TRUE);
        }
    }
  else
    {
      err = g_error_new_valist (WOCKY_CONNECTOR_ERROR, code, fmt, args);
    }
  va_end (args);

  if (priv->sock != NULL)
    {
      g_object_unref (priv->sock);
      priv->sock = NULL;
    }
  priv->state = WCON_DISCONNECTED;

  g_simple_async_result_set_from_error (priv->result, err);
  g_simple_async_result_complete (priv->result);
  g_object_unref (priv->result);
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
      case PROP_PASS:
        g_free (priv->pass);
        priv->pass = g_value_dup_string (value);
        if (priv->pass == NULL)
          priv->pass = g_strdup ("");
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
        if (priv->xmpp_port == 0)
          priv->xmpp_port = 5222;
        break;
      case PROP_XMPP_HOST:
        g_free (priv->xmpp_host);
        priv->xmpp_host = g_value_dup_string (value);
        break;
      case PROP_IDENTITY:
        g_free (priv->identity);
        priv->identity = g_value_dup_string (value);
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
      "Whether auth info can be sent in the clear", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_AUTH_INSECURE_OK, spec);

  spec = g_param_spec_boolean ("tls-required", "TLS required",
      "Whether SSL/TLS is required", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_TLS_REQUIRED, spec);

  spec = g_param_spec_string ("jid", "jid", "The XMPP jid", NULL,
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_JID, spec);

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
      "XMPP port", 0, 65535, 5222,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_XMPP_PORT, spec);

  spec = g_param_spec_object ("features", "XMPP Features",
      "Last XMPP Feature Stanza advertised by server", WOCKY_TYPE_XMPP_STANZA,
      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_FEATURES, spec);
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
      const gchar *host = rindex (priv->jid, '@') + 1;
      DEBUG ("SRV connect failed: %s", error->message);
      DEBUG ("Falling back to direct connection");
      g_error_free (error);
      priv->state = WCON_TCP_CONNECTING;
      g_socket_client_connect_to_host_async (priv->client,
          host, priv->xmpp_port, NULL, tcp_host_connected, connector);
    }
  else
    {
      priv->connected = TRUE;
      priv->state = WCON_TCP_CONNECTED;
      xmpp_init (self, TRUE);
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
      abort_connect (connector, error, WOCKY_CONNECTOR_ERROR_DISCONNECTED,
          "connection failed");
      g_error_free (error);
    }
  else
    {
      priv->connected = TRUE;
      priv->state = WCON_TCP_CONNECTED;
      xmpp_init (self, TRUE);
    }
}

static void
xmpp_init (WockyConnector *connector, gboolean new_conn)
{
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (new_conn)
    priv->conn = wocky_xmpp_connection_new (G_IO_STREAM(priv->sock));
  wocky_xmpp_connection_send_open_async (priv->conn, priv->domain, NULL,
      "1.0", NULL, NULL, xmpp_init_sent_cb, connector);
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
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_DISCONNECTED,
          "Failed to send open stanza");
      g_error_free (error);
      return;
    }

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

  gchar *version;
  gchar *from;

  if (!wocky_xmpp_connection_recv_open_finish (priv->conn, result, NULL,
          &from, &version, NULL, &error))
    {
      char *msg = state_message (priv, error->message);
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_DISCONNECTED, msg);
      g_free (msg);
      g_error_free (error);
      goto out;
    }

  if (wocky_strdiff (version, "1.0"))
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER,
          "Server not XMPP 1.0 Compliant");
      goto out;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
      xmpp_features_cb, data);

 out:
  g_free (version);
  g_free (from);
}

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
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_DISCONNECTED,
          "disconnected before XMPP features stanza");
      g_error_free (error);
      return;
    }

  node = stanza->node;

  if (wocky_strdiff (node->name, "features") ||
      wocky_strdiff (wocky_xmpp_node_get_ns (node), WOCKY_XMPP_NS_STREAM))
    {
      char *msg = state_message (priv, "Malformed feature stanza");
      abort_connect (data, NULL, WOCKY_CONNECTOR_ERROR_BAD_FEATURES, msg);
      g_free (msg);
      goto out;
    }

  /* cache the current feature set: according to the RFC, we should forget
   * any previous feature set as soon as we open a new stream, so that
   * happens elsewhere */
  priv->features = g_object_ref (stanza);

  can_encrypt =
    wocky_xmpp_node_get_child_ns (node, "starttls", WOCKY_XMPP_NS_TLS) != NULL;
  can_bind =
    wocky_xmpp_node_get_child_ns (node, "bind", WOCKY_XMPP_NS_BIND) != NULL;

  /* conditions:
   * not encrypted, not encryptable, require encryption → ABORT
   * encryptable                                        → STARTTLS
   * encrypted || not encryptable                       → AUTH
   * not bound && can bind                              → BIND
   */

  if (!priv->encrypted && !can_encrypt && priv->tls_required)
    {
      abort_connect (data, NULL, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE,
          "TLS requested but lack server support");
      goto out;
    }

  if (!priv->encrypted && can_encrypt)
    {
      WockyXmppStanza *starttls = wocky_xmpp_stanza_new ("starttls");
      wocky_xmpp_node_set_ns (starttls->node, WOCKY_XMPP_NS_TLS);
      wocky_xmpp_connection_send_stanza_async (priv->conn, starttls,
          NULL, starttls_sent_cb, data);
      g_object_unref (starttls);
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
    abort_connect (data, NULL, WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE,
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
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result,
          &error))
    {
      abort_connect (data, error, WOCKY_CONNECTOR_ERROR_DISCONNECTED,
          "Failed to send STARTTLS stanza");
      g_error_free (error);
      return;
    }

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
      abort_connect (data, error, WOCKY_CONNECTOR_ERROR_DISCONNECTED,
          "STARTTLS reply not received");
      g_error_free (error);
      goto out;
    }

  node = stanza->node;

  if (wocky_strdiff (node->name, "proceed") ||
      wocky_strdiff (wocky_xmpp_node_get_ns (node), WOCKY_XMPP_NS_TLS))
    {
      if (priv->tls_required)
          abort_connect (data, NULL, WOCKY_CONNECTOR_ERROR_TLS_REFUSED,
              "STARTTLS refused by server");
      else
        request_auth (self, stanza);
      goto out;
    }
  else
    {
      priv->tls_sess = g_tls_session_new (G_IO_STREAM (priv->sock));
      priv->tls = g_tls_session_handshake (priv->tls_sess, NULL, &error);

      if (priv->tls == NULL)
        {
          abort_connect (data, error, WOCKY_CONNECTOR_ERROR_TLS_FAILED,
              "TLS Handshake Error");
          g_error_free (error);
          goto out;
        }

      priv->encrypted = TRUE;
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
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_AUTH_FAILED, "");
      g_error_free (error);
      return;
    }

  priv->state = WCON_XMPP_AUTHED;
  priv->authed = TRUE;
  wocky_xmpp_connection_reset (priv->conn);
  xmpp_init (self, FALSE);
}

/* ************************************************************************* */
/* BIND calls */
static void
iq_bind_resource (WockyConnector *self)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *bind =
    (priv->resource != NULL && *(priv->resource)) ?
    wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
        NULL, NULL,
        WOCKY_NODE_ATTRIBUTE, "id", wocky_xmpp_connection_new_id (priv->conn),
        WOCKY_NODE, "bind", WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_BIND,
        WOCKY_NODE, "resource", WOCKY_NODE_TEXT,  priv->resource,
        WOCKY_NODE_END,
        WOCKY_NODE_END,
        WOCKY_STANZA_END) :
    wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
        NULL, NULL,
        WOCKY_NODE_ATTRIBUTE, "id", wocky_xmpp_connection_new_id (priv->conn),
        WOCKY_NODE, "bind", WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_BIND,
        WOCKY_NODE_END,
        WOCKY_STANZA_END);
  wocky_xmpp_connection_send_stanza_async (priv->conn, bind, NULL,
      iq_bind_resource_sent_cb, self);
  g_object_unref (bind);
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
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
          "Failed to send bind iq set");
      g_error_free (error);
      return;
    }

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

  if (reply == NULL)
    {
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
          "Failed to receive bind iq result");
      g_error_free (error);
      return;
    }

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
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
        tag = ((node != NULL) && (node->name != NULL) && (*(node->name))) ?
          node->name : "unknown-error";

        if (!wocky_strdiff ("bad-request", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_INVALID;
        else if (!wocky_strdiff ("not-allowed", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_DENIED;
        else if (!wocky_strdiff ("conflict", tag))
          code = WOCKY_CONNECTOR_ERROR_BIND_CONFLICT;
        else
          code = WOCKY_CONNECTOR_ERROR_BIND_REJECTED;

        abort_connect (self, NULL, code, tag);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        node = wocky_xmpp_node_get_child (reply->node, "bind");
        if (node != NULL)
          node = wocky_xmpp_node_get_child (node, "jid");

        /* store the returned id (or the original if none came back)*/
        g_free (priv->identity);
        if ((node != NULL) && (node->content != NULL) && *(node->content))
          priv->identity = node->content;
        else
          priv->identity = priv->jid;

        priv->state = WCON_XMPP_BOUND;
        establish_session (self);
        break;

      default:
        abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_BIND_FAILED,
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
      WockyXmppStanza *session =
        wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
            WOCKY_STANZA_SUB_TYPE_SET,
            NULL, NULL,
            WOCKY_NODE_ATTRIBUTE, "id", wocky_xmpp_connection_new_id (conn),
            WOCKY_NODE, "session", WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_SESSION,
            WOCKY_NODE_END,
            WOCKY_STANZA_END);
      wocky_xmpp_connection_send_stanza_async (conn, session, NULL,
          establish_session_sent_cb, self);
      g_object_unref (session);
    }
  else
    g_simple_async_result_complete (priv->result);
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
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
          "Failed to send session iq set");
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
      abort_connect (self, error, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
          "Failed to receive session iq result");
      g_error_free (error);
      return;
    }

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
          "Session iq response invalid");
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

        abort_connect (self, NULL, code, tag);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        g_simple_async_result_complete (priv->result);
        break;

      default:
        abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_SESSION_FAILED,
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
    gchar **jid)
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
        g_warning ("overwriting non-NULL gchar * pointer arg");
      *jid = g_strdup (priv->identity);
    }

  return priv->conn;
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
  const gchar *host = priv->jid ? rindex (priv->jid, '@') : NULL;

  if (priv->result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), cb, user_data,
          WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_IN_PROGRESS,
          "Connection already established or in progress");
      return;
    }

  priv->result = g_simple_async_result_new (G_OBJECT (self),
      cb,
      user_data,
      wocky_connector_connect_finish);

  if (host == NULL)
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_BAD_JID, "Invalid JID");
      return;
    }

  if (*(++host) == '\0')
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERROR_BAD_JID,
          "Missing Domain");
      return;
    }

  priv->user   = g_strndup (priv->jid, (host - priv->jid - 1));
  priv->domain = g_strdup (host);
  priv->client = g_socket_client_new ();
  priv->state  = WCON_TCP_CONNECTING;

  /* if the user specified a specific server to connect to, try to use that:
     if not, try to find a SRV record for the 'host' extracted from the JID
     above */
  if (priv->xmpp_host)
    {
      DEBUG ("host: %s; port: %d\n", priv->xmpp_host, priv->xmpp_port);
      g_socket_client_connect_to_host_async (priv->client,
          priv->xmpp_host, priv->xmpp_port, NULL,
          tcp_host_connected, self);
    }
  else
    {
      g_socket_client_connect_to_service_async (priv->client,
          host, "xmpp-client", NULL, tcp_srv_connected, self);
    }
  return;
}

WockyConnector *
wocky_connector_new (const gchar *jid,
    const gchar *pass)
{
  return
    g_object_new (WOCKY_TYPE_CONNECTOR, "jid", jid, "password", pass, NULL);
}

