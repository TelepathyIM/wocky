#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pubsub-node.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-pubsub-test-helpers.h"
#include "wocky-test-helper.h"
#include "wocky-test-stream.h"


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

  node = wocky_pubsub_service_ensure_node (pubsub, "node1");
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
  WockyXmppNode *pubsub_node, *publish, *item;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);
  session = wocky_session_new (connection);
  pubsub = wocky_pubsub_service_new (session, "pubsub.localhost");
  node = wocky_pubsub_service_ensure_node (pubsub, "track1");

  stanza = wocky_pubsub_node_make_publish_stanza (node, &pubsub_node, &publish,
      &item);
  g_assert (stanza != NULL);
  g_assert (pubsub_node != NULL);
  g_assert (publish != NULL);
  g_assert (item != NULL);

  /* I've embraced and extended pubsub, and want to put stuff on the <pubsub>
   * and <publish> nodes... */
  wocky_xmpp_node_set_attribute (pubsub_node, "gig", "tomorrow");
  wocky_xmpp_node_set_attribute (publish, "kaki", "king");

  /* Oh, and I should probably publish something. */
  wocky_xmpp_node_add_child_with_content_ns (item, "castle", "bone chaos",
      "urn:example:songs");

  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET,
      NULL, "pubsub.localhost",
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE_ATTRIBUTE, "gig", "tomorrow",
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

/* Test subscribing to a node. */
static gboolean
test_subscribe_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE, "subscription",
            WOCKY_NODE_ATTRIBUTE, "node", "node1",
            WOCKY_NODE_ATTRIBUTE, "jid", "mighty@pirate.lit",
            WOCKY_NODE_ATTRIBUTE, "subscription", "subscribed",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_subscribe_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPubsubNode *node = WOCKY_PUBSUB_NODE (source);
  test_data_t *test = (test_data_t *) user_data;
  WockyPubsubSubscription *sub;

  sub = wocky_pubsub_node_subscribe_finish (WOCKY_PUBSUB_NODE (source),
      res, NULL);
  g_assert (sub != NULL);
  /* the node name should be the same. */
  g_assert_cmpstr (wocky_pubsub_node_get_name (sub->node),
      ==, wocky_pubsub_node_get_name (node));
  /* in fact, they should be the same node. */
  g_assert (sub->node == node);
  wocky_pubsub_subscription_free (sub);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_subscribe (void)
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
      test_subscribe_iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "subscribe",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
          WOCKY_NODE_ATTRIBUTE, "jid", "mighty@pirate.lit",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  node = wocky_pubsub_service_ensure_node (pubsub, "node1");
  g_assert (node != NULL);

  wocky_pubsub_node_subscribe_async (node, "mighty@pirate.lit", NULL,
      test_subscribe_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (node);
  g_object_unref (pubsub);

  test_close_both_porters (test);
  teardown_test (test);
}

/* Test unsubscribing from a node. */
typedef struct {
    test_data_t *test;
    gboolean expect_subid;
} TestUnsubscribeCtx;

#define EXPECTED_SUBID "⚞♥⚟"

static gboolean
test_unsubscribe_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  TestUnsubscribeCtx *ctx = user_data;
  test_data_t *test = ctx->test;
  WockyXmppNode *unsubscribe;
  const gchar *subid;
  WockyXmppStanza *reply;

  unsubscribe = wocky_xmpp_node_get_child (
      wocky_xmpp_node_get_child_ns (stanza->node,
          "pubsub", WOCKY_XMPP_NS_PUBSUB),
      "unsubscribe");
  g_assert (unsubscribe != NULL);

  subid = wocky_xmpp_node_get_attribute (unsubscribe, "subid");

  if (ctx->expect_subid)
    g_assert_cmpstr (EXPECTED_SUBID, ==, subid);
  else
    g_assert_cmpstr (NULL, ==, subid);

  reply = wocky_xmpp_stanza_build_iq_result (stanza, WOCKY_STANZA_END);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_unsubscribe_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  TestUnsubscribeCtx *ctx = user_data;
  test_data_t *test = ctx->test;
  gboolean ret;

  ret = wocky_pubsub_node_unsubscribe_finish (WOCKY_PUBSUB_NODE (source), res,
      NULL);
  g_assert (ret);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_unsubscribe (void)
{
  test_data_t *test = setup_test ();
  TestUnsubscribeCtx ctx = { test, };
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_unsubscribe_iq_cb, &ctx,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "unsubscribe",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
          WOCKY_NODE_ATTRIBUTE, "jid", "mighty@pirate.lit",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  node = wocky_pubsub_service_ensure_node (pubsub, "node1");
  g_assert (node != NULL);

  /* first, test unsubscribing with no subid */
  ctx.expect_subid = FALSE;
  wocky_pubsub_node_unsubscribe_async (node, "mighty@pirate.lit", NULL,
      NULL, test_unsubscribe_cb, &ctx);
  test->outstanding += 2;
  test_wait_pending (test);

  /* then test unsubscribing with a subid */
  ctx.expect_subid = TRUE;
  wocky_pubsub_node_unsubscribe_async (node, "mighty@pirate.lit",
      EXPECTED_SUBID, NULL, test_unsubscribe_cb, &ctx);
  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (node);
  g_object_unref (pubsub);

  test_close_both_porters (test);
  teardown_test (test);
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

  test_close_both_porters (test);
  teardown_test (test);
}

