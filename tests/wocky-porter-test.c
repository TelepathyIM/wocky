#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

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
  porter = wocky_c2s_porter_new (connection, "juliet@example.com/Balcony");

  g_assert (porter != NULL);
  g_assert_cmpstr (wocky_porter_get_full_jid (porter), ==,
        "juliet@example.com/Balcony");
  g_assert_cmpstr (wocky_porter_get_bare_jid (porter), ==,
        "juliet@example.com");
  g_assert_cmpstr (wocky_porter_get_resource (porter), ==,
        "Balcony");

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
  WockyStanza *s;
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;
  WockyStanza *expected;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);
  g_assert_no_error (error);
  g_assert (s != NULL);

  expected = g_queue_pop_head (data->expected_stanzas);
  g_assert (expected != NULL);

  test_assert_stanzas_equal (s, expected);

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
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_send_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
send_stanza_cancelled_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_send_finish (WOCKY_PORTER (source), res, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_error_free (error);
  g_assert (!ok);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_send (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *s;

  test_open_connection (test);

  /* Send a stanza */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send_async (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send a stanza */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "tybalt@example.net",
    NULL);

  wocky_porter_send_async (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send two stanzas and cancel them immediately */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "peter@example.net",
    NULL);

  wocky_porter_send_async (test->sched_in, s, test->cancellable,
      send_stanza_cancelled_cb, test);
  g_object_unref (s);
  test->outstanding++;

  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "samson@example.net",
    NULL);

  wocky_porter_send_async (test->sched_in, s, test->cancellable,
      send_stanza_cancelled_cb, test);
  g_object_unref (s);
  test->outstanding++;

  /* the stanza are not added to expected_stanzas as it was cancelled */
  g_cancellable_cancel (test->cancellable);

  /* ... and a second (using the simple send method) */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "nurse@example.net",
    NULL);

  wocky_porter_send (test->sched_in, s);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send a last stanza using the full method so we are sure that all async
   * sending operation have been finished. This is important because
   * test_close_connection() will have to use this function to close the
   * connection. If there is still a pending sending operation, it will fail. */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "tybalt@example.net",
    NULL);

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
    WockyStanza *stanza,
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
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_close_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
close_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_xmpp_connection_send_close_finish (WOCKY_XMPP_CONNECTION (source),
      res, &error);
  g_assert_no_error (error);
  g_assert (ok);

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

static void
test_receive (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *s;

  test_open_both_connections (test);

  /* Send a stanza */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send_async (test->sched_in, s, NULL,
      send_stanza_cb, test);
  g_queue_push_tail (test->expected_stanzas, s);
  /* We are waiting for the stanza to be sent and received on the other
   * side */
  test->outstanding += 2;

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE, 0,
      test_receive_stanza_received_cb, test, NULL);

  wocky_porter_start (test->sched_out);

  test_wait_pending (test);

  test_close_porter (test);
  teardown_test (test);
}

/* filter testing */
static gboolean
test_filter_iq_received_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static gboolean
test_filter_presence_received_cb (WockyPorter *porter,
    WockyStanza *stanza,
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
  WockyStanza *msg, *iq;

  test_open_both_connections (test);

  /* register an IQ filter */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 0,
      test_filter_iq_received_cb, test, NULL);

  /* register a presence filter */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_PRESENCE, WOCKY_STANZA_SUB_TYPE_NONE, 0,
      test_filter_presence_received_cb, test, NULL);

  wocky_porter_start (test->sched_out);

  /* Send a message */
  msg = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send (test->sched_in, msg);
  /* We don't expect this stanza as we didn't register any message filter */

  /* Send an IQ */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);

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
  WockyStanza *s;
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (connection, res, &error);
  if (g_queue_get_length (test->expected_stanzas) > 0)
    {
      WockyStanza *expected;
      g_assert (s != NULL);

      expected = g_queue_pop_head (test->expected_stanzas);
      g_assert (expected != NULL);

      test_assert_stanzas_equal (s, expected);

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
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_close_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
closing_cb (WockyPorter *porter,
    test_data_t *test)
{
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_flush (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *s;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);

  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);
  wocky_porter_send (test->sched_in, s);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_stanza_received_cb, test);

  g_signal_connect (test->sched_in, "closing",
      G_CALLBACK (closing_cb), test);

  wocky_porter_close_async (test->sched_in, NULL,
      test_close_sched_close_cb, test);

  test->outstanding += 3;
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
  GError *error = NULL;
  gboolean ok;

  ok = wocky_xmpp_connection_send_close_finish (WOCKY_XMPP_CONNECTION (source),
      res, &error);
  g_assert_no_error (error);
  g_assert (ok);

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
test_close_cancel_force_closed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_force_close_finish (
      WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_cancel (void)
{
  test_data_t *test = setup_test ();

  wocky_test_stream_set_write_mode (test->stream->stream0_output,
    WOCKY_TEST_STREAM_WRITE_COMPLETE);

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);

  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
      wait_close_cb, test);
  wocky_porter_close_async (test->sched_out, test->cancellable,
      sched_close_cancelled_cb, test);

  g_cancellable_cancel (test->cancellable);

  test->outstanding += 2;
  test_wait_pending (test);

  wocky_porter_force_close_async (test->sched_out, NULL,
        test_close_cancel_force_closed_cb, test);

  test->outstanding++;
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
  WockyStanza *s;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      wait_close_cb, test);
  wocky_porter_close_async (test->sched_in, NULL, sched_close_cb,
      test);

  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

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
    WockyStanza *stanza,
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
    WockyStanza *stanza,
    gpointer user_data)
{
  /* This handler has the lowest priority and is not supposed to be called */
  g_assert_not_reached ();
  return TRUE;
}

static gboolean
test_handler_priority_10 (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaSubType sub_type;

  test_expected_stanza_received (test, stanza);

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);
  /* This handler is supposed to only handle the get stanza */
  g_assert_cmpint (sub_type, ==, WOCKY_STANZA_SUB_TYPE_GET);
  return TRUE;
}

static gboolean
test_handler_priority_15 (WockyPorter *porter,
    WockyStanza *stanza,
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
  WockyStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler with a priority of 10 */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 10,
      test_handler_priority_10, test, NULL);

  /* register an IQ handler with a priority of 5 */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 5,
      test_handler_priority_5, test, NULL);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  /* register an IQ handler with a priority of 15 */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 15,
      test_handler_priority_15, test, NULL);

  /* Send a 'set' IQ */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* Test unregistering a handler */
static gboolean
test_unregister_handler_10 (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  /* this handler is unregistred so shouldn't called */
  g_assert_not_reached ();
  return TRUE;
}

static gboolean
test_unregister_handler_5 (WockyPorter *porter,
    WockyStanza *stanza,
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
  WockyStanza *iq;
  guint id;

  test_open_both_connections (test);

  /* register an IQ handler with a priority of 10 */
  id = wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 10,
      test_unregister_handler_10, test, NULL);

  /* register an IQ handler with a priority of 5 */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 5,
      test_unregister_handler_5, test, NULL);

  wocky_porter_start (test->sched_out);

  /* unregister the first handler */
  wocky_porter_unregister_handler (test->sched_out, id);

  /* Send a 'get' IQ */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test registering a handler using a bare JID as filter criteria */
