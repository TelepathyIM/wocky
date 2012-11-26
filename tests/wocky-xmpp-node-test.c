#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

#define DUMMY_NS_A "urn:wocky:test:dummy:namespace:a"
#define DUMMY_NS_B "urn:wocky:test:dummy:namespace:b"

static void
test_node_equal (void)
{
  WockyStanza *a, *b;

  /* Simple IQ node */
  a = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", NULL);
  test_assert_stanzas_equal (a, a);

  /* Same as 'a' but with an ID attribute */
  b = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      '@', "id", "one",
      NULL);
  test_assert_stanzas_equal (b, b);

  test_assert_stanzas_not_equal (a, b);
  test_assert_stanzas_not_equal (b, a);

  g_object_unref (a);
  g_object_unref (b);
}

static void
test_node_add_build (void)
{
  WockyNode *n, *child;

  n = wocky_node_new ("testtree", DUMMY_NS_A);
  wocky_node_add_build (n,
      '(', "testnode", '@', "test", "attribute",
        '$', "testcontent",
      ')',
    NULL);

  g_assert_cmpint (g_slist_length (n->children), ==, 1);

  child = wocky_node_get_first_child (n);
  g_assert_cmpstr (child->name, ==, "testnode");
  g_assert_cmpstr (wocky_node_get_ns (child), ==, DUMMY_NS_A);
  g_assert_cmpstr (child->content, ==, "testcontent");

  g_assert_cmpint (g_slist_length (child->attributes), ==, 1);
  g_assert_cmpstr (wocky_node_get_attribute (child, "test"),
      ==, "attribute");

  wocky_node_free (n);
}

static void
test_set_attribute (void)
{
  WockyStanza *a, *b, *c;

  a = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", NULL);

  b = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      '@', "foo", "badger",
      NULL);

  test_assert_stanzas_not_equal (a, b);
  wocky_node_set_attribute (wocky_stanza_get_top_node (a),
      "foo", "badger");
  test_assert_stanzas_equal (a, b);

  c = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      '@', "foo", "snake",
      NULL);

  test_assert_stanzas_not_equal (b, c);
  wocky_node_set_attribute (wocky_stanza_get_top_node (b),
      "foo", "snake");
  test_assert_stanzas_equal (b, c);

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
  WockyStanza *a;
  const gchar *content = "badger badger badger";
  guint i;
  size_t l;

  a = wocky_stanza_new ("message", WOCKY_XMPP_NS_JABBER_CLIENT);

  l = strlen (content);
  /* Append content byte by byte */
  for (i = 0; i < l; i++)
    {
      wocky_node_append_content_n (wocky_stanza_get_top_node (a),
          content + i, 1);
    }
  g_assert (!wocky_strdiff (wocky_stanza_get_top_node (a)->content, content));

  g_object_unref (a);
}

