/*
 * wocky-test-sasl-auth-server.c - Source for TestSaslAuthServer
 * Copyright (C) 2006 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd@luon.net>
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

#include "wocky-test-sasl-auth-server.h"

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-xmpp-connection.h>

#include <wocky/wocky-namespaces.h>

#include <sasl/sasl.h>

#define CHECK_SASL_RETURN(x)                                \
G_STMT_START   {                                            \
    if (x < SASL_OK) {                                      \
      fprintf (stderr, "sasl error (%d): %s\n",             \
           ret, sasl_errdetail (priv->sasl_conn));          \
      g_assert_not_reached ();                              \
    }                                                       \
} G_STMT_END

G_DEFINE_TYPE(TestSaslAuthServer, test_sasl_auth_server, G_TYPE_OBJECT)

#if 0
/* signal enum */
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
#endif

typedef enum {
  AUTH_STATE_STARTED,
  AUTH_STATE_CHALLENGE,
  AUTH_STATE_FINAL_CHALLENGE,
  AUTH_STATE_AUTHENTICATED,
} AuthState;

/* private structure */
typedef struct _TestSaslAuthServerPrivate TestSaslAuthServerPrivate;

struct _TestSaslAuthServerPrivate
{
  gboolean dispose_has_run;
  WockyXmppConnection *conn;
  GIOStream *stream;
  sasl_conn_t *sasl_conn;
  gchar *username;
  gchar *password;
  gchar *mech;
  AuthState state;
  ServerProblem problem;
  GCancellable *recv_cancel;
};

#define TEST_SASL_AUTH_SERVER_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_SASL_AUTH_SERVER, \
   TestSaslAuthServerPrivate))

static void
received_stanza (GObject *source, GAsyncResult *result, gpointer user_data);

static void
test_sasl_auth_server_init (TestSaslAuthServer *obj)
{
  TestSaslAuthServerPrivate *priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (obj);
  priv->username = NULL;
  priv->password = NULL;
  priv->mech = NULL;
  priv->state = AUTH_STATE_STARTED;
  priv->recv_cancel = g_cancellable_new ();

  /* allocate any data required by the object here */
}

static void test_sasl_auth_server_dispose (GObject *object);
static void test_sasl_auth_server_finalize (GObject *object);

static void
test_sasl_auth_server_class_init (
    TestSaslAuthServerClass *test_sasl_auth_server_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (test_sasl_auth_server_class);

  g_type_class_add_private (test_sasl_auth_server_class,
      sizeof (TestSaslAuthServerPrivate));

  object_class->dispose = test_sasl_auth_server_dispose;
  object_class->finalize = test_sasl_auth_server_finalize;

}

void
test_sasl_auth_server_dispose (GObject *object)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (object);
  TestSaslAuthServerPrivate *priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_cancellable_cancel (priv->recv_cancel);
  g_object_unref (priv->recv_cancel);
  priv->recv_cancel = NULL;

  /* release any references held by the object here */
  g_object_unref (priv->conn);
  priv->conn = NULL;

  g_object_unref (priv->stream);
  priv->stream = NULL;

  sasl_dispose (&priv->sasl_conn);
  priv->sasl_conn = NULL;

  if (G_OBJECT_CLASS (test_sasl_auth_server_parent_class)->dispose)
    G_OBJECT_CLASS (test_sasl_auth_server_parent_class)->dispose (object);
}

void
test_sasl_auth_server_finalize (GObject *object)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (object);
  TestSaslAuthServerPrivate *priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);

  /* free any data held directly by the object here */
  g_free (priv->username);
  g_free (priv->password);
  g_free (priv->mech);

  G_OBJECT_CLASS (test_sasl_auth_server_parent_class)->finalize (object);
}

static void
features_sent (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_stanza_finish (
    WOCKY_XMPP_CONNECTION (source), res, NULL));

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (source),
    NULL, received_stanza, user_data);
}


