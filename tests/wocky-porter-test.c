#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-porter.h>
#include <wocky/wocky-utils.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

static void
test_instantiation (void)
{
  WockyPorter *porter;
  WockyXmppConnection *connection;
  WockyTestStream *stream;;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  porter = wocky_porter_new (connection);

  g_assert (porter != NULL);

  g_object_unref (porter);
  g_object_unref (connection);
  g_object_unref (stream);
}

/* send testing */

static void
send_stanza_received_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source);
  WockyXmppStanza *s;
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;
  WockyXmppStanza *expected;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);
  g_assert (s != NULL);

  expected = g_queue_pop_head (data->expected_stanzas);
  g_assert (expected != NULL);

  g_assert (wocky_xmpp_node_equal (s->node, expected->node));

  if (g_queue_get_length (data->expected_stanzas) > 0)
    {
      /* We need to receive more stanzas */
      wocky_xmpp_connection_recv_stanza_async (
          WOCKY_XMPP_CONNECTION (source), NULL, send_stanza_received_cb,
          user_data);

      data->outstanding++;
    }

  g_object_unref (s);
  g_object_unref (expected);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
send_stanza_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  g_assert (wocky_porter_send_finish (
      WOCKY_PORTER (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
send_stanza_cancelled_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_send_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert (error->domain == G_IO_ERROR);
  g_assert (error->code == G_IO_ERROR_CANCELLED);
  g_error_free (error);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_send (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *s;

  test_open_connection (test);

  /* Send a stanza */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_async (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send a stanza */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "tybalt@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_async (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send two stanzas and cancel them immediately */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "peter@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_async (test->sched_in, s, test->cancellable,
      send_stanza_cancelled_cb, test);
  g_object_unref (s);
  test->outstanding++;

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "samson@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_async (test->sched_in, s, test->cancellable,
      send_stanza_cancelled_cb, test);
  g_object_unref (s);
  test->outstanding++;

  /* the stanza are not added to expected_stanzas as it was cancelled */
  g_cancellable_cancel (test->cancellable);

  /* ... and a second (using the simple send method) */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "nurse@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send (test->sched_in, s);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send a last stanza using the full method so we are sure that all async
   * sending operation have been finished. This is important because
   * test_close_connection() will have to use this function to close the
   * connection. If there is still a pending sending operation, it will fail. */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "tybalt@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_async (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      send_stanza_received_cb, test);

  test_wait_pending (test);

  test_close_connection (test);
  teardown_test (test);
}

/* receive testing */
static gboolean
test_receive_stanza_received_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
sched_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  g_assert (wocky_porter_close_finish (
      WOCKY_PORTER (source), res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
close_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_close_finish (
    WOCKY_XMPP_CONNECTION (source), res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
wait_close_cb (GObject *source,
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

static void
test_receive (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *s;

  test_open_both_connections (test);

  /* Send a stanza */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_async (test->sched_in, s, NULL,
      send_stanza_cb, test);
  g_queue_push_tail (test->expected_stanzas, s);
  /* We are waiting for the stanza to be sent and received on the other
   * side */
  test->outstanding += 2;

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 0,
      test_receive_stanza_received_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  test_wait_pending (test);

  test_close_porter (test);
  teardown_test (test);
}

/* filter testing */
static gboolean
test_filter_iq_received_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static gboolean
test_filter_presence_received_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  /* We didn't send any presence stanza so this callback shouldn't be
   * called */
  g_assert_not_reached ();
  return TRUE;
}

static void
test_filter (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *msg, *iq;

  test_open_both_connections (test);

  /* register an IQ filter */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 0,
      test_filter_iq_received_cb, test, WOCKY_STANZA_END);

  /* register a presence filter */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_PRESENCE, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 0,
      test_filter_presence_received_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* Send a message */
  msg = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send (test->sched_in, msg);
  /* We don't expect this stanza as we didn't register any message filter */

  /* Send an IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send (test->sched_in, iq);
  /* We expect to receive this stanza */
  g_queue_push_tail (test->expected_stanzas, iq);
  test->outstanding++;

  test_wait_pending (test);
  g_object_unref (msg);

  test_close_porter (test);
  teardown_test (test);
}

/* test if the send queue is flushed before closing the connection */
static void
test_close_stanza_received_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source);
  WockyXmppStanza *s;
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);
  if (g_queue_get_length (test->expected_stanzas) > 0)
    {
      WockyXmppStanza *expected;
      g_assert (s != NULL);

      expected = g_queue_pop_head (test->expected_stanzas);
      g_assert (expected != NULL);

      g_assert (wocky_xmpp_node_equal (s->node, expected->node));

      wocky_xmpp_connection_recv_stanza_async (connection, NULL,
          test_close_stanza_received_cb, user_data);
      test->outstanding++;

      g_object_unref (s);
      g_object_unref (expected);
    }
  else
    {
      g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
          WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
      g_error_free (error);

      /* close on our side */
      wocky_xmpp_connection_send_close_async (connection, NULL,
          close_sent_cb, test);
    }

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_sched_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  g_assert (wocky_porter_close_finish (
      WOCKY_PORTER (source), res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_flush (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *s;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);
  wocky_porter_send (test->sched_in, s);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_stanza_received_cb, test);

  wocky_porter_close_async (test->sched_in, NULL,
      test_close_sched_close_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* test if the right error is raised when trying to close a not started
 * porter */
static void
test_close_not_started_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_NOT_STARTED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_not_started (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_porter_close_async (test->sched_in, NULL,
      test_close_not_started_cb, test);

  test->outstanding++;
  test_wait_pending (test);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      wait_close_cb, test);

  wocky_porter_start (test->sched_in);

  wocky_porter_close_async (test->sched_in, NULL, sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* test if the right error is raised when trying to close the porter
 * twice */
static void
test_close_twice_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PENDING);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_twice_cb2 (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_CLOSED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_twice (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      wait_close_cb, test);

  wocky_porter_start (test->sched_in);

  wocky_porter_close_async (test->sched_in, NULL, sched_close_cb,
      test);
  wocky_porter_close_async (test->sched_in, NULL, test_close_twice_cb,
      test);

  test->outstanding += 3;
  test_wait_pending (test);

  /* Retry now that the porter has been closed */
  wocky_porter_close_async (test->sched_in, NULL, test_close_twice_cb2,
      test);
  test->outstanding++;
  test_wait_pending (test);

  teardown_test (test);
}

/* Test if the remote-closed signal is emitted when the other side closes his
 * XMPP connection */
static void
remote_closed_cb (WockyPorter *porter,
    test_data_t *test)
{
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_remote_close_in_close_send_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;

  g_assert (wocky_xmpp_connection_send_close_finish (
    WOCKY_XMPP_CONNECTION (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_remote_close (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);

  g_signal_connect (test->sched_out, "remote-closed",
      G_CALLBACK (remote_closed_cb), test);
  test->outstanding++;

  wocky_xmpp_connection_send_close_async (
    WOCKY_XMPP_CONNECTION (test->in),
    NULL, test_remote_close_in_close_send_cb, test);
  test->outstanding++;

  test_wait_pending (test);

  wocky_porter_close_async (test->sched_out, NULL, sched_close_cb,
      test);
  test->outstanding++;
  test_wait_pending (test);

  teardown_test (test);
}

/* Test cancelling a close operation */
static void
sched_close_cancelled_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error));

  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_cancel (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);

  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
      wait_close_cb, test);
  wocky_porter_close_async (test->sched_out, test->cancellable,
      sched_close_cancelled_cb, test);

  g_cancellable_cancel (test->cancellable);

  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* Test if the remote-error signal is fired when porter got a read error */
static void
remote_error_cb (WockyPorter *porter,
    GQuark domain,
    guint code,
    const gchar *message,
    test_data_t *test)
{
  GError *err = g_error_new_literal (domain, code, message);

  g_assert_error (err, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_error_free (err);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_reading_error (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  g_signal_connect (test->sched_out, "remote-error",
      G_CALLBACK (remote_error_cb), test);
  test->outstanding++;

  wocky_test_input_stream_set_read_error (test->stream->stream1_input);

  wocky_porter_start (test->sched_out);
  test_wait_pending (test);

  test_close_porter (test);
  teardown_test (test);
}

/* Test if the right error is raised when trying to send a stanza through a
 * closed porter */
static void
test_send_closing_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_send_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_CLOSING);

  g_error_free (error);
  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_send_closed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_send_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED);

  g_error_free (error);
  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_send_closed (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *s;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      wait_close_cb, test);
  wocky_porter_close_async (test->sched_in, NULL, sched_close_cb,
      test);

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  /* try to send a stanza while closing */
  wocky_porter_send_async (test->sched_in, s, NULL,
      test_send_closing_cb, test);
  test->outstanding += 3;
  test_wait_pending (test);

  /* try to send a stanza after the closing */
  wocky_porter_send_async (test->sched_in, s, NULL,
      test_send_closed_cb, test);
  g_object_unref (s);
  test->outstanding++;
  test_wait_pending (test);

  teardown_test (test);
}

/* test if the handler with the higher priority is called */
static void
send_stanza (test_data_t *test,
    WockyXmppStanza *stanza,
    gboolean expected)
{
  wocky_porter_send (test->sched_in, stanza);
  if (expected)
    {
      g_queue_push_tail (test->expected_stanzas, stanza);
      test->outstanding++;
    }
  else
    {
      g_object_unref (stanza);
    }

  test_wait_pending (test);
}

static gboolean
test_handler_priority_5 (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  /* This handler has the lowest priority and is not supposed to be called */
  g_assert_not_reached ();
  return TRUE;
}

static gboolean
test_handler_priority_10 (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaSubType sub_type;

  test_expected_stanza_received (test, stanza);

  wocky_xmpp_stanza_get_type_info (stanza, NULL, &sub_type);
  /* This handler is supposed to only handle the get stanza */
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_GET);
  return TRUE;
}

static gboolean
test_handler_priority_15 (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
test_handler_priority (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler with a priority of 10 */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 10,
      test_handler_priority_10, test, WOCKY_STANZA_END);

  /* register an IQ handler with a priority of 5 */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 5,
      test_handler_priority_5, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  send_stanza (test, iq, TRUE);

  /* register an IQ handler with a priority of 15 */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 15,
      test_handler_priority_15, test, WOCKY_STANZA_END);

  /* Send a 'set' IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* Test unregistering a handler */
static gboolean
test_unregister_handler_10 (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  /* this handler is unregistred so shouldn't called */
  g_assert_not_reached ();
  return TRUE;
}

static gboolean
test_unregister_handler_5 (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
test_unregister_handler (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;
  guint id;

  test_open_both_connections (test);

  /* register an IQ handler with a priority of 10 */
  id = wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 10,
      test_unregister_handler_10, test, WOCKY_STANZA_END);

  /* register an IQ handler with a priority of 5 */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 5,
      test_unregister_handler_5, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* unregister the first handler */
  wocky_porter_unregister_handler (test->sched_out, id);

  /* Send a 'get' IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test registering a handler using a bare JID as filter criteria */
static gboolean
test_handler_bare_jid_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
test_handler_bare_jid (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler for all IQ from a bare jid */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com", 0,
      test_handler_bare_jid_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ from the bare jid */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);
  send_stanza (test, iq, TRUE);

  /* Send a 'get' IQ from another contact */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "samson@example.com/House", "romeo@example.net",
    WOCKY_STANZA_END);
  send_stanza (test, iq, FALSE);

  /* Send a 'get' IQ from the bare jid + resource */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com/Pub", "romeo@example.net",
    WOCKY_STANZA_END);
  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test registering a handler using a full JID as filter criteria */
static gboolean
test_handler_full_jid_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
test_handler_full_jid (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler for all IQ from a bare jid */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      "juliet@example.com/Pub", 0,
      test_handler_full_jid_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ from the bare jid */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);
  send_stanza (test, iq, FALSE);

  /* Send a 'get' IQ from another contact */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "samson@example.com/House", "romeo@example.net",
    WOCKY_STANZA_END);
  send_stanza (test, iq, FALSE);

  /* Send a 'get' IQ from the bare jid + resource */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com/Pub", "romeo@example.net",
    WOCKY_STANZA_END);
  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test registering a handler using a stanza as filter criteria */
