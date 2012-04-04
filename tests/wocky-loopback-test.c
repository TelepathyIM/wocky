#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

/* I'm not happy about this, but the existing test stuff really relies
 * on having multiple streams */
typedef struct
{
  test_data_t data;
  GIOStream *stream;
  WockyXmppConnection *conn;
  WockySession *session;
  WockyPorter *porter;
} loopback_test_t;

static gboolean
test_timeout (gpointer data)
{
  g_test_message ("Timeout reached :(");
  g_assert_not_reached ();

  return FALSE;
}

static loopback_test_t *
setup (void)
{
  loopback_test_t *test = g_slice_new0 (loopback_test_t);

  test->data.loop = g_main_loop_new (NULL, FALSE);
  test->data.cancellable = g_cancellable_new ();
  test->data.timeout_id = g_timeout_add_seconds (10, test_timeout, NULL);
  test->data.expected_stanzas = g_queue_new ();

  test->stream = wocky_loopback_stream_new ();

  test->conn = wocky_xmpp_connection_new (test->stream);

  test->session = wocky_session_new_with_connection (test->conn,
      "example.com");

  test->porter = wocky_session_get_porter (test->session);

  return test;
}

static void
send_received_open_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppConnection *conn = WOCKY_XMPP_CONNECTION (source);
  loopback_test_t *d = (loopback_test_t *) user_data;

  g_assert (wocky_xmpp_connection_recv_open_finish (conn, res,
      NULL, NULL, NULL, NULL, NULL, NULL));

  wocky_session_start (d->session);

  d->data.outstanding--;
  g_main_loop_quit (d->data.loop);
}

static void
send_open_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppConnection *conn = WOCKY_XMPP_CONNECTION (source);

  g_assert (wocky_xmpp_connection_send_open_finish (conn,
      res, NULL));

  wocky_xmpp_connection_recv_open_async (conn,
      NULL, send_received_open_cb, user_data);
}

static void
start_test (loopback_test_t *test)
{
  wocky_xmpp_connection_send_open_async (test->conn,
      NULL, NULL, NULL, NULL, NULL, NULL, send_open_cb, test);

  test->data.outstanding++;

  test_wait_pending (&(test->data));
}

static void
close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  loopback_test_t *test = user_data;

  g_assert (wocky_porter_close_finish (WOCKY_PORTER (source),
          res, NULL));

  test->data.outstanding--;
  g_main_loop_quit (test->data.loop);

  g_main_loop_unref (test->data.loop);

  g_object_unref (test->session);
  g_object_unref (test->conn);
  g_object_unref (test->data.cancellable);
  g_source_remove (test->data.timeout_id);

  g_assert (g_queue_get_length (test->data.expected_stanzas) == 0);
  g_queue_free (test->data.expected_stanzas);

  g_slice_free (loopback_test_t, test);
}

static void
cleanup (loopback_test_t *test)
{
  wocky_porter_close_async (test->porter, NULL, close_cb, test);

  test->data.outstanding++;
  test_wait_pending (&(test->data));
}

static void
send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;

  g_assert (wocky_porter_send_finish (
          WOCKY_PORTER (source), res, NULL));

  data->outstanding--;
  g_main_loop_quit (data->loop);
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
test_receive (void)
{
  loopback_test_t *test = setup ();
  WockyStanza *s;

  start_test (test);

  wocky_porter_register_handler_from_anyone (test->porter,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE, 0,
      test_receive_stanza_received_cb, test, NULL);

  /* Send a stanza */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
    NULL);

  wocky_porter_send_async (test->porter, s, NULL,
      send_stanza_cb, test);
  g_queue_push_tail (test->data.expected_stanzas, s);
  /* We are waiting for the stanza to be sent and received on the other
   * side */
  test->data.outstanding += 2;

  test_wait_pending (&(test->data));

  cleanup (test);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/loopback-porter/receive", test_receive);

  result = g_test_run ();
  test_deinit ();
  return result;
}
