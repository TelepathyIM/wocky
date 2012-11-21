/*
 * wocky-test-connector-server.c - Source for TestConnectorServer
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>

#include "wocky-test-connector-server.h"

#include <wocky/wocky.h>

#define INITIAL_STREAM_ID "0-HAI"

/* We're being a bit naughty here by including wocky-debug.h, but we're
 * internal *enough*.
 */
#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_CONNECTOR
#define WOCKY_COMPILATION
#include <wocky/wocky-debug-internal.h>
#undef WOCKY_COMPILATION

G_DEFINE_TYPE (TestConnectorServer, test_connector_server, G_TYPE_OBJECT);

typedef void (*stanza_func)(TestConnectorServer *self, WockyStanza *xml);
typedef struct _stanza_handler stanza_handler;
struct _stanza_handler {
  const gchar *ns;
  const gchar *name;
  stanza_func func;
};

typedef struct _iq_handler iq_handler;
struct _iq_handler {
  WockyStanzaSubType subtype;
  const gchar *payload;
  const gchar *ns;
  stanza_func func;
};

static void xmpp_init (GObject *source, GAsyncResult *result, gpointer data);
static void starttls (GObject *source, GAsyncResult *result, gpointer data);
static void finished (GObject *source, GAsyncResult *, gpointer data);
static void quit (GObject *source, GAsyncResult *result, gpointer data);

static void server_enc_outstanding (TestConnectorServer *self);
static gboolean server_dec_outstanding (TestConnectorServer *self);

/* ************************************************************************* */
/* test connector server object definition */
typedef enum {
  SERVER_STATE_START,
  SERVER_STATE_CLIENT_OPENED,
  SERVER_STATE_SERVER_OPENED,
  SERVER_STATE_FEATURES_SENT
} server_state;

static struct { CertSet set; const gchar *key; const gchar *crt; } certs[] =
  { { CERT_STANDARD, TLS_SERVER_KEY_FILE,  TLS_SERVER_CRT_FILE  },
    { CERT_EXPIRED,  TLS_EXP_KEY_FILE,     TLS_EXP_CRT_FILE     },
    { CERT_NOT_YET,  TLS_NEW_KEY_FILE,     TLS_NEW_CRT_FILE     },
    { CERT_UNKNOWN,  TLS_UNKNOWN_KEY_FILE, TLS_UNKNOWN_CRT_FILE },
    { CERT_SELFSIGN, TLS_SS_KEY_FILE,      TLS_SS_CRT_FILE      },
    { CERT_REVOKED,  TLS_REV_KEY_FILE,     TLS_REV_CRT_FILE     },
    { CERT_WILDCARD, TLS_WILD_KEY_FILE,    TLS_WILD_CRT_FILE    },
    { CERT_BADWILD,  TLS_BADWILD_KEY_FILE, TLS_BADWILD_CRT_FILE },
    { CERT_NONE,     NULL,                 NULL                 } };

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
  gchar *version;

  gchar *used_mech;

  CertSet cert;
  WockyTLSSession *tls_sess;

  GCancellable *cancellable;
  gint outstanding;
  GSimpleAsyncResult *teardown_result;

  struct { ServerProblem sasl; ConnectorProblem *connector; } problem;

  gchar *other_host;
  guint other_port;
};

static void
test_connector_server_dispose (GObject *object)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (object);
  TestConnectorServerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  if (priv->conn != NULL)
    g_object_unref (priv->conn);
  priv->conn = NULL;

  if (priv->stream != NULL)
    g_object_unref (priv->stream);
  priv->stream = NULL;

  if (priv->sasl != NULL)
    g_object_unref (priv->sasl);
  priv->sasl = NULL;

  if (priv->tls_sess != NULL)
    g_object_unref (priv->tls_sess);
  priv->tls_sess = NULL;

  if (G_OBJECT_CLASS (test_connector_server_parent_class)->dispose)
    G_OBJECT_CLASS (test_connector_server_parent_class)->dispose (object);
}

static void
test_connector_server_finalise (GObject *object)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (object);
  TestConnectorServerPrivate *priv = self->priv;
  /* free any data held directly by the object here */
  g_free (priv->mech);
  g_free (priv->user);
  g_free (priv->pass);
  g_free (priv->version);
  g_free (priv->used_mech);

  G_OBJECT_CLASS (test_connector_server_parent_class)->finalize (object);
}

static void
test_connector_server_init (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TEST_TYPE_CONNECTOR_SERVER,
      TestConnectorServerPrivate);
  priv = self->priv;

  priv->tls_started = FALSE;
  priv->authed      = FALSE;
  priv->cancellable = g_cancellable_new ();
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
static void xmpp_handler (GObject *source,
    GAsyncResult *result,
    gpointer user_data);
static void handle_auth     (TestConnectorServer *self,
    WockyStanza *xml);
static void handle_starttls (TestConnectorServer *self,
    WockyStanza *xml);

static void
after_auth (GObject *source,
    GAsyncResult *res,
    gpointer data);
