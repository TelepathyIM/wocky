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
 * Invokes a callback with a WockyConnector (or an error) when it finishes.
 */

/*
 * tcp_srv_connected
 * ├→ tcp_host_connected
 * │  ↓
 * └→ xmpp_init
 *    ↓
 *    xmpp_init_sent_cb ←────┬──┐
 *    ↓                      │  │
 *    xmpp_init_recv_cb      │  │
 *    │ ↓                    │  │
 *    │ xmpp_features_cb     │  │
 *    │ │ ↓                  │  │
 *    │ │ starttls_sent_cb   │  │
 *    │ │ ↓                  │  │
 *    │ │ starttls_recv_cb ──┘  │
 *    │ ↓                       │
 *    │ request-auth            │
 *    │ ↓                       │
 *    │ auth_done ──────────────┘
 *    ↓
 *    authed and possibly starttlsed, make the callback call
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


G_DEFINE_TYPE( WockyConnector, wocky_connector, G_TYPE_OBJECT );

static void wocky_connector_class_init (WockyConnectorClass *klass);
static void tcp_srv_connected (GObject *source, GAsyncResult *result,
    gpointer connector);
static void tcp_host_connected (GObject *source, GAsyncResult *result,
    gpointer connector);
static void xmpp_init (GObject *connector);
static void xmpp_init_sent_cb (GObject *source, GAsyncResult *result,
    gpointer data);
static void xmpp_init_recv_cb (GObject *source, GAsyncResult *result,
    gpointer data);
static void xmpp_features_cb (GObject *source, GAsyncResult *result,
    gpointer data);
static void starttls_sent_cb (GObject *source, GAsyncResult *result,
    gpointer data);
static void starttls_recv_cb (GObject *source, GAsyncResult *result,
    gpointer data);
static void request_auth (GObject *object, WockyXmppStanza *stanza);
static void auth_done (GObject *source, GAsyncResult *result,  gpointer data);

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
  PROP_CONNECTION
};

typedef enum
{
  WCON_DISCONNECTED,
  WCON_TCP_CONNECTING,
  WCON_TCP_CONNECTED,
  WCON_XMPP_INITIALISED,
  WCON_XMPP_TLS_STARTED
} connector_state;


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
  gchar     *jid;
  gchar     *resource;
  gboolean   tls_required;
  guint      xmpp_port;
  gchar     *xmpp_host;
  gchar     *pass;
  /* volatile/derived property: jid + resource, may be updated by server: */
  gchar     *user;     /* the pre @ part of the initial JID */
  gchar     *identity; /* if the server hands us a new JID (not handled yet) */
  gchar     *domain;   /* the post @ part of the initial JID */

  /* misc internal state: */
  GError *error /* no this is not an uninitialisd GError. really */;
  connector_state state;
  gboolean defunct;
  gboolean authed;
  gboolean encrypted;
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

/* during XMPP setup, we have to loop through very similar states
   (handling the same stanza types) about 3 times: the handling is
   almost identical, with the few differences depending on which
   of these states we are in: AUTHENTICATED, ENCRYPTED or INITIAL
   This macro wraps up the logic of inspecting our internal state
   and deciding which condition applies: */
#define WOCKY_CONNECTOR_CHOOSE_BY_STATE(p, authenticated, _encrypted, initial) \
  (p->authed) ? authenticated : (p->encrypted) ? _encrypted : initial

/* if (p->authed) v = a; else if (p->encrypted) v = b; else v = c; */

static gboolean
copy_error (WockyConnector *connector, GError *error)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);
  g_error_free (priv->error);
  priv->error = NULL;
  return g_simple_async_result_propagate_error (priv->result, &priv->error);
}

