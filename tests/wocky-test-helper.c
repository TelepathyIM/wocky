#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-xmpp-scheduler.h>
#include "wocky-test-helper.h"

gboolean
test_timeout_cb (gpointer data)
{
  g_test_message ("Timeout reached :(");
  g_assert_not_reached ();

  return FALSE;
}

test_data_t *
setup_test (void)
{
  test_data_t *data;

  data = g_new0 (test_data_t, 1);
  data->loop = g_main_loop_new (NULL, FALSE);

  data->stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  data->in = wocky_xmpp_connection_new (data->stream->stream0);
  data->out = wocky_xmpp_connection_new (data->stream->stream1);

  data->sched_in = wocky_xmpp_scheduler_new (data->in);
  data->sched_out = wocky_xmpp_scheduler_new (data->out);

  data->expected_stanzas = g_queue_new ();

  g_timeout_add (1000, test_timeout_cb, NULL);

  return data;
}

void
teardown_test (test_data_t *data)
{
  g_main_loop_unref (data->loop);
  g_object_unref (data->stream);
  g_object_unref (data->in);
  g_object_unref (data->out);
  g_object_unref (data->sched_in);
  g_object_unref (data->sched_out);

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
      NULL, NULL, NULL, NULL, NULL));

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
      NULL, NULL, NULL, NULL,
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
  WockyXmppStanza *s;
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
  g_assert (wocky_xmpp_scheduler_close_finish (
      WOCKY_XMPP_SCHEDULER (source), res, NULL));

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
  WockyXmppStanza *s;
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
test_close_scheduler (test_data_t *test)
{
  /* close connections */
  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
      wait_sched_close_cb, test);

  wocky_xmpp_scheduler_close_async (test->sched_out, NULL, sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);
}