/* Test retrieving a list of subscribers. See XEP-0060 §8.8.1 Retrieve
 * Subscriptions List
 * <http://xmpp.org/extensions/xep-0060.html#owner-subscriptions-retrieve>
 */

static CannedSubscriptions example_183[] = {
    { "princely_musings",
      "hamlet@denmark.lit",
      "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
      NULL },
    { "princely_musings",
      "polonius@denmark.lit",
      "unconfigured", WOCKY_PUBSUB_SUBSCRIPTION_UNCONFIGURED,
      NULL },
    { "princely_musings",
      "bernardo@denmark.lit",
      "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
      "123-abc" },
    { "princely_musings",
      "bernardo@denmark.lit",
      "subscribed", WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
      "004-yyy" },
    { NULL, }
};

static gboolean
test_list_subscribers_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *expected, *reply;
  WockyXmppNode *subscriptions;

  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, NULL, "pubsub.localhost",
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "subscriptions",
          WOCKY_NODE_ATTRIBUTE, "node", "princely_musings",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  test_assert_stanzas_equal (stanza, expected);

  g_object_unref (expected);

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "subscriptions",
          WOCKY_NODE_ATTRIBUTE, "node", "princely_musings",
          WOCKY_NODE_ASSIGN_TO, &subscriptions,
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);
  test_pubsub_add_subscription_nodes (subscriptions, example_183, FALSE);
  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_list_subscribers_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GList *subscribers;

  g_assert (wocky_pubsub_node_list_subscribers_finish (
      WOCKY_PUBSUB_NODE (source), res, &subscribers, NULL));

  test_pubsub_check_and_free_subscriptions (subscribers, example_183);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_list_subscribers (void)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_list_subscribers_iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "subscriptions",
          WOCKY_NODE_ATTRIBUTE, "node", "princely_musings",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  node = wocky_pubsub_service_ensure_node (pubsub, "princely_musings");
  g_assert (node != NULL);

  wocky_pubsub_node_list_subscribers_async (node, NULL,
      test_list_subscribers_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (node);
  g_object_unref (pubsub);

  test_close_both_porters (test);
  teardown_test (test);
}


/* Test retrieving a list of entities affiliated to a node you own. See
 * XEP-0060 §8.9.1 Retrieve Affiliations List
 * <http://xmpp.org/extensions/xep-0060.html#owner-affiliations-retrieve>
 */

static gboolean
test_list_affiliates_iq_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *expected, *reply;

  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, NULL, "pubsub.localhost",
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "affiliations",
          WOCKY_NODE_ATTRIBUTE, "node", "princely_musings",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  test_assert_stanzas_equal (stanza, expected);

  g_object_unref (expected);

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "affiliations",
          WOCKY_NODE_ATTRIBUTE, "node", "princely_musings",
          WOCKY_NODE, "affiliation",
            WOCKY_NODE_ATTRIBUTE, "jid", "hamlet@denmark.lit",
            WOCKY_NODE_ATTRIBUTE, "affiliation", "owner",
          WOCKY_NODE_END,
          WOCKY_NODE, "affiliation",
            WOCKY_NODE_ATTRIBUTE, "jid", "polonius@denmark.lit",
            WOCKY_NODE_ATTRIBUTE, "affiliation", "outcast",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
  return TRUE;
}