static void xmpp_close (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void xmpp_closed (GObject *source,
    GAsyncResult *result,
    gpointer data);

static void iq_get_query_JABBER_AUTH (TestConnectorServer *self,
    WockyStanza *xml);
static void iq_set_query_JABBER_AUTH (TestConnectorServer *self,
    WockyStanza *xml);
static void iq_set_bind_XMPP_BIND (TestConnectorServer *self,
    WockyStanza *xml);
static void iq_set_session_XMPP_SESSION (TestConnectorServer *self,
    WockyStanza *xml);

static void iq_get_query_XEP77_REGISTER (TestConnectorServer *self,
    WockyStanza *xml);
static void iq_set_query_XEP77_REGISTER (TestConnectorServer *self,
    WockyStanza *xml);

static void iq_sent (GObject *source,
    GAsyncResult *result,
    gpointer data);
static void iq_sent_unregistered (GObject *source,
    GAsyncResult *result,
    gpointer data);

#define HANDLER(ns,x) { WOCKY_XMPP_NS_##ns, #x, handle_##x }
static stanza_handler handlers[] =
  {
    HANDLER (SASL_AUTH, auth),
    HANDLER (TLS, starttls),
    { NULL, NULL, NULL }
  };

#define IQH(S,s,name,nsp,ns) \
  { WOCKY_STANZA_SUB_TYPE_##S, #name, WOCKY_##nsp##_NS_##ns, \
    iq_##s##_##name##_##nsp##_##ns }

static iq_handler iq_handlers[] =
  {
    IQH (SET, set, bind, XMPP, BIND),
    IQH (SET, set, session, XMPP, SESSION),
    IQH (GET, get, query, JABBER, AUTH),
    IQH (SET, set, query, JABBER, AUTH),
    IQH (GET, get, query, XEP77, REGISTER),
    IQH (SET, set, query, XEP77, REGISTER),
    { WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL, NULL }
  };

/* ************************************************************************* */
/* error stanza                                                              */
static WockyStanza *
error_stanza (const gchar *cond,
    const gchar *msg, gboolean extended)
{
  WockyStanza *error = wocky_stanza_new ("error", WOCKY_XMPP_NS_STREAM);
  WockyNode *node = wocky_stanza_get_top_node (error);

  wocky_node_add_child_ns (node, cond, WOCKY_XMPP_NS_STREAMS);

  if ((msg != NULL) && (*msg != '\0'))
    wocky_node_add_child_with_content_ns (node, "text", msg,
        WOCKY_XMPP_NS_STREAMS);

  if (extended)
    wocky_node_add_child_with_content_ns (node, "something", "blah",
        "urn:ietf:a:namespace:I:made:up");

  return error;
}

/* ************************************************************************* */
static void
iq_set_query_XEP77_REGISTER (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *iq = NULL;
  WockyNode *env = wocky_stanza_get_top_node (xml);
  WockyNode *query = wocky_node_get_child (env, "query");
  const gchar *id = wocky_node_get_attribute (env, "id");
  gpointer cb = iq_sent;

  DEBUG ("");

  if (priv->problem.connector->xep77 & XEP77_PROBLEM_ALREADY)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '@', "id", id,
          '(', "query", ':', WOCKY_XEP77_NS_REGISTER,
          '(', "registered", ')',
          '(', "username", '$', "foo", ')',
          '(', "password", '$', "bar", ')',
          ')',
          NULL);
    }
  else if (priv->problem.connector->xep77 & XEP77_PROBLEM_FAIL_CONFLICT)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '@', "id", id,
          '(', "error", '@', "type", "cancel",
          '(', "conflict",
          ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else if (priv->problem.connector->xep77 & XEP77_PROBLEM_FAIL_REJECTED)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '@', "id", id,
          '(', "error", '@', "type", "modify",
          '(', "not-acceptable",
          ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else
    {
      if (wocky_node_get_child (query, "remove") == NULL)
        {
          iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
              WOCKY_STANZA_SUB_TYPE_RESULT,
              NULL, NULL,
              '@', "id", id,
              NULL);
        }
      else
        {
          XEP77Problem problem = priv->problem.connector->xep77;
          XEP77Problem p = XEP77_PROBLEM_NONE;

          DEBUG ("handling CANCEL");

          if ((p = problem & XEP77_PROBLEM_CANCEL_REJECTED) ||
              (p = problem & XEP77_PROBLEM_CANCEL_DISABLED) ||
              (p = problem & XEP77_PROBLEM_CANCEL_FAILED))
            {
              const gchar *error = NULL;
              const gchar *etype = NULL;
              const gchar *ecode = NULL;

              switch (p)
                {
                  case XEP77_PROBLEM_CANCEL_REJECTED:
                    error = "bad-request";
                    etype = "modify";
                    ecode = "400";
                    break;
                  case XEP77_PROBLEM_CANCEL_DISABLED:
                    error = "not-allowed";
                    etype = "cancel";
                    ecode = "405";
                    break;
                  default:
                    error = "forbidden";
                    etype = "cancel";
                    ecode = "401";
                }

              DEBUG ("error: %s/%s", error, etype);
              iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
                  WOCKY_STANZA_SUB_TYPE_ERROR,
                  NULL, NULL,
                  '@', "id", id,
                  '(', "error",
                  '@', "type", etype,
                  '@', "code", ecode,
                  '(', error, ':', WOCKY_XMPP_NS_STANZAS,
                  ')',
                  ')',
                  NULL);
            }
          else
            {
              if (priv->problem.connector->xep77 & XEP77_PROBLEM_CANCEL_STREAM)
                {
                  iq = error_stanza ("not-authorized", NULL, FALSE);
                  cb = finished;
                }
              else
                {
                  cb = iq_sent_unregistered;
                  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
                      WOCKY_STANZA_SUB_TYPE_RESULT,
                      NULL, NULL,
                      '@', "id", id,
                      NULL);
                }
            }
        }
    }

  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, iq,
    priv->cancellable, cb, self);
  g_object_unref (xml);
  g_object_unref (iq);
}

