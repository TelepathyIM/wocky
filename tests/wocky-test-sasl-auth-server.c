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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "wocky-test-sasl-auth-server.h"
#include "wocky-test-helper.h"

#ifdef HAVE_LIBSASL2

#include <sasl/sasl.h>
#include <sasl/saslplug.h>
#define CHECK_SASL_RETURN(x)                                \
G_STMT_START   {                                            \
    if (x < SASL_OK) {                                      \
      fprintf (stderr, "sasl error (%d): %s\n",             \
           ret, sasl_errdetail (priv->sasl_conn));          \
      g_assert_not_reached ();                              \
    }                                                       \
} G_STMT_END

/* Apparently, we're allowed to typedef the same thing *again* if it's
 * the same signature, so this allows for backwards compatiblity with
 * older libsasl2s and also works with newer ones too. This'll only
 * break if libsasl2 change the type of sasl_callback_ft. I sure hope
 * they don't! */
typedef int (*sasl_callback_ft)(void);

#else

#define SASL_OK 0
#define SASL_BADPROT  -5
#define SASL_BADAUTH -13
#define SASL_NOUSER  -20
#define CHECK_SASL_RETURN(x) \
G_STMT_START   {                                       \
    if (x < SASL_OK) {                                 \
      fprintf (stderr, "sasl error (%d): ???\n", ret); \
      g_assert_not_reached ();                         \
    }                                                  \
} G_STMT_END

#endif

#ifdef WOCKY_SCRAM_NONSTANDARD
#define BINDING_SCRAM_MECH "SCRAM-SHA-512-PLUS"
#define BINDING_SCRAM_ALGO G_CHECKSUM_SHA512
#else
#define BINDING_SCRAM_MECH "SCRAM-SHA-256-PLUS"
#define BINDING_SCRAM_ALGO G_CHECKSUM_SHA256
#endif

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
struct _TestSaslAuthServerPrivate
{
  gboolean dispose_has_run;
  WockyXmppConnection *conn;
  GIOStream *stream;
#ifdef HAVE_LIBSASL2
  sasl_conn_t *sasl_conn;
#endif
  gchar *username;
  gchar *password;
  gchar *mech;
  gchar *selected_mech;
  AuthState state;
  ServerProblem problem;
  GTask *task;
  GCancellable *cancellable;
  WockySaslScram *scram;
};

G_DEFINE_TYPE_WITH_CODE (TestSaslAuthServer, test_sasl_auth_server, G_TYPE_OBJECT,
          G_ADD_PRIVATE (TestSaslAuthServer))

static void
received_stanza (GObject *source, GAsyncResult *result, gpointer user_data);

static void
test_sasl_auth_server_init (TestSaslAuthServer *self)
{
  TestSaslAuthServerPrivate *priv;

  self->priv = test_sasl_auth_server_get_instance_private (self);
  priv = self->priv;

  priv->username = NULL;
  priv->password = NULL;
  priv->mech = NULL;
  priv->state = AUTH_STATE_STARTED;
}

static void test_sasl_auth_server_dispose (GObject *object);
static void test_sasl_auth_server_finalize (GObject *object);

static void
test_sasl_auth_server_class_init (
    TestSaslAuthServerClass *test_sasl_auth_server_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (test_sasl_auth_server_class);

  object_class->dispose = test_sasl_auth_server_dispose;
  object_class->finalize = test_sasl_auth_server_finalize;

}

void
test_sasl_auth_server_dispose (GObject *object)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (object);
  TestSaslAuthServerPrivate *priv = self->priv;

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

#ifdef HAVE_LIBSASL2
  if (&priv->sasl_conn != NULL)
    sasl_dispose (&priv->sasl_conn);
  priv->sasl_conn = NULL;
#endif

  g_warn_if_fail (priv->task == NULL);
  g_warn_if_fail (priv->cancellable == NULL);

  if (G_OBJECT_CLASS (test_sasl_auth_server_parent_class)->dispose)
    G_OBJECT_CLASS (test_sasl_auth_server_parent_class)->dispose (object);
}

void
test_sasl_auth_server_finalize (GObject *object)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (object);
  TestSaslAuthServerPrivate *priv = self->priv;

  /* free any data held directly by the object here */
  g_free (priv->username);
  g_free (priv->password);
  g_free (priv->mech);
  g_free (priv->selected_mech);

  G_OBJECT_CLASS (test_sasl_auth_server_parent_class)->finalize (object);
}