static gboolean
test_handler_bare_jid_cb (WockyPorter *porter,
    WockyStanza *stanza,
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
  WockyStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler for all IQ from a bare jid */
  wocky_porter_register_handler_from (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com", 0,
      test_handler_bare_jid_cb, test, NULL);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ from the bare jid */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);
  send_stanza (test, iq, TRUE);

  /* Send a 'get' IQ from another contact */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "samson@example.com/House", "romeo@example.net",
    NULL);
  send_stanza (test, iq, FALSE);

  /* Send a 'get' IQ from the bare jid + resource */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com/Pub", "romeo@example.net",
    NULL);
  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test registering a handler using a full JID as filter criteria */
static gboolean
test_handler_full_jid_cb (WockyPorter *porter,
    WockyStanza *stanza,
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
  WockyStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler for all IQ from a bare jid */
  wocky_porter_register_handler_from (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      "juliet@example.com/Pub", 0,
      test_handler_full_jid_cb, test, NULL);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ from the bare jid */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);
  send_stanza (test, iq, FALSE);

  /* Send a 'get' IQ from another contact */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "samson@example.com/House", "romeo@example.net",
    NULL);
  send_stanza (test, iq, FALSE);

  /* Send a 'get' IQ from the bare jid + resource */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com/Pub", "romeo@example.net",
    NULL);
  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

/* test registering a handler using a stanza as filter criteria */
static gboolean
test_handler_stanza_jingle_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *id;

  test_expected_stanza_received (test, stanza);
  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");
  g_assert (!wocky_strdiff (id, "3") || !wocky_strdiff (id, "4"));
  return TRUE;
}

static gboolean
test_handler_stanza_terminate_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *id;

  test_expected_stanza_received (test, stanza);
  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");
  g_assert_cmpstr (id, ==, "5");
  return TRUE;
}

static void
test_handler_stanza (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler for all the jingle stanzas related to one jingle
   * session */
  wocky_porter_register_handler_from (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      "juliet@example.com", 0,
      test_handler_stanza_jingle_cb, test,
      '(', "jingle",
        ':', "urn:xmpp:jingle:1",
        '@', "sid", "my_sid",
      ')', NULL);

  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* Send a not jingle IQ */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    '@', "id", "1",
    NULL);
  send_stanza (test, iq, FALSE);

  /* Send a jingle IQ but related to another session */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    '@', "id", "2",
    '(', "jingle",
      ':', "urn:xmpp:jingle:1",
      '@', "sid", "another_sid",
    ')', NULL);
  send_stanza (test, iq, FALSE);

  /* Send a jingle IQ with the right sid but from the wrong contact */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "tybalt@example.com", "romeo@example.net",
    '@', "id", "2",
    '(', "jingle",
      ':', "urn:xmpp:jingle:1",
      '@', "sid", "my_sid",
    ')', NULL);
  send_stanza (test, iq, FALSE);

  /* Send a jingle IQ related to the right session */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    '@', "id", "3",
    '(', "jingle",
      ':', "urn:xmpp:jingle:1",
      '@', "sid", "my_sid",
    ')', NULL);
  send_stanza (test, iq, TRUE);

  /* register a new IQ handler,with higher priority, handling session-terminate
   * with a specific test message */
  wocky_porter_register_handler_from (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      "juliet@example.com", 10,
      test_handler_stanza_terminate_cb, test,
      '(', "jingle",
        ':', "urn:xmpp:jingle:1",
        '@', "action", "session-terminate",
        '(', "reason",
          '(', "success", ')',
          '(', "test",
            '$', "Sorry, gotta go!",
          ')',
        ')',
      ')', NULL);

  /* Send a session-terminate with the wrong message */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    '@', "id", "4",
    '(', "jingle",
      ':', "urn:xmpp:jingle:1",
      '@', "sid", "my_sid",
      '@', "action", "session-terminate",
        '(', "reason",
          '(', "success", ')',
          '(', "test",
            '$', "Bye Bye",
          ')',
        ')',
    ')', NULL);
  send_stanza (test, iq, TRUE);

  /* Send a session-terminate with the right message */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    '@', "id", "5",
    '(', "jingle",
      ':', "urn:xmpp:jingle:1",
      '@', "sid", "my_sid",
      '@', "action", "session-terminate",
        '(', "reason",
          '(', "success", ')',
          '(', "test",
            '$', "Sorry, gotta go!",
          ')',
        ')',
    ')', NULL);
  send_stanza (test, iq, TRUE);

  test_close_both_porters (test);
  teardown_test (test);
}

/* Cancel the sending of a stanza after it has been received */
static gboolean
test_cancel_sent_stanza_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);

  test_cancel_in_idle (test->cancellable);
  return TRUE;
}

static void
test_cancel_sent_stanza_cancelled (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  /* Stanza has already be sent to _finish success */
  ok = wocky_porter_send_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_cancel_sent_stanza (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *stanza;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register a message handler */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
      0,
      test_cancel_sent_stanza_cb, test, NULL);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com", "romeo@example.net",
    NULL);
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
  WockyStanza *s;

  test_open_connection (test);

  wocky_test_output_stream_set_write_error (test->stream->stream0_output);

  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

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
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_send_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static gboolean
test_send_iq_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  const gchar *id;
  gboolean cancelled;
  WockyStanzaSubType sub_type;

  test_expected_stanza_received (test, stanza);

  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);

  /* Reply of the "set" IQ is not expected as we are going to cancel it */
  cancelled = (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  if (cancelled)
    g_cancellable_cancel (test->cancellable);

  /* Send a spoofed reply; should be ignored */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "oscar@example.net", "juliet@example.com",
    '@', "id", id,
    NULL);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  /* Send a reply without 'id' attribute; should be ignored */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    NULL);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  /* Send reply */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    '@', "id", id,
    NULL);

  wocky_porter_send_async (porter, reply,
      NULL, test_send_iq_sent_cb, test);
  if (!cancelled)
    g_queue_push_tail (test->expected_stanzas, reply);
  else
    g_object_unref (reply);

  test->outstanding++;
  return TRUE;
}

static gboolean
test_send_iq_abnormal_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  const gchar *id;
  WockyStanzaSubType sub_type;

  test_expected_stanza_received (test, stanza);

  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);

  /* Send a spoofed reply; should be ignored */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "oscar@example.net", "juliet@example.com",
    '@', "id", id,
    NULL);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  /* Send a reply without 'id' attribute; should be ignored */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "rOmeO@examplE.neT", "juLiet@Example.cOm",
    NULL);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  /* Send reply */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "roMeo@eXampLe.net", "JulieT@ExamplE.com",
    '@', "id", id,
    NULL);

  wocky_porter_send_async (porter, reply,
      NULL, test_send_iq_sent_cb, test);

  g_queue_push_tail (test->expected_stanzas, reply);

  test->outstanding++;
  return TRUE;
}


