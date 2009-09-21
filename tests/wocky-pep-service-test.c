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

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pep-service/instantiation", test_instantiation);
  g_test_add_func ("/pep-service/changed-signal", test_changed_signal);

  result = g_test_run ();
  test_deinit ();
  return result;
}