static void
features_sent (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (user_data);
  TestSaslAuthServerPrivate *priv = self->priv;
  gboolean ok;
  GError *error = NULL;

  ok = wocky_xmpp_connection_send_stanza_finish (
    WOCKY_XMPP_CONNECTION (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (source),
    priv->cancellable, received_stanza, user_data);
}


static void
stream_open_sent (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER(user_data);
  TestSaslAuthServerPrivate * priv = self->priv;
  WockyStanza *stanza;
  gboolean ok;
  GError *error = NULL;

  ok = wocky_xmpp_connection_send_open_finish (
    WOCKY_XMPP_CONNECTION (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  /* Send stream features */
  stanza = wocky_stanza_new ("features", WOCKY_XMPP_NS_STREAM);

  test_sasl_auth_server_set_mechs (G_OBJECT (self), stanza, NULL);

  wocky_xmpp_connection_send_stanza_async (priv->conn, stanza,
    priv->cancellable, features_sent, user_data);
  g_object_unref (stanza);
}

static void
stream_open_received (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER(user_data);
  TestSaslAuthServerPrivate * priv = self->priv;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_xmpp_connection_recv_open_finish (
    WOCKY_XMPP_CONNECTION (source), res,
    NULL, NULL, NULL, NULL, NULL,
    &error);
  g_assert_no_error (error);
  g_assert (ok);

  wocky_xmpp_connection_send_open_async (priv->conn,
    NULL, "testserver", "1.0", NULL, "0-HA2",
    NULL, stream_open_sent, self);
}

static void
post_auth_close_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  gboolean ok;

  ok = wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);
}

static void
post_auth_recv_stanza (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER(user_data);
  TestSaslAuthServerPrivate * priv = self->priv;
  WockyStanza *stanza;
  GError *error = NULL;

  /* ignore all stanza until close */
  stanza = wocky_xmpp_connection_recv_stanza_finish (
    WOCKY_XMPP_CONNECTION (source), result, &error);

  if (stanza != NULL)
    {
      g_object_unref (stanza);
      wocky_xmpp_connection_recv_stanza_async (
          WOCKY_XMPP_CONNECTION (source), priv->cancellable,
          post_auth_recv_stanza, user_data);
    }
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      GTask *t = priv->task;

      priv->task = NULL;

      if (priv->cancellable != NULL)
        g_object_unref (priv->cancellable);

      priv->cancellable = NULL;

      g_task_return_error (t, error);

      g_object_unref (t);
    }
  else
    {
      g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
          WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
      wocky_xmpp_connection_send_close_async (WOCKY_XMPP_CONNECTION (source),
          priv->cancellable, post_auth_close_sent, user_data);
      g_error_free (error);
    }
}

static void
post_auth_features_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER(user_data);
  TestSaslAuthServerPrivate * priv = self->priv;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_xmpp_connection_send_stanza_finish (
    WOCKY_XMPP_CONNECTION (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (source),
      priv->cancellable, post_auth_recv_stanza, user_data);
}

static void
post_auth_open_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestSaslAuthServer *tsas = TEST_SASL_AUTH_SERVER (user_data);
  TestSaslAuthServerPrivate *priv = tsas->priv;
  gboolean ok;
  GError *error = NULL;

  ok = wocky_xmpp_connection_send_open_finish (
    WOCKY_XMPP_CONNECTION (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);

  /* if our caller wanted control back, hand it back here: */
  if (priv->task != NULL)
    {
      GTask *t = priv->task;

      priv->task = NULL;

      if (priv->cancellable != NULL)
        g_object_unref (priv->cancellable);

      priv->cancellable = NULL;

      g_task_return_boolean (t, TRUE);
      g_object_unref (t);
    }
  else
    {
      WockyStanza *s = wocky_stanza_new ("features", WOCKY_XMPP_NS_STREAM);
      wocky_xmpp_connection_send_stanza_async (WOCKY_XMPP_CONNECTION (source),
          s, NULL, post_auth_features_sent, user_data);
      g_object_unref (s);
    }
}

static void
post_auth_open_received (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  gboolean ok;

  ok = wocky_xmpp_connection_recv_open_finish (
    WOCKY_XMPP_CONNECTION (source), result,
    NULL, NULL, NULL, NULL, NULL,
    &error);
  g_assert_no_error (error);
  g_assert (ok);

  wocky_xmpp_connection_send_open_async ( WOCKY_XMPP_CONNECTION (source),
    NULL, "testserver", "1.0", NULL, "0-HA1",
    NULL, post_auth_open_sent, user_data);
}

static void
success_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  gboolean ok;
  TestSaslAuthServer *tsas = TEST_SASL_AUTH_SERVER (user_data);
  TestSaslAuthServerPrivate *priv = test_sasl_auth_server_get_instance_private (tsas);

  ok = wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);

  if (priv->problem == SERVER_PROBLEM_SCRAMBLED_BINDING && priv->task)
    {
      GTask *t = priv->task;

      priv->task = NULL;

      if (priv->cancellable != NULL)
        g_object_unref (priv->cancellable);

      priv->cancellable = NULL;

      g_task_return_boolean (t, TRUE);
      g_object_unref (t);
      return;
    }

  wocky_xmpp_connection_reset (WOCKY_XMPP_CONNECTION (source));

  wocky_xmpp_connection_recv_open_async (WOCKY_XMPP_CONNECTION (source),
    NULL, post_auth_open_received, user_data);
}

static void
auth_succeeded (TestSaslAuthServer *self, const gchar *challenge)
{
  TestSaslAuthServerPrivate *priv = self->priv;
  WockyStanza *s;

  g_assert_cmpint (priv->state, <, AUTH_STATE_AUTHENTICATED);
  priv->state = AUTH_STATE_AUTHENTICATED;

  s = wocky_stanza_new ("success", WOCKY_XMPP_NS_SASL_AUTH);
  wocky_node_set_content (wocky_stanza_get_top_node (s), challenge);

  wocky_xmpp_connection_send_stanza_async (priv->conn, s, NULL,
    success_sent, self);

  g_object_unref (s);
}