static void
test_send_iq_reply_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source),
      res, &error);
  g_assert_no_error (error);
  g_assert (reply != NULL);

  test_expected_stanza_received (test, reply);
  g_object_unref (reply);
}

static void
test_send_iq_cancelled_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
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
  WockyStanza *iq;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register an IQ handler */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      0,
      test_send_iq_cb, test, NULL);

  /* Send an IQ query. We are going to cancel it after it has been received
   * but before we receive the reply so the callback won't be called.*/
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_iq_cancelled_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, iq);

  test->outstanding += 2;
  test_wait_pending (test);

  /* Send an IQ query */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "1",
    NULL);

  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_send_iq_reply_cb, test);
  g_queue_push_tail (test->expected_stanzas, iq);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

static gboolean
test_acknowledge_iq_acknowledge_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  wocky_porter_acknowledge_iq (porter, stanza,
      '(', "sup-dawg",
        '@', "lions", "tigers",
      ')',
      NULL);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_iq_reply_no_id_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source);
  test_data_t *test = user_data;
  WockyStanza *reply;
  WockyStanza *expected_reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (porter, result, &error);
  g_assert_no_error (error);
  g_assert (reply != NULL);

  expected_reply = g_queue_pop_head (test->expected_stanzas);
  g_assert (expected_reply != NULL);

  /* If we got the reply dispatched to us, the ID was correct — this is tested
   * elsewhere. So we don't need to test it again here. */
  test_assert_stanzas_equal_no_id (reply, expected_reply);

  g_object_unref (reply);
  g_object_unref (expected_reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

/* Tests wocky_porter_acknowledge_iq(). */
static void
test_acknowledge_iq (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq, *expected_reply;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      0,
      test_acknowledge_iq_acknowledge_cb, test, NULL);

  /* We re-construct expected_reply for every test because…
   * test_assert_stanzas_equal_no_id() modifies the stanzas it's comparing to
   * add an id='' to the one which doesn't have one.
   */

  /* Send a legal IQ (with a single child element). */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '(', "sup-dawg", ')', NULL);
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg",
        '@', "lions", "tigers",
      ')',
      NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);
  g_object_unref (iq);

  /* Send an illegal IQ with two child elements. We expect that
   * wocky_porter_acknowledge_iq() should cope. */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '(', "sup-dawg", ')',
      '(', "i-heard-you-like-stanzas", ')',
      NULL);
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg",
        '@', "lions", "tigers",
      ')',
      NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);
  g_object_unref (iq);

  /* Send another illegal IQ, with no child element at all. Obviously in real
   * life it should be nacked, but wocky_porter_acknowledge_iq() should still
   * cope. */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      NULL);
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg",
        '@', "lions", "tigers",
      ')',
      NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);
  g_object_unref (iq);

  /* Finally, send an IQ that doesn't have an id='' attribute. This is really
   * illegal, but wocky_porter_acknowledge_iq() needs to deal because it
   * happens in practice.
   */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      NULL);
  wocky_porter_send (test->sched_in, iq);
  /* In this case, we only expect the recipient's callback to fire. There's no
   * way for it to send us an IQ back, so we don't need to wait for a reply
   * there.
   */
  test->outstanding += 1;
  g_object_unref (iq);

  /* Off we go! */
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

static gboolean
test_send_iq_error_nak_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  wocky_porter_send_iq_error (porter, stanza,
      WOCKY_XMPP_ERROR_BAD_REQUEST, "bye bye beautiful");

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

/* Tests wocky_porter_send_iq_error(). */
static void
test_send_iq_error (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq, *expected_reply;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      0,
      test_send_iq_error_nak_cb, test, NULL);

  /* Send a legal IQ (with a single child element). */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '(', "sup-dawg", ')',
      NULL);
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg", ')',
      '(', "error",
        '@', "code", "400",
        '@', "type", "modify",
        '(', "bad-request", ':', WOCKY_XMPP_NS_STANZAS, ')',
        '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
          '$', "bye bye beautiful",
        ')',
      ')', NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);
  g_object_unref (iq);

  /* Send an illegal IQ with two child elements. We expect that
   * wocky_porter_send_iq_error() should cope by just picking the first one.
   */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '(', "sup-dawg", ')',
      '(', "i-heard-you-like-stanzas", ')',
      NULL);
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg", ')',
      '(', "error",
        '@', "code", "400",
        '@', "type", "modify",
        '(', "bad-request", ':', WOCKY_XMPP_NS_STANZAS, ')',
        '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
          '$', "bye bye beautiful",
        ')',
      ')', NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);
  g_object_unref (iq);

  /* Send another illegal IQ, with no child element at all.
   * wocky_porter_send_iq_error() should not blow up.
   */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      NULL);
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "romeo@example.net", "juliet@example.com",
      '(', "error",
        '@', "code", "400",
        '@', "type", "modify",
        '(', "bad-request", ':', WOCKY_XMPP_NS_STANZAS, ')',
        '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
          '$', "bye bye beautiful",
        ')',
      ')', NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);
  g_object_unref (iq);

  /* Finally, send an IQ that doesn't have an id='' attribute. This is really
   * illegal, but wocky_porter_send_iq_error() needs to deal because it
   * happens in practice.
   */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      NULL);
  wocky_porter_send (test->sched_in, iq);
  /* In this case, we only expect the recipient's callback to fire. There's no
   * way for it to send us an IQ back, so we don't need to wait for a reply
   * there.
   */
  test->outstanding += 1;
  g_object_unref (iq);

  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

typedef struct {
    test_data_t *test;
    GError error;
} TestSendIqGErrorCtx;

static gboolean
test_send_iq_gerror_nak_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  TestSendIqGErrorCtx *ctx = user_data;

  wocky_porter_send_iq_gerror (porter, stanza, &ctx->error);

  ctx->test->outstanding--;
  g_main_loop_quit (ctx->test->loop);
  return TRUE;
}

/* Tests wocky_porter_send_iq_gerror(). */
static void
test_send_iq_gerror (void)
{
  test_data_t *test = setup_test ();
  TestSendIqGErrorCtx ctx = { test, };
  WockyStanza *iq, *expected_reply;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      0,
      test_send_iq_gerror_nak_cb, &ctx, NULL);

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '(', "sup-dawg", ')',
      NULL);

  /* Test responding with a simple error */
  ctx.error.domain = WOCKY_XMPP_ERROR;
  ctx.error.code = WOCKY_XMPP_ERROR_UNEXPECTED_REQUEST;
  ctx.error.message = "i'm twelve years old and what is this?";
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg", ')',
      '(', "error",
        '@', "code", "400",
        '@', "type", "wait",
        '(', "unexpected-request", ':', WOCKY_XMPP_NS_STANZAS, ')',
        '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
          '$', ctx.error.message,
        ')',
      ')', NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);

  test_wait_pending (test);

  /* Test responding with an application-specific error */
  ctx.error.domain = WOCKY_JINGLE_ERROR;
  ctx.error.code = WOCKY_JINGLE_ERROR_OUT_OF_ORDER;
  ctx.error.message = "i'm twelve years old and what is this?";
  expected_reply = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "romeo@example.net", "juliet@example.com",
      '(', "sup-dawg", ')',
      '(', "error",
        '@', "code", "400",
        '@', "type", "wait",
        '(', "unexpected-request", ':', WOCKY_XMPP_NS_STANZAS, ')',
        '(', "out-of-order", ':', WOCKY_XMPP_NS_JINGLE_ERRORS, ')',
        '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
          '$', ctx.error.message,
        ')',
      ')', NULL);
  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_iq_reply_no_id_cb, test);
  test->outstanding += 2;
  g_queue_push_tail (test->expected_stanzas, expected_reply);

  test_wait_pending (test);

  g_object_unref (iq);
  test_close_both_porters (test);
  teardown_test (test);
}