static void
iq_get_query_XEP77_REGISTER (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *iq = NULL;
  WockyNode *env = wocky_stanza_get_top_node (xml);
  WockyNode *query = NULL;
  const gchar *id = wocky_node_get_attribute (env, "id");

  DEBUG ("");
  if (priv->problem.connector->xep77 & XEP77_PROBLEM_NOT_AVAILABLE)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '@', "id", id,
          '(', "error", '@', "type", "cancel",
          '(', "service-unavailable",
          ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else if (priv->problem.connector->xep77 & XEP77_PROBLEM_QUERY_NONSENSE)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL,
          '@', "id", id,
          '(', "plankton", ':', WOCKY_XEP77_NS_REGISTER,
          ')',
          NULL);
    }
  else if (priv->problem.connector->xep77 & XEP77_PROBLEM_QUERY_ALREADY)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '@', "id", id,
          '(', "query", ':', WOCKY_XEP77_NS_REGISTER,
          '(', "registered", ')',
          '(', "username", '$', "foo", ')',
          '(', "password", '$', "bar", ')',
          ')',
          NULL);
    }
  else
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '@', "id", id,
          '(', "query", ':', WOCKY_XEP77_NS_REGISTER,
            '*', &query,
          ')',
          NULL);

      if (!(priv->problem.connector->xep77 & XEP77_PROBLEM_NO_ARGS))
        {
          wocky_node_add_child (query, "username");
          wocky_node_add_child (query, "password");

          if (priv->problem.connector->xep77 & XEP77_PROBLEM_EMAIL_ARG)
            wocky_node_add_child (query, "email");
          if (priv->problem.connector->xep77 & XEP77_PROBLEM_STRANGE_ARG)
            wocky_node_add_child (query, "wildebeest");
        }
    }

  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, iq, NULL, iq_sent, self);
  g_object_unref (xml);
  g_object_unref (iq);
}

static void
iq_get_query_JABBER_AUTH (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *iq = NULL;
  WockyNode *env = wocky_stanza_get_top_node (xml);
  const gchar *id = wocky_node_get_attribute (env, "id");
  WockyNode *query = wocky_node_get_child (env, "query");
  WockyNode *user  = (query != NULL) ?
    wocky_node_get_child (query, "username") : NULL;
  const gchar *name = (user != NULL) ? user->content : NULL;

  DEBUG ("");
  if (priv->problem.connector->jabber & JABBER_PROBLEM_AUTH_NIH)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '@', "id", id,
          '(', "error", '@', "type", "cancel",
          '(', "service-unavailable",
          ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else if (name == NULL || *name == '\0')
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '@', "id", id,
          '(', "error", '@', "type", "modify",
            '(', "not-acceptable",
              ':', WOCKY_XMPP_NS_STANZAS,
            ')',
            '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
              '$',
                "You must include the username in the initial IQ get to work "
                "around a bug in jabberd 1.4. See "
                "https://bugs.freedesktop.org/show_bug.cgi?id=24013",
            ')',
          ')',
          NULL);
    }
  else if (priv->mech != NULL)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '@', "id", id,
          '(', "query", ':', WOCKY_JABBER_NS_AUTH,
          '(', "username", ')',
          '(', priv->mech, ')',
          '(', "resource", ')',
          ')',
          NULL);
    }
  else
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '@', "id", id,
          '(', "query", ':', WOCKY_JABBER_NS_AUTH,
          '(', "username", ')',
          '(', "password", ')',
          '(', "resource", ')',
          '(', "digest", ')',
          ')',
          NULL);
    }

  DEBUG ("responding to iq get");
  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, iq,
    priv->cancellable, iq_sent, self);
  DEBUG ("sent iq get response");
  g_object_unref (xml);
  g_object_unref (iq);
}

static void
iq_set_query_JABBER_AUTH (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *iq = NULL;
  WockyNode *env = wocky_stanza_get_top_node (xml);
  WockyNode *qry = wocky_node_get_child (env, "query");
  JabberProblem problems = priv->problem.connector->jabber;
  JabberProblem jp = JABBER_PROBLEM_NONE;
  WockyNode *username = wocky_node_get_child (qry, "username");
  WockyNode *password = wocky_node_get_child (qry, "password");
  WockyNode *resource = wocky_node_get_child (qry, "resource");
  WockyNode *sha1hash = wocky_node_get_child (qry, "digest");
  const gchar *id = wocky_node_get_attribute (env, "id");

  DEBUG ("");
  if (username == NULL || resource == NULL)
    problems |= JABBER_PROBLEM_AUTH_PARTIAL;
  else if (password != NULL)
    {
      if (wocky_strdiff (priv->user, username->content) ||
          wocky_strdiff (priv->pass, password->content))
        problems |= JABBER_PROBLEM_AUTH_REJECT;
    }
  else if (sha1hash != NULL)
    {
      gchar *hsrc = g_strconcat (INITIAL_STREAM_ID, priv->pass, NULL);
      gchar *sha1 = g_compute_checksum_for_string (G_CHECKSUM_SHA1, hsrc, -1);
      DEBUG ("checksum: %s vs %s", sha1, sha1hash->content);
      if (wocky_strdiff (priv->user, username->content) ||
          wocky_strdiff (sha1, sha1hash->content))
        problems |= JABBER_PROBLEM_AUTH_REJECT;

      g_free (hsrc);
      g_free (sha1);
    }
  else
    problems |= JABBER_PROBLEM_AUTH_PARTIAL;

  if ((jp = problems & JABBER_PROBLEM_AUTH_REJECT)  ||
      (jp = problems & JABBER_PROBLEM_AUTH_BIND)    ||
      (jp = problems & JABBER_PROBLEM_AUTH_PARTIAL) ||
      (jp = problems & JABBER_PROBLEM_AUTH_FAILED))
    {
      const gchar *error = NULL;
      const gchar *etype = NULL;
      const gchar *ecode = NULL;

      switch (jp)
        {
          case JABBER_PROBLEM_AUTH_REJECT:
            error = "not-authorized";
            etype = "auth";
            ecode = "401";
            break;
          case JABBER_PROBLEM_AUTH_BIND:
            error = "conflict";
            etype = "cancel";
            ecode = "409";
            break;
          case JABBER_PROBLEM_AUTH_PARTIAL:
            error = "not-acceptable";
            etype = "modify";
            ecode = "406";
            break;
          default:
            error = "bad-request";
            etype = "modify";
            ecode = "500";
            break;
        }

      DEBUG ("error: %s/%s", error, etype);
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '@', "id", id,
          '(', "error",
          '@', "type", etype,
          '@', "code", ecode,
          '(', error, ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else if (problems & JABBER_PROBLEM_AUTH_STRANGE)
    {
      DEBUG ("auth WEIRD");
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_SET,
          NULL, NULL,
          '@', "id", id,
          '(', "surstromming", ':', WOCKY_XMPP_NS_BIND,
          ')',
          NULL);
    }
  else if (problems & JABBER_PROBLEM_AUTH_NONSENSE)
    {
      DEBUG ("auth NONSENSE");
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL,
          '@', "id", id,
          '(', "surstromming", ':', WOCKY_XMPP_NS_BIND,
          ')',
          NULL);
    }
  else
    {
      DEBUG ("auth OK");
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '@', "id", id,
          NULL);
    }

  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, iq, priv->cancellable,
    iq_sent, self);
  g_object_unref (iq);
  g_object_unref (xml);
}

