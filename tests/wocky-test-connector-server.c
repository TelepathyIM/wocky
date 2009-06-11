/*
 * wocky-test-connector-server.c - Source for TestConnectorServer
 * Copyright (C) 2006 Collabora Ltd.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gio/gnio.h>

#include "wocky-test-connector-server.h"

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-xmpp-connection.h>

#include <wocky/wocky-namespaces.h>

#include <sasl/sasl.h>

G_DEFINE_TYPE (TestConnectorServer, test_connector_server, G_TYPE_OBJECT);

typedef void (*stanza_func)(TestConnectorServer *self, WockyXmppStanza *xml);
typedef struct _stanza_handler stanza_handler;
struct _stanza_handler {
  const gchar *ns;
  const gchar *name;
  stanza_func func;
};

static void xmpp_init (GObject *source, GAsyncResult *result,
    gpointer user_data);

/* ************************************************************************* */
/* test connector server object definition */
typedef enum {
  SERVER_STATE_START,
  SERVER_STATE_CLIENT_OPENED,
  SERVER_STATE_SERVER_OPENED,
  SERVER_STATE_FEATURES_SENT
} server_state;

typedef struct _TestConnectorServerPrivate TestConnectorServerPrivate;

struct _TestConnectorServerPrivate
{
  gboolean dispose_has_run;
  WockyXmppConnection *conn;
  GIOStream *stream;
  server_state state;
  gboolean tls_started;
  gboolean authed;

  TestSaslAuthServer *sasl;
  gchar *mech;
  gchar *user;
  gchar *pass;

  GTLSSession *tls_sess;
  GTLSConnection *tls_conn;

  struct { ServerProblem sasl; ConnectorProblem connector; } problem;
};

#define TEST_CONNECTOR_SERVER_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_CONNECTOR_SERVER, \
   TestConnectorServerPrivate))

static void
test_connector_server_dispose (GObject *object)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (object);
  TestConnectorServerPrivate *priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  g_object_unref (priv->conn);
  priv->conn = NULL;

  g_object_unref (priv->stream);
  priv->stream = NULL;

  g_object_unref (priv->sasl);
  priv->sasl = NULL;

  if (G_OBJECT_CLASS (test_connector_server_parent_class)->dispose)
    G_OBJECT_CLASS (test_connector_server_parent_class)->dispose (object);
}

static void
test_connector_server_finalise (GObject *object)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (object);
  TestConnectorServerPrivate *priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);
  /* free any data held directly by the object here */
  g_free (priv->mech);
  g_free (priv->user);
  g_free (priv->pass);

  G_OBJECT_CLASS (test_connector_server_parent_class)->finalize (object);
}

static void
test_connector_server_init (TestConnectorServer *obj)
{
  TestConnectorServerPrivate *priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (obj);
  priv->tls_started = FALSE;
  priv->authed      = FALSE;
}

static void
test_connector_server_class_init (TestConnectorServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TestConnectorServerPrivate));

  object_class->dispose  = test_connector_server_dispose;
  object_class->finalize = test_connector_server_finalise;
}

/* ************************************************************************* */
/* xmpp stanza handling: */
static void handle_auth     (TestConnectorServer *self, WockyXmppStanza *xml);
static void handle_starttls (TestConnectorServer *self, WockyXmppStanza *xml);

#define HANDLER(ns,x) { WOCKY_XMPP_NS_##ns, #x, handle_##x }
static stanza_handler handlers[] =
  {
    HANDLER (SASL_AUTH, auth),
    HANDLER (TLS, starttls),
    { NULL, NULL, NULL }
  };

static void
handle_auth (TestConnectorServer *self, WockyXmppStanza *xml)
{
  TestConnectorServerPrivate *priv = TEST_CONNECTOR_SERVER_GET_PRIVATE(self);
  TestSaslAuthServer *sasl = priv->sasl;

  /* after this the sasl auth server object is in charge: control of
     the stream does not return to us */
  test_sasl_auth_server_take_over (G_OBJECT (sasl), priv->conn, xml);
}

static void
handle_starttls (TestConnectorServer *self, WockyXmppStanza *xml)
{
  TestConnectorServerPrivate *priv = TEST_CONNECTOR_SERVER_GET_PRIVATE(self);
  if (!priv->tls_started)
    {
      GError *error = NULL;
      WockyXmppConnection *conn = priv->conn;
      WockyXmppStanza *proceed = wocky_xmpp_stanza_new ("proceed");
      wocky_xmpp_node_set_ns (xml->node, WOCKY_XMPP_NS_TLS);

      /* set up the tls server session */
      priv->tls_sess = g_tls_session_server_new (priv->stream, 1024,
          "/home/vivek/src/key.pem", "/home/vivek/src/cert.pem", NULL, NULL);
      priv->tls_conn = g_tls_session_handshake (priv->tls_sess, NULL, &error);

      if (priv->tls_conn == NULL)
        {
          g_error ("TLS Server Setup failed: %s\n", error->message);
          return;
        }

      priv->conn = wocky_xmpp_connection_new (G_IO_STREAM (priv->tls_conn));
      priv->tls_started = TRUE;

      /* send the response (on the old, non-TLS connection saved in 'conn')
         once we have done that, the client should re-open the stream so
         we should loop back into the xmpp_init handler */
      priv->state = SERVER_STATE_START;
      wocky_xmpp_connection_send_stanza_async (conn, proceed, NULL, xmpp_init,
          self);
      g_object_unref (proceed);
    }
}

