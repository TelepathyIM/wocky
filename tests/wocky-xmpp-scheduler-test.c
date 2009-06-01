#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-scheduler.h>
#include "wocky-test-stream.h"

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

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-scheduler/initiation", test_instantiation);
  return g_test_run ();
}