static void
stream_open_sent (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER(user_data);
  TestSaslAuthServerPrivate * priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  WockyXmppNode *mechnode = NULL;

  g_assert (wocky_xmpp_connection_send_open_finish (
    WOCKY_XMPP_CONNECTION (source), res, NULL));

  /* Send stream features */
  stanza = wocky_xmpp_stanza_new ("features");
  wocky_xmpp_node_set_ns (stanza->node, WOCKY_XMPP_NS_STREAM);

  if (priv->problem != SERVER_PROBLEM_NO_SASL)
    {
      mechnode = wocky_xmpp_node_add_child_ns (stanza->node,
          "mechanisms", WOCKY_XMPP_NS_SASL_AUTH);
      if (priv->problem == SERVER_PROBLEM_NO_MECHANISMS)
        {
          /* lalala */
        }
      else if (priv->mech != NULL)
        {
          wocky_xmpp_node_add_child_with_content (mechnode, "mechanism",
              priv->mech);
        }
      else
        {
          const gchar *mechs;
          gchar **mechlist;
          gchar **tmp;
          int ret;
          ret = sasl_listmech (priv->sasl_conn, NULL, "","\n","", &mechs,
              NULL,NULL);
          CHECK_SASL_RETURN (ret);
          mechlist = g_strsplit (mechs, "\n", -1);
          for (tmp = mechlist; *tmp != NULL; tmp++)
            {
              wocky_xmpp_node_add_child_with_content (mechnode,
                "mechanism", *tmp);
            }
          g_strfreev (mechlist);
        }
    }

  wocky_xmpp_connection_send_stanza_async (priv->conn, stanza,
    NULL, features_sent, user_data);
  g_object_unref (stanza);
}

static void
stream_open_received (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER(user_data);
  TestSaslAuthServerPrivate * priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);

  g_assert (wocky_xmpp_connection_recv_open_finish (
    WOCKY_XMPP_CONNECTION (source), res,
    NULL, NULL, NULL, NULL,
    NULL));

  wocky_xmpp_connection_send_open_async (priv->conn,
    NULL, "testserver", "1.0", NULL,
    NULL, stream_open_sent, self);
}

static void
post_auth_close_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));
}

static void
post_auth_recv_stanza (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  WockyXmppStanza *stanza;
  GError *error = NULL;

  /* ignore all stanza until close */
  stanza = wocky_xmpp_connection_recv_stanza_finish (
    WOCKY_XMPP_CONNECTION (source), result, &error);

  if (stanza != NULL)
    {
      g_object_unref (stanza);
      wocky_xmpp_connection_recv_stanza_async (
          WOCKY_XMPP_CONNECTION (source), NULL,
          post_auth_recv_stanza, user_data);
    }
  else
    {
      g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
          WOCKY_XMPP_CONNECTION_ERROR_CLOSED));
      wocky_xmpp_connection_send_close_async (WOCKY_XMPP_CONNECTION (source),
          NULL, post_auth_close_sent, user_data);
      g_error_free (error);
    }
}

static void
post_auth_features_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_stanza_finish (
    WOCKY_XMPP_CONNECTION (source), result, NULL));

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (source),
      NULL, post_auth_recv_stanza, user_data);
}

static void
post_auth_open_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppStanza *s;
  g_assert (wocky_xmpp_connection_send_open_finish (
    WOCKY_XMPP_CONNECTION (source), result, NULL));

  s = wocky_xmpp_stanza_new ("features");
  wocky_xmpp_node_set_ns (s->node, WOCKY_XMPP_NS_STREAM);

  wocky_xmpp_connection_send_stanza_async (WOCKY_XMPP_CONNECTION (source), s,
      NULL, post_auth_features_sent, user_data);

  g_object_unref (s);
}

static void
post_auth_open_received (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_recv_open_finish (
    WOCKY_XMPP_CONNECTION (source), result,
    NULL, NULL, NULL, NULL,
    user_data));

  wocky_xmpp_connection_send_open_async ( WOCKY_XMPP_CONNECTION (source),
    NULL, "testserver", "1.0", NULL,
    NULL, post_auth_open_sent, user_data);
}

static void
success_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));

  wocky_xmpp_connection_reset (WOCKY_XMPP_CONNECTION (source));

  wocky_xmpp_connection_recv_open_async (WOCKY_XMPP_CONNECTION (source),
    NULL, post_auth_open_received, user_data);
}

static void
auth_succeeded (TestSaslAuthServer *self)
{
  TestSaslAuthServerPrivate * priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE(self);
  WockyXmppStanza *s;

  g_assert (priv->state < AUTH_STATE_AUTHENTICATED);
  priv->state = AUTH_STATE_AUTHENTICATED;

  s = wocky_xmpp_stanza_new ("success");
  wocky_xmpp_node_set_ns (s->node, WOCKY_XMPP_NS_SASL_AUTH);

  wocky_xmpp_connection_send_stanza_async (priv->conn, s, NULL,
    success_sent, NULL);

  g_object_unref (s);
}