static void
test_send_iq_abnormal (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register an IQ handler (to send both the good and spoofed reply) */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      0,
      test_send_iq_abnormal_cb, test, NULL);

  /* Send an IQ query */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "julIet@exampLe.com", "RoMeO@eXample.net",
    '@', "id", "1",
    NULL);

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
test_error_while_sending_iq (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq;

  test_open_connection (test);

  wocky_test_output_stream_set_write_error (test->stream->stream0_output);

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "one",
    NULL);

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
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanzaSubType sub_type;
  gboolean result;

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);
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
    WockyStanza *stanza,
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
  WockyStanza *iq;

  test_open_both_connections (test);

  /* register an IQ handler which will act as a filter */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 10,
      test_handler_filter_get_filter, test, NULL);

  /* register another handler with a smaller priority which will be called
   * after the filter */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, 5,
      test_handler_filter_cb, test, NULL);

  wocky_porter_start (test->sched_out);

  /* Send a 'get' IQ that will be filtered */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  /* Send a 'set' IQ that won't be filtered */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  test_close_porter (test);
  teardown_test (test);
}

static void
unhandled_iq_reply_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source);
  test_data_t *test = user_data;
  GError *error = NULL;
  WockyStanza *reply = wocky_porter_send_iq_finish (porter, result, &error);
  gboolean is_error;
  WockyXmppErrorType type;
  GError *core = NULL;
  GError *specialized = NULL;
  WockyNode *specialized_node;

  g_assert_no_error (error);
  g_assert (reply != NULL);

  is_error = wocky_stanza_extract_errors (reply, &type, &core, &specialized,
      &specialized_node);

  /* The reply should have type='error'. */
  g_assert (is_error);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);
  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_SERVICE_UNAVAILABLE);

  /* There should be no non-XMPP Core error condition. */
  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_clear_error (&core);
  g_object_unref (reply);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_unhandled_iq (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL, NULL,
      '(', "framed-photograph",
        ':', "http://kimjongillookingatthings.tumblr.com",
      ')', NULL);

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  wocky_porter_send_iq_async (test->sched_out, iq, NULL,
      unhandled_iq_reply_cb, test);

  test->outstanding++;
  test_wait_pending (test);

  g_object_unref (iq);
  test_close_both_porters (test);
  teardown_test (test);
}

static gboolean
test_handler_filter_from_juliet_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  const gchar *from;

  from = wocky_stanza_get_from (stanza);
  g_assert_cmpstr (from, ==, "juliet@example.com");

  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static gboolean
test_handler_filter_from_anyone_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
test_handler_filter_from (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq;

  test_open_both_connections (test);

  /* Register a handler for IQs with from=juliet@example.com */
  wocky_porter_register_handler_from (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com",
      10, test_handler_filter_from_juliet_cb, test, NULL);

  /* Register another handler, at a lower priority, for IQs from anyone */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      5, test_handler_filter_from_anyone_cb, test, NULL);

  wocky_porter_start (test->sched_out);

  /* Send an IQ that will be filtered by from_juliet only */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  /* Send an IQ that will be filtered by from_null only */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "romeo@example.com", "juliet@example.net",
    NULL);

  send_stanza (test, iq, TRUE);

  /* Send an IQ that will be filtered by from_null only */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, NULL, "romeo@example.net",
    NULL);

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
  WockyStanza *reply;
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
  WockyStanza *iq;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);

  /* Try to send a message as an IQ */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_invalid_iq_cb, test);
  g_object_unref (iq);
  test->outstanding++;

  /* Try to send an IQ reply */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_invalid_iq_cb, test);
  g_object_unref (iq);
  test->outstanding++;

  test_wait_pending (test);

  test_close_porter (test);
  teardown_test (test);
}

/* Test sending IQ's to the server (no 'to' attribute). The JID we believe we
 * have matters, here. */
static gboolean
test_send_iq_server_received_cb (WockyPorter *porter,
    WockyStanza *iq,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  WockyNode *node;
  const gchar *id;
  const gchar *from;

  test_expected_stanza_received (test, iq);

  node = wocky_stanza_get_top_node (iq);
  id = wocky_node_get_attribute (node, "id");

  if (wocky_node_get_child (node, "first") != NULL)
    /* No from attribute */
    from = NULL;
  else if (wocky_node_get_child (node, "second") != NULL)
    /* bare JID */
    from = "juliet@example.com";
  else if (wocky_node_get_child (node, "third") != NULL)
    /* full JID */
    from = "juliet@example.com/Balcony";
  else
    g_assert_not_reached ();

  /* Send reply */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, from, "juliet@example.com/Balcony",
    '@', "id", id,
    NULL);

  wocky_porter_send_async (porter, reply,
      NULL, test_send_iq_sent_cb, test);
  g_queue_push_tail (test->expected_stanzas, reply);

  test->outstanding++;
  return TRUE;
}

static void
test_send_iq_server (void)
{
  /* In this test "in" is Juliet, and "out" is her server */
  test_data_t *test = setup_test_with_jids ("juliet@example.com/Balcony",
      "example.com");
  WockyStanza *iq;
  const gchar *node[] = { "first", "second", "third", NULL };
  guint i;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register an IQ handler */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      test_send_iq_server_received_cb, test, NULL);

  /* From XMPP RFC:
   * "When a server generates a stanza from the server itself for delivery to
   * a connected client (e.g., in the context of data storage services
   * provided by the server on behalf of the client), the stanza MUST either
   * (1) not include a 'from' attribute or (2) include a 'from' attribute
   * whose value is the account's bare JID (<node@domain>) or client's full
   * JID (<node@domain/resource>)".
   *
   * Each reply will test one of these 3 options. */

  for (i = 0; node[i] != NULL; i++)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
        WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", NULL,
        '(', node[i], ')',
        NULL);

      wocky_porter_send_iq_async (test->sched_in, iq,
          test->cancellable, test_send_iq_reply_cb, test);
      g_queue_push_tail (test->expected_stanzas, iq);
      test->outstanding += 2;
      test_wait_pending (test);
    }

  /* The same, but sending to our own bare JID. For instance, when we query
   * disco#info on our own bare JID on Prosody 0.6.1, the reply has no 'from'
   * attribute. */

  for (i = 0; node[i] != NULL; i++)
    {
      iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
        WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "JULIET@EXAMPLE.COM",
        '(', node[i], ')',
        NULL);

      wocky_porter_send_iq_async (test->sched_in, iq,
          test->cancellable, test_send_iq_reply_cb, test);
      g_queue_push_tail (test->expected_stanzas, iq);
      test->outstanding += 2;
      test_wait_pending (test);
    }

  test_close_both_porters (test);
  teardown_test (test);
}

