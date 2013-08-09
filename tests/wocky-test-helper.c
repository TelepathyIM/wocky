#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <wocky/wocky.h>

#include "wocky-test-helper.h"
#include "wocky-test-stream.h"

#define TIMEOUT 10

gboolean
test_timeout_cb (gpointer data)
{
  g_test_message ("Timeout reached :(");
  g_assert_not_reached ();

  return FALSE;
}

static test_data_t *
setup_test_full (guint timeout,
    const gchar *in_jid,
    const gchar *out_jid)
{
  test_data_t *data;

  data = g_new0 (test_data_t, 1);
  data->loop = g_main_loop_new (NULL, FALSE);

  data->stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  data->in = wocky_xmpp_connection_new (data->stream->stream0);
  data->out = wocky_xmpp_connection_new (data->stream->stream1);

  data->session_in = wocky_session_new_with_connection (data->in, in_jid);
  data->session_out = wocky_session_new_with_connection (data->out, out_jid);

  data->sched_in = wocky_session_get_porter (data->session_in);
  data->sched_out = wocky_session_get_porter (data->session_out);

  data->expected_stanzas = g_queue_new ();

  data->cancellable = g_cancellable_new ();
  data->timeout_id = g_timeout_add_seconds (timeout, test_timeout_cb, NULL);

  return data;
}

test_data_t *
setup_test_with_jids (const gchar *in_jid,
    const gchar *out_jid)
{
  return setup_test_full (TIMEOUT, in_jid, out_jid);
}

test_data_t *
setup_test_with_timeout (guint timeout)
{
  return setup_test_full (timeout, "example.com", "example.com");
}

test_data_t *
setup_test (void)
{
  return setup_test_full (TIMEOUT, "example.com", "example.com");
}

void
teardown_test (test_data_t *data)
{
  g_main_loop_unref (data->loop);
  g_object_unref (data->stream);
  g_object_unref (data->in);
  g_object_unref (data->out);
  if (data->session_in != NULL)
    g_object_unref (data->session_in);
  if (data->session_out != NULL)
    g_object_unref (data->session_out);
  g_object_unref (data->cancellable);
  g_source_remove (data->timeout_id);

  /* All the stanzas should have been received */
  g_assert (g_queue_get_length (data->expected_stanzas) == 0);
  g_queue_free (data->expected_stanzas);

  g_free (data);
}

void
test_wait_pending (test_data_t *test)
{
  while (test->outstanding > 0)
    g_main_loop_run (test->loop);
}

static void
send_received_open_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  WockyXmppConnection *conn = WOCKY_XMPP_CONNECTION (source);
  test_data_t *d = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_recv_open_finish (conn, res,
      NULL, NULL, NULL, NULL, NULL, NULL));

  d->outstanding--;
  g_main_loop_quit (d->loop);
}

static void
send_open_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  WockyXmppConnection *conn;

  g_assert (wocky_xmpp_connection_send_open_finish (
      WOCKY_XMPP_CONNECTION (source), res, NULL));

  /* The other connection has to receive the opening */
  if (WOCKY_XMPP_CONNECTION (source) == data->in)
    conn = data->out;
  else
    conn = data->in;

  wocky_xmpp_connection_recv_open_async (conn,
      NULL, send_received_open_cb, user_data);
}

static void
open_connection (test_data_t *test,
    WockyXmppConnection *connection)
{
  wocky_xmpp_connection_send_open_async (connection,
      NULL, NULL, NULL, NULL, NULL,
      NULL, send_open_cb, test);

  test->outstanding++;

  test_wait_pending (test);
}

/* Open XMPP the 'in' connection */
void
test_open_connection (test_data_t *test)
{
  open_connection (test, test->in);
}

/* Open both XMPP connections */
void
test_open_both_connections (test_data_t *test)
{
  open_connection (test, test->in);
  open_connection (test, test->out);
}

static void
wait_close_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  WockyStanza *s;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (WOCKY_XMPP_CONNECTION (source),
      res, &error);

  g_assert (s == NULL);
  /* connection has been disconnected */
  g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
  g_error_free (error);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
close_sent_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_close_finish (
    WOCKY_XMPP_CONNECTION (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

/* Close XMPP connections on both sides */
void
test_close_connection (test_data_t *test)
{
  wocky_xmpp_connection_recv_stanza_async (test->out, NULL, wait_close_cb,
      test);
  test->outstanding++;

  /* Close the connection */
  wocky_xmpp_connection_send_close_async (
    WOCKY_XMPP_CONNECTION (test->in),
    NULL, close_sent_cb, test);
  test->outstanding++;

  test_wait_pending (test);
}

static void
sched_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ret = wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error);

  g_assert_no_error (error);
  g_assert (ret);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
wait_sched_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source);
  WockyStanza *s;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);

  g_assert (s == NULL);
  /* connection has been disconnected */
  g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
  g_error_free (error);

  /* close on our side */
  wocky_xmpp_connection_send_close_async (connection, NULL,
      close_sent_cb, test);

  /* Don't decrement test->outstanding as we are waiting for another
   * callback */
  g_main_loop_quit (test->loop);
}

void
test_close_porter (test_data_t *test)
{
  /* close connections */
  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
      wait_sched_close_cb, test);

  wocky_porter_close_async (test->sched_out, NULL, sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);
}

void
test_expected_stanza_received (test_data_t *test,
    WockyStanza *stanza)
{
  WockyStanza *expected;

  expected = g_queue_pop_head (test->expected_stanzas);
  g_assert (expected != NULL);
  test_assert_nodes_equal (wocky_stanza_get_top_node (stanza),
    wocky_stanza_get_top_node (expected));
  g_object_unref (expected);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

void
test_close_both_porters (test_data_t *test)
{
  wocky_porter_close_async (test->sched_out, NULL, sched_close_cb,
      test);
  wocky_porter_close_async (test->sched_in, NULL, sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);
}

void
test_init (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");
  g_type_init ();
  wocky_init ();
}

void
test_deinit (void)
{
  wocky_deinit ();
}

static gboolean
cancel_in_idle_cb (gpointer cancellable)
{
  g_cancellable_cancel (cancellable);
  return FALSE;
}

void
test_cancel_in_idle (GCancellable *cancellable)
{
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
      cancel_in_idle_cb,
      g_object_ref (cancellable),
      g_object_unref);
}
