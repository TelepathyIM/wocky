#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-session.h>
#include <wocky/wocky-utils.h>

#include "wocky-test-helper.h"

static void
test_instantiation (void)
{
  WockyTestStream *stream;
  WockyXmppConnection *connection;
  WockySession *session;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);

  session = wocky_session_new (connection);
  g_assert (session != NULL);
  g_assert (WOCKY_IS_SESSION (session));

  g_object_unref (stream);
  g_object_unref (connection);
  g_object_unref (session);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/session/instantiation", test_instantiation);

  result = g_test_run ();
  test_deinit ();
  return result;
}