static void
xmpp_handler (GObject *source, GAsyncResult *result, gpointer user_data)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;
  WockyXmppStanza *xml = NULL;
  WockyXmppConnection *conn = NULL;
  const gchar *ns = NULL;
  const gchar *name = NULL;
  gboolean handled = FALSE;
  GError *error = NULL;
  int i;

  self = TEST_CONNECTOR_SERVER (user_data);
  priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);
  conn = WOCKY_XMPP_CONNECTION (source);

  xml  = wocky_xmpp_connection_recv_stanza_finish (conn, result, &error);
  ns   = wocky_xmpp_node_get_ns (xml->node);
  name = xml->node->name;

  /* if we find a handler, the handler is responsible for listening for the
     next stanza and setting up the next callback in the chain: */
  for (i = 0; handlers[i].ns != NULL; i++)
    {
      if (!strcmp (ns, handlers[i].ns) && !strcmp (name, handlers[i].name))
        {
          (handlers[i].func) (self, xml);
          handled = TRUE;
          break;
        }
    }

  /* no handler found: just complain and sit waiting for the next stanza */
  if (!handled)
    g_warning ("<%s xmlns=\"%s\"… not handled\n", name, ns);
  wocky_xmpp_connection_recv_stanza_async (conn, NULL, xmpp_handler, self);

  g_object_unref (xml);
}

/* ************************************************************************* */
/* initial XMPP stream setup, up to sending features stanza */
static WockyXmppStanza *
feature_stanza (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);
  WockyXmppStanza *features = wocky_xmpp_stanza_new ("features");
  WockyXmppNode *node = features->node;
  ConnectorProblem problem = priv->problem.connector;
  wocky_xmpp_node_set_ns (node, WOCKY_XMPP_NS_STREAM);

  if (priv->problem.sasl != SERVER_PROBLEM_NO_SASL)
    {
      priv->sasl = test_sasl_auth_server_new (NULL, priv->mech,
          priv->user, priv->pass, priv->problem.sasl, FALSE);
      test_sasl_auth_server_set_mechs (G_OBJECT (priv->sasl), features);
    }

  if ((problem != CONNECTOR_PROBLEM_NO_TLS) && !priv->tls_started)
    wocky_xmpp_node_add_child_ns (node, "starttls", WOCKY_XMPP_NS_TLS);

  return features;
}

static void
xmpp_init (GObject *source, GAsyncResult *result, gpointer user_data)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;
  WockyXmppStanza *xml;
  WockyXmppConnection *conn;

  self = TEST_CONNECTOR_SERVER (user_data);
  priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);
  conn = (source == NULL) ? priv->conn : WOCKY_XMPP_CONNECTION (source);

  switch (priv->state)
    {
      /* wait for <stream:stream… from the client */
    case SERVER_STATE_START:
      priv->state = SERVER_STATE_CLIENT_OPENED;
      wocky_xmpp_connection_recv_open_async (conn, NULL, xmpp_init, self);
      break;

      /* send our own <stream:stream… */
    case SERVER_STATE_CLIENT_OPENED:
      priv->state = SERVER_STATE_SERVER_OPENED;
      wocky_xmpp_connection_recv_open_finish (conn, result,
          NULL, NULL, NULL, NULL, NULL);
      wocky_xmpp_connection_send_open_async (conn, NULL, "testserver", "1.0",
          NULL, NULL, xmpp_init, self);
      break;

      /* send our feature set */
    case SERVER_STATE_SERVER_OPENED:
      priv->state = SERVER_STATE_FEATURES_SENT;
      wocky_xmpp_connection_send_open_finish (conn, result, NULL);
      xml = feature_stanza (self);
      wocky_xmpp_connection_send_stanza_async (conn, xml,
          NULL, xmpp_init, self);
      g_object_unref (xml);
      break;

      /* ok, we're done with initial stream setup */
    case SERVER_STATE_FEATURES_SENT:
      wocky_xmpp_connection_send_stanza_finish (conn, result, NULL);
      wocky_xmpp_connection_recv_stanza_async (conn, NULL, xmpp_handler, self);
    }
}

/* ************************************************************************* */
/* exposed methods */

TestConnectorServer *
test_connector_server_new (GIOStream *stream,
    gchar *mech,
    const gchar *user,
    const gchar *pass,
    ConnectorProblem problem,
    ServerProblem sasl_problem)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;

  self = g_object_new (TEST_TYPE_CONNECTOR_SERVER, NULL);
  priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);

  priv->stream = g_object_ref (stream);
  priv->mech   = g_strdup (mech);
  priv->user   = g_strdup (user);
  priv->pass   = g_strdup (pass);
  priv->problem.sasl      = sasl_problem;
  priv->problem.connector = problem;
  priv->conn   = wocky_xmpp_connection_new (stream);

  return self;
}

void
test_connector_server_start (GObject *object)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;

  self = TEST_CONNECTOR_SERVER (object);
  priv = TEST_CONNECTOR_SERVER_GET_PRIVATE (self);
  priv->state = SERVER_STATE_START;
  xmpp_init (NULL,NULL,self);
}