/* Unref the porter in the async close callback */
static void
test_unref_when_closed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_close_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  /* Porter has been closed, unref it */
  g_object_unref (test->session_in);
  test->session_in = NULL;

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_unref_when_closed (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_stanza_received_cb, test);

  wocky_porter_close_async (test->sched_in, NULL,
      test_unref_when_closed_cb, test);

  test->outstanding += 3;
  test_wait_pending (test);

  teardown_test (test);
}

/* Both sides try to close the connection at the same time */
static void
test_close_simultanously_recv_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *s;
  GError *error = NULL;

  s = wocky_xmpp_connection_recv_stanza_finish (WOCKY_XMPP_CONNECTION (source),
      res, &error);

  g_assert (s == NULL);
  g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_simultanously (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);

  /* Sent close from one side */
  wocky_xmpp_connection_send_close_async (test->out, NULL,
      close_sent_cb, test);

  /* .. and from the other */
  wocky_porter_close_async (test->sched_in, NULL,
      test_unref_when_closed_cb, test);

  /* Wait that the 'in' side received the close */
  test->outstanding += 2;
  test_wait_pending (test);

  /* Now read the close on the 'out' side */
  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_simultanously_recv_stanza_cb, test);
  test->outstanding++;
  test_wait_pending (test);

  teardown_test (test);
}

/* We sent our close stanza but a reading error occurs (as a disconnection for
 * example) before the other side sends his close */
static void
test_close_error_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);

  g_error_free (error);
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_error (void)
{
  test_data_t *test = setup_test ();

  wocky_test_stream_set_write_mode (test->stream->stream0_output,
    WOCKY_TEST_STREAM_WRITE_COMPLETE);

  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);

  /* Sent close */
  wocky_porter_close_async (test->sched_in, NULL,
      test_close_error_cb, test);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_simultanously_recv_stanza_cb, test);

  /* Wait that the 'out' side received the close */
  test->outstanding += 1;
  test_wait_pending (test);

  /* Something goes wrong */
  wocky_test_input_stream_set_read_error (test->stream->stream0_input);

  /* The close operation is completed with an error */
  test->outstanding += 1;
  test_wait_pending (test);

  teardown_test (test);
}

/* Try to send an IQ using a closing porter and cancel it immediately */
static void
test_cancel_iq_closing (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);

  /* Start to close the porter */
  wocky_porter_close_async (test->sched_in, NULL,
      test_close_sched_close_cb, test);

  /* Try to send a stanza using the closing porter */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send_iq_async (test->sched_in, iq,
      test->cancellable, test_send_closing_cb, test);

  /* Cancel the sending */
  g_cancellable_cancel (test->cancellable);

  test->outstanding += 1;
  test_wait_pending (test);

  /* Make the call to wocky_porter_close_async() finish... */
  wocky_xmpp_connection_send_close_async (test->out, NULL, close_sent_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (iq);
  teardown_test (test);
}

/* test stream errors */
static void
test_stream_error_force_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_force_close_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_stream_error_cb (WockyPorter *porter,
    GQuark domain,
    guint code,
    const gchar *message,
    test_data_t *test)
{
  GError *err = g_error_new_literal (domain, code, message);

  g_assert_error (err, WOCKY_XMPP_STREAM_ERROR,
      WOCKY_XMPP_STREAM_ERROR_CONFLICT);
  g_error_free (err);

  /* force closing of the porter */
  wocky_porter_force_close_async (porter, NULL,
        test_stream_error_force_close_cb, test);
}

static void
test_stream_error (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *error;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);

  g_signal_connect (test->sched_out, "remote-error",
      G_CALLBACK (test_stream_error_cb), test);
  test->outstanding++;

  /* Try to send a stanza using the closing porter */
  error = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    ':', WOCKY_XMPP_NS_STREAM,
    '(', "conflict",
      ':', WOCKY_XMPP_NS_STREAMS,
    ')',
    NULL);

  wocky_porter_send_async (test->sched_in, error, NULL, send_stanza_cb,
      test);
  test->outstanding++;

  test_wait_pending (test);

  g_object_unref (error);
  teardown_test (test);
}

/* test wocky_porter_close_force */
static void
test_close_force_stanza_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_send_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_FORCIBLY_CLOSED);

  data->outstanding--;
  g_error_free (error);
  g_main_loop_quit (data->loop);
}

static void
test_close_force_closed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_porter_close_finish (
      WOCKY_PORTER (source), res, &error));
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_FORCIBLY_CLOSED);

  test->outstanding--;
  g_error_free (error);
  g_main_loop_quit (test->loop);
}

static void
test_close_force_force_closed_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_force_close_finish (WOCKY_PORTER (source), res, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_force_closing_cb (WockyPorter *porter,
    test_data_t *test)
{
  static gboolean fired = FALSE;

  g_assert (!fired);
  fired = TRUE;
  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_force (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *s;

  wocky_test_stream_set_write_mode (test->stream->stream0_output,
    WOCKY_TEST_STREAM_WRITE_COMPLETE);

  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);

  /* Try to send a stanza; it will never reach the other side */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

  g_signal_connect (test->sched_in, "closing",
      G_CALLBACK (test_close_force_closing_cb), test);

  wocky_porter_send_async (test->sched_in, s, NULL,
      test_close_force_stanza_sent_cb, test);

  /* Try to properly close the connection; we'll give up before it has been
   * done */
  wocky_porter_close_async (test->sched_in, NULL,
      test_close_force_closed_cb, test);

  /* force closing */
  wocky_porter_force_close_async (test->sched_in, NULL,
        test_close_force_force_closed_cb, test);

  test->outstanding += 4;
  test_wait_pending (test);

  g_object_unref (s);
  teardown_test (test);
}

/* call force_close after an error appeared on the connection */
static void
test_close_force_after_error_error_cb (WockyPorter *porter,
    GQuark domain,
    guint code,
    const gchar *message,
    test_data_t *test)
{
  GError *err = g_error_new_literal (domain, code, message);

  g_assert_error (err, WOCKY_XMPP_STREAM_ERROR,
      WOCKY_XMPP_STREAM_ERROR_CONFLICT);
  g_error_free (err);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_force_after_error (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *error;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);

  g_signal_connect (test->sched_out, "remote-error",
      G_CALLBACK (test_close_force_after_error_error_cb), test);
  test->outstanding++;

  error = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    ':', WOCKY_XMPP_NS_STREAM,
    '(', "conflict",
      ':', WOCKY_XMPP_NS_STREAMS,
    ')',
    NULL);

  wocky_porter_send_async (test->sched_in, error, NULL, send_stanza_cb,
      test);
  test->outstanding++;
  test_wait_pending (test);

  /* Stream error has been handled, now force closing */
  wocky_porter_force_close_async (test->sched_out, NULL,
      test_close_force_force_closed_cb, test);
  test->outstanding++;
  test_wait_pending (test);

  g_object_unref (error);
  teardown_test (test);
}