static void
failure_sent (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestSaslAuthServer *tsas = TEST_SASL_AUTH_SERVER (user_data);
  TestSaslAuthServerPrivate *priv = tsas->priv;
  GTask *t = priv->task;
  GError *error = NULL;
  gboolean ok;

  priv->task = NULL;
  ok = wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);

  if (t != NULL)
    {
      if (priv->cancellable != NULL)
        g_object_unref (priv->cancellable);

      priv->cancellable = NULL;
      g_task_return_boolean (t, TRUE);
      g_object_unref (t);
    }
  /* release the grip, we're done */
  g_object_unref (tsas);
}

static void
not_authorized (TestSaslAuthServer *self)
{
  TestSaslAuthServerPrivate *priv = self->priv;
  WockyStanza *s;

  g_assert_cmpint (priv->state, <, AUTH_STATE_AUTHENTICATED);
  priv->state = AUTH_STATE_AUTHENTICATED;

  s = wocky_stanza_build (WOCKY_STANZA_TYPE_FAILURE,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
      '(', "not-authorized", ')',
    NULL);
  /* Ensure we have something to handle the callback at the end of the test */
  g_object_ref (self);
  wocky_xmpp_connection_send_stanza_async (priv->conn, s, priv->cancellable,
    failure_sent, self);

  g_object_unref (s);
}

/* check if the return of the sasl function  was as expected, if not FALSE is
 * returned and the call function should stop processing */
static gboolean
check_sasl_return (TestSaslAuthServer *self, int ret)
{
  TestSaslAuthServerPrivate * priv = self->priv;

  switch (ret)
    {
      case SASL_BADAUTH:
        /* Bad password provided */
        g_assert_true (priv->problem == SERVER_PROBLEM_MANGLED_BINDING_FLAG
                    || priv->problem == SERVER_PROBLEM_MANGLED_BINDING_DATA
                    || priv->problem == SERVER_PROBLEM_INVALID_PASSWORD);
        not_authorized (self);
        return FALSE;
#if SASL_VERSION_FULL <= 0x02011B
      case SASL_BADPROT:
        /* Bad password provided - scram pre #545 */
        g_assert_cmpint (priv->problem, ==, SERVER_PROBLEM_INVALID_PASSWORD);
        not_authorized (self);
        return FALSE;
#endif /* SASL_VERSION_FULL */
      case SASL_NOUSER:
        /* Unknown user */
        g_assert_cmpint (priv->problem, ==, SERVER_PROBLEM_INVALID_USERNAME);
        not_authorized (self);
        return FALSE;
      default:
        /* sasl auth should be ok */
        CHECK_SASL_RETURN (ret);
        break;
    }

  return TRUE;
}

enum
{
  BEFORE_KEY,
  INSIDE_KEY,
  AFTER_KEY,
  AFTER_EQ,
  INSIDE_VALUE,
  AFTER_VALUE,
};

/* insert space, CRLF, TAB etc at strategic locations in the challenge *
 * to make sure our challenge parser is sufficently robust             */
static gchar * space_challenge (const gchar *challenge, unsigned *len)
{
  GString *spaced = g_string_new_len (challenge, (gssize) *len);
  gchar *c;
  gchar q = '\0';
  gsize pos;
  gulong state = BEFORE_KEY;
  gchar spc[] = { ' ', '\t', '\r', '\n' };

  for (pos = 0; pos < spaced->len; pos++)
    {
      c = spaced->str + pos;

      switch (state)
        {
          case BEFORE_KEY:
            if (!g_ascii_isspace (*c) && *c != '\0' && *c != '=')
              state = INSIDE_KEY;
            break;
          case INSIDE_KEY:
            if (*c != '=')
              break;
            g_string_insert_c (spaced, pos++, spc [rand () % sizeof (spc)]);
            state = AFTER_EQ;
            break;
          case AFTER_KEY:
            if (*c != '=')
              break;
            state = AFTER_EQ;
            break;
          case AFTER_EQ:
            if (g_ascii_isspace (*c))
              break;
            q = *c;
            g_string_insert_c (spaced, pos++, spc [rand () % sizeof (spc)]);
            state = INSIDE_VALUE;
            break;
          case INSIDE_VALUE:
            if (q == '"' && *c != '"')
              break;
            if (q != '"' && !g_ascii_isspace (*c) && *c != ',')
              break;
            if (q != '"')
              {
                g_string_insert_c (spaced, pos++, spc [rand () % sizeof (spc)]);
                g_string_insert_c (spaced, ++pos, spc [rand () % sizeof (spc)]);
              }
            state = AFTER_VALUE;
            break;
          case AFTER_VALUE:
            if (*c == ',')
              {
                g_string_insert_c (spaced, pos++, spc [rand () % sizeof (spc)]);
                g_string_insert_c (spaced, ++pos, spc [rand () % sizeof (spc)]);
              }
            state = BEFORE_KEY;
            break;
          default:
            g_assert_not_reached ();
        }
    }

  *len = spaced->len;
  return g_string_free (spaced, FALSE);
}

/* insert a bogus parameter with a \" and a \\ sequence in it
 * scatter some \ characters through the " quoted challenge values */