static void
abort_connect (WockyConnector *connector,
    GError *error,
    int code,
    const char *fmt,
    ...)
{
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (connector);

  va_list args;
  /* if there was no error to copy, generate one if need be */
  if ((error == NULL) || !copy_error (connector,error))
    {
      if (code != 0)
        {
          g_clear_error (&priv->error);
          va_start (args, fmt);
          priv->error =
            g_error_new_valist (WOCKY_CONNECTOR_ERROR, code, fmt, args);
          va_end (args);
        }
      else if (priv->error == NULL)
        {
          va_start (args, fmt);
          priv->error =
            g_error_new_literal (WOCKY_CONNECTOR_ERROR, -1, "aborted");
          va_end (args);
        }
    }

  g_simple_async_result_set_from_error (priv->result, priv->error);
  g_simple_async_result_complete_in_idle ( priv->result );
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
  /* WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (obj); */
  return;
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
      break;
    case PROP_RESOURCE:
      g_free (priv->resource);
      priv->resource = g_value_dup_string (value);
      break;
    case PROP_XMPP_PORT:
      priv->xmpp_port = g_value_get_uint (value);
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
    case PROP_CONNECTION:
      g_value_set_object (value, priv->conn);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

#define PATTR      ( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS )
#define INIT_PATTR ( PATTR | G_PARAM_CONSTRUCT )

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

  spec = g_param_spec_boolean ("insecure-tls-ok", "insecure-tls-ok" ,
      "Whether recoverable TLS errors should be ignored", TRUE, INIT_PATTR);
  g_object_class_install_property (oclass, PROP_TLS_INSECURE_OK, spec);

  spec = g_param_spec_boolean ("insecure-auth-ok", "insecure-auth-ok" ,
      "Whether auth info can be sent in the clear", FALSE, INIT_PATTR);
  g_object_class_install_property (oclass, PROP_AUTH_INSECURE_OK, spec);

  spec = g_param_spec_boolean ("encrypted-plain-auth-ok", "enc-plain-auth-ok",
      "Whether auth info can be sent in the clear", TRUE, INIT_PATTR);
  g_object_class_install_property (oclass, PROP_AUTH_INSECURE_OK, spec);

  spec = g_param_spec_boolean ("tls-required", "tls" ,
      "Whether SSL/TLS is required" , TRUE, INIT_PATTR);
  g_object_class_install_property (oclass, PROP_TLS_REQUIRED, spec);

  spec = g_param_spec_string ("jid", "jid", "The XMPP jid", NULL, PATTR);
  g_object_class_install_property (oclass, PROP_JID, spec);

  spec = g_param_spec_string ("password", "pass", "Password", NULL, PATTR);
  g_object_class_install_property (oclass, PROP_PASS, spec);

  spec = g_param_spec_string ("resource", "resource",
      "XMPP resource to append to the jid", g_strdup("wocky"), INIT_PATTR);
  g_object_class_install_property (oclass, PROP_RESOURCE, spec);

  spec = g_param_spec_string ("identity", "identity",
      "jid + resource (set by XMPP server)", NULL, PATTR);
  g_object_class_install_property (oclass, PROP_IDENTITY, spec);

  spec = g_param_spec_string ("xmpp-server", "server",
      "XMPP connect server", NULL, PATTR);
  g_object_class_install_property (oclass, PROP_XMPP_HOST, spec);

  spec = g_param_spec_uint ("xmpp-port", "port",
      "XMPP port", 0, 65535, 5222, INIT_PATTR);
  g_object_class_install_property (oclass, PROP_XMPP_PORT, spec);

  spec = g_param_spec_pointer ("connection", "connection",
      "WockyXmppConnection object",
      (G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));
  g_object_class_install_property (oclass, PROP_CONNECTION, spec);
}

#define UNREF_AND_FORGET(x) if (x != NULL) { g_object_unref (x); x = NULL; }
#define GFREE_AND_FORGET(x) g_free (x); x = NULL;