/* Test calling force_close after close has been called and the close stream
 * stanza has been sent */
static void
test_close_force_after_close_sent_stanza_cb (GObject *source,
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

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_force_after_close_sent (void)
{
  test_data_t *test = setup_test ();

  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_force_after_close_sent_stanza_cb, test);

  /* Try to properly close the connection; we'll give up before it has been
   * done */
  wocky_porter_close_async (test->sched_in, NULL,
      test_close_force_closed_cb, test);

  /* Wait for the close stanza */
  test->outstanding++;
  test_wait_pending (test);

  /* force closing */
  wocky_porter_force_close_async (test->sched_in, NULL,
        test_close_force_force_closed_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* The remote connection is closed while we are waiting for an IQ reply */
static void
open_connections_and_send_one_iq (test_data_t *test,
    GAsyncReadyCallback send_iq_callback)
{
  WockyStanza *iq;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register an IQ handler */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      0,
      test_receive_stanza_received_cb, test, NULL);

  /* Send an IQ query */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "1",
    NULL);

  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, send_iq_callback, test);
  g_queue_push_tail (test->expected_stanzas, iq);

  /* wait that the IQ has been received */
  test->outstanding += 1;
  test_wait_pending (test);
}

static void
test_wait_iq_reply_close_reply_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source),
      res, &error);
  g_assert (reply == NULL);
  g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_wait_iq_reply_close (void)
{
  test_data_t *test = setup_test ();

  open_connections_and_send_one_iq (test, test_wait_iq_reply_close_reply_cb);

  /* the other side closes the connection (and so won't send the IQ reply) */
  wocky_porter_close_async (test->sched_out, NULL, test_close_sched_close_cb,
      test);

  /* the send IQ operation is finished (with an error) */
  test->outstanding += 1;
  test_wait_pending (test);

  wocky_porter_close_async (test->sched_in, NULL, test_close_sched_close_cb,
      test);

  /* the 2 close operations are completed */
  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* Send an IQ and then force the closing of the connection before we received
 * the reply */
static void
test_wait_iq_reply_force_close_reply_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source),
      res, &error);
  g_assert (reply == NULL);
  g_assert_error (error, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_FORCIBLY_CLOSED);
  g_error_free (error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_wait_iq_reply_force_close (void)
{
  test_data_t *test = setup_test ();

  open_connections_and_send_one_iq (test,
      test_wait_iq_reply_force_close_reply_cb);

  /* force closing of our connection */
  wocky_porter_force_close_async (test->sched_in, NULL,
      test_close_force_force_closed_cb, test);

  /* the send IQ operation is finished (with an error) and the force close
   * operation is completed */
  test->outstanding += 2;
  test_wait_pending (test);

  wocky_porter_force_close_async (test->sched_out, NULL,
      test_close_force_force_closed_cb, test);

  test->outstanding += 1;
  test_wait_pending (test);

  teardown_test (test);
}

/* this tries to catch the case where we receive a remote-error, which   *
 * results in a wocky_porter_force_close_async in the connected signal   *
 * handler but the internal stanza_received_cb _also_ attempts to force  *
 * a shutdown: when correctly functioning, only one of these will result *
 * in a force close operation and the other will noop.                   *
 * Typical failure cases are the shutdown not happening at all or being  *
 * attempted twice and failing because the porter can only support one   *
 * force close attempt (both indicate broken logic in the porter)        */
static void
test_remote_error (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *error =
    wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
        WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
        ':', WOCKY_XMPP_NS_STREAM,
        '(', "conflict",
        ':', WOCKY_XMPP_NS_STREAMS,
        ')',
        NULL);

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* this callback will force_close_async the OUT porter, *
   * and decrement test->outstanding when it has done so  */
  g_signal_connect (test->sched_out, "remote-error",
      G_CALLBACK (test_stream_error_cb), test);

  /* this pumps a stream error through the IN (server) porter    *
   * which triggers the remote-error signal from the OUT porter */
  wocky_porter_send_async (test->sched_in, error, NULL, send_stanza_cb, test);
  test->outstanding++;
  test_wait_pending (test);

  test->outstanding++; /* this is for the signal connect above */

  /* this closes the IN (server) porter so that the test doesn't fail (we *
   * don't really care about the IN porter for the purposes of this test) */
  wocky_porter_force_close_async (test->sched_in, NULL,
      test_close_force_force_closed_cb, test);
  test->outstanding++;

  /* now wait for the IN porter to shutdown and the remote-error callback *
   * to shut down the OUT porter                                          */
  test_wait_pending (test);

  g_object_unref (error);
  teardown_test (test);
}

/* Herein lies a regression test for a bug where, if a stanza had been passed
 * by the porter to the XmppConnection but not actually sent when the porter
 * was unreffed, the subsequent callback from the XmppConnection would crash
 * us.
 */
static gboolean
idle_main_loop_quit (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return FALSE;
}

static void
send_and_disconnect (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *lions = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, NULL, NULL,
      '(', "dummy", ':', "xmpp:stanza",
        '(', "nothing-to-see-here", ')',
      ')', NULL);
  GMainLoop *loop = g_main_loop_ref (test->loop);

  /* We try to send any old stanza. */
  wocky_porter_send_async (test->sched_in, lions, NULL, NULL, NULL);

  /* Without giving the mainloop a chance to spin to call any callbacks at all,
   * we tear everything down, including the porters. This will make sched_in
   * add an idle to dispatch the (non-existent) callback for the stanza sent
   * above, saying that sending it failed.
   */
  teardown_test (test);

  /* Now we spin the main loop until all (higher-priority) idles have fired.
   *
   * The bug we're testing for occured because the porter didn't keep itself
   * alive for the duration of the wocky_xmpp_connection_send_stanza_async()
   * call. So, when it'd finished calling the callback above, it'd die, and
   * then the send_stanza() operations would finish, and the porter's callback
   * would try to use the newly-freed porter and choke.
   */
  g_idle_add_full (G_PRIORITY_LOW, idle_main_loop_quit, loop, NULL);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_object_unref (lions);
}

static gboolean
got_stanza_for_example_com (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  g_assert_cmpstr (wocky_stanza_get_from (stanza), ==, "example.com");
  test->outstanding--;
  g_main_loop_quit (test->loop);

  return TRUE;
}

/* This is a regression test for a bug where registering a handler for a JID
 * with no node part was equivalent to registering a handler with from=NULL;
 * that is, we'd erroneously pass stanzas from *any* server to the handler
 * function even if it explicitly specified a JID which was just a domain, as
 * opposed to a JID with an '@' sign in it.
 */