static gchar * slash_challenge (const gchar *challenge, unsigned *len)
{
  GString *slashed = g_string_new_len (challenge, (gssize) *len);
  gchar *c;
  gchar q = '\0';
  gsize pos;
  gulong state = BEFORE_KEY;

  for (pos = 0; pos < slashed->len; pos++)
    {
      c = slashed->str + pos;

      switch (state)
        {
          case BEFORE_KEY:
            if (!g_ascii_isspace (*c) && *c != '\0' && *c != '=')
              state = INSIDE_KEY;
            break;
          case INSIDE_KEY:
            if (*c != '=')
              break;
            state = AFTER_EQ;
            break;
          case AFTER_EQ:
            if (g_ascii_isspace (*c))
              break;
            q = *c;
            state = INSIDE_VALUE;
            break;
          case INSIDE_VALUE:
            if (q == '"' && *c != '"')
              {
                if ((rand () % 3) == 0)
                  g_string_insert_c (slashed, pos++, '\\');
                break;
              }
            if (q != '"' && !g_ascii_isspace (*c) && *c != ',')
              break;
            state = AFTER_VALUE;
            break;
          case AFTER_VALUE:
            state = BEFORE_KEY;
            break;
          default:
            g_assert_not_reached ();
        }
    }

  g_string_prepend (slashed, "ignore-me = \"(a slash \\\\ a quote \\\")\", ");

  *len = slashed->len;
  return g_string_free (slashed, FALSE);
}

typedef struct {
  TestSaslAuthServer *srv;
  gchar *challenge;
  gboolean complete;
} ScramRes;

static void
handle_auth_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  WockySaslScram *scram = WOCKY_SASL_SCRAM (source);
  GError *error = NULL;
  ScramRes *res = data;
  TestSaslAuthServerPrivate *priv = test_sasl_auth_server_get_instance_private (res->srv);
  gchar *username = NULL;

  g_object_get (source, "username", &username, NULL);
  if (priv->problem == SERVER_PROBLEM_INVALID_USERNAME)
    {
      g_assert_cmpstr (username, !=, priv->username);
    }
  else
    {
      g_assert_cmpstr (username, ==, priv->username);
      g_object_set (source, "password", priv->password, NULL);
    }
  g_free (username);

  res->challenge = wocky_sasl_scram_server_start_finish (scram, result, &error);

  if (priv->problem == SERVER_PROBLEM_INVALID_USERNAME)
    {
      g_assert_error (error, WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_CREDENTIALS);
      g_assert_null (res->challenge);
      g_error_free (error);
    }
  else
    {
      g_assert_no_error (error);
      g_assert_nonnull (res->challenge);
    }

  res->complete = TRUE;
}