static void
iq_set_bind_XMPP_BIND (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *iq = NULL;
  BindProblem problems = priv->problem.connector->bind;
  BindProblem bp = BIND_PROBLEM_NONE;

  DEBUG("");
  if ((bp = problems & BIND_PROBLEM_INVALID)  ||
      (bp = problems & BIND_PROBLEM_DENIED)   ||
      (bp = problems & BIND_PROBLEM_CONFLICT) ||
      (bp = problems & BIND_PROBLEM_REJECTED))
    {
      const gchar *error = NULL;
      const gchar *etype = NULL;
      switch (bp)
        {
        case BIND_PROBLEM_INVALID:
          error = "bad-request";
          etype = "modify";
          break;
        case BIND_PROBLEM_DENIED:
          error = "not-allowed";
          etype = "cancel";
          break;
        case BIND_PROBLEM_CONFLICT:
          error = "conflict";
          etype = "cancel";
          break;
        default:
          error = "badger-badger-badger-mushroom";
          etype = "moomins";
        }
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '(', "bind", ':', WOCKY_XMPP_NS_BIND,
          ')',
          '(', "error", '@', "type", etype,
          '(', error, ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else if (problems & BIND_PROBLEM_FAILED)
    {
      /* deliberately nonsensical response */
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_SET,
          NULL, NULL,
          '(', "bind", ':', WOCKY_XMPP_NS_BIND,
          ')',
          NULL);
    }
  else if (problems & BIND_PROBLEM_NONSENSE)
    {
      /* deliberately nonsensical response */
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL,
          '(', "bind", ':', WOCKY_XMPP_NS_BIND,
          ')',
          NULL);
    }
  else if (problems & BIND_PROBLEM_CLASH)
    {
      iq = error_stanza ("conflict", NULL, FALSE);
    }
  else
    {
      WockyNode *ciq = wocky_stanza_get_top_node (xml);
      WockyNode *bind =
        wocky_node_get_child_ns (ciq, "bind", WOCKY_XMPP_NS_BIND);
      WockyNode *res = wocky_node_get_child (bind, "resource");
      const gchar *uniq = NULL;
      gchar *jid = NULL;

      if (res != NULL)
        uniq = res->content;
      if (uniq == NULL)
        uniq = "a-made-up-resource";

      if (problems & BIND_PROBLEM_NO_JID)
        {
          iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
              WOCKY_STANZA_SUB_TYPE_RESULT, NULL, NULL,
              '(', "bind", ':', WOCKY_XMPP_NS_BIND,
              ')', NULL);
        }
      else
        {
          jid = g_strdup_printf ("user@some.doma.in/%s", uniq);
          iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
              WOCKY_STANZA_SUB_TYPE_RESULT,
              NULL, NULL,
              '(', "bind", ':', WOCKY_XMPP_NS_BIND,
              '(', "jid", '$', jid, ')',
              ')',
              NULL);
          g_free (jid);
        }
    }

  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, iq, priv->cancellable,
    iq_sent, self);
  g_object_unref (xml);
  g_object_unref (iq);
}

static void
iq_set_session_XMPP_SESSION (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *iq = NULL;
  SessionProblem problems = priv->problem.connector->session;
  SessionProblem sp = SESSION_PROBLEM_NONE;

  DEBUG ("");
  if ((sp = problems & SESSION_PROBLEM_FAILED)   ||
      (sp = problems & SESSION_PROBLEM_DENIED)   ||
      (sp = problems & SESSION_PROBLEM_CONFLICT) ||
      (sp = problems & SESSION_PROBLEM_REJECTED))
    {
      const gchar *error = NULL;
      const gchar *etype = NULL;
      switch (sp)
        {
        case SESSION_PROBLEM_FAILED:
          error = "internal-server-error";
          etype = "wait";
          break;
        case SESSION_PROBLEM_DENIED:
          error = "forbidden";
          etype = "auth";
          break;
        case SESSION_PROBLEM_CONFLICT:
          error = "conflict";
          etype = "cancel";
          break;
        default:
          error = "snaaaaake";
          etype = "mushroom";
          break;
        }
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_ERROR,
          NULL, NULL,
          '(', "session", ':', WOCKY_XMPP_NS_SESSION,
          ')',
          '(', "error", '@', "type", etype,
          '(', error, ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          ')',
          NULL);
    }
  else if (problems & SESSION_PROBLEM_NO_SESSION)
    {
      iq = error_stanza ("resource-constraint", "Out of Cheese Error", FALSE);
    }
  else if (problems & SESSION_PROBLEM_NONSENSE)
    {
      /* deliberately nonsensical response */
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL,
          '(', "surstromming", ':', WOCKY_XMPP_NS_BIND,
          ')',
          NULL);
    }
  else
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
          WOCKY_STANZA_SUB_TYPE_RESULT,
          NULL, NULL,
          '(', "session", ':', WOCKY_XMPP_NS_SESSION,
          ')',
          NULL);
    }

  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, iq, NULL, iq_sent, self);
  g_object_unref (xml);
  g_object_unref (iq);
}