static void
wocky_connector_dispose (GObject *object)
{
  WockyConnector *self = WOCKY_CONNECTOR (object);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (priv->defunct)
    return;

  priv->defunct = TRUE;
  UNREF_AND_FORGET (priv->conn);
  UNREF_AND_FORGET (priv->client);
  UNREF_AND_FORGET (priv->sock);
  UNREF_AND_FORGET (priv->tls_sess);
  UNREF_AND_FORGET (priv->tls);

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

  if (priv->sock == NULL)
    {
      const gchar *host = rindex (priv->jid, '@') + 1;
      DEBUG ("SRV connect failed: %s\n", error->message);
      DEBUG ("Falling back to direct connection\n");
      g_error_free (error);
      priv->state = WCON_TCP_CONNECTING;
      g_socket_client_connect_to_host_async (priv->client,
          host, priv->xmpp_port, NULL, tcp_host_connected, connector);
    }
  else
    {
      priv->state = WCON_TCP_CONNECTED;
      xmpp_init (connector);
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

  priv->sock =
    g_socket_client_connect_to_host_finish (G_SOCKET_CLIENT (source),
        result, &error);

  if (priv->sock == NULL)
    {
      DEBUG ("HOST connect failed: %s\n", error->message);
      abort_connect (connector, error, WOCKY_CONNECTOR_ERR_DISCONNECTED,
          "connection failed");
      priv->state = WCON_DISCONNECTED;
      return;
    }
  else
    {
      priv->state = WCON_TCP_CONNECTED;
      xmpp_init (connector);
    }
}

static void
xmpp_init (GObject *connector)
{
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  priv->conn = wocky_xmpp_connection_new (G_IO_STREAM(priv->sock));
  wocky_xmpp_connection_send_open_async (priv->conn, priv->domain, NULL,
      "1.0", NULL, NULL, xmpp_init_sent_cb, connector);
}

static void
xmpp_init_sent_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (!wocky_xmpp_connection_send_open_finish (priv->conn, result,
          &priv->error))
    {
      const char *msg = WOCKY_CONNECTOR_CHOOSE_BY_STATE (priv,
          "Failed to send post-auth open",
          "Failed to send post-TLS open",
          "Failed to send 1st XMPP open");
      priv->state = WCON_DISCONNECTED;
      abort_connect (self, NULL,
          WOCKY_CONNECTOR_ERR_DISCONNECTED, msg);
      return;
    }

  /* we are just after a successful auth: trigger the callback */
  if (priv->authed)
    {
      g_simple_async_result_complete (priv->result);
      return;
    }

  wocky_xmpp_connection_recv_open_async (priv->conn, NULL,
      xmpp_init_recv_cb, data);
}

static void
xmpp_init_recv_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  gchar *version;
  gchar *from;

  if (!wocky_xmpp_connection_recv_open_finish (priv->conn, result, NULL,
          &from, &version, NULL, &priv->error))
    {
      const char *msg =
        WOCKY_CONNECTOR_CHOOSE_BY_STATE (priv,
            "post-auth open response not received",
            "post-TLS open response not received",
            "XMPP open response not received");
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_DISCONNECTED, msg);
      return;
    }

  if (wocky_strdiff (version, "1.0"))
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_MALFORMED_XMPP,
          "Server not XMPP Compliant");
      return;
    }

  if (!priv->authed)
    wocky_xmpp_connection_recv_stanza_async (priv->conn, NULL,
        xmpp_features_cb, data);

  g_free (version);
  g_free (from);
}