static void
handle_auth (TestSaslAuthServer *self, WockyStanza *stanza)
{
  TestSaslAuthServerPrivate *priv = self->priv;
  guchar *response = NULL;
  const gchar *challenge = NULL;
  unsigned challenge_len = 0;
  gsize response_len = 0;
  int ret;
  WockyNode *auth = wocky_stanza_get_top_node (stanza);
  const gchar *gjdd = NULL;

  g_free (priv->selected_mech);
  priv->selected_mech = g_strdup (wocky_node_get_attribute (
    wocky_stanza_get_top_node (stanza), "mechanism"));

  if (wocky_stanza_get_top_node (stanza)->content != NULL)
    {
      response = g_base64_decode (wocky_stanza_get_top_node (stanza)->content,
          &response_len);
    }

  g_assert_cmpint (priv->state, ==, AUTH_STATE_STARTED);
  gjdd = wocky_node_get_attribute_ns (auth,
      "client-uses-full-bind-result", WOCKY_GOOGLE_NS_AUTH);
  switch (priv->problem)
    {
      case SERVER_PROBLEM_REQUIRE_GOOGLE_JDD:
        if ((gjdd == NULL) || wocky_strdiff ("true", gjdd))
          {
            not_authorized (self);
            goto out;
          }
        break;
      case SERVER_PROBLEM_DISLIKE_GOOGLE_JDD:
        if (gjdd && !wocky_strdiff ("true", gjdd))
          {
            not_authorized (self);
            goto out;
          }
        break;
      default:
        break;
    }

  priv->state = AUTH_STATE_CHALLENGE;

  if (!wocky_strdiff ("X-TEST", priv->selected_mech))
    {
      challenge = "";
      challenge_len = 0;
      ret = wocky_strdiff ((gchar *) response, priv->password) ?
          SASL_BADAUTH : SASL_OK;
    }
  else if (!wocky_strdiff (BINDING_SCRAM_MECH, priv->selected_mech))
    {
      ScramRes res = { self, NULL, FALSE };
      GIOStream *ios = NULL;

      g_assert_nonnull (priv->conn);
      g_assert_nonnull (priv->scram);

#if G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION) > G_ENCODE_VERSION(2,66)
      g_object_get (G_OBJECT (priv->conn), "base-stream", &ios, NULL);
      if (G_IS_TLS_CONNECTION (ios))
        {
          GError *err = NULL;
          gchar *cb64 = NULL;
          GByteArray *cb = g_byte_array_new ();
          const gchar *cb_str = g_getenv ("WOCKY_CHANNEL_BINDING_TYPE");
          int g_tls_cb_t = G_TLS_CHANNEL_BINDING_TLS_UNIQUE;
          WockyTLSBindingType cb_type = WOCKY_TLS_BINDING_TLS_UNIQUE;

          /* by default it is tls-unique, but if it's something else what we
           * has some knowledge about - let's use it, otherwise we fail */
          if (g_str_has_prefix ((gchar *) response, "p="))
            {
              GEnumClass *gec = g_type_class_ref (WOCKY_TYPE_TLS_BINDING_TYPE);
              if (g_str_has_prefix ((gchar *) (response + 2), "tls-server-end-point,"))
                {
                  cb_type = WOCKY_TLS_BINDING_TLS_SERVER_END_POINT;
                  g_tls_cb_t = G_TLS_CHANNEL_BINDING_TLS_SERVER_END_POINT;
                }
              else if (g_str_has_prefix ((gchar *) (response + 2), "tls-exporter,"))
                {
                  cb_type = WOCKY_TLS_BINDING_TLS_EXPORTER;
                  g_tls_cb_t = 100500;
                }
              cb_str = g_enum_get_value (gec, cb_type)->value_nick;
              g_type_class_unref (gec);
            }

          if (!g_tls_connection_get_channel_binding_data ((GTlsConnection *) ios, g_tls_cb_t, cb, &err))
            {
              g_assert_error (err, G_TLS_CHANNEL_BINDING_ERROR,
                  G_TLS_CHANNEL_BINDING_ERROR_NOT_IMPLEMENTED);
              g_error_free (err);
              if (priv->problem == SERVER_PROBLEM_SCRAMBLED_BINDING
                  || priv->problem == SERVER_PROBLEM_MANGLED_BINDING_FLAG
                  || priv->problem == SERVER_PROBLEM_MANGLED_BINDING_DATA)
                g_test_skip ("GIO backend (glib-networking) does not support tls channel binding");
            }
          else
            {
              if (priv->problem == SERVER_PROBLEM_MANGLED_BINDING_DATA)
                cb->data[0] ^= cb->data[1];
              else if (priv->problem == SERVER_PROBLEM_MANGLED_BINDING_FLAG
                    || priv->problem == SERVER_PROBLEM_SCRAMBLED_BINDING)
                {
                  gchar *r = g_strdup_printf ("p=%s,", cb_str);
                  g_assert_true (g_str_has_prefix ((gchar *) response, r));
                  g_free (r);
                  r = g_strdup_printf ("n%s", response + 2 + strlen (cb_str));
                  g_free (response);
                  response = (guchar *) r;
                }

              cb64 = g_base64_encode (cb->data, cb->len);
              g_byte_array_unref (cb);
              g_object_set (G_OBJECT (priv->scram),
                        "cb-type", cb_type,
                        "cb-data", cb64,
                        NULL);
              g_debug ("TLS binding: %s", cb64);
              g_free (cb64);
            }
        }
      else
        g_debug ("No TLS");
#else
      (void)(ios);
#endif /* GLIB_VERSION_2_66 */

      wocky_sasl_scram_server_start_async (priv->scram, (gchar *) response,
          handle_auth_cb, priv->cancellable, &res);

      while (!res.complete)
        g_main_context_iteration (NULL, FALSE);

      if (res.challenge)
        {
          ret = 1; /* SASL_CONTINUE */
          challenge = res.challenge;
          challenge_len = strlen (challenge);
        }
      else
        ret = SASL_NOUSER;
    }
  else if (priv->scram && g_str_has_prefix (priv->selected_mech, "SCRAM-SHA-"))
    {
      ScramRes res = { self, NULL, FALSE };

      wocky_sasl_scram_server_start_async (priv->scram, (gchar *) response,
          handle_auth_cb, priv->cancellable, &res);

      while (!res.complete)
        g_main_context_iteration (NULL, FALSE);

      if (res.challenge)
        {
          ret = 1; /* SASL_CONTINUE */
          challenge = res.challenge;
          challenge_len = strlen (challenge);
        }
      else
        ret = SASL_NOUSER;
    }
  else
    {
#if HAVE_LIBSASL2
      ret = sasl_server_start (priv->sasl_conn,
          priv->selected_mech, (gchar *) response,
          (unsigned) response_len, &challenge, &challenge_len);
#else
      challenge = "";
      challenge_len = 0;
      g_assert_cmpstr ("PLAIN", ==, priv->selected_mech);
      /* response format: ^@ u s e r ^@ p a s s    */
      /* require at least 1 char user and password */
      if (response_len >= 4)
        {
          const gchar *user = ((gchar *) response) + 1;
          int ulen = strlen (user);
          gchar *pass = g_strndup (user + ulen + 1, response_len - ulen - 2);
          ret = ( wocky_strdiff (user, priv->username) ? SASL_NOUSER  :
                  wocky_strdiff (pass, priv->password) ? SASL_BADAUTH : SASL_OK );
          g_free (pass);
        }
      else
        ret = SASL_BADAUTH;
#endif
    }

  if (!check_sasl_return (self, ret))
    goto out;

  if (challenge_len > 0)
    {
      WockyStanza *c;
      gchar *challenge64;

      if (ret == SASL_OK)
        {
          priv->state = AUTH_STATE_FINAL_CHALLENGE;
        }

      if (priv->problem == SERVER_PROBLEM_SPACE_CHALLENGE)
        {
          unsigned slen = challenge_len;
          gchar *spaced = space_challenge (challenge, &slen);
          challenge64 = g_base64_encode ((guchar *) spaced, slen);
          g_free (spaced);
        }
      else if (priv->problem == SERVER_PROBLEM_SLASH_CHALLENGE)
        {
          unsigned slen = challenge_len;
          gchar *slashc = slash_challenge (challenge, &slen);
          challenge64 = g_base64_encode ((guchar *) slashc, slen);
          g_free (slashc);
        }
      else
        {
          challenge64 = g_base64_encode ((guchar *) challenge, challenge_len);
        }

      c = wocky_stanza_new ("challenge", WOCKY_XMPP_NS_SASL_AUTH);
      wocky_node_set_content (wocky_stanza_get_top_node (c), challenge64);
      wocky_xmpp_connection_send_stanza_async (priv->conn, c,
        NULL, NULL, NULL);
      g_object_unref (c);

      g_free (challenge64);
    }
  else if (ret == SASL_OK)
    {
      auth_succeeded (self, NULL);
    }
  else
    {
      g_assert_not_reached ();
    }

out:
  g_free (response);
}

