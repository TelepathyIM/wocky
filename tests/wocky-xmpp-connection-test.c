#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-xmpp-stanza.h>
#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

#define SIMPLE_MESSAGE \
"<?xml version='1.0' encoding='UTF-8'?>                                    " \
"<stream:stream xmlns='jabber:client'                                      " \
"  xmlns:stream='http://etherx.jabber.org/streams'>                        " \
"  <message to='juliet@example.com' from='romeo@example.net' xml:lang='en' " \
"   id=\"0\">                                                              " \
"    <body>Art thou not Romeo, and a Montague?</body>                      " \
"  </message>                                                              " \
"</stream:stream>"

static void
test_instantiation (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);

  g_assert (connection != NULL);

  g_object_unref (connection);
  g_object_unref (stream);
}

/* Simple message test */
static void
stanza_received_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source);
  WockyXmppStanza *s;
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);

  if (!data->parsed_stanza)
    {
      g_assert (s != NULL);
      data->parsed_stanza = TRUE;
      wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (source),
        NULL, stanza_received_cb, data);

      g_object_unref (s);
    }
  else
   {
      g_assert (s == NULL);
      g_main_loop_quit (data->loop);
      g_error_free (error);
   }
}

static void
received_open_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  WockyXmppConnection *conn = WOCKY_XMPP_CONNECTION (source);

  if (!wocky_xmpp_connection_recv_open_finish (conn, res,
      NULL, NULL, NULL, NULL, NULL))
    g_assert_not_reached ();

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (source),
    NULL, stanza_received_cb, user_data);
}

#define CHUNK_SIZE 13

static void
test_recv_simple_message (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  gsize len;
  gsize offset = 0;
  gchar message[] = SIMPLE_MESSAGE;
  GMainLoop *loop = NULL;
  test_data_t data = { NULL, FALSE };

  loop = g_main_loop_new (NULL, FALSE);

  len = strlen (message);

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);

  g_timeout_add (1000, test_timeout_cb, NULL);

  data.loop = loop;
  wocky_xmpp_connection_recv_open_async (connection,
      NULL, received_open_cb, &data);

  while (offset < len)
    {
      guint l = MIN (len - offset, CHUNK_SIZE);

      while (g_main_context_iteration (NULL, FALSE))
        ;
      g_output_stream_write_all (stream->stream1_output,
        message + offset, l, NULL, NULL, NULL);
      offset += l;

    }

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_object_unref (stream);
  g_object_unref (connection);
}

/* simple send message testing */
static void
send_stanza_received_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source);
  WockyXmppStanza *s;
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);
  g_assert (s != NULL);

  g_assert (!data->parsed_stanza);
  data->parsed_stanza = TRUE;

  g_object_unref (s);
  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
send_stanza_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  g_assert (wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_send_simple_message (void)
{
  WockyXmppStanza *s;
  test_data_t *test = setup_test ();

  test_open_connection (test);

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
      WOCKY_NODE, "html", WOCKY_NODE_XMLNS, "http://www.w3.org/1999/xhtml",
        WOCKY_NODE, "body",
          WOCKY_NODE_TEXT, "Art thou not Romeo, and a Montague?",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  wocky_xmpp_connection_send_stanza_async (WOCKY_XMPP_CONNECTION (test->in),
    s, NULL, send_stanza_cb, test);

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (test->out),
    NULL, send_stanza_received_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_connection (test);
  g_object_unref (s);
  teardown_test (test);
}

/* Test for various error codes */
static void
error_pending_open_received_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_recv_open_finish (
      WOCKY_XMPP_CONNECTION (source), result,
      NULL, NULL, NULL, NULL, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_pending_recv_open_pending_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_recv_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL, NULL, NULL, NULL, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_PENDING));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  g_error_free (error);
}

static void
error_pending_stanza_received_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *s;

  g_assert ((s = wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL)) != NULL);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  g_object_unref (s);
}

static void
error_pending_recv_stanza_pending_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error) == NULL);

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_PENDING));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  g_error_free (error);
}


static void
error_pending_open_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_pending_open_pending_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_PENDING));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  g_error_free (error);
}

static void
error_pending_stanza_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_pending_stanza_pending_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_PENDING));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  g_error_free (error);
}

static void
error_pending_close_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_pending_close_pending_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_PENDING));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  g_error_free (error);
}

static void
test_error_pending_send_pending (test_data_t *test)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "a"," b", WOCKY_STANZA_END);

  /* should get a _PENDING error */
  wocky_xmpp_connection_send_open_async (test->in, NULL, NULL, NULL, NULL,
    NULL, error_pending_open_pending_cb, test);

  /* should get a _PENDING error */
  wocky_xmpp_connection_send_stanza_async (test->in, stanza, NULL,
    error_pending_stanza_pending_cb, test);

  /* should get a _PENDING error */
  wocky_xmpp_connection_send_close_async (test->in, NULL,
    error_pending_close_pending_cb, test);

  test->outstanding += 3;

  g_object_unref (stanza);
}

