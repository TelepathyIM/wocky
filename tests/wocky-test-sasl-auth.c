#include <stdio.h>

#include "wocky-test-sasl-auth-server.h"
#include "wocky-test-stream.h"

#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-sasl-auth.h>

typedef struct {
  gchar *description;
  gchar *mech;
  gboolean allow_plain;
  GQuark domain;
  int code;
  ServerProblem problem;
} test_t;

GMainLoop *mainloop;
WockyXmppConnection *conn;
WockySaslAuth *sasl = NULL;

const gchar *username = "test";
const gchar *password = "test123";
const gchar *servername = "testserver";

gboolean authenticated = FALSE;
gboolean run_done = FALSE;

test_t *current_test = NULL;
GError *error = NULL;

static void
got_error (GQuark domain, int code, const gchar *message)
{
  g_set_error (&error, domain, code, "%s", message);
  run_done = TRUE;
  g_main_loop_quit (mainloop);
}

static gchar *
return_str (WockySaslAuth *auth, gpointer user_data)
{
  return g_strdup (user_data);
}

static void
auth_success (WockySaslAuth *sasl_, gpointer user_data)
{
  authenticated = TRUE;
  /* Reopen the connection */
  wocky_xmpp_connection_restart (conn);
  wocky_xmpp_connection_open (conn, servername, NULL, "1.0");
}

static void
auth_failed (WockySaslAuth *sasl_, GQuark domain,
    int code, gchar *message, gpointer user_data)
{
  got_error (domain, code, message);
}

static void
parse_error (WockyXmppConnection *connection, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
stream_opened (WockyXmppConnection *connection,
              gchar *from, gchar *to, gchar *version, gpointer user_data)
{
  if (authenticated)
    wocky_xmpp_connection_close (conn);
}

static void
stream_closed (WockyXmppConnection *connection, gpointer user_data)
{
  run_done = TRUE;
  g_main_loop_quit (mainloop);
}

static void
received_stanza (WockyXmppConnection *connection, WockyXmppStanza *stanza,
   gpointer user_data)
{

  if (sasl == NULL)
    {
      GError *err = NULL;
      sasl = wocky_sasl_auth_new ();

      g_signal_connect (sasl, "username-requested",
          G_CALLBACK (return_str), (gpointer)username);
      g_signal_connect (sasl, "password-requested",
          G_CALLBACK (return_str), (gpointer)password);
      g_signal_connect (sasl, "authentication-succeeded",
          G_CALLBACK (auth_success), NULL);
      g_signal_connect (sasl, "authentication-failed",
          G_CALLBACK (auth_failed), NULL);

      if (!wocky_sasl_auth_authenticate (sasl, servername, connection, stanza,
          current_test->allow_plain, &err))
        {
          got_error (err->domain, err->code, err->message);
          g_error_free (err);
        }
    }
}

static void
run_test (gconstpointer user_data)
{
  TestSaslAuthServer *server;
  WockyTestStream *stream;
  test_t *test = (test_t *)user_data;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);

  server = test_sasl_auth_server_new (stream->stream0, test->mech, username,
      password, test->problem);

  authenticated = FALSE;
  run_done = FALSE;
  current_test = test;

  conn = wocky_xmpp_connection_new (stream->stream1);

  g_signal_connect (conn, "parse-error", G_CALLBACK (parse_error), NULL);
  g_signal_connect (conn, "stream-opened", G_CALLBACK (stream_opened), NULL);
  g_signal_connect (conn, "stream-closed", G_CALLBACK (stream_closed), NULL);
  g_signal_connect (conn, "received-stanza",
      G_CALLBACK (received_stanza), NULL);
  wocky_xmpp_connection_open (conn, servername, NULL, "1.0");

  if (!run_done)
    {
      g_main_loop_run (mainloop);
    }

  if (sasl != NULL)
    {
      g_object_unref (sasl);
      sasl = NULL;
    }

  g_object_unref (stream);
  g_object_unref (conn);

  if (test->domain == 0)
    g_assert (error == NULL);
  else
    g_assert (g_error_matches (error, test->domain, test->code));

  if (error != NULL)
    g_error_free (error);

  error = NULL;
}

#define SUCCESS(desc, mech, allow_plain)                 \
 { desc, mech, allow_plain, 0, 0, SERVER_PROBLEM_NO_PROBLEM }

#define NUMBER_OF_TEST 7

int
main (int argc,
    char **argv)
{
  test_t tests[NUMBER_OF_TEST] = {
    SUCCESS("/xmpp-sasl/normal-auth", NULL, TRUE),
    SUCCESS("/xmpp-sasl/no-plain", NULL, FALSE),
    SUCCESS("/xmpp-sasl/only-plain", "PLAIN", TRUE),
    SUCCESS("/xmpp-sasl/only-digest-md5", "DIGEST-MD5", TRUE),

    { "/xmpp-sasl/no-supported-mechs", "NONSENSE", TRUE,
       WOCKY_SASL_AUTH_ERROR, WOCKY_SASL_AUTH_ERROR_NO_SUPPORTED_MECHANISMS,
       SERVER_PROBLEM_NO_PROBLEM },
    { "/xmpp-sasl/refuse-plain-only", "PLAIN", FALSE,
       WOCKY_SASL_AUTH_ERROR, WOCKY_SASL_AUTH_ERROR_NO_SUPPORTED_MECHANISMS,
       SERVER_PROBLEM_NO_PROBLEM },
    { "/xmpp-sasl/no-sasl-support", NULL, TRUE,
       WOCKY_SASL_AUTH_ERROR, WOCKY_SASL_AUTH_ERROR_SASL_NOT_SUPPORTED,
       SERVER_PROBLEM_NO_SASL },
  };

  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  mainloop = g_main_loop_new (NULL, FALSE);

  for (int i = 0; i < NUMBER_OF_TEST; i++)
    g_test_add_data_func (tests[i].description,
      &tests[i], run_test);

  return g_test_run ();
}