static void
test_list_affiliates_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  GList *affiliates;
  WockyPubsubAffiliation *aff;

  g_assert (wocky_pubsub_node_list_affiliates_finish (
      WOCKY_PUBSUB_NODE (source), res, &affiliates, NULL));

  g_assert_cmpuint (2, ==, g_list_length (affiliates));

  aff = affiliates->data;
  g_assert_cmpstr ("princely_musings", ==,
      wocky_pubsub_node_get_name (aff->node));
  g_assert_cmpstr ("hamlet@denmark.lit", ==, aff->jid);
  g_assert_cmpuint (WOCKY_PUBSUB_AFFILIATION_OWNER, ==, aff->state);

  aff = affiliates->next->data;
  g_assert_cmpstr ("princely_musings", ==,
      wocky_pubsub_node_get_name (aff->node));
  g_assert_cmpstr ("polonius@denmark.lit", ==, aff->jid);
  g_assert_cmpuint (WOCKY_PUBSUB_AFFILIATION_OUTCAST, ==, aff->state);

  wocky_pubsub_affiliation_list_free (affiliates);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_list_affiliates (void)
{
  test_data_t *test = setup_test ();
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);

  wocky_porter_start (test->sched_out);
  wocky_session_start (test->session_in);

  pubsub = wocky_pubsub_service_new (test->session_in, "pubsub.localhost");

  wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_list_affiliates_iq_cb, test,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "affiliations",
          WOCKY_NODE_ATTRIBUTE, "node", "princely_musings",
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

  node = wocky_pubsub_service_ensure_node (pubsub, "princely_musings");
  g_assert (node != NULL);

  wocky_pubsub_node_list_affiliates_async (node, NULL,
      test_list_affiliates_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  g_object_unref (node);
  g_object_unref (pubsub);

  test_close_both_porters (test);
  teardown_test (test);
}

/* Test that the 'event-received' signals are fired when we expect them to be.
 */

gboolean service_event_received;
gboolean node_event_received;
WockyPubsubNode *expected_node;

static void
service_event_received_cb (WockyPubsubService *service,
    WockyPubsubNode *node,
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *items_node,
    GList *items,
    test_data_t *test)
{
  WockyXmppNode *item;

  /* Check that we're not winding up with multiple nodes for the same thing. */
  if (expected_node != NULL)
    g_assert (node == expected_node);

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
  service_event_received = TRUE;
}

static void
node_event_received_cb (WockyPubsubNode *node,
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
  node_event_received = TRUE;
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

  g_signal_connect (pubsub, "event-received",
      (GCallback) service_event_received_cb, test);
  g_signal_connect (node, "event-received",
      (GCallback) node_event_received_cb, test);

  /* send event from the right service for that node */
  node_event_received = FALSE;
  service_event_received = FALSE;
  expected_node = node;
  send_pubsub_event (test->sched_in, "pubsub.localhost", "lol");

  test->outstanding += 2;
  test_wait_pending (test);
  g_assert (node_event_received);
  g_assert (service_event_received);

  node_event_received = FALSE;
  service_event_received = FALSE;

  /* send event from the right service on a different node */
  expected_node = NULL;
  send_pubsub_event (test->sched_in, "pubsub.localhost", "whut");

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (!node_event_received);
  g_assert (service_event_received);

  service_event_received = FALSE;

  /* send event from a different service, on a node with the same name */
  send_pubsub_event (test->sched_in, "pubsub.elsewhere", "lol");

  g_object_unref (node);
  g_object_unref (pubsub);

  /* send event from the right service and node, after we dropped our ref to
   * the node and service. nothing else should be keeping it hanging around, so
   * our signal handlers should have been disconnected. */
  send_pubsub_event (test->sched_in, "pubsub.localhost", "lol");

  test_close_both_porters (test);
  teardown_test (test);

  /* None of the subsequent events should have triggered event-received. */
  g_assert (!node_event_received);
  g_assert (!service_event_received);
}

/* /pubsub-node/subscription-state-changed */
typedef struct {
    test_data_t *test;
    const gchar *expecting_service_ssc_received_for;
    gboolean expecting_node_ssc_received;
    WockyPubsubSubscriptionState expected_state;
} TestSSCCtx;