static void
handler_for_domain (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *irrelevant, *relevant;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  wocky_porter_register_handler_from (test->sched_in, WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "example.com",
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      got_stanza_for_example_com, test,
      NULL);

  /* Send a stanza from some other random jid (at example.com, for the sake of
   * argument). The porter should ignore this stanza.
   */
  irrelevant = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "lol@example.com", NULL,
      '(', "this-is-bullshit", ')', NULL);
  wocky_porter_send (test->sched_out, irrelevant);
  g_object_unref (irrelevant);

  relevant = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "example.com", NULL,
      '(', "i-am-a-fan-of-cocaine", ')', NULL);
  wocky_porter_send (test->sched_out, relevant);
  g_object_unref (relevant);

  test->outstanding += 1;
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

static gboolean
got_stanza_from_anyone (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;
  WockyNode *top = wocky_stanza_get_top_node (stanza);
  WockyNode *query = wocky_node_get_first_child (top);

  g_assert_cmpstr (query->name, ==, "anyone");
  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static gboolean
got_stanza_from_ourself (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;
  WockyNode *top = wocky_stanza_get_top_node (stanza);
  WockyNode *query = wocky_node_get_first_child (top);

  g_assert_cmpstr (query->name, ==, "ourself");
  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static gboolean
got_stanza_from_server (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;
  WockyNode *top = wocky_stanza_get_top_node (stanza);
  WockyNode *query = wocky_node_get_first_child (top);

  g_assert_cmpstr (query->name, ==, "server");
  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
send_query_from (
    test_data_t *test,
    const gchar *from,
    const gchar *query)
{
  WockyStanza *s = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, from, NULL,
      '(', query, ')', NULL);
  wocky_porter_send (test->sched_out, s);
  g_object_unref (s);

  test->outstanding += 1;
  test_wait_pending (test);
}

static void
handler_from_anyone (void)
{
  test_data_t *test = setup_test_with_jids ("juliet@capulet.lit/Balcony",
      "capulet.lit");

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);


  wocky_c2s_porter_register_handler_from_server (
      WOCKY_C2S_PORTER (test->sched_in),
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL + 10,
      got_stanza_from_server, test, NULL);

  /* A catch-all IQ get handler. */
  wocky_porter_register_handler_from_anyone (test->sched_in,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      got_stanza_from_anyone, test, NULL);

  /* And, for completeness, a handler for IQs sent by any incarnation
   * of ourself, at a lower priority to the handler for stanzas from the
   * server, but a higher priority to the catch-all handler. */
  wocky_porter_register_handler_from (test->sched_in,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      "juliet@capulet.lit",
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL + 5,
      got_stanza_from_ourself, test, NULL);

  /* All of the handlers assert on the name of the first child node, and then
   * return TRUE to prevent the stanza being handed to a lower-priority
   * handler. */

  /* A stanza from a contact on a completely different server should be picked
   * up only by the general handler. */
  send_query_from (test, "romeo@montague.lit/Garden", "anyone");

  /* A stanza from a contact on our server should be picked up only by the
   * general handler (irrespective of whether they have a resource). */
  send_query_from (test, "tybalt@capulet.lit", "anyone");
  send_query_from (test, "tybalt@capulet.lit/FIXME", "anyone");

  /* A stanza from our server's domain should be matched by
   * got_stanza_from_server(). See fd.o#39057. */
  send_query_from (test, "capulet.lit", "server");

  /* On the other hand, a stanza with no sender should be picked up by
   * got_stanza_from_server(). */
  send_query_from (test, NULL, "server");

  /* Similarly, stanzas from our bare JID should be handed to
   * got_stanza_from_server(). Because that function returns TRUE, the stanza
   * should not be handed to got_stanza_from_ourself().
   */
  send_query_from (test, "juliet@capulet.lit", "server");
  send_query_from (test, "jULIet@cAPUlet.lIT", "server");

  /* Similarly, stanzas from our own full JID go to got_stanza_from_server. */
  send_query_from (test, "juliet@capulet.lit/Balcony", "server");
  send_query_from (test, "JUlIet@CAPulet.LIt/Balcony", "server");

  /* But stanzas from our other resources should go to
   * got_stanza_from_ourself(). */
  send_query_from (test, "juliet@capulet.lit/FIXME", "ourself");
  /* Heh, heh, resources are case-sensitive */
  send_query_from (test, "juliet@capulet.lit/balcony", "ourself");

  /* Meanwhile, back in communist russia: */
  /*
  КАПУЛЭТ
  капулэт
  */

  test_close_both_porters (test);
  teardown_test (test);
}

static void
closed_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source);
  test_data_t *test = user_data;
  gboolean ret;
  GError *error = NULL;

  ret = wocky_porter_close_finish (porter, result, &error);
  g_assert_no_error (error);
  g_assert (ret);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
sent_stanza_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source);
  test_data_t *test = user_data;
  gboolean ret;
  GError *error = NULL;

  ret = wocky_porter_send_finish (porter, result, &error);
  g_assert_no_error (error);
  g_assert (ret);

  /* Close up both porters. There's no reason why either of these operations
   * should fail.
   */
  wocky_porter_close_async (test->sched_out, NULL, closed_cb, test);
  wocky_porter_close_async (test->sched_in, NULL, closed_cb, test);
}

static void
close_from_send_callback (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
      '(', "body", '$', "I am made of chalk.", ')', NULL);

  /* Fire up porters in both directions. */
  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);
  wocky_porter_start (test->sched_out);

  /* Send a stanza. Once it's been safely sent, we should be able to close up
   * the connection in both directions without any trouble.
   */
  wocky_porter_send_async (test->sched_in, stanza, NULL, sent_stanza_cb, test);
  g_object_unref (stanza);

  /* The two outstanding events are both porters ultimately closing
   * successfully. */
  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* Callbacks used in send_from_send_callback() */
static gboolean
message_received_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  test_expected_stanza_received (test, stanza);
  return TRUE;
}

static void
sent_second_or_third_stanza_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = user_data;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_send_finish (WOCKY_PORTER (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
sent_first_stanza_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  test_data_t *test = user_data;
  WockyStanza *third_stanza;
  GError *error = NULL;
  gboolean ok;

  ok = wocky_porter_send_finish (WOCKY_PORTER (source), result, &error);
  g_assert_no_error (error);
  g_assert (ok);

  third_stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
      '(', "body", '$', "I am made of dur butter.", ')', NULL);
  wocky_porter_send_async (test->sched_in, third_stanza, NULL,
      sent_second_or_third_stanza_cb, test);
  g_queue_push_tail (test->expected_stanzas, third_stanza);
  /* One for the callback; one for the receiving end. */
  test->outstanding += 2;

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
send_from_send_callback (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *stanza;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);
  wocky_porter_start (test->sched_out);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, message_received_cb, test,
      '(', "body", ')', NULL);

  /* Send a stanza; in the callback for this stanza, we'll send another stanza.
   */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
      '(', "body", '$', "I am made of chalk.", ')', NULL);
  wocky_porter_send_async (test->sched_in, stanza, NULL,
      sent_first_stanza_cb, test);
  g_queue_push_tail (test->expected_stanzas, stanza);
  /* One for the callback; one for the receiving end. */
  test->outstanding += 2;

  /* But before we've had a chance to send that one, send a second. */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
      '(', "body", '$', "I am made of jelly.", ')', NULL);
  wocky_porter_send_async (test->sched_in, stanza, NULL,
      sent_second_or_third_stanza_cb, test);
  g_queue_push_tail (test->expected_stanzas, stanza);
  /* One for the callback; one for the receiving end. */
  test->outstanding += 2;

  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