static void
test_set_attribute_ns (void)
{
  WockyStanza *sa;
  WockyStanza *sb;
  WockyNode *na;
  WockyNode *nb;
  const gchar *ca;
  const gchar *cb;
  const gchar *cx;
  const gchar *cy;
  gchar *xml_a;
  gchar *xml_b;
  gchar *pa;
  gchar *pb;
  GQuark qa;

  sa = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", NULL);
  sb = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", NULL);
  na = wocky_stanza_get_top_node (sa);
  nb = wocky_stanza_get_top_node (sb);

  test_assert_nodes_equal (na, nb);

  /* *********************************************************************** */
  wocky_node_set_attribute_ns (na, "one", "1", DUMMY_NS_A);
  ca = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_node_get_attribute (na, "one");

  test_assert_nodes_not_equal (na, nb);
  g_assert (ca != NULL);
  g_assert (cb == NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "1"));

  /* *********************************************************************** */
  /* set the attribute in the second node to make them equal again           */
  wocky_node_set_attribute_ns (nb, "one", "1", DUMMY_NS_A);
  ca = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_node_get_attribute (na, "one");

  test_assert_nodes_equal (na, nb);
  g_assert (ca != NULL);
  g_assert (cb != NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "1"));
  g_assert (!strcmp (ca, cb));

  wocky_node_set_attribute_ns (nb, "one", "1", DUMMY_NS_A);
  cb = wocky_node_get_attribute_ns (nb, "one", DUMMY_NS_A);

  test_assert_nodes_equal (na, nb);
  g_assert (cb != NULL);
  g_assert (!strcmp (ca, cb));

  /* *********************************************************************** */
  /* change the namespaced atttribute                                        */
  wocky_node_set_attribute_ns (na, "one", "2", DUMMY_NS_A);
  ca = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_node_get_attribute (na, "one");

  test_assert_nodes_not_equal (na, nb);
  g_assert (ca != NULL);
  g_assert (cb != NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "2"));
  g_assert (strcmp (ca, cb));

  /* *********************************************************************** */
  /* add another attribute in a different namespace                          */
  wocky_node_set_attribute_ns (na, "one", "3", DUMMY_NS_B);
  ca = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_node_get_attribute (na, "one");

  test_assert_nodes_not_equal (na, nb);
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

  xml_a = wocky_node_to_string (na);
  pa = g_strdup (wocky_node_attribute_ns_get_prefix_from_urn (DUMMY_NS_B));
  pb = g_strdup (wocky_node_attribute_ns_get_prefix_from_quark (qa));

  g_assert (!strcmp (pa, pb));
  g_free (pb);

  /* change the prefix and re-write the attribute */
  wocky_node_attribute_ns_set_prefix (qa, "moose");
  wocky_node_set_attribute_ns (na, "one", "1", DUMMY_NS_B);
  xml_b = wocky_node_to_string (na);
  pb = g_strdup (wocky_node_attribute_ns_get_prefix_from_quark (qa));

  g_assert (strcmp (pa, pb));
  g_assert (_check_attr_prefix (DUMMY_NS_B, pa, "one", xml_a));
  g_assert (_check_attr_prefix (DUMMY_NS_B, pb, "one", xml_b));

  g_free (pa);
  g_free (pb);
  g_free (xml_a);
  g_free (xml_b);

  /* *********************************************************************** */
  wocky_node_set_attribute_ns (na, "one", "4", DUMMY_NS_B);
  cx = wocky_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  g_assert (cx != NULL);
  g_assert (!strcmp (cx, "4"));

  g_object_unref (sa);
  g_object_unref (sb);
}

static void
do_test_iteration (WockyNodeIter *iter, const gchar **names)
{
  WockyNode *node;
  int i = 0;

  while (wocky_node_iter_next (iter, &node))
    {
      g_assert (names[i] != NULL && "Unexpected node");

      g_assert_cmpstr (names[i], ==,
        wocky_node_get_attribute (node, "name"));
      i++;
    }

  g_assert (names[i] == NULL && "Expected more nodes");
}

static void
test_node_iteration (void)
{
  WockyStanza *stanza;
  WockyNodeIter iter;
  const gchar *all[] = { "SPEEX", "THEORA", "GSM", "H264",
                          "VIDEO?", "other", NULL };
  const gchar *payloads[] = { "SPEEX", "THEORA", "GSM", "H264", NULL };
  const gchar *audio[] = { "SPEEX", "GSM", NULL };
  const gchar *video[] = { "THEORA", "H264", NULL };
  const gchar *video_ns[] = { "THEORA", "H264", "VIDEO?", NULL };
  const gchar *nothing[] = { NULL };

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_SET, "to", "from",
    '(', "payload-type",
      ':', WOCKY_NS_GOOGLE_SESSION_PHONE,
      '@', "name", "SPEEX",
    ')',
    '(', "payload-type",
      ':', WOCKY_NS_GOOGLE_SESSION_VIDEO,
      '@', "name", "THEORA",
    ')',
    '(', "payload-type",
      ':', WOCKY_NS_GOOGLE_SESSION_PHONE,
      '@', "name", "GSM",
    ')',
    '(', "payload-type",
      ':', WOCKY_NS_GOOGLE_SESSION_VIDEO,
      '@', "name", "H264",
    ')',
    '(', "video",
      ':', WOCKY_NS_GOOGLE_SESSION_VIDEO,
      '@', "name", "VIDEO?",
    ')',
    '(', "misc",
      '@', "name", "other",
    ')',
    NULL);

  /* All children */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza),
      NULL, NULL);
  do_test_iteration (&iter, all);

  /* Only the payloads */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza),
      "payload-type", NULL);
  do_test_iteration (&iter, payloads);

  /* Only phone payloads */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza),
      "payload-type", WOCKY_NS_GOOGLE_SESSION_PHONE);
  do_test_iteration (&iter, audio);

  /* Only nodes with the phone namespace */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza), NULL,
    WOCKY_NS_GOOGLE_SESSION_PHONE);
  do_test_iteration (&iter, audio);

  /* only video payloads */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza),
      "payload-type", WOCKY_NS_GOOGLE_SESSION_VIDEO);
  do_test_iteration (&iter, video);

  /* only nodes with the video namespace */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza), NULL,
      WOCKY_NS_GOOGLE_SESSION_VIDEO);
  do_test_iteration (&iter, video_ns);

  /* nothing */
  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza),
      "badgers", NULL);
  do_test_iteration (&iter, nothing);

  wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza), NULL,
      "snakes");
  do_test_iteration (&iter, nothing);

  g_object_unref (stanza);
}