static void
handle_response_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      data)
{
  WockySaslScram *scram = WOCKY_SASL_SCRAM (source);
  GError *error = NULL;
  ScramRes *res = data;
  TestSaslAuthServerPrivate *priv = test_sasl_auth_server_get_instance_private (res->srv);


  res->challenge = wocky_sasl_scram_server_step_finish (scram, result, &error);

  if (priv->problem == SERVER_PROBLEM_INVALID_PASSWORD)
    {
      g_assert_error (error, WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED);
      g_assert_null (res->challenge);
    }
  else if (priv->problem == SERVER_PROBLEM_MANGLED_BINDING_DATA)
    {
      if (!g_test_failed ())
        {
          g_assert_error (error, WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE);
          g_assert_null (res->challenge);
        }
      else
        {
          g_clear_pointer (&res->challenge, g_free);
          g_assert_no_error (error);
        }
    }
  else if (priv->problem == SERVER_PROBLEM_MANGLED_BINDING_FLAG)
    {
      if (!g_test_failed ())
        {
          g_assert_error (error, WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE);
          g_assert_null (res->challenge);
        }
      else
        {
          g_clear_pointer (&res->challenge, g_free);
          g_assert_no_error (error);
        }
    }
  else if (priv->problem == SERVER_PROBLEM_SCRAMBLED_BINDING)
    {
      if (!g_test_failed ())
        {
          g_assert_error (error, WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED);
          g_assert_null (res->challenge);
        }
      else
        {
          g_clear_pointer (&res->challenge, g_free);
          g_assert_no_error (error);
        }

      /* Now pretend it didn't happen and provide phony server verification */
      res->challenge = g_strdup_printf ("v=%s",
          "QRx7t5sNW98H5eXodJPJKiQKZbEJBb1XKRnK9hb1ON+WU5/D4z/9wBtG1J2OKGt/gHdDzblO7BYbBhHVb6CXtg==");
    }
  else
    {
      g_assert_no_error (error);
      g_assert_nonnull (res->challenge);
    }

  res->complete = TRUE;
}