static gboolean
test_handler_stanza_jingle_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *id;

  test_expected_stanza_received (test, stanza);
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (!wocky_strdiff (id, "3") ||
      !wocky_strdiff (id, "4"));
  return TRUE;
}

static gboolean
test_handler_stanza_terminate_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *id;

  test_expected_stanza_received (test, stanza);
  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  g_assert (!wocky_strdiff (id, "5"));
  return TRUE;
}

static void
test_handler_stanza (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler for all the jingle stanzas related to one jingle
   * session */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      NULL, 0,
      test_handler_stanza_jingle_cb, test,
      WOCKY_NODE, "jingle",
        WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
        WOCKY_NODE_ATTRIBUTE, "sid", "my_sid",
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* Send a not jingle IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "1",
    WOCKY_STANZA_END);
  send_stanza (test, iq, FALSE);

  /* Send a jingle IQ but related to another session */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "2",
    WOCKY_NODE, "jingle",
      WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
      WOCKY_NODE_ATTRIBUTE, "sid", "another_sid",
    WOCKY_NODE_END, WOCKY_STANZA_END);
  send_stanza (test, iq, FALSE);

  /* Send a jingle IQ related to the right session */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "3",
    WOCKY_NODE, "jingle",
      WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
      WOCKY_NODE_ATTRIBUTE, "sid", "my_sid",
    WOCKY_NODE_END, WOCKY_STANZA_END);
  send_stanza (test, iq, TRUE);

  /* register a new IQ handler,with higher priority, handling session-terminate
   * with a specific test message */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      NULL, 10,
      test_handler_stanza_terminate_cb, test,
      WOCKY_NODE, "jingle",
        WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
        WOCKY_NODE_ATTRIBUTE, "action", "session-terminate",
        WOCKY_NODE, "reason",
          WOCKY_NODE, "success", WOCKY_NODE_END,
          WOCKY_NODE, "test",
            WOCKY_NODE_TEXT, "Sorry, gotta go!",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  /* Send a session-terminate with the wrong message */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "4",
    WOCKY_NODE, "jingle",
      WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
      WOCKY_NODE_ATTRIBUTE, "sid", "my_sid",
      WOCKY_NODE_ATTRIBUTE, "action", "session-terminate",
        WOCKY_NODE, "reason",
          WOCKY_NODE, "success", WOCKY_NODE_END,
          WOCKY_NODE, "test",
            WOCKY_NODE_TEXT, "Bye Bye",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
    WOCKY_NODE_END, WOCKY_STANZA_END);
  send_stanza (test, iq, TRUE);

  /* Send a session-terminate with the right message */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "5",
    WOCKY_NODE, "jingle",
      WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
      WOCKY_NODE_ATTRIBUTE, "sid", "my_sid",
      WOCKY_NODE_ATTRIBUTE, "action", "session-terminate",
        WOCKY_NODE, "reason",
          WOCKY_NODE, "success", WOCKY_NODE_END,
          WOCKY_NODE, "test",
            WOCKY_NODE_TEXT, "Sorry, gotta go!",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
    WOCKY_NODE_END, WOCKY_STANZA_END);
  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* Cancel the sending of a stanza after it has been received */
