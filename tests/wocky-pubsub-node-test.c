#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pubsub-node.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"


/* Test to instantiate a WockyPubsubNode object */
static void
test_instantiation (void)
{
  WockyPubsubService *pubsub;
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  WockySession *session;
  WockyPubsubNode *node;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  session = wocky_session_new (connection);

  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  g_assert (pubsub != NULL);

  node = wocky_pubsub_node_new (pubsub, "node1");
  g_assert (node != NULL);

  g_assert (!wocky_strdiff (wocky_pubsub_node_get_name (node), "node1"));

  g_object_unref (node);
  g_object_unref (pubsub);
  g_object_unref (session);
  g_object_unref (connection);
  g_object_unref (stream);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pubsub-node/instantiation", test_instantiation);

  result = g_test_run ();
  test_deinit ();
  return result;
}