static void
handle_response (TestSaslAuthServer *self, WockyStanza *stanza)
{
  TestSaslAuthServerPrivate * priv = self->priv;
  guchar *response = NULL;
  const gchar *challenge = NULL;
  unsigned challenge_len = 0;
  gsize response_len = 0;
  int ret;

  if (priv->state == AUTH_STATE_FINAL_CHALLENGE)
    {
      g_assert (wocky_stanza_get_top_node (stanza)->content == NULL);
      auth_succeeded (self, NULL);
      return;
    }

  g_assert_cmpint (priv->state, ==, AUTH_STATE_CHALLENGE);

  if (wocky_stanza_get_top_node (stanza)->content != NULL)
    {
      response = g_base64_decode (wocky_stanza_get_top_node (stanza)->content,
          &response_len);
    }

  if (!wocky_strdiff (BINDING_SCRAM_MECH, priv->selected_mech))
    {
      ScramRes res = { self, NULL, FALSE };

      g_assert_nonnull (priv->scram);

      if (priv->problem == SERVER_PROBLEM_SCRAMBLED_BINDING)
        {
          gchar *r, *rd = g_strstr_len ((gchar *) response, -1, ",");

          g_assert_true (response[0] == 'c' && response[1] == '=' && rd != NULL);

          r = g_strdup_printf ("c=biws,%s", rd + 1);
          g_free (response);
          response = (guchar *) r;
        }

      wocky_sasl_scram_server_step_async (priv->scram, (gchar *) response,
          handle_response_cb, priv->cancellable, &res);

      while (!res.complete)
        g_main_context_iteration (NULL, FALSE);

      if (res.challenge)
        {
          ret = SASL_OK;
          challenge = res.challenge;
          challenge_len = strlen (challenge);
        }
      else
        ret = SASL_BADAUTH;
    }
  else if (priv->scram && g_str_has_prefix (priv->selected_mech, "SCRAM-SHA-"))
    {
      ScramRes res = { self, NULL, FALSE };

      wocky_sasl_scram_server_step_async (priv->scram, (gchar *) response,
          handle_response_cb, priv->cancellable, &res);

      while (!res.complete)
        g_main_context_iteration (NULL, FALSE);

      if (res.challenge)
        {
          ret = SASL_OK;
          challenge = res.challenge;
          challenge_len = strlen (challenge);
        }
      else
        ret = SASL_BADAUTH;
    }
  else
    {
#ifdef HAVE_LIBSASL2
  ret = sasl_server_step (priv->sasl_conn, (gchar *) response,
      (unsigned) response_len, &challenge, &challenge_len);
#else
  ret = SASL_OK;
  challenge_len = 0;
  challenge = "";
#endif
    }

  if (!check_sasl_return (self, ret))
    goto out;

  if (challenge_len > 0)
    {
      WockyStanza *c;
      gchar *challenge64;

      if (ret == SASL_OK)
        {
          priv->state = AUTH_STATE_FINAL_CHALLENGE;
        }

      if (priv->problem == SERVER_PROBLEM_SPACE_CHALLENGE)
        {
          unsigned slen = challenge_len;
          gchar *spaced = space_challenge (challenge, &slen);
          challenge64 = g_base64_encode ((guchar *) spaced, slen);
          g_free (spaced);
        }
      else if (priv->problem == SERVER_PROBLEM_SLASH_CHALLENGE)
        {
          unsigned slen = challenge_len;
          gchar *slashc = slash_challenge (challenge, &slen);
          challenge64 = g_base64_encode ((guchar *) slashc, slen);
          g_free (slashc);
        }
      else
        {
          challenge64 = g_base64_encode ((guchar *) challenge, challenge_len);
        }

      if (priv->state == AUTH_STATE_FINAL_CHALLENGE &&
          (priv->problem == SERVER_PROBLEM_FINAL_DATA_IN_SUCCESS
           || priv->problem == SERVER_PROBLEM_SCRAMBLED_BINDING))
        {
          auth_succeeded (self, challenge64);
        }
      else
        {
          c = wocky_stanza_new ("challenge", WOCKY_XMPP_NS_SASL_AUTH);
          wocky_node_set_content (wocky_stanza_get_top_node (c),
              challenge64);
          wocky_xmpp_connection_send_stanza_async (priv->conn, c,
            NULL, NULL, NULL);
          g_object_unref (c);
        }
      g_free (challenge64);
    }
  else if (ret == SASL_OK)
    {
      auth_succeeded (self, NULL);
    }
  else
    {
      g_assert_not_reached ();
    }

out:
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
  WockyStanza *stanza;
  GError *error = NULL;
  struct {
    const gchar *name;
    void (*func)(TestSaslAuthServer *self, WockyStanza *stanza);
  } handlers[] = { HANDLE(auth), HANDLE(response), { NULL, NULL } };

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
  priv = self->priv;

  g_assert (stanza != NULL);

  if (wocky_strdiff (wocky_node_get_ns (
        wocky_stanza_get_top_node (stanza)), WOCKY_XMPP_NS_SASL_AUTH))
    {
      g_assert_not_reached ();
    }

  for (i = 0 ; handlers[i].name != NULL; i++)
    {
      if (!wocky_strdiff (wocky_stanza_get_top_node (stanza)->name,
          handlers[i].name))
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

#ifdef HAVE_LIBSASL2
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
    { "scram_secret_generate", "1"},
    { NULL, NULL },
  };
  *result = NULL;

  for (i = 0; options[i].name != NULL; i++)
    {
      if (!wocky_strdiff (option, options[i].name))
        {
          *result = options[i].value;
          if (len != NULL)
            *len = strlen (options[i].value);
        }
    }

  return SASL_OK;
}
#endif

TestSaslAuthServer *
test_sasl_auth_server_new (GIOStream *stream, gchar *mech,
    const gchar *user, const gchar *password,
    const gchar *servername, ServerProblem problem,
    gboolean start)
{
  TestSaslAuthServer *server;
  TestSaslAuthServerPrivate *priv;

#ifdef HAVE_LIBSASL2
  static gboolean sasl_initialized = FALSE;
  int ret;
  static sasl_callback_t callbacks[] = {
    { SASL_CB_LOG, (sasl_callback_ft) test_sasl_server_auth_log, NULL },
    { SASL_CB_GETOPT, (sasl_callback_ft) test_sasl_server_auth_getopt, NULL },
    { SASL_CB_LIST_END, NULL, NULL },
  };

  if (!sasl_initialized)
    {
      sasl_server_init (NULL, NULL);
      sasl_initialized = TRUE;
    }
#endif

  server = g_object_new (TEST_TYPE_SASL_AUTH_SERVER, NULL);
  priv = server->priv;

  priv->state = AUTH_STATE_STARTED;

#ifdef HAVE_LIBSASL2
  ret = sasl_server_new ("xmpp", servername, NULL, NULL, NULL, callbacks,
      SASL_SUCCESS_DATA, &(priv->sasl_conn));
  CHECK_SASL_RETURN (ret);

  ret = sasl_setpass (priv->sasl_conn, user, password, strlen (password),
      NULL, 0, SASL_SET_CREATE);

  CHECK_SASL_RETURN (ret);
#endif

  priv->username = g_strdup (user);
  priv->password = g_strdup (password);
  priv->mech = g_strdup (mech);
  priv->problem = problem;

  if (!wocky_strdiff (BINDING_SCRAM_MECH, mech))
    {
      priv->scram = g_object_new (WOCKY_TYPE_SASL_SCRAM, "server", servername,
          "hash-algo", BINDING_SCRAM_ALGO, NULL);
    }

  if (start)
    {
      priv->stream = g_object_ref (stream);
      priv->conn = wocky_xmpp_connection_new (stream);
      priv->cancellable = g_cancellable_new ();
      wocky_xmpp_connection_recv_open_async (priv->conn,
          priv->cancellable, stream_open_received, server);
    }

  return server;
}