static gboolean
test_reply_from_domain_handler_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyStanza *reply;
  const gchar *id;

  test_expected_stanza_received (test, stanza);

  id = wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
      "id");

  /* Reply with from="domain" */
  reply = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "example.com", "juliet@example.com",
    '@', "id", id,
    NULL);
  wocky_porter_send_async (porter, reply,
      NULL, test_send_iq_sent_cb, test);
  g_queue_push_tail (test->expected_stanzas, reply);
  test->outstanding++;

  return TRUE;
}

static void
test_reply_from_domain (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *iq;

  g_test_bug ("39057");

  /* Testing that when we send an iq to server, it can reply from the domain
     instead of full/bare jid. This happens with xmpp.messenger.live.com.

     <iq type="get" id="1062691559">...</iq>
     <iq from="domain.com" id="1062691559"
         to="user@domain.com/resource">...</iq>
   */

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* register an IQ handler */
  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
      0,
      test_reply_from_domain_handler_cb, test, NULL);

  /* Send an IQ query */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, NULL, NULL,
    '@', "id", "1",
    NULL);

  wocky_porter_send_iq_async (test->sched_in, iq,
      NULL, test_send_iq_reply_cb, test);
  g_queue_push_tail (test->expected_stanzas, iq);

  test->outstanding += 2;
  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

/* Callbacks used in wildcard_handlers() */
const gchar * const ROMEO = "romeo@montague.lit";
const gchar * const JULIET = "juliet@montague.lit";

static gboolean
any_stanza_received_from_romeo_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  g_assert_cmpstr (wocky_stanza_get_from (stanza), ==, ROMEO);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return FALSE;
}

static gboolean
any_stanza_received_from_server_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  g_assert_cmpstr (wocky_stanza_get_from (stanza), ==, NULL);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return FALSE;
}

static gboolean
any_stanza_received_from_anyone_cb (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = user_data;

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return FALSE;
}

static void
wildcard_handlers (void)
{
  test_data_t *test = setup_test ();
  WockyStanza *stanza;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_in);
  wocky_porter_start (test->sched_out);

  wocky_porter_register_handler_from_anyone (test->sched_out,
      WOCKY_STANZA_TYPE_NONE, WOCKY_STANZA_SUB_TYPE_NONE,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      any_stanza_received_from_anyone_cb, test,
      NULL);
  wocky_porter_register_handler_from (test->sched_out,
      WOCKY_STANZA_TYPE_NONE, WOCKY_STANZA_SUB_TYPE_NONE,
      ROMEO,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      any_stanza_received_from_romeo_cb, test,
      NULL);
  wocky_c2s_porter_register_handler_from_server (
      WOCKY_C2S_PORTER (test->sched_out),
      WOCKY_STANZA_TYPE_NONE, WOCKY_STANZA_SUB_TYPE_NONE,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      any_stanza_received_from_server_cb, test,
      NULL);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_HEADLINE, ROMEO, NULL, NULL);
  wocky_porter_send (test->sched_in, stanza);
  g_object_unref (stanza);
  test->outstanding += 2;

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, JULIET, NULL, NULL);
  wocky_porter_send (test->sched_in, stanza);
  g_object_unref (stanza);
  test->outstanding += 1;

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_FEATURES,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL, NULL);
  wocky_porter_send (test->sched_in, stanza);
  g_object_unref (stanza);
  test->outstanding += 2;

  test_wait_pending (test);

  test_close_both_porters (test);
  teardown_test (test);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

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
  g_test_add_func ("/xmpp-porter/handler-full-jid", test_handler_full_jid);
  g_test_add_func ("/xmpp-porter/handler-stanza", test_handler_stanza);
  g_test_add_func ("/xmpp-porter/cancel-sent-stanza",
      test_cancel_sent_stanza);
  g_test_add_func ("/xmpp-porter/writing-error", test_writing_error);
  g_test_add_func ("/xmpp-porter/send-iq", test_send_iq);
  g_test_add_func ("/xmpp-porter/acknowledge-iq", test_acknowledge_iq);
  g_test_add_func ("/xmpp-porter/send-iq-error", test_send_iq_error);
  g_test_add_func ("/xmpp-porter/send-iq-gerror", test_send_iq_gerror);
  g_test_add_func ("/xmpp-porter/send-iq-denormalised", test_send_iq_abnormal);
  g_test_add_func ("/xmpp-porter/error-while-sending-iq",
      test_error_while_sending_iq);
  g_test_add_func ("/xmpp-porter/handler-filter", test_handler_filter);
  g_test_add_func ("/xmpp-porter/unhandled-iq", test_unhandled_iq);
  g_test_add_func ("/xmpp-porter/send-invalid-iq", test_send_invalid_iq);
  g_test_add_func ("/xmpp-porter/handler-filter-from",
      test_handler_filter_from);
  g_test_add_func ("/xmpp-porter/send-iq-server", test_send_iq_server);
  g_test_add_func ("/xmpp-porter/unref-when-closed", test_unref_when_closed);
  g_test_add_func ("/xmpp-porter/close-simultanously",
      test_close_simultanously);
  g_test_add_func ("/xmpp-porter/close-error", test_close_error);
  g_test_add_func ("/xmpp-porter/cancel-iq-closing", test_cancel_iq_closing);
  g_test_add_func ("/xmpp-porter/stream-error", test_stream_error);
  g_test_add_func ("/xmpp-porter/close-force", test_close_force);
  g_test_add_func ("/xmpp-porter/close-force-after-error",
      test_close_force_after_error);
  g_test_add_func ("/xmpp-porter/close-force-after-close-sent",
      test_close_force_after_close_sent);
  g_test_add_func ("/xmpp-porter/wait-iq-reply-close",
      test_wait_iq_reply_close);
  g_test_add_func ("/xmpp-porter/wait-iq-reply-force-close",
      test_wait_iq_reply_force_close);
  g_test_add_func ("/xmpp-porter/avoid-double-force-close", test_remote_error);
  g_test_add_func ("/xmpp-porter/send-and-disconnect", send_and_disconnect);
  g_test_add_func ("/xmpp-porter/handler-for-domain", handler_for_domain);
  g_test_add_func ("/xmpp-porter/handler-from-anyone", handler_from_anyone);
  g_test_add_func ("/xmpp-porter/close-from-send-callback",
      close_from_send_callback);
  g_test_add_func ("/xmpp-porter/send-from-send-callback",
      send_from_send_callback);
  g_test_add_func ("/xmpp-porter/reply-from-domain",
      test_reply_from_domain);
  g_test_add_func ("/xmpp-porter/wildcard-handlers", wildcard_handlers);

  result = g_test_run ();
  test_deinit ();
  return result;
}
