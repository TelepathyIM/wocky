#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pubsub-service.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"


/* Test to instantiate a WockyPubsubService object */
static WockySession *
create_session (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  WockySession *session;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  session = wocky_session_new (connection);

  g_object_unref (connection);
  g_object_unref (stream);
  return session;
}

static void
test_instantiation (void)
{
  WockyPubsubService *pubsub;
  WockySession *session;

  session = create_session ();

  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  g_assert (pubsub != NULL);

  g_object_unref (pubsub);
  g_object_unref (session);
}

/* Test wocky_pubsub_service_ensure_node */
static void
test_ensure_node (void)
{
  WockyPubsubService *pubsub;
  WockySession *session;
  WockyPubsubNode *node;

  session = create_session ();

  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  node = wocky_pubsub_service_lookup_node (pubsub, "node1");
  g_assert (node == NULL);

  node = wocky_pubsub_service_ensure_node (pubsub, "node1");
  g_assert (node != NULL);

  node = wocky_pubsub_service_lookup_node (pubsub, "node1");
  g_assert (node != NULL);

  /* destroy the node */
  g_object_unref (node);
  node = wocky_pubsub_service_lookup_node (pubsub, "node1");
  g_assert (node == NULL);

  g_object_unref (pubsub);
  g_object_unref (session);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pubsub-service/instantiation", test_instantiation);
  g_test_add_func ("/pubsub-service/ensure_node", test_ensure_node);

  result = g_test_run ();
  test_deinit ();
  return result;
}
