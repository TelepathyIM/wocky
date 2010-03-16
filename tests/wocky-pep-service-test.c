#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pep-service.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

#define TEST_NODE1 "http://test.com/node1"
#define TEST_NODE2 "http://test.com/node2"

/* Test to instantiate a WockyPepService object */
static void
test_instantiation (void)
{
  WockyPepService *pep;

  pep = wocky_pep_service_new ("http://test.com/badger", FALSE);
  g_assert (pep != NULL);

  g_object_unref (pep);
}

/* Test that the 'changed' signal is properly fired */
gboolean event_received;

static void
test_changed_signal_cb (WockyPepService *pep,
    WockyBareContact *contact,
    WockyXmppStanza *stanza,
    test_data_t *test)
{
  g_assert (!wocky_strdiff (wocky_bare_contact_get_jid (contact),
        "alice@example.org"));

  test->outstanding--;
  g_main_loop_quit (test->loop);
  event_received = TRUE;
}

static void
send_pep_event (WockyPorter *porter,
    const gchar *node)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_NONE,
      "alice@example.org", NULL,
      WOCKY_NODE, "event",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_EVENT,
        WOCKY_NODE, "items",
        WOCKY_NODE_ATTRIBUTE, "node", node,
          WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "id", "1",
            WOCKY_NODE, "payload", WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_porter_send (porter, stanza);
  g_object_unref (stanza);
}

static void
test_changed_signal (void)
{
  test_data_t *test = setup_test ();
  WockyPepService *pep;

  pep = wocky_pep_service_new (TEST_NODE1, FALSE);
  test_open_both_connections (test);

  g_signal_connect (pep, "changed", G_CALLBACK (test_changed_signal_cb), test);

  wocky_pep_service_start (pep, test->session_out);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  /* send event on the right node */
  event_received = FALSE;
  send_pep_event (test->sched_in, TEST_NODE1);

  test->outstanding += 1;
  test_wait_pending (test);
  g_assert (event_received);
  event_received = FALSE;

  /* send event on the wrong node */
  send_pep_event (test->sched_in, TEST_NODE2);

  g_object_unref (pep);

  /* send event to the right node after the PEP service has been destroyed */
  send_pep_event (test->sched_in, TEST_NODE1);

  test_close_both_porters (test);
  teardown_test (test);
  g_assert (!event_received);
}

/* Test wocky_pep_service_get_async */
static void
test_send_query_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  reply = wocky_pep_service_get_finish (WOCKY_PEP_SERVICE (source_object), res,
      NULL);
  g_assert (reply != NULL);

  wocky_xmpp_stanza_get_type_info (reply, &type, &sub_type);
  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_RESULT);

  g_object_unref (reply);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static gboolean
test_send_query_stanza_received_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;

  reply = wocky_xmpp_stanza_build_iq_result (stanza,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "items",
          WOCKY_NODE_ATTRIBUTE, "node", "node1",
          WOCKY_NODE, "item",
            WOCKY_NODE_ATTRIBUTE, "id", "1",
            WOCKY_NODE, "payload", WOCKY_NODE_END,
          WOCKY_NODE_END,
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
test_send_query_failed_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  test_data_t *test = (test_data_t *) user_data;
  WockyXmppStanza *reply;
  GError *error = NULL;

  reply = wocky_pep_service_get_finish (WOCKY_PEP_SERVICE (source_object), res,
      &error);
  g_assert (reply == NULL);
  g_assert_error (error, WOCKY_XMPP_CONNECTION_ERROR,
      WOCKY_XMPP_CONNECTION_ERROR_CLOSED);
  g_clear_error (&error);

  test->outstanding--;
  g_main_loop_quit (test->loop);
}

static void
test_get (void)
{
  test_data_t *test = setup_test ();
  WockyPepService *pep;
  WockyContactFactory *contact_factory;
  WockyBareContact *contact;
  guint handler_id;

  pep = wocky_pep_service_new (TEST_NODE1, FALSE);
  test_open_both_connections (test);

  wocky_pep_service_start (pep, test->session_in);
  wocky_porter_start (test->sched_out);
  wocky_porter_start (test->sched_in);

  handler_id = wocky_porter_register_handler (test->sched_out,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      test_send_query_stanza_received_cb, test, WOCKY_STANZA_END);

  contact_factory = wocky_session_get_contact_factory (test->session_in);
  contact = wocky_contact_factory_ensure_bare_contact (contact_factory,
      "juliet@example.org");

  wocky_pep_service_get_async (pep, contact, NULL, test_send_query_cb, test);

  test->outstanding += 2;
  test_wait_pending (test);

  /* Regression test for a bug where wocky_pep_service_get_async's callback
   * would crash if sending the IQ failed.
   */
  wocky_porter_unregister_handler (test->sched_out, handler_id);
  wocky_pep_service_get_async (pep, contact, NULL, test_send_query_failed_cb,
      test);
  test->outstanding += 1;
  test_close_both_porters (test);

  g_object_unref (contact);
  g_object_unref (pep);
  teardown_test (test);
}

/* Test wocky_pep_service_make_publish_stanza */
static void
test_make_publish_stanza (void)
{
  WockyPepService *pep;
  WockyXmppStanza *stanza;
  WockyXmppNode *item = NULL, *n;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  pep = wocky_pep_service_new (TEST_NODE1, FALSE);

  stanza = wocky_pep_service_make_publish_stanza (pep, &item);

  g_assert (stanza != NULL);

  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);
  g_assert (type == WOCKY_STANZA_TYPE_IQ);
  g_assert (sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  n = wocky_xmpp_node_get_child_ns (stanza->node, "pubsub",
      WOCKY_XMPP_NS_PUBSUB);
  g_assert (n != NULL);

  n = wocky_xmpp_node_get_child (n, "publish");
  g_assert (n != NULL);
  g_assert (!wocky_strdiff (wocky_xmpp_node_get_attribute (n, "node"),
        TEST_NODE1));

  n = wocky_xmpp_node_get_child (n, "item");
  g_assert (n != NULL);
  g_assert (n == item);

  g_object_unref (stanza);
  g_object_unref (pep);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pep-service/instantiation", test_instantiation);
  g_test_add_func ("/pep-service/changed-signal", test_changed_signal);
  g_test_add_func ("/pep-service/get", test_get);
  g_test_add_func ("/pep-service/make-publish-stanza",
      test_make_publish_stanza);

  result = g_test_run ();
  test_deinit ();
  return result;
}