static void
iq_sent_unregistered (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;
  WockyStanza *es = NULL;

  DEBUG("");

  if (!wocky_xmpp_connection_send_stanza_finish (conn, result, &error))
    {
      DEBUG ("send iq response failed: %s", error->message);
      g_error_free (error);
      server_dec_outstanding (self);
      return;
    }

  if (server_dec_outstanding (self))
    return;

  es = error_stanza ("not-authorized", NULL, FALSE);
  server_enc_outstanding (self);
  wocky_xmpp_connection_send_stanza_async (conn, es, priv->cancellable,
    finished, self);
  g_object_unref (es);
}


static void
iq_sent (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GError *error = NULL;
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->conn;

  DEBUG("");

  if (!wocky_xmpp_connection_send_stanza_finish (conn, result, &error))
    {
      DEBUG ("send iq response failed: %s", error->message);
      g_error_free (error);
      server_dec_outstanding (self);
      return;
    }

  if (server_dec_outstanding (self))
    return;

  server_enc_outstanding (self);

  DEBUG ("waiting for next stanza from client");
  wocky_xmpp_connection_recv_stanza_async (conn,
    priv->cancellable, xmpp_handler, data);
}

static void
handle_auth (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;
  GObject *sasl = G_OBJECT (priv->sasl);

  DEBUG ("");
  /* after this the sasl auth server object is in charge:
     control should return to us after the auth stages, at the point
     when we need to send our final feature stanza:
     the stream does not return to us */
  /* this will also unref *xml when it has finished with it */
  server_enc_outstanding (self);
  test_sasl_auth_server_auth_async (sasl, priv->conn, xml,
    after_auth, priv->cancellable, self);
}

static void
handle_starttls (TestConnectorServer *self,
    WockyStanza *xml)
{
  TestConnectorServerPrivate *priv = self->priv;

  DEBUG ("");
  if (!priv->tls_started)
    {
      WockyXmppConnection *conn = priv->conn;
      ConnectorProblem *problem = priv->problem.connector;
      WockyStanza *reply = NULL;
      GAsyncReadyCallback cb = finished;

      if (problem->xmpp & XMPP_PROBLEM_TLS_LOAD)
        {
          reply = error_stanza ("resource-constraint", "Load Too High", FALSE);
        }
      else if (problem->xmpp & XMPP_PROBLEM_TLS_REFUSED)
        {
          reply = wocky_stanza_new ("failure", WOCKY_XMPP_NS_TLS);
        }
      else
        {
          reply = wocky_stanza_new ("proceed", WOCKY_XMPP_NS_TLS);
          cb = starttls;
          /* set up the tls server session */
          /* gnutls_global_set_log_function ((gnutls_log_func)debug_gnutls);
           * gnutls_global_set_log_level (10); */
          if (problem->death & SERVER_DEATH_TLS_NEG)
            priv->tls_sess = wocky_tls_session_server_new (priv->stream,
                1024, NULL, NULL);
          else
            {
              int x;
              const gchar *key = TLS_SERVER_KEY_FILE;
              const gchar *crt = TLS_SERVER_CRT_FILE;

              for (x = 0; certs[x].set != CERT_NONE; x++)
                {
                  if (certs[x].set == priv->cert)
                    {
                      key = certs[x].key;
                      crt = certs[x].crt;
                      break;
                    }
                }
              DEBUG ("cert file: %s", crt);

              priv->tls_sess =
                wocky_tls_session_server_new (priv->stream, 1024, key, crt);
            }
        }
      server_enc_outstanding (self);
      wocky_xmpp_connection_send_stanza_async (conn, reply, NULL, cb, self);
      g_object_unref (reply);
    }
  g_object_unref (xml);
}

static void
finished (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;
  DEBUG ("");

  if (server_dec_outstanding (self))
    return;
  server_enc_outstanding (self);
  wocky_xmpp_connection_send_close_async (priv->conn,
    priv->cancellable, quit, data);
}

static void
quit (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;

  DEBUG ("");
  wocky_xmpp_connection_send_close_finish (priv->conn, result, NULL);
  server_dec_outstanding (self);
}

static void
handshake_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (user_data);
  TestConnectorServerPrivate *priv = self->priv;
  WockyTLSConnection *tls_conn;
  GError *error = NULL;

  DEBUG ("TLS/SSL handshake finished");

  tls_conn = wocky_tls_session_handshake_finish (
    WOCKY_TLS_SESSION (source),
    result,
    &error);

  if (server_dec_outstanding (self))
    goto out;

  if (tls_conn == NULL)
    {
      DEBUG ("SSL or TLS Server Setup failed: %s", error->message);
      g_io_stream_close (priv->stream, NULL, NULL);
      goto out;
    }

  if (priv->conn != NULL)
    g_object_unref (priv->conn);

  priv->state = SERVER_STATE_START;
  priv->conn = wocky_xmpp_connection_new (G_IO_STREAM (tls_conn));
  g_object_unref (tls_conn);
  priv->tls_started = TRUE;
  xmpp_init (NULL,NULL,self);

out:
  if (error != NULL)
    g_error_free (error);
}


