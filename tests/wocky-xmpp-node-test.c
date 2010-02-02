#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-xmpp-error.h>

#include "wocky-test-helper.h"

#define DUMMY_NS_A "urn:wocky:test:dummy:namespace:a"
#define DUMMY_NS_B "urn:wocky:test:dummy:namespace:b"

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
  g_assert (wocky_xmpp_node_equal (b->node, b->node));

  g_assert (!wocky_xmpp_node_equal (a->node, b->node));
  g_assert (!wocky_xmpp_node_equal (b->node, a->node));

  g_object_unref (a);
  g_object_unref (b);
}

static void
test_set_attribute (void)
{
  WockyXmppStanza *a, *b, *c;

  a = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", WOCKY_STANZA_END);

  b = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      WOCKY_NODE_ATTRIBUTE, "foo", "badger",
      WOCKY_STANZA_END);

  g_assert (!wocky_xmpp_node_equal (a->node, b->node));
  wocky_xmpp_node_set_attribute (a->node, "foo", "badger");
  g_assert (wocky_xmpp_node_equal (a->node, b->node));

  c = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      WOCKY_NODE_ATTRIBUTE, "foo", "snake",
      WOCKY_STANZA_END);

  g_assert (!wocky_xmpp_node_equal (b->node, c->node));
  wocky_xmpp_node_set_attribute (b->node, "foo", "snake");
  g_assert (wocky_xmpp_node_equal (b->node, c->node));

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
}

static gboolean
_check_attr_prefix (const gchar *urn,
    const gchar *prefix,
    const gchar *attr,
    const gchar *xml)
{
  gboolean rval = FALSE;
  gchar *ns_str_a = g_strdup_printf (" xmlns:%s='%s'", prefix, urn);
  gchar *ns_str_b = g_strdup_printf (" xmlns:%s=\"%s\"", prefix, urn);
  gchar *attr_str = g_strdup_printf (" %s:%s=", prefix, attr);

  rval = ((strstr (xml, ns_str_a) != NULL) ||
          (strstr (xml, ns_str_a) != NULL)) && (strstr (xml, attr_str) != NULL);

  g_free (ns_str_a);
  g_free (ns_str_b);
  g_free (attr_str);

  return rval;
}

static void
test_append_content_n (void)
{
  WockyXmppStanza *a;
  const gchar *content = "badger badger badger";
  guint i;
  size_t l;

  a = wocky_xmpp_stanza_new ("message");

  l = strlen (content);
  /* Append content byte by byte */
  for (i = 0; i < l; i++)
    {
      wocky_xmpp_node_append_content_n (a->node, content + i, 1);
    }
  g_assert (!wocky_strdiff (a->node->content, content));

  g_object_unref (a);
}

static void
test_set_attribute_ns (void)
{
  WockyXmppStanza *sa;
  WockyXmppStanza *sb;
  WockyXmppNode *na;
  WockyXmppNode *nb;
  const gchar *ca;
  const gchar *cb;
  const gchar *cx;
  const gchar *cy;
  gchar *xml_a;
  gchar *xml_b;
  gchar *pa;
  gchar *pb;
  GQuark qa;

  sa = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", WOCKY_STANZA_END);
  sb = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", WOCKY_STANZA_END);
  na = sa->node;
  nb = sb->node;

  g_assert (wocky_xmpp_node_equal (na, nb));

  /* *********************************************************************** */
  wocky_xmpp_node_set_attribute_ns (na, "one", "1", DUMMY_NS_A);
  ca = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_xmpp_node_get_attribute (na, "one");

  g_assert (!wocky_xmpp_node_equal (na, nb));
  g_assert (ca != NULL);
  g_assert (cb == NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "1"));

  /* *********************************************************************** */
  /* set the attribute in the second node to make them equal again           */
  wocky_xmpp_node_set_attribute_ns (nb, "one", "1", DUMMY_NS_A);
  ca = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_xmpp_node_get_attribute (na, "one");

  g_assert (wocky_xmpp_node_equal (na, nb));
  g_assert (ca != NULL);
  g_assert (cb != NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "1"));
  g_assert (!strcmp (ca, cb));

  wocky_xmpp_node_set_attribute_ns (nb, "one", "1", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);

  g_assert (wocky_xmpp_node_equal (na, nb));
  g_assert (cb != NULL);
  g_assert (!strcmp (ca, cb));

  /* *********************************************************************** */
  /* change the namespaced atttribute                                        */
  wocky_xmpp_node_set_attribute_ns (na, "one", "2", DUMMY_NS_A);
  ca = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_xmpp_node_get_attribute (na, "one");

  g_assert (!wocky_xmpp_node_equal (na, nb));
  g_assert (ca != NULL);
  g_assert (cb != NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "2"));
  g_assert (strcmp (ca, cb));

  /* *********************************************************************** */
  /* add another attribute in a different namespace                          */
  wocky_xmpp_node_set_attribute_ns (na, "one", "3", DUMMY_NS_B);
  ca = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_xmpp_node_get_attribute (na, "one");

  g_assert (!wocky_xmpp_node_equal (na, nb));
  g_assert (ca != NULL);
  g_assert (cb != NULL);
  g_assert (cx != NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "2"));
  g_assert (!strcmp (cx, "3"));
  g_assert (strcmp (ca, cb));

  /* *********************************************************************** */
  /* swap out the prefix for another one                                     */
  /* then check to see the right prefixes were assigned                      */
  qa = g_quark_from_string (DUMMY_NS_B);

  xml_a = wocky_xmpp_node_to_string (na);
  pa = g_strdup (wocky_xmpp_node_attribute_ns_get_prefix_from_urn (DUMMY_NS_B));
  pb = g_strdup (wocky_xmpp_node_attribute_ns_get_prefix_from_quark (qa));

  g_assert (!strcmp (pa, pb));
  g_free (pb);

  /* change the prefix and re-write the attribute */
  wocky_xmpp_node_attribute_ns_set_prefix (qa, "moose");
  wocky_xmpp_node_set_attribute_ns (na, "one", "1", DUMMY_NS_B);
  xml_b = wocky_xmpp_node_to_string (na);
  pb = g_strdup (wocky_xmpp_node_attribute_ns_get_prefix_from_quark (qa));

  g_assert (strcmp (pa, pb));
  g_assert (_check_attr_prefix (DUMMY_NS_B, pa, "one", xml_a));
  g_assert (_check_attr_prefix (DUMMY_NS_B, pb, "one", xml_b));

  g_free (pa);
  g_free (pb);
  g_free (xml_a);
  g_free (xml_b);

  /* *********************************************************************** */
  wocky_xmpp_node_set_attribute_ns (na, "one", "4", DUMMY_NS_B);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  g_assert (cx != NULL);
  g_assert (!strcmp (cx, "4"));

  g_object_unref (sa);
  g_object_unref (sb);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-node/node-equal", test_node_equal);
  g_test_add_func ("/xmpp-node/set-attribute", test_set_attribute);
  g_test_add_func ("/xmpp-node/append-content-n", test_append_content_n);
  g_test_add_func ("/xmpp-node/set-attribute-ns", test_set_attribute_ns);

  result = g_test_run ();
  test_deinit ();
  return result;
}
