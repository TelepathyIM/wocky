#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-utils.h>

static void
test_node_equal (void)
{
  WockyXmppStanza *a, *b;

  /* Simple IQ node */
  a = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", WOCKY_STANZA_END);
  g_assert (wocky_xmpp_node_equal (a->node, (a->node)));

  /* Same as 'a' but with an ID attribute */
  b = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_STANZA_END);
  g_assert (wocky_xmpp_node_equal (b->node, (b->node)));

  g_assert (!wocky_xmpp_node_equal (a->node, (b->node)));
  g_assert (!wocky_xmpp_node_equal (b->node, (a->node)));

  g_object_unref (a);
  g_object_unref (b);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-node/node-equal", test_node_equal);
  return g_test_run ();
}