static gboolean
test_cancel_sent_stanza_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);

  g_cancellable_cancel (test->cancellable);
  return TRUE;
}

static void
test_cancel_sent_stanza_cancelled (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  /* Stanza has already be sent to _finish success */
  g_assert (wocky_porter_send_finish (
      WOCKY_PORTER (source), res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_cancel_sent_stanza (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *stanza;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register a message handler */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
      NULL, 0,
      test_cancel_sent_stanza_cb, test, WOCKY_STANZA_END);

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);
  wocky_porter_send_async (test->sched_in, stanza,
      test->cancellable, test_cancel_sent_stanza_cancelled,
      test);
  g_queue_push_tail (test->expected_stanzas, stanza);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

/* Test if the error is correctly propagated when a writing error occurs */
static void
test_writing_error_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_send_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_writing_error (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *s;

  test_open_connection (test);

  wocky_test_output_stream_set_write_error (test->stream->stream0_output);

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  test->outstanding++;
  wocky_porter_send_async (test->sched_in, s, NULL,
      test_writing_error_cb, test);

  test_wait_pending (test);

  g_object_unref (s);
  teardown_test (test);
}

/* Test send with reply */
static void
test_send_iq_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  g_assert (wocky_porter_send_finish (
      WOCKY_PORTER (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static gboolean
test_send_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  const gchar *id;
  gboolean cancelled;
  WockyStanzaSubType sub_type;

  test_expected_stanza_received (test, stanza);

  id = wocky_xmpp_node_get_attribute (stanza->node, "id");

  wocky_xmpp_stanza_get_type_info (stanza, NULL, &sub_type);

  /* Reply of the "set" IQ is not expected as we are going to cancel it */
  cancelled = (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  if (cancelled)
    g_cancellable_cancel (test->cancellable);

  /* Send a spoofed reply */
  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "oscar@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", id,
    WOCKY_STANZA_END);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  /* Send reply */
  reply = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", id,
    WOCKY_STANZA_END);

  wocky_porter_send_async (porter, reply,
      NULL, test_send_iq_sent_cb, test);
  if (!cancelled)
    g_queue_push_tail (test->expected_stanzas, reply);
  else
    g_object_unref (reply);

  test->outstanding++;
  return TRUE;
}

static void
test_send_iq_reply_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source),
      res, NULL);
  g_assert (reply != NULL);

  test_expected_stanza_received (test, reply);
}

