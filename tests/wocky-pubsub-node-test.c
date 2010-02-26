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

/* Test that the 'event-received' signal is properly fired */
gboolean event_received;

static void
event_received_cb (WockyPubsubNode *node,
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *items_node,
    GList *items,
    test_data_t *test)
{
  WockyXmppNode *item;

  g_assert_cmpstr ("event", ==, event_node->name);
  g_assert_cmpstr ("items", ==, items_node->name);
  g_assert_cmpuint (2, ==, g_list_length (items));

  item = g_list_nth_data (items, 0);
  g_assert_cmpstr ("item", ==, item->name);
  g_assert_cmpstr ("1", ==, wocky_xmpp_node_get_attribute (item, "id"));

  item = g_list_nth_data (items, 1);
  g_assert_cmpstr ("item", ==, item->name);
  g_assert_cmpstr ("snakes", ==, wocky_xmpp_node_get_attribute (item, "id"));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  event_received = TRUE;
}

static void
send_pubsub_event (WockyPorter *porter,
    const gchar *service,
    const gchar *node)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE,
      service, NULL,
      WOCKY_NODE, "event",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_EVENT,
        WOCKY_NODE, "items",
        WOCKY_NODE_ATTRIBUTE, "node", node,
          WOCKY_NODE, "item",
            WOCKY_NODE_ATTRIBUTE, "id", "1",
            WOCKY_NODE, "payload", WOCKY_NODE_END,
          WOCKY_NODE_END,
          WOCKY_NODE, "item",
            WOCKY_NODE_ATTRIBUTE, "id", "snakes",
            WOCKY_NODE, "payload", WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, stanza);
  g_object_unref (stanza);
}

static void
test_receive_event (void)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  pubsub = wocky_pubsub_service_new (test->session_out, "pubsub.localhost");
  node = wocky_pubsub_service_ensure_node (pubsub, "lol");

  g_signal_connect (node, "event-received", (GCallback) event_received_cb, test);

  /* send event from the right service and node */
  event_received = FALSE;
  send_pubsub_event (test->sched_in, "pubsub.localhost", "lol");

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (event_received);

  event_received = FALSE;

  /* send event from the right service on a different node */
  send_pubsub_event (test->sched_in, "pubsub.localhost", "whut");

  /* send event from a different service, on a node with the same name */
  send_pubsub_event (test->sched_in, "pubsub.elsewhere", "lol");

  g_object_unref (node);
  g_object_unref (pubsub);

  /* send event from the right service and node, after we dropped our ref to
   * the node. nothing else should be keeping it hanging around, so our signal
   * handler should have been disconnected. */
  send_pubsub_event (test->sched_in, "pubsub.localhost", "lol");

  test_close_both_porters (test);
  teardown_test (test);

  /* None of the subsequent events should have triggered event-received. */
  g_assert (!event_received);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pubsub-node/instantiation", test_instantiation);
  g_test_add_func ("/pubsub-node/make-publish-stanza", test_make_publish_stanza);
  g_test_add_func ("/pubsub-node/delete", test_delete);
  g_test_add_func ("/pubsub-node/receive-event", test_receive_event);

  result = g_test_run ();
  test_deinit ();
  return result;
}