static void
test_error_pending (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "a"," b", WOCKY_STANZA_END);

  wocky_xmpp_connection_recv_open_async (test->out, NULL,
    error_pending_open_received_cb, test);
  test->outstanding++;

  wocky_xmpp_connection_recv_open_async (test->out, NULL,
    error_pending_open_pending_cb, test);
  test->outstanding++;

  g_main_loop_run (test->loop);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
    error_pending_recv_stanza_pending_cb, test);
  test->outstanding++;

  g_main_loop_run (test->loop);

  /* Should succeed */
  wocky_xmpp_connection_send_open_async (test->in, NULL, NULL, NULL, NULL,
    NULL, error_pending_open_sent_cb, test);
  test->outstanding++;

  test_error_pending_send_pending (test);
  test_wait_pending (test);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
    error_pending_stanza_received_cb, test);
  test->outstanding++;

  wocky_xmpp_connection_recv_open_async (test->out, NULL,
    error_pending_recv_open_pending_cb, test);
  test->outstanding++;

  g_main_loop_run (test->loop);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
    error_pending_recv_stanza_pending_cb, test);
  test->outstanding++;

  g_main_loop_run (test->loop);

  /* should succeed */
  wocky_xmpp_connection_send_stanza_async (test->in, stanza, NULL,
    error_pending_stanza_sent_cb, test);
  test->outstanding++;

  test_error_pending_send_pending (test);
  test_wait_pending (test);

  /* should succeed */
  wocky_xmpp_connection_send_close_async (test->in, NULL,
    error_pending_close_sent_cb, test);
  test->outstanding++;

  test_error_pending_send_pending (test);
  test_wait_pending (test);

  teardown_test (test);
  g_object_unref (stanza);
}

/* not open errors */
static void
error_not_open_send_stanza_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_not_open_send_close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_not_open_recv_stanza_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error) == NULL);

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_error_not_open (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "a"," b", WOCKY_STANZA_END);

  wocky_xmpp_connection_send_stanza_async (test->in, stanza, NULL,
    error_not_open_send_stanza_cb, test);

  wocky_xmpp_connection_send_close_async (test->in, NULL,
    error_not_open_send_close_cb, test);

  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
    error_not_open_recv_stanza_cb, test);

  test->outstanding = 3;
  test_wait_pending (test);

  teardown_test (test);
  g_object_unref (stanza);
}

/* is open tests */
static void
error_is_open_send_open_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_OPEN));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
is_open_send_open_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL));

  wocky_xmpp_connection_send_open_async (WOCKY_XMPP_CONNECTION (source),
    NULL, NULL, NULL, NULL,
    NULL, error_is_open_send_open_cb, user_data);
}

static void
error_is_open_recv_open_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_recv_open_finish (
      WOCKY_XMPP_CONNECTION (source), result,
      NULL, NULL, NULL, NULL,
      &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_OPEN));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
is_open_recv_open_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  g_assert (wocky_xmpp_connection_recv_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, NULL, NULL, NULL, NULL, NULL));

  wocky_xmpp_connection_recv_open_async (WOCKY_XMPP_CONNECTION (source),
    NULL, error_is_open_recv_open_cb, user_data);
}

static void
is_open_send_close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result,
      NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
is_open_recv_close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result,
      &error) == NULL);

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_CLOSED));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_is_closed_send_open_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_open_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_is_closed_send_stanza_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_is_closed_send_close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_send_close_finish (
      WOCKY_XMPP_CONNECTION (source), result, &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_is_closed_recv_open_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_connection_recv_open_finish (
      WOCKY_XMPP_CONNECTION (source), result,
      NULL, NULL, NULL, NULL,
      &error));

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
error_is_closed_recv_stanza_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), result,
      &error) == NULL);

  g_assert (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED));
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_error_is_open_or_closed (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "a"," b", WOCKY_STANZA_END);


  wocky_xmpp_connection_send_open_async (WOCKY_XMPP_CONNECTION (test->in),
    NULL, NULL, NULL, NULL,
    NULL, is_open_send_open_cb, test);

  wocky_xmpp_connection_recv_open_async (WOCKY_XMPP_CONNECTION (test->out),
    NULL, is_open_recv_open_cb, test);

  test->outstanding = 2;
  test_wait_pending (test);

  /* Input and output side are open, so they can be closed */
  wocky_xmpp_connection_send_close_async (WOCKY_XMPP_CONNECTION (test->in),
    NULL, is_open_send_close_cb, test);
  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (test->out),
    NULL, is_open_recv_close_cb, test);

  test->outstanding = 2;
  test_wait_pending (test);

  /* both sides are closed, all calls should yield _IS_CLOSED errors */
  wocky_xmpp_connection_send_open_async (WOCKY_XMPP_CONNECTION (test->in),
    NULL, NULL, NULL, NULL,
    NULL, error_is_closed_send_open_cb, test);

  wocky_xmpp_connection_send_stanza_async (WOCKY_XMPP_CONNECTION (test->in),
    stanza, NULL, error_is_closed_send_stanza_cb, test);

  wocky_xmpp_connection_send_close_async (WOCKY_XMPP_CONNECTION (test->in),
    NULL, error_is_closed_send_close_cb, test);

  wocky_xmpp_connection_recv_open_async (WOCKY_XMPP_CONNECTION (test->out),
    NULL, error_is_closed_recv_open_cb, test);

  wocky_xmpp_connection_recv_stanza_async (WOCKY_XMPP_CONNECTION (test->out),
    NULL, error_is_closed_recv_stanza_cb, test);

  test->outstanding = 5;
  test_wait_pending (test);


  teardown_test (test);
  g_object_unref (stanza);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-connection/initiation", test_instantiation);
  g_test_add_func ("/xmpp-connection/recv-simple-message",
    test_recv_simple_message);
  g_test_add_func ("/xmpp-connection/send-simple-message",
    test_send_simple_message);
  g_test_add_func ("/xmpp-connection/error-pending", test_error_pending);
  g_test_add_func ("/xmpp-connection/error-not-open", test_error_not_open);
  g_test_add_func ("/xmpp-connection/error-is-open-or-closed",
    test_error_is_open_or_closed);
  return g_test_run ();
}