static void
failure_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));
}

static void
not_authorized (TestSaslAuthServer *self)
{
  TestSaslAuthServerPrivate * priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE(self);
  WockyXmppStanza *s;

  g_assert (priv->state < AUTH_STATE_AUTHENTICATED);
  priv->state = AUTH_STATE_AUTHENTICATED;

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_FAILURE,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
      WOCKY_NODE, "not-authorized", WOCKY_NODE_END,
    WOCKY_STANZA_END);
  wocky_xmpp_connection_send_stanza_async (priv->conn, s, NULL,
    failure_sent, NULL);

  g_object_unref (s);
}

/* check if the return of the sasl function  was as expected, if not FALSE is
 * returend and the call function should stop processing */
static gboolean
check_sasl_return (TestSaslAuthServer *self, int ret)
{
  TestSaslAuthServerPrivate * priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);

  switch (ret)
    {
      case SASL_BADAUTH:
        /* Bad password provided */
        g_assert (priv->problem == SERVER_PROBLEM_INVALID_PASSWORD);
        not_authorized (self);
        return FALSE;
      case SASL_NOUSER:
        /* Unknown user */
        g_assert (priv->problem == SERVER_PROBLEM_INVALID_USERNAME);
        not_authorized (self);
        return FALSE;
      default:
        /* sasl auth should be ok */
        CHECK_SASL_RETURN (ret);
        break;
    }

  return TRUE;
}

static void
handle_auth (TestSaslAuthServer *self, WockyXmppStanza *stanza)
{
  TestSaslAuthServerPrivate *priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);
  const gchar *mech = wocky_xmpp_node_get_attribute (stanza->node,
      "mechanism");
  guchar *response = NULL;
  const gchar *challenge;
  unsigned challenge_len;
  gsize response_len = 0;
  int ret;

  if (stanza->node->content != NULL)
    {
      response = g_base64_decode (stanza->node->content, &response_len);
    }

  g_assert (priv->state == AUTH_STATE_STARTED);
  priv->state = AUTH_STATE_CHALLENGE;

  ret = sasl_server_start (priv->sasl_conn, mech, (gchar *) response,
      (unsigned) response_len, &challenge, &challenge_len);

  if (!check_sasl_return (self, ret))
    return;

  if (challenge_len > 0)
    {
      WockyXmppStanza *c;
      gchar *challenge64;

      if (ret == SASL_OK)
        {
          priv->state = AUTH_STATE_FINAL_CHALLENGE;
        }

      challenge64 = g_base64_encode ((guchar *) challenge, challenge_len);

      c = wocky_xmpp_stanza_new ("challenge");
      wocky_xmpp_node_set_ns (c->node, WOCKY_XMPP_NS_SASL_AUTH);
      wocky_xmpp_node_set_content (c->node, challenge64);
      wocky_xmpp_connection_send_stanza_async (priv->conn, c,
        NULL, NULL, NULL);
      g_object_unref (c);

      g_free (challenge64);
    }
  else if (ret == SASL_OK)
    {
      auth_succeeded (self);
    }
  else
    {
      g_assert_not_reached ();
    }

  g_free (response);
}

static void
handle_response (TestSaslAuthServer *self, WockyXmppStanza *stanza)
{
  TestSaslAuthServerPrivate * priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);
  guchar *response = NULL;
  const gchar *challenge;
  unsigned challenge_len;
  gsize response_len = 0;
  int ret;


  if (priv->state == AUTH_STATE_FINAL_CHALLENGE)
    {
      g_assert (stanza->node->content == NULL);
      auth_succeeded (self);
      return;
    }

  g_assert (priv->state == AUTH_STATE_CHALLENGE);

  if (stanza->node->content != NULL)
    {
      response = g_base64_decode (stanza->node->content, &response_len);
    }

  ret = sasl_server_step (priv->sasl_conn, (gchar *) response,
      (unsigned) response_len, &challenge, &challenge_len);

  if (!check_sasl_return (self, ret))
    return;

  if (challenge_len > 0)
    {
      WockyXmppStanza *c;
      gchar *challenge64;

      if (ret == SASL_OK)
        {
          priv->state = AUTH_STATE_FINAL_CHALLENGE;
        }

      challenge64 = g_base64_encode ((guchar *) challenge, challenge_len);

      c = wocky_xmpp_stanza_new ("challenge");
      wocky_xmpp_node_set_ns (c->node, WOCKY_XMPP_NS_SASL_AUTH);
      wocky_xmpp_node_set_content (c->node, challenge64);
      wocky_xmpp_connection_send_stanza_async (priv->conn, c,
        NULL, NULL, NULL);
      g_object_unref (c);

      g_free (challenge64);
    }
  else if (ret == SASL_OK)
    {
      auth_succeeded (self);
    }
  else
    {
      g_assert_not_reached ();
    }

  g_free (response);
}