static void
starttls (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;
  WockyXmppConnection *conn = WOCKY_XMPP_CONNECTION (source);
  GError *error = NULL;

  DEBUG ("");

  if (!wocky_xmpp_connection_send_stanza_finish (conn, result, &error))
    {
      DEBUG ("Sending starttls '<proceed...>' failed: %s", error->message);
      g_error_free (error);
      server_dec_outstanding (self);
      return;
    }

  if (server_dec_outstanding (self))
    return;

  server_enc_outstanding (self);
  wocky_tls_session_handshake_async (priv->tls_sess,
    G_PRIORITY_DEFAULT,
    NULL,
    handshake_cb,
    self);
}


static void
xmpp_handler (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;
  WockyStanza *xml = NULL;
  WockyXmppConnection *conn = NULL;
  const gchar *ns = NULL;
  const gchar *name = NULL;
  gboolean handled = FALSE;
  GError *error = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType subtype = WOCKY_STANZA_SUB_TYPE_NONE;
  int i;

  DEBUG ("");
  self = TEST_CONNECTOR_SERVER (user_data);
  priv = self->priv;
  conn = priv->conn;
  xml  = wocky_xmpp_connection_recv_stanza_finish (conn, result, &error);

  /* A real XMPP server would need to do some error handling here, but if
   * we got this far, we can just exit: The client (ie the test) will
   * report any error that actually needs reporting - we don't need to */
  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          server_dec_outstanding (self);
          return;
        }
      g_assert_not_reached ();
    }

  if (server_dec_outstanding (self))
    return;

  ns   = wocky_node_get_ns (wocky_stanza_get_top_node (xml));
  name = wocky_stanza_get_top_node (xml)->name;
  wocky_stanza_get_type_info (xml, &type, &subtype);

  /* if we find a handler, the handler is responsible for listening for the
     next stanza and setting up the next callback in the chain: */
  if (type == WOCKY_STANZA_TYPE_IQ)
    for (i = 0; iq_handlers[i].payload != NULL; i++)
      {
        iq_handler *iq = &iq_handlers[i];
        WockyNode *payload =
          wocky_node_get_child_ns (wocky_stanza_get_top_node (xml),
              iq->payload, iq->ns);
        /* namespace, stanza subtype and payload tag name must match: */
        if ((payload == NULL) || (subtype != iq->subtype))
          continue;
        DEBUG ("test_connector_server:invoking iq handler %s", iq->payload);
        (iq->func) (self, xml);
        handled = TRUE;
        break;
      }
  else
    for (i = 0; handlers[i].ns != NULL; i++)
      {
        if (!strcmp (ns, handlers[i].ns) && !strcmp (name, handlers[i].name))
          {
            DEBUG ("test_connector_server:invoking handler %s.%s", ns, name);
            (handlers[i].func) (self, xml);
            handled = TRUE;
            break;
          }
      }

  /* no handler found: just complain and sit waiting for the next stanza */
  if (!handled)
    {
      DEBUG ("<%s xmlns=\"%s\"… not handled", name, ns);
      server_enc_outstanding (self);
      wocky_xmpp_connection_recv_stanza_async (conn, priv->cancellable,
          xmpp_handler, self);
      g_object_unref (xml);
    }
}
/* ************************************************************************* */
/* resume control after the sasl auth server is done:                        */
static void
after_auth (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  GError *error = NULL;
  WockyStanza *feat = NULL;
  WockyNode *node = NULL;
  TestSaslAuthServer *tsas = TEST_SASL_AUTH_SERVER (source);
  TestConnectorServer *tcs = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = tcs->priv;
  WockyXmppConnection *conn = priv->conn;

  DEBUG ("Auth finished: %d", priv->outstanding);
  if (!test_sasl_auth_server_auth_finish (tsas, res, &error))
    {
      g_object_unref (priv->sasl);
      priv->sasl = NULL;

      if (server_dec_outstanding (tcs))
        return;

      server_enc_outstanding (tcs);
      wocky_xmpp_connection_send_close_async (conn, priv->cancellable,
          xmpp_close, data);
      return;
    }

  priv->used_mech = g_strdup (test_sasl_auth_server_get_selected_mech
    (priv->sasl));

  g_object_unref (priv->sasl);
  priv->sasl = NULL;

  if (server_dec_outstanding (tcs))
    return;

  feat = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_FEATURES,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL, NULL);

  node = wocky_stanza_get_top_node (feat);

  if (!(priv->problem.connector->xmpp & XMPP_PROBLEM_NO_SESSION))
    wocky_node_add_child_ns (node, "session", WOCKY_XMPP_NS_SESSION);

  if (!(priv->problem.connector->xmpp & XMPP_PROBLEM_CANNOT_BIND))
    wocky_node_add_child_ns (node, "bind", WOCKY_XMPP_NS_BIND);

  priv->state = SERVER_STATE_FEATURES_SENT;

  server_enc_outstanding (tcs);
  wocky_xmpp_connection_send_stanza_async (conn, feat, priv->cancellable,
    xmpp_init, data);

  g_object_unref (feat);
}