static void
xmpp_features_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  WockyXmppNode   *tls;
  WockyXmppNode   *node;
  WockyXmppStanza *starttls;
  gboolean         can_encrypt = FALSE;

  stanza =
    wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &priv->error);

  if (stanza == NULL)
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_DISCONNECTED,
          "disconnected before XMPP features stanza");
      return;
    }

  node = stanza->node;

  if (wocky_strdiff (node->name, "features") ||
      wocky_strdiff (wocky_xmpp_node_get_ns (node), WOCKY_XMPP_NS_STREAM))
    {
      const char *msg =
        WOCKY_CONNECTOR_CHOOSE_BY_STATE (priv,
            "Malformed post-auth feature stanza",
            "Malformed post-TLS feature stanza",
            "Malformed XMPP feature stanza");
      abort_connect (data, NULL, WOCKY_CONNECTOR_ERR_MALFORMED_XMPP, msg);
      return;
    }

  tls =
    wocky_xmpp_node_get_child_ns (node, "starttls", WOCKY_XMPP_NS_TLS);
  can_encrypt = (tls != NULL);

  /* conditions:
   * not encrypted, not encryptable, require encryption → ABORT
   * encryptable                                        → STARTTLS
   * encrypted || not encryptable                       → AUTH
   */

  if (!priv->encrypted && !can_encrypt && priv->tls_required)
    {
      abort_connect (data, NULL, WOCKY_CONNECTOR_ERR_NOT_SUPPORTED,
          "TLS requested but lack server support");
      g_object_unref (stanza);
      return;
    }

  if (!priv->encrypted && can_encrypt)
    {
      starttls = wocky_xmpp_stanza_new ("starttls");
      wocky_xmpp_node_set_ns (starttls->node, WOCKY_XMPP_NS_TLS);
      wocky_xmpp_connection_send_stanza_async (priv->conn, starttls,
          NULL, starttls_sent_cb, data);
      g_object_unref (starttls);
      g_object_unref (stanza );
      return;
    }

  request_auth (G_OBJECT (self), stanza);
  g_object_unref (stanza);
  return;
}

static void
starttls_sent_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  if (!wocky_xmpp_connection_send_stanza_finish (priv->conn, result,
          &priv->error))
    {
      abort_connect (data, NULL, WOCKY_CONNECTOR_ERR_DISCONNECTED,
          "Failed to send STARTTLS stanza");
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->conn,
      NULL, starttls_recv_cb, data);
}

static void
starttls_recv_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  WockyXmppStanza *stanza;
  GError *error = NULL;
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppNode *node;

  stanza =
    wocky_xmpp_connection_recv_stanza_finish (priv->conn, result, &priv->error);

  if (stanza == NULL)
    {
      abort_connect (data, NULL, WOCKY_CONNECTOR_ERR_DISCONNECTED,
          "STARTTLS reply not received");
      return;
    }

  node = stanza->node;

  if (wocky_strdiff (node->name, "proceed") ||
      wocky_strdiff (wocky_xmpp_node_get_ns (node), WOCKY_XMPP_NS_TLS))
    {
      if (priv->tls_required)
        {
          abort_connect (data, NULL, WOCKY_CONNECTOR_ERR_REFUSED,
              "STARTTLS refused by server");
          return;
        }
      request_auth (G_OBJECT (self), stanza);
      return;
    }
  else
    {
      priv->tls_sess = g_tls_session_new (G_IO_STREAM (priv->sock));
      priv->tls = g_tls_session_handshake (priv->tls_sess, NULL, &error);

      if (priv->tls == NULL)
        {
          abort_connect (data, error, WOCKY_CONNECTOR_ERR_REFUSED,
              "TLS Handshake Error");
          g_error_free (error);
          return;
        }

      priv->encrypted = TRUE;
      priv->conn = wocky_xmpp_connection_new (G_IO_STREAM (priv->tls));
      wocky_xmpp_connection_send_open_async (priv->conn, priv->domain,
          NULL, "1.0", NULL, NULL, xmpp_init_sent_cb, data);
    }
}

