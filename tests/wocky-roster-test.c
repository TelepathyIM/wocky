#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-roster.h>
#include <wocky/wocky-porter.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-xmpp-connection.h>

#include "wocky-test-stream.h"

static void
test_instantiation (void)
{
  WockyRoster *roster;
  WockyXmppConnection *connection;
  WockyPorter *porter;
  WockyTestStream *stream;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  porter = wocky_porter_new (connection);

  roster = wocky_roster_new (connection, porter);

  g_assert (roster != NULL);

  g_object_unref (roster);
  g_object_unref (porter);
  g_object_unref (connection);
  g_object_unref (stream);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-roster/instantiation", test_instantiation);

  return g_test_run ();
}