/* ************************************************************************* */
/* initial XMPP stream setup, up to sending features stanza */
static WockyStanza *
feature_stanza (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv = self->priv;
  XmppProblem problem = priv->problem.connector->xmpp;
  const gchar *name = NULL;
  WockyStanza *feat = NULL;
  WockyNode *node = NULL;

  DEBUG ("");
  if (problem & XMPP_PROBLEM_OTHER_HOST)
    return error_stanza ("host-unknown", "some sort of DNS error", TRUE);

  name = (problem & XMPP_PROBLEM_FEATURES) ? "badger" : "features";
  feat = wocky_stanza_new (name, WOCKY_XMPP_NS_STREAM);
  node = wocky_stanza_get_top_node (feat);

  DEBUG ("constructing <%s...>... stanza", name);

  if (priv->problem.sasl != SERVER_PROBLEM_NO_SASL)
    {
      if (priv->sasl == NULL)
        priv->sasl = test_sasl_auth_server_new (NULL, priv->mech,
            priv->user, priv->pass, NULL, priv->problem.sasl, FALSE);
      test_sasl_auth_server_set_mechs (G_OBJECT (priv->sasl), feat);
    }

  if (problem & XMPP_PROBLEM_OLD_AUTH_FEATURE)
    wocky_node_add_child_ns (node, "auth", WOCKY_JABBER_NS_AUTH_FEATURE);

  if (!(problem & XMPP_PROBLEM_NO_TLS) && !priv->tls_started)
    wocky_node_add_child_ns (node, "starttls", WOCKY_XMPP_NS_TLS);

  return feat;
}

static void
xmpp_close (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;

  DEBUG ("Closing connection");
  wocky_xmpp_connection_send_close_async (priv->conn, NULL, xmpp_closed, self);
}

static void
xmpp_closed (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (data);
  TestConnectorServerPrivate *priv = self->priv;
  DEBUG ("Connection closed");
  wocky_xmpp_connection_send_close_finish (priv->conn, result, NULL);
}

static void startssl (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv = self->priv;
  ConnectorProblem *problem = priv->problem.connector;

  g_assert (!priv->tls_started);

  DEBUG ("creating SSL Session [server]");
  if (problem->death & SERVER_DEATH_TLS_NEG)
    priv->tls_sess =
      wocky_tls_session_server_new (priv->stream, 1024, NULL, NULL);
  else
    {
      int x;
      const gchar *key = TLS_SERVER_KEY_FILE;
      const gchar *crt = TLS_SERVER_CRT_FILE;

      for (x = 0; certs[x].set != CERT_NONE; x++)
        {
          if (certs[x].set == priv->cert)
            {
              key = certs[x].key;
              crt = certs[x].crt;
              break;
            }
        }

      priv->tls_sess =
        wocky_tls_session_server_new (priv->stream, 1024, key, crt);
    }

  DEBUG ("starting server SSL handshake");
  server_enc_outstanding (self);
  wocky_tls_session_handshake_async (priv->tls_sess,
    G_PRIORITY_DEFAULT,
    priv->cancellable,
    handshake_cb,
    self);
}

static void
force_closed_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestConnectorServer *self = TEST_CONNECTOR_SERVER (user_data);

  DEBUG ("Connection force closed");

  g_assert (wocky_xmpp_connection_force_close_finish (
    WOCKY_XMPP_CONNECTION (source),
    result, NULL));

  server_dec_outstanding (self);
}

static void
see_other_host_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestConnectorServer *self = user_data;

  g_assert (wocky_xmpp_connection_send_stanza_finish (self->priv->conn,
      result, NULL));

  if (server_dec_outstanding (self))
    return;

  server_enc_outstanding (self);
  wocky_xmpp_connection_force_close_async (self->priv->conn,
      self->priv->cancellable, force_closed_cb, self);
}

static void
xmpp_init (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;
  WockyStanza *xml;
  WockyXmppConnection *conn;

  self = TEST_CONNECTOR_SERVER (data);
  priv = self->priv;
  conn = priv->conn;

  DEBUG ("test_connector_server:xmpp_init %d", priv->state);
  DEBUG ("connection: %p", conn);

  switch (priv->state)
    {
      /* wait for <stream:stream… from the client */
    case SERVER_STATE_START:
      DEBUG ("SERVER_STATE_START");
      priv->state = SERVER_STATE_CLIENT_OPENED;

      server_enc_outstanding (self);
      if (priv->problem.connector->death & SERVER_DEATH_SERVER_START)
        {
          wocky_xmpp_connection_force_close_async (conn,
            priv->cancellable, force_closed_cb, self);
        }
      else
        {
          wocky_xmpp_connection_recv_open_async (conn, priv->cancellable,
            xmpp_init, self);
        }
      break;

      /* send our own <stream:stream… */
    case SERVER_STATE_CLIENT_OPENED:
      DEBUG ("SERVER_STATE_CLIENT_OPENED");
      priv->state = SERVER_STATE_SERVER_OPENED;
      wocky_xmpp_connection_recv_open_finish (conn, result,
          NULL, NULL, NULL, NULL, NULL, NULL);
      if (server_dec_outstanding (self))
        return;

      server_enc_outstanding (self);
      if (priv->problem.connector->death & SERVER_DEATH_CLIENT_OPEN)
        {
          wocky_xmpp_connection_force_close_async (conn,
            priv->cancellable, force_closed_cb, self);
        }
      else
        {
          wocky_xmpp_connection_send_open_async (conn, NULL,
              "testserver",
             priv->version, NULL, INITIAL_STREAM_ID,
             priv->cancellable, xmpp_init, self);
        }
      break;

      /* send our feature set */
    case SERVER_STATE_SERVER_OPENED:
      DEBUG ("SERVER_STATE_SERVER_OPENED");
      priv->state = SERVER_STATE_FEATURES_SENT;
      wocky_xmpp_connection_send_open_finish (conn, result, NULL);
      if (server_dec_outstanding (self))
        return;

      if (priv->problem.connector->death & SERVER_DEATH_SERVER_OPEN)
        {
          server_enc_outstanding (self);
          wocky_xmpp_connection_force_close_async (conn,
            priv->cancellable, force_closed_cb, self);
        }
      else if (priv->problem.connector->xmpp & XMPP_PROBLEM_OLD_SERVER)
        {
          DEBUG ("diverting to old-jabber-auth");
          server_enc_outstanding (self);
          wocky_xmpp_connection_recv_stanza_async (priv->conn,
              priv->cancellable,
              xmpp_handler, self);
        }
      else if (priv->problem.connector->xmpp & XMPP_PROBLEM_SEE_OTHER_HOST)
        {
          WockyStanza *stanza;
          WockyNode *node;
          gchar *host_and_port;

          host_and_port = g_strdup_printf ("%s:%u", self->priv->other_host,
              self->priv->other_port);

          DEBUG ("Redirect to another host: %s", host_and_port);

          stanza = wocky_stanza_new ("error", WOCKY_XMPP_NS_STREAM);
          node = wocky_stanza_get_top_node (stanza);
          wocky_node_add_child_with_content_ns (node, "see-other-host",
              host_and_port, WOCKY_XMPP_NS_STREAMS);

          server_enc_outstanding (self);
          wocky_xmpp_connection_send_stanza_async (self->priv->conn, stanza,
              self->priv->cancellable, see_other_host_cb, self);

          g_object_unref (stanza);
        }
      else
        {
          xml = feature_stanza (self);
          server_enc_outstanding (self);
          wocky_xmpp_connection_send_stanza_async (conn, xml,
              priv->cancellable, xmpp_init, self);
          g_object_unref (xml);
        }
      break;

      /* ok, we're done with initial stream setup */
    case SERVER_STATE_FEATURES_SENT:
      DEBUG ("SERVER_STATE_FEATURES_SENT");
      wocky_xmpp_connection_send_stanza_finish (conn, result, NULL);
      if (server_dec_outstanding (self))
        return;

      server_enc_outstanding (self);
      if (priv->problem.connector->death & SERVER_DEATH_FEATURES)
        {
          wocky_xmpp_connection_force_close_async (conn,
            priv->cancellable, force_closed_cb, self);
        }
      else
        {
          wocky_xmpp_connection_recv_stanza_async (conn, priv->cancellable,
            xmpp_handler, self);
        }
      break;

    default:
      DEBUG ("Unknown Server state. Broken code flow.");
    }
}