static void
test_send_iq_cancelled_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source),
      res, &error);
  g_assert (reply == NULL);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_send_iq (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register an IQ handler */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      NULL, 0,
      test_send_iq_cb, test, WOCKY_STANZA_END);

  /* Send an IQ query. We are going to cancel it after it has been received
   * but before we receive the reply so the callback won't be called.*/
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);
  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_iq_cancelled_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, iq);

  test->outstanding += 2;
  test_wait_pending (test);

  /* Send an IQ query */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "1",
    WOCKY_STANZA_END);

  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_send_iq_reply_cb, test);
  g_queue_push_tail (test->expected_stanzas, iq);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

/* Test if the error is correctly propagated when a writing error occurs while
 * sending an IQ */
static void
test_send_iq_error_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_send_iq_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_send_iq_error (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_connection (test);

  wocky_test_output_stream_set_write_error (test->stream->stream0_output);

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
    WOCKY_STANZA_END);

  test->outstanding++;
  wocky_porter_send_iq_async (test->sched_in, iq, NULL,
      test_send_iq_error_cb, test);

  test_wait_pending (test);

  g_object_unref (iq);
  teardown_test (test);
}

/* Test implementing a filter using handlers */
static gboolean
test_handler_filter_get_filter (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaSubType sub_type;
  gboolean result;

  wocky_xmpp_stanza_get_type_info (stanza, NULL, &sub_type);
  if (sub_type == WOCKY_STANZA_SUB_TYPE_GET)
    {
      /* We filter 'get' IQ. Return TRUE to say that we handled this stanza so
       * the handling process will be stopped */
      result = TRUE;
    }
  else
    {
      /* We don't handle this stanza so the other callback will be called */
      g_queue_push_tail (test->expected_stanzas, stanza);
      g_object_ref (stanza);
      test->outstanding++;
      result = FALSE;
    }

  test_expected_stanza_received (test, stanza);
  return result;
}

