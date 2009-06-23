#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-utils.h>

static void
test_build_iq_result (void)
{
  WockyXmppStanza *iq, *reply, *expected;

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  /* Send a simple ACK */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_result (iq, WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);

  /* Send a more complex reply */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "streamhostproxy.example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Bytestreams Proxy",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_result (iq,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "streamhostproxy.example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Bytestreams Proxy",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-stanza/build-iq-result", test_build_iq_result);
  return g_test_run ();
}
