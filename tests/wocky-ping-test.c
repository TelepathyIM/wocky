#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-ping.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-xmpp-error.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

#define PING_COUNT 3
#define PING_INTERVAL 1

#define TOTAL_TIMEOUT (PING_COUNT * PING_INTERVAL + 1)

static gboolean
ping_recv_cb (WockyPorter *porter, WockyStanza *stanza, gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  WockyStanza *reply;

  reply = wocky_stanza_build_iq_result (stanza, NULL);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  data->outstanding--;
  g_main_loop_quit (data->loop);
  return TRUE;
}

static void
test_periodic_ping (void)
{
  WockyPing *ping;
  test_data_t *test = setup_test_with_timeout (TOTAL_TIMEOUT);

  ping = wocky_ping_new (test->sched_in, PING_INTERVAL);

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);
  wocky_porter_start (test->sched_out);

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, ping_recv_cb, test,
      '(', "ping",
          ':', WOCKY_XMPP_NS_PING,
      ')', NULL);

  test->outstanding += PING_COUNT;

  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (ping);

  teardown_test (test);
}

static void
send_ping_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  test_data_t *data = (test_data_t *) user_data;
  WockyStanza *reply;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, NULL);
  g_assert (reply != NULL);
  g_object_unref (reply);

  data->outstanding--;
  g_main_loop_quit (data->loop);
}

static void
test_pong (void)
{
  WockyStanza *s;
  WockyPing *ping;
  test_data_t *test = setup_test ();

  ping = wocky_ping_new (test->sched_in, 0);

  test_open_both_connections (test);

  wocky_porter_start (test->sched_in);
  wocky_porter_start (test->sched_out);

  /* Server pings us */
  s = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "capulet.lit", "juliet@capulet.lit/balcony",
      '(', "ping",
        ':', WOCKY_XMPP_NS_PING,
      ')', NULL);

  wocky_porter_send_iq_async (test->sched_out, s, NULL, send_ping_cb, test);
  g_object_unref (s);

  test->outstanding++;

  test_wait_pending (test);

  test_close_both_porters (test);
  g_object_unref (ping);

  teardown_test (test);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-ping/pong", test_pong);
  g_test_add_func ("/xmpp-ping/periodic", test_periodic_ping);

  result = g_test_run ();
  test_deinit ();
  return result;
}

