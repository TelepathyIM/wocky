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