static void
send_subscription_state_change (WockyPorter *porter,
    const gchar *service,
    const gchar *node,
    const gchar *state)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE,
      service, NULL,
      WOCKY_NODE, "event",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_EVENT,
        WOCKY_NODE, "subscription",
          WOCKY_NODE_ATTRIBUTE, "node", node,
          WOCKY_NODE_ATTRIBUTE, "jid", "mighty@pirate.lit",
          WOCKY_NODE_ATTRIBUTE, "subscription", state,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, stanza);
  g_object_unref (stanza);
}

static void
service_subscription_state_changed_cb (
    WockyPubsubService *service,
    WockyPubsubNode *node,
    WockyXmppStanza *stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *subscription_node,
    WockyPubsubSubscription *subscription,
    TestSSCCtx *ctx)
{
  g_assert (ctx->expecting_service_ssc_received_for != NULL);

  g_assert_cmpstr (wocky_pubsub_node_get_name (node), ==,
      ctx->expecting_service_ssc_received_for);

  g_assert_cmpstr (event_node->name, ==, "event");
  g_assert_cmpstr (wocky_xmpp_node_get_ns (event_node), ==,
      WOCKY_XMPP_NS_PUBSUB_EVENT);

  g_assert_cmpstr (subscription_node->name, ==, "subscription");

  g_assert (subscription->node == node);
  g_assert_cmpstr (subscription->jid, ==, "mighty@pirate.lit");
  g_assert_cmpuint (subscription->state, ==, ctx->expected_state);
  g_assert (subscription->subid == NULL);

  ctx->expecting_service_ssc_received_for = NULL;
  ctx->test->outstanding--;
  g_main_loop_quit (ctx->test->loop);
}

static void
node_subscription_state_changed_cb (
    WockyPubsubNode *node,
    WockyXmppStanza *stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *subscription_node,
    WockyPubsubSubscription *subscription,
    TestSSCCtx *ctx)
{
  g_assert (ctx->expecting_node_ssc_received);
  ctx->expecting_node_ssc_received = FALSE;

  g_assert_cmpstr (wocky_pubsub_node_get_name (node), ==, "dairy-farmer");

  g_assert_cmpstr (event_node->name, ==, "event");
  g_assert_cmpstr (wocky_xmpp_node_get_ns (event_node), ==,
      WOCKY_XMPP_NS_PUBSUB_EVENT);

  g_assert_cmpstr (subscription_node->name, ==, "subscription");

  g_assert (subscription->node == node);
  g_assert_cmpstr (subscription->jid, ==, "mighty@pirate.lit");
  g_assert_cmpuint (subscription->state, ==, ctx->expected_state);
  g_assert (subscription->subid == NULL);

  ctx->test->outstanding--;
  g_main_loop_quit (ctx->test->loop);
}

static void
test_subscription_state_changed (void)
{
  test_data_t *test = setup_test ();
  TestSSCCtx ctx = { test, FALSE, FALSE, WOCKY_PUBSUB_SUBSCRIPTION_NONE };
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  pubsub = wocky_pubsub_service_new (test->session_out, "pubsub.localhost");
  g_signal_connect (pubsub, "subscription-state-changed",
      (GCallback) service_subscription_state_changed_cb, &ctx);

  node = wocky_pubsub_service_ensure_node (pubsub, "dairy-farmer");
  g_signal_connect (node, "subscription-state-changed",
      (GCallback) node_subscription_state_changed_cb, &ctx);

  /* Send a subscription change notification for a different node. */
  send_subscription_state_change (test->sched_in, "pubsub.localhost", "cow",
      "pending");
  ctx.expecting_service_ssc_received_for = "cow";
  ctx.expecting_node_ssc_received = FALSE;
  ctx.expected_state = WOCKY_PUBSUB_SUBSCRIPTION_PENDING;

  test->outstanding += 1;
  test_wait_pending (test);

  /* Send a subscription change notification for @node. */
  send_subscription_state_change (test->sched_in, "pubsub.localhost",
      "dairy-farmer", "unconfigured");
  ctx.expecting_service_ssc_received_for = "dairy-farmer";
  ctx.expecting_node_ssc_received = TRUE;
  ctx.expected_state = WOCKY_PUBSUB_SUBSCRIPTION_UNCONFIGURED;

  test->outstanding += 2;
  test_wait_pending (test);

  g_assert (!ctx.expecting_node_ssc_received);

  test_close_both_porters (test);
  teardown_test (test);

  g_object_unref (node);
  g_object_unref (pubsub);
}

