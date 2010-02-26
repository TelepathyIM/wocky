#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pubsub-node.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"


/* Test instantiating a WockyPubsubNode object */
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

/* test wocky_pubsub_node_make_publish_stanza() */
static void
test_make_publish_stanza (void)
{
  WockyPubsubService *pubsub;
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  WockySession *session;
  WockyPubsubNode *node;
  WockyXmppStanza *stanza, *expected;
  WockyXmppNode *publish, *item;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  session = wocky_session_new (connection);
  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  node = wocky_pubsub_node_new (pubsub, "track1");

  stanza = wocky_pubsub_node_make_publish_stanza (node, &publish, &item);
  g_assert (stanza != NULL);
  g_assert (publish != NULL);
  g_assert (item != NULL);

  /* I've embraced and extended pubsub, and want to put stuff on the <publish>
   * node... */
  wocky_xmpp_node_set_attribute (publish, "kaki", "king");

  /* Oh, and I should probably publish something. */
  wocky_xmpp_node_add_child_with_content_ns (item, "castle", "bone chaos",
      "urn:example:songs");

  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET,
      NULL, "pubsub.localhost",
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE, "publish",
            WOCKY_NODE_ATTRIBUTE, "kaki", "king",
            WOCKY_NODE_ATTRIBUTE, "node", "track1",
            WOCKY_NODE, "item",
              WOCKY_NODE, "castle",
                WOCKY_NODE_XMLNS, "urn:example:songs",
                WOCKY_NODE_TEXT, "bone chaos",
              WOCKY_NODE_END,
            WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  test_assert_nodes_equal (stanza->node, expected->node);

  g_object_unref (expected);
  g_object_unref (stanza);
  g_object_unref (node);
  g_object_unref (pubsub);
  g_object_unref (session);
  g_object_unref (connection);
  g_object_unref (stream);
}

/* test wocky_pubsub_node_delete_async */
static gboolean
test_delete_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_result (stanza, WOCKY_STANZA_END);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_delete_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;

  g_assert (wocky_pubsub_node_delete_finish (WOCKY_PUBSUB_NODE (source),
        res, NULL));

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_delete (void)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_delete_iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "delete",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  node = wocky_pubsub_service_ensure_node (pubsub, "node1");
  g_assert (node != NULL);

  wocky_pubsub_node_delete_async (node, NULL, test_delete_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (node);
  g_object_unref (pubsub);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pubsub-node/instantiation", test_instantiation);
  g_test_add_func ("/pubsub-node/make-publish-stanza", test_make_publish_stanza);
  g_test_add_func ("/pubsub-node/delete", test_delete);

  result = g_test_run ();
  test_deinit ();
  return result;
}
