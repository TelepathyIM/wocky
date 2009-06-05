#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-scheduler.h>
#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

static void
test_instantiation (void)
{
  WockyXmppScheduler *scheduler;
  WockyXmppConnection *connection;
  WockyTestStream *stream;;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  scheduler = wocky_xmpp_scheduler_new (connection);

  g_assert (scheduler != NULL);

  g_object_unref (scheduler);
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
  g_assert (wocky_xmpp_scheduler_send_full_finish (
      WOCKY_XMPP_SCHEDULER (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
send_stanza_cancelled_cb (GObject *source, GAsyncResult *res,
  gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  GError *error = NULL;

  g_assert (!wocky_xmpp_scheduler_send_full_finish (
      WOCKY_XMPP_SCHEDULER (source), res, &error));
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
  GCancellable *cancellable;

  test_open_connection (test);

  /* Send a stanza */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send_full (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send a stanza */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "tybalt@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send_full (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send two stanzas and cancel them immediately */
  cancellable = g_cancellable_new ();

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "peter@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send_full (test->sched_in, s, cancellable,
      send_stanza_cancelled_cb, test);
  g_object_unref (s);
  test->outstanding++;

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "samson@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send_full (test->sched_in, s, cancellable,
      send_stanza_cancelled_cb, test);
  g_object_unref (s);
  test->outstanding++;

  /* the stanza are not added to expected_stanzas as it was cancelled */
  g_cancellable_cancel (cancellable);

  /* ... and a second (using the simple send method) */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "nurse@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send (test->sched_in, s);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  /* Send a last stanza using the full method so we are sure that all async
   * sending operation have been finished. This is important because
   * test_close_connection() will have to use this function to close the
   * connection. If there is still a pending sending operation, it will fail. */
  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "tybalt@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send_full (test->sched_in, s, NULL, send_stanza_cb,
      test);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      send_stanza_received_cb, test);

  g_object_unref (cancellable);

  test_wait_pending (test);

  test_close_connection (test);
  teardown_test (test);
}

/* receive testing */
static void
test_receive_stanza_received_cb (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *expected;

  expected = g_queue_pop_head (test->expected_stanzas);
  g_assert (expected != NULL);
  g_assert (wocky_xmpp_node_equal (stanza->node, expected->node));

  test->outstanding--;
  g_main_loop_quit (test->loop);
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

  wocky_xmpp_scheduler_send_full (test->sched_in, s, NULL,
      send_stanza_cb, test);
  g_queue_push_tail (test->expected_stanzas, s);
  /* We are waiting for the stanza to be sent and received on the other
   * side */
  test->outstanding += 2;

  wocky_xmpp_scheduler_add_stanza_filter (test->sched_out, NULL,
      test_receive_stanza_received_cb, test);

  wocky_xmpp_scheduler_start (test->sched_out);

  test_wait_pending (test);
  g_object_unref (s);

  /* close connections */
  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
      wait_close_cb, test);

  wocky_xmpp_scheduler_close (test->sched_out, NULL, sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

/* filter testing */
static gboolean
test_filter_iq_filter (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  WockyStanzaType type;

  wocky_xmpp_stanza_get_type_info (stanza, &type, NULL);
  return type == WOCKY_STANZA_TYPE_IQ;
}

static void
test_filter_iq_received_cb (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *expected;

  expected = g_queue_pop_head (test->expected_stanzas);
  g_assert (expected != NULL);
  g_assert (wocky_xmpp_node_equal (stanza->node, expected->node));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static gboolean
test_filter_presence_filter (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  WockyStanzaType type;

  wocky_xmpp_stanza_get_type_info (stanza, &type, NULL);
  return type == WOCKY_STANZA_TYPE_PRESENCE;
}

static void
test_filter_presence_received_cb (WockyXmppScheduler *scheduler,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  /* We didn't send any presence stanza so this callback shouldn't be
   * called */
  g_assert_not_reached ();
}

static void
test_filter (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *msg, *iq;

  test_open_both_connections (test);

  /* register an IQ filter */
  wocky_xmpp_scheduler_add_stanza_filter (test->sched_out,
      test_filter_iq_filter, test_filter_iq_received_cb, test);

  /* register a presence filter */
  wocky_xmpp_scheduler_add_stanza_filter (test->sched_out,
      test_filter_presence_filter, test_filter_presence_received_cb, test);

  wocky_xmpp_scheduler_start (test->sched_out);

  /* Send a message */
  msg = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send (test->sched_in, msg);
  /* We don't expect this stanza as we didn't register any message filter */

  /* Send an IQ */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);

  wocky_xmpp_scheduler_send (test->sched_in, iq);
  /* We expect to receive this stanza */
  g_queue_push_tail (test->expected_stanzas, iq);
  test->outstanding++;

  test_wait_pending (test);
  g_object_unref (msg);
  g_object_unref (iq);

  /* close connections */
  wocky_xmpp_connection_recv_stanza_async (test->in, NULL,
      wait_close_cb, test);

  wocky_xmpp_scheduler_close (test->sched_out, NULL, sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);

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
  g_assert (wocky_xmpp_scheduler_close_finish (
      WOCKY_XMPP_SCHEDULER (source), res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_close_flush (void)
{
  test_data_t *test = setup_test ();
  WockyXmppStanza *s;

  test_open_both_connections (test);

  wocky_xmpp_scheduler_start (test->sched_in);

  s = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    WOCKY_STANZA_END);
  wocky_xmpp_scheduler_send (test->sched_in, s);
  g_queue_push_tail (test->expected_stanzas, s);
  test->outstanding++;

  wocky_xmpp_connection_recv_stanza_async (test->out, NULL,
      test_close_stanza_received_cb, test);

  wocky_xmpp_scheduler_close (test->sched_in, NULL, test_close_sched_close_cb,
      test);

  test->outstanding += 2;
  test_wait_pending (test);

  teardown_test (test);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-scheduler/initiation", test_instantiation);
  g_test_add_func ("/xmpp-scheduler/send", test_send);
  g_test_add_func ("/xmpp-scheduler/receive", test_receive);
  g_test_add_func ("/xmpp-scheduler/filter", test_filter);
  g_test_add_func ("/xmpp-scheduler/close-flush", test_close_flush);
  return g_test_run ();
}