/* /pubsub-node/deleted */
typedef struct {
    test_data_t *test;
    const gchar *expecting_service_node_deleted_for;
    gboolean expecting_node_deleted;
} TestDeletedCtx;

static void
send_deleted (WockyPorter *porter,
    const gchar *service,
    const gchar *node)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE,
      service, NULL,
      WOCKY_NODE, "event",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_EVENT,
        WOCKY_NODE, "delete",
          WOCKY_NODE_ATTRIBUTE, "node", node,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, stanza);
  g_object_unref (stanza);
}

static void
service_node_deleted_cb (
    WockyPubsubService *service,
    WockyPubsubNode *node,
    WockyXmppStanza *stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *delete_node,
    TestDeletedCtx *ctx)
{
  g_assert (ctx->expecting_service_node_deleted_for != NULL);

  g_assert_cmpstr (wocky_pubsub_node_get_name (node), ==,
      ctx->expecting_service_node_deleted_for);

  g_assert_cmpstr (event_node->name, ==, "event");
  g_assert_cmpstr (wocky_xmpp_node_get_ns (event_node), ==,
      WOCKY_XMPP_NS_PUBSUB_EVENT);

  g_assert_cmpstr (delete_node->name, ==, "delete");

  ctx->expecting_service_node_deleted_for = NULL;
  ctx->test->outstanding--;
  g_main_loop_quit (ctx->test->loop);
}

static void
node_deleted_cb (
    WockyPubsubNode *node,
    WockyXmppStanza *stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *delete_node,
    TestDeletedCtx *ctx)
{
  g_assert (ctx->expecting_node_deleted);
  ctx->expecting_node_deleted = FALSE;

  g_assert_cmpstr (wocky_pubsub_node_get_name (node), ==, "dairy-farmer");

  g_assert_cmpstr (event_node->name, ==, "event");
  g_assert_cmpstr (wocky_xmpp_node_get_ns (event_node), ==,
      WOCKY_XMPP_NS_PUBSUB_EVENT);

  g_assert_cmpstr (delete_node->name, ==, "delete");

  ctx->test->outstanding--;
  g_main_loop_quit (ctx->test->loop);
}

static void
test_deleted (void)
{
  test_data_t *test = setup_test ();
  TestDeletedCtx ctx = { test, FALSE, FALSE };
  WockyPubsubService *pubsub;
  WockyPubsubNode *node;

  test_open_both_connections (test);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  pubsub = wocky_pubsub_service_new (test->session_out, "pubsub.localhost");
  g_signal_connect (pubsub, "node-deleted",
      (GCallback) service_node_deleted_cb, &ctx);

  node = wocky_pubsub_service_ensure_node (pubsub, "dairy-farmer");
  g_signal_connect (node, "deleted",
      (GCallback) node_deleted_cb, &ctx);

  /* Send a deletion notification for a different node. */
  send_deleted (test->sched_in, "pubsub.localhost", "cow");
  ctx.expecting_service_node_deleted_for = "cow";
  ctx.expecting_node_deleted = FALSE;

  test->outstanding += 1;
  test_wait_pending (test);

  g_assert (ctx.expecting_service_node_deleted_for == NULL);

  /* Send a subscription change notification for @node. */
  send_deleted (test->sched_in, "pubsub.localhost", "dairy-farmer");
  ctx.expecting_service_node_deleted_for = "dairy-farmer";
  ctx.expecting_node_deleted = TRUE;

  test->outstanding += 2;
  test_wait_pending (test);

  g_assert (ctx.expecting_service_node_deleted_for == NULL);
  g_assert (!ctx.expecting_node_deleted);

  test_close_both_porters (test);
  teardown_test (test);

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
  g_test_add_func ("/pubsub-node/subscribe", test_subscribe);
  g_test_add_func ("/pubsub-node/unsubscribe", test_unsubscribe);
  g_test_add_func ("/pubsub-node/delete", test_delete);

  g_test_add_func ("/pubsub-node/list-subscribers", test_list_subscribers);
  g_test_add_func ("/pubsub-node/list-affiliates", test_list_affiliates);

  g_test_add_func ("/pubsub-node/receive-event", test_receive_event);
  g_test_add_func ("/pubsub-node/subscription-state-changed",
      test_subscription_state_changed);
  g_test_add_func ("/pubsub-node/deleted", test_deleted);

  result = g_test_run ();
  test_deinit ();
  return result;
}