static void
request_auth (GObject *object, WockyXmppStanza *stanza)
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
auth_done (GObject *source, GAsyncResult *result,  gpointer data)
{
  WockyConnector *self = WOCKY_CONNECTOR (data);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (source);

  if (!wocky_sasl_auth_authenticate_finish (sasl, result, &priv->error))
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_AUTH_FAILED,
          "Auth Failure");
      return;
    }

  priv->authed = TRUE;
  wocky_xmpp_connection_reset (priv->conn);
  wocky_xmpp_connection_send_open_async (priv->conn, priv->domain, NULL,
      "1.0", NULL, NULL, xmpp_init_sent_cb, self);
}


/* *************************************************************************
 * exposed methods
 * ************************************************************************* */
WockyXmppConnection *
wocky_connector_connect_finish (GObject *connector,
    GAsyncResult *res,
    GError **error)
{
  WockyConnector        *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);
  WockyXmppConnection   *rval = NULL;

  if (priv->authed)
    {
      rval = priv->conn;
    }
  else
    {
      if (error && (*error == NULL))
        *error = g_error_copy (priv->error);
    }

  return rval;
}

gboolean
wocky_connector_connect_async (GObject *connector,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  WockyConnector *self = WOCKY_CONNECTOR (connector);
  WockyConnectorPrivate *priv = WOCKY_CONNECTOR_GET_PRIVATE (self);

  /* 'host' is (by default) the part of the jid after the @
   *  it must be non-empty (although this test may need to be changed
   *  for serverless XMPP (eg 'Bonjour')): if the xmpp_host property
   *  is set, it takes precedence for the purposes of finding a
   *  an XMPP server: Otherwise we look for a SRV record for 'host',
   *  falling back to a direct connection to 'host' if that fails.
   */
  const gchar *host = priv->jid ? rindex (priv->jid, '@') : NULL;

  priv->result = g_simple_async_result_new (connector,
      cb ,
      user_data ,
      wocky_connector_connect_finish);

  if (priv->state != WCON_DISCONNECTED)
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_IS_CONNECTED,
          "Invalid state: Cannot connect");
      return FALSE;
    }
  if ( host == NULL )
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_BAD_JID, "Missing JID");
      return FALSE;
    }
  if (*(++host) == '\0')
    {
      abort_connect (self, NULL, WOCKY_CONNECTOR_ERR_BAD_JID, "Missing Domain");
      return FALSE;
    }

  priv->user   = g_strndup (priv->jid, (host - priv->jid - 1));
  priv->domain = g_strdup (host);
  priv->client = g_socket_client_new ();
  priv->state  = WCON_TCP_CONNECTING;

  if (priv->xmpp_host)
    {
      g_socket_client_connect_to_host_async (priv->client,
          priv->xmpp_host, priv->xmpp_port, NULL,
          tcp_host_connected, connector);
    }
  else
    {
      g_socket_client_connect_to_service_async (priv->client,
          host, "xmpp-client", NULL, tcp_srv_connected, connector);
    }
  return TRUE;
}

WockyConnector *
wocky_connector_new (const gchar *jid, const gchar *pass)
{
  return g_object_new (WOCKY_TYPE_CONNECTOR, "jid", jid, "password", pass);
}

#define IFNULL(val,def) (((val) == NULL) ? (def) : (val))
#define IFZERO(val,def) (((val) == 0) ? (def) : (val))

WockyConnector *
wocky_connector_new_full (const gchar *jid, const gchar *pass,
    const gchar *resource,
    const gchar *host,
    guint port,
    gboolean tls_required,
    gboolean insecure_tls_ok,
    gboolean insecure_auth_ok,
    gboolean encrypted_plain_auth_ok)
{
  return g_object_new (WOCKY_TYPE_CONNECTOR,
      "jid", jid,
      "password", IFNULL(pass,""),
      "resource", IFNULL(resource,"wocky"),
      "xmpp-server", host,
      "xmpp-port", IFZERO(port, 5222),
      "insecure-tls-ok", insecure_tls_ok,
      "insecure-auth-ok", insecure_auth_ok,
      "encrypted-plain-auth-ok", encrypted_plain_auth_ok,
      "tls-required", tls_required);
}