static void
test_node_iter_remove (void)
{
  WockyNode *top, *child;
  WockyNodeTree *tree = wocky_node_tree_new ("foo", "wocky:test",
      '*', &top,
      '(', "remove-me", ')',
      '(', "remove-me", ')',
      '(', "preserve-me", ')',
      '(', "remove-me", ')',
      '(', "preserve-me", ')',
      '(', "remove-me", ')',
      '(', "remove-me", ')',
      NULL);
  WockyNodeIter iter;

  wocky_node_iter_init (&iter, top, "remove-me", NULL);
  while (wocky_node_iter_next (&iter, NULL))
    wocky_node_iter_remove (&iter);

  g_assert_cmpuint (g_slist_length (top->children), ==, 2);

  wocky_node_iter_init (&iter, top, NULL, NULL);
  while (wocky_node_iter_next (&iter, &child))
    g_assert_cmpstr (child->name, ==, "preserve-me");

  g_object_unref (tree);
}

static void
test_get_first_child (void)
{
  WockyNodeTree *tree = wocky_node_tree_new ("my-elixir", "my:poison",
      '(', "th5", ')',
      '(', "holomovement", ':', "chinese:lantern", ')',
      '(', "th5", ':', "chinese:lantern", ')',
      NULL);
  WockyNode *top = wocky_node_tree_get_top_node (tree);
  WockyNode *n;

  n = wocky_node_get_first_child (top);
  g_assert (n != NULL);
  g_assert_cmpstr ("th5", ==, n->name);
  g_assert_cmpstr ("my:poison", ==, wocky_node_get_ns (n));

  n = wocky_node_get_child (top, "th5");
  g_assert (n != NULL);
  g_assert_cmpstr ("th5", ==, n->name);
  g_assert_cmpstr ("my:poison", ==, wocky_node_get_ns (n));

  n = wocky_node_get_child_ns (top, "th5", "chinese:lantern");
  g_assert (n != NULL);
  g_assert_cmpstr ("th5", ==, n->name);
  g_assert_cmpstr ("chinese:lantern", ==, wocky_node_get_ns (n));

  n = wocky_node_get_first_child_ns (top, "chinese:lantern");
  g_assert (n != NULL);
  g_assert_cmpstr ("holomovement", ==, n->name);
  g_assert_cmpstr ("chinese:lantern", ==, wocky_node_get_ns (n));

  g_object_unref (tree);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-node/node-equal", test_node_equal);
  g_test_add_func ("/xmpp-node/add-build", test_node_add_build);
  g_test_add_func ("/xmpp-node/set-attribute", test_set_attribute);
  g_test_add_func ("/xmpp-node/append-content-n", test_append_content_n);
  g_test_add_func ("/xmpp-node/set-attribute-ns", test_set_attribute_ns);
  g_test_add_func ("/xmpp-node/node-iterator", test_node_iteration);
  g_test_add_func ("/xmpp-node/node-iterator-remove", test_node_iter_remove);
  g_test_add_func ("/xmpp-node/get-first-child", test_get_first_child);

  result = g_test_run ();
  test_deinit ();
  return result;
}