/* ************************************************************************* */
/* exposed methods */

TestConnectorServer *
test_connector_server_new (GIOStream *stream,
    gchar *mech,
    const gchar *user,
    const gchar *pass,
    const gchar *version,
    ConnectorProblem *problem,
    ServerProblem sasl_problem,
    CertSet cert)
{
  TestConnectorServer *self;
  TestConnectorServerPrivate *priv;

  DEBUG ("test_connector_server_new");

  self = g_object_new (TEST_TYPE_CONNECTOR_SERVER, NULL);
  priv = self->priv;

  priv->stream = g_object_ref (stream);
  priv->mech   = g_strdup (mech);
  priv->user   = g_strdup (user);
  priv->pass   = g_strdup (pass);
  priv->problem.sasl      = sasl_problem;
  priv->problem.connector = problem;
  priv->conn   = wocky_xmpp_connection_new (stream);
  priv->cert   = cert;

  DEBUG ("connection: %p", priv->conn);

  if (problem->xmpp & XMPP_PROBLEM_OLD_SERVER)
    priv->version = g_strdup ((version == NULL) ? "0.9" : version);
  else
    priv->version = g_strdup ((version == NULL) ? "1.0" : version);

  return self;
}

static void
server_enc_outstanding (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv = self->priv;
  priv->outstanding++;

  DEBUG ("Upped outstanding to %d", priv->outstanding);
}

static gboolean
server_dec_outstanding (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv = self->priv;

  priv->outstanding--;
  g_assert (priv->outstanding >= 0);

  if (priv->teardown_result != NULL && priv->outstanding == 0)
    {
      GSimpleAsyncResult *r = priv->teardown_result;
      priv->teardown_result = NULL;

      DEBUG ("Tearing down, bye bye");

      g_simple_async_result_complete (r);
      g_object_unref (r);
      DEBUG ("Unreffed!");
      return TRUE;
    }

  DEBUG ("Outstanding: %d", priv->outstanding);

  return FALSE;
}

void
test_connector_server_teardown (TestConnectorServer *self,
  GAsyncReadyCallback callback,
  gpointer user_data)
{
  TestConnectorServerPrivate *priv = self->priv;
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, test_connector_server_teardown);

  /* For now, we'll assert if this gets called twice */
  g_assert (priv->cancellable != NULL);

  DEBUG ("Requested to stop: %d", priv->outstanding);

  g_cancellable_cancel (priv->cancellable);
  g_object_unref (priv->cancellable);
  priv->cancellable = NULL;

  if (priv->outstanding == 0)
    {
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
    }
  else
    {
      priv->teardown_result = result;
    }
}

gboolean
test_connector_server_teardown_finish (TestConnectorServer *self,
  GAsyncResult *result,
  GError *error)
{
  return TRUE;
}


void
test_connector_server_start (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv;

  DEBUG("test_connector_server_start");

  priv = self->priv;
  priv->state = SERVER_STATE_START;
  DEBUG ("connection: %p", priv->conn);
  if (priv->problem.connector->xmpp & XMPP_PROBLEM_OLD_SSL)
    {
      startssl (self);
    }
  else
    {
      xmpp_init (NULL,NULL,self);
    }
}

const gchar *
test_connector_server_get_used_mech (TestConnectorServer *self)
{
  TestConnectorServerPrivate *priv = self->priv;

  return priv->used_mech;
}

void
test_connector_server_set_other_host (TestConnectorServer *self,
    const gchar *host,
    guint port)
{
  g_return_if_fail (TEST_IS_CONNECTOR_SERVER (self));
  g_return_if_fail (self->priv->other_host == NULL);
  g_return_if_fail (self->priv->other_port == 0);

  self->priv->other_host = g_strdup (host);
  self->priv->other_port = port;

}