#define HANDLE(x) { #x, handle_##x }
static void
received_stanza (GObject *source,
  GAsyncResult *result,
    gpointer user_data)
{
  TestSaslAuthServer *self;
  TestSaslAuthServerPrivate *priv;
  int i;
  WockyXmppStanza *stanza;
  GError *error = NULL;
  struct {
    const gchar *name;
    void (*func)(TestSaslAuthServer *self, WockyXmppStanza *stanza);
  } handlers[] = { HANDLE(auth), HANDLE(response) };

  stanza = wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error);

  if (stanza == NULL
    && (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
        || g_error_matches (error,
            WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_EOS)))
    {
      g_error_free (error);
      return;
    }

  self = TEST_SASL_AUTH_SERVER (user_data);
  priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (self);

  g_assert (stanza != NULL);

  if (strcmp (wocky_xmpp_node_get_ns (stanza->node),
      WOCKY_XMPP_NS_SASL_AUTH))
    {
      g_assert_not_reached ();
    }

  for (i = 0 ; handlers[i].name != NULL; i++)
    {
      if (!strcmp (stanza->node->name, handlers[i].name))
        {
          handlers[i].func (self, stanza);
          if (priv->state < AUTH_STATE_AUTHENTICATED)
            {
              wocky_xmpp_connection_recv_stanza_async (priv->conn,
                NULL, received_stanza, user_data);
            }
          g_object_unref (stanza);
          return;
        }
    }

  g_assert_not_reached ();
}

static int
test_sasl_server_auth_log (void *context, int level, const gchar *message)
{
  return SASL_OK;
}

static int
test_sasl_server_auth_getopt (void *context, const char *plugin_name,
  const gchar *option, const gchar **result, guint *len)
{
  int i;
  static const struct {
    const gchar *name;
    const gchar *value;
  } options[] = {
    { "auxprop_plugin", "sasldb"},
    { "sasldb_path", "./sasl-test.db"},
    { NULL, NULL },
  };

  for (i = 0; options[i].name != NULL; i++)
    {
      if (!strcmp (option, options[i].name))
        {
          *result = options[i].value;
          if (len != NULL)
            *len = strlen (options[i].value);
        }
    }

  return SASL_OK;
}

TestSaslAuthServer *
test_sasl_auth_server_new (GIOStream *stream, gchar *mech,
    const gchar *user, const gchar *password, ServerProblem problem)
{
  TestSaslAuthServer *server;
  TestSaslAuthServerPrivate *priv;
  static gboolean sasl_initialized = FALSE;
  int ret;
  static sasl_callback_t callbacks[] = {
    { SASL_CB_LOG, test_sasl_server_auth_log, NULL },
    { SASL_CB_GETOPT, test_sasl_server_auth_getopt, NULL },
    { SASL_CB_LIST_END, NULL, NULL },
  };

  if (!sasl_initialized)
    {
      sasl_server_init (NULL, NULL);
      sasl_initialized = TRUE;
    }

  server = g_object_new (TEST_TYPE_SASL_AUTH_SERVER, NULL);
  priv = TEST_SASL_AUTH_SERVER_GET_PRIVATE (server);

  priv->state = AUTH_STATE_STARTED;
  priv->stream = g_object_ref (stream);

  ret = sasl_server_new ("xmpp", NULL, NULL, NULL, NULL, callbacks,
      SASL_SUCCESS_DATA, &(priv->sasl_conn));
  CHECK_SASL_RETURN (ret);

  ret = sasl_setpass (priv->sasl_conn, user, password, strlen (password),
      NULL, 0, SASL_SET_CREATE);

  CHECK_SASL_RETURN (ret);

  priv->username = g_strdup (user);
  priv->password = g_strdup (password);
  priv->mech = g_strdup (mech);
  priv->problem = problem;

  priv->conn = wocky_xmpp_connection_new (stream);

  wocky_xmpp_connection_recv_open_async (priv->conn,
    NULL, stream_open_received, server);

  return server;
}