static gboolean
test_handler_filter_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
test_handler_filter (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler which will act as a filter */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 10,
      test_handler_filter_get_filter, test, WOCKY_STANZA_END);

  /* register another handler with a smaller priority which will be called
   * after the filter */
  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL, 5,
      test_handler_filter_cb, test, WOCKY_STANZA_END);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ that will be filtered */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  send_stanza (test, iq, TRUE);

  /* Send a 'set' IQ that won't be filtered */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test if the right error is raised when trying to send an invalid IQ */
static void
test_send_invalid_iq_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source),
      res, &error);
  g_assert (reply == NULL);
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_NOT_IQ);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_send_invalid_iq (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *iq;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);

  /* Try to send a message as an IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_invalid_iq_cb, test);
  g_object_unref (iq);
  test->outstanding++;

  /* Try to send an IQ reply */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_invalid_iq_cb, test);
  g_object_unref (iq);
  test->outstanding++;

  /* Try to send an IQ without recipient */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", NULL,
    WOCKY_STANZA_END);

  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_invalid_iq_cb, test);
  g_object_unref (iq);
  test->outstanding++;

  test_wait_pending (test);

  test_close_porter (test);
  teardown_test (test);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-porter/initiation", test_instantiation);
  g_test_add_func ("/xmpp-porter/send", test_send);
  g_test_add_func ("/xmpp-porter/receive", test_receive);
  g_test_add_func ("/xmpp-porter/filter", test_filter);
  g_test_add_func ("/xmpp-porter/close-flush", test_close_flush);
  g_test_add_func ("/xmpp-porter/close-not-started", test_close_not_started);
  g_test_add_func ("/xmpp-porter/close-twice", test_close_twice);
  g_test_add_func ("/xmpp-porter/remote-close", test_remote_close);
  g_test_add_func ("/xmpp-porter/close-cancel", test_close_cancel);
  g_test_add_func ("/xmpp-porter/reading-error", test_reading_error);
  g_test_add_func ("/xmpp-porter/send-closed", test_send_closed);
  g_test_add_func ("/xmpp-porter/handler-priority", test_handler_priority);
  g_test_add_func ("/xmpp-porter/unregister-handler",
      test_unregister_handler);
  g_test_add_func ("/xmpp-porter/handler-bare-jid", test_handler_bare_jid);
  g_test_add_func ("/xmpp-porter/handler-bare-jid", test_handler_full_jid);
  g_test_add_func ("/xmpp-porter/handler-stanza", test_handler_stanza);
  g_test_add_func ("/xmpp-porter/cancel-sent-stanza",
      test_cancel_sent_stanza);
  g_test_add_func ("/xmpp-porter/writing-error", test_writing_error);
  g_test_add_func ("/xmpp-porter/send-iq", test_send_iq);
  g_test_add_func ("/xmpp-porter/send-iq-error", test_send_iq_error);
  g_test_add_func ("/xmpp-porter/handler-filter", test_handler_filter);
  g_test_add_func ("/xmpp-porter/send-invalid-iq", test_send_invalid_iq);
  return g_test_run ();
}