void
test_sasl_auth_server_stop (TestSaslAuthServer *self)
{
  TestSaslAuthServerPrivate *priv = self->priv;

  if (priv->cancellable != NULL)
    {
      test_cancel_in_idle (priv->cancellable);
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  if (priv->conn != NULL)
    g_object_unref (priv->conn);

  priv->conn = NULL;
}

gboolean
test_sasl_auth_server_auth_finish (TestSaslAuthServer *self,
    GAsyncResult *res,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  g_return_val_if_fail (g_task_propagate_boolean (G_TASK (res), error), FALSE);

  return (self->priv->state == AUTH_STATE_AUTHENTICATED);
}

void
test_sasl_auth_server_auth_async (GObject *obj,
    WockyXmppConnection *conn,
    WockyStanza *auth,
    GAsyncReadyCallback cb,
    GCancellable *cancellable,
    gpointer data)
{
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (obj);
  TestSaslAuthServerPrivate *priv = self->priv;

  /* We expect the server to not be started but just in case */
  test_sasl_auth_server_stop (TEST_SASL_AUTH_SERVER (obj));

  priv->state = AUTH_STATE_STARTED;
  priv->conn = g_object_ref (conn);

  /* save the details of the point ot which we will hand back control */
  if (cb != NULL)
    {
      if (cancellable != NULL)
        priv->cancellable = g_object_ref (cancellable);

      priv->task = g_task_new (obj, cancellable, cb, data);
    }

  handle_auth (self, auth);
  if (priv->state < AUTH_STATE_AUTHENTICATED)
    {
      wocky_xmpp_connection_recv_stanza_async (priv->conn,
          priv->cancellable, received_stanza, self);
    }
  g_object_unref (auth);
}

gint
test_sasl_auth_server_set_mechs (GObject *obj,
    WockyStanza *feat,
    const gchar *must)
{
  int ret = 0;
  TestSaslAuthServer *self = TEST_SASL_AUTH_SERVER (obj);
  TestSaslAuthServerPrivate *priv = self->priv;
  WockyNode *mechnode = NULL;
  const gchar *mech = (must) ? must : priv->mech;
  gboolean hazmech = FALSE;

  if (priv->problem != SERVER_PROBLEM_NO_SASL)
    {
      mechnode = wocky_node_add_child_ns (
        wocky_stanza_get_top_node (feat),
          "mechanisms", WOCKY_XMPP_NS_SASL_AUTH);
      if (priv->problem == SERVER_PROBLEM_NO_MECHANISMS)
        {
          /* lalala */
        }
      else
        {
          const gchar *mechs;
          gchar **mechlist;
          gchar **tmp;

#ifdef HAVE_LIBSASL2
          ret = sasl_listmech (priv->sasl_conn, NULL, "","\n","", &mechs,
              NULL,NULL);
          CHECK_SASL_RETURN (ret);
#else
          mechs = "PLAIN";
#endif

          mechlist = g_strsplit (mechs, "\n", -1);
          for (tmp = mechlist; *tmp != NULL; tmp++)
            {
              if (priv->mech && wocky_strdiff (priv->mech, *tmp))
                continue;

              wocky_node_add_child_with_content (mechnode,
                "mechanism", *tmp);

              if (!hazmech && !wocky_strdiff (*tmp, mech))
                hazmech = TRUE;
            }
          g_strfreev (mechlist);

          if (!hazmech && mech != NULL
              && g_str_has_prefix (mech, "SCRAM-SHA-"))
            {
              /* as said before, this is ridiculous so let's fix that */
              if (g_str_has_prefix (mech, "SCRAM-SHA-256"))
                {
                  if (priv->scram == NULL)
                    priv->scram = g_object_new (WOCKY_TYPE_SASL_SCRAM,
                        "server", "whatever",
                        "hash-algo", G_CHECKSUM_SHA256,
                        NULL);
                  wocky_node_add_child_with_content (mechnode,
                      "mechanism", mech);
                }
              else if (g_str_has_prefix (mech, "SCRAM-SHA-512"))
                {
                  if (priv->scram == NULL)
                    priv->scram = g_object_new (WOCKY_TYPE_SASL_SCRAM,
                        "server", "whatever",
                        "hash-algo", G_CHECKSUM_SHA512,
                        NULL);
                  wocky_node_add_child_with_content (mechnode,
                      "mechanism", mech);
                }
            }
          else if (!hazmech && priv->mech)
            {
              wocky_node_add_child_with_content (mechnode,
                  "mechanism", priv->mech);
            }
        }
    }
  return ret;
}

const gchar *
test_sasl_auth_server_get_selected_mech (TestSaslAuthServer *self)
{
  TestSaslAuthServerPrivate *priv = self->priv;

  return priv->selected_mech;
}
