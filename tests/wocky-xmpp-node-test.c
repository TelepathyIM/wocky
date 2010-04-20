#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-stanza.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-xmpp-error.h>

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
  wocky_xmpp_node_set_attribute (wocky_stanza_get_top_node (a),
      "foo", "badger");
  test_assert_stanzas_equal (a, b);

  c = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org",
      '@', "foo", "snake",
      NULL);

  test_assert_stanzas_not_equal (b, c);
  wocky_xmpp_node_set_attribute (wocky_stanza_get_top_node (b),
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

  a = wocky_stanza_new ("message");

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
  WockyStanza *sa;
  WockyStanza *sb;
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

  sa = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", NULL);
  sb = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      "juliet@example.com", "romeo@example.org", NULL);
  na = sa->node;
  nb = sb->node;

  test_assert_nodes_equal (na, nb);

  /* *********************************************************************** */
  wocky_xmpp_node_set_attribute_ns (na, "one", "1", DUMMY_NS_A);
  ca = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_xmpp_node_get_attribute (na, "one");

  test_assert_nodes_not_equal (na, nb);
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

  test_assert_nodes_equal (na, nb);
  g_assert (ca != NULL);
  g_assert (cb != NULL);
  g_assert (cx == NULL);
  g_assert (cy != NULL);
  g_assert (!strcmp (ca, "1"));
  g_assert (!strcmp (ca, cb));

  wocky_xmpp_node_set_attribute_ns (nb, "one", "1", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);

  test_assert_nodes_equal (na, nb);
  g_assert (cb != NULL);
  g_assert (!strcmp (ca, cb));

  /* *********************************************************************** */
  /* change the namespaced atttribute                                        */
  wocky_xmpp_node_set_attribute_ns (na, "one", "2", DUMMY_NS_A);
  ca = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_A);
  cb = wocky_xmpp_node_get_attribute_ns (nb, "one", DUMMY_NS_A);
  cx = wocky_xmpp_node_get_attribute_ns (na, "one", DUMMY_NS_B);
  cy = wocky_xmpp_node_get_attribute (na, "one");

  test_assert_nodes_not_equal (na, nb);
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

static void
do_test_iteration (WockyXmppNodeIter *iter, const gchar **names)
{
  WockyXmppNode *node;
  int i = 0;

  while (wocky_xmpp_node_iter_next (iter, &node))
    {
      g_assert (names[i] != NULL && "Unexpected node");

      g_assert_cmpstr (names[i], ==,
        wocky_xmpp_node_get_attribute (node, "name"));
      i++;
    }

  g_assert (names[i] == NULL && "Expected more nodes");
}

static void
test_node_iteration (void)
{
  WockyStanza *stanza;
  WockyXmppNodeIter iter;
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
  wocky_xmpp_node_iter_init (&iter, stanza->node, NULL, NULL);
  do_test_iteration (&iter, all);

  /* Only the payloads */
  wocky_xmpp_node_iter_init (&iter, stanza->node, "payload-type", NULL);
  do_test_iteration (&iter, payloads);

  /* Only phone payloads */
  wocky_xmpp_node_iter_init (&iter, stanza->node, "payload-type",
    WOCKY_NS_GOOGLE_SESSION_PHONE);
  do_test_iteration (&iter, audio);

  /* Only nodes with the phone namespace */
  wocky_xmpp_node_iter_init (&iter, stanza->node, NULL,
    WOCKY_NS_GOOGLE_SESSION_PHONE);
  do_test_iteration (&iter, audio);

  /* only video payloads */
  wocky_xmpp_node_iter_init (&iter, stanza->node, "payload-type",
    WOCKY_NS_GOOGLE_SESSION_VIDEO);
  do_test_iteration (&iter, video);

  /* only nodes with the video namespace */
  wocky_xmpp_node_iter_init (&iter, stanza->node, NULL,
    WOCKY_NS_GOOGLE_SESSION_VIDEO);
  do_test_iteration (&iter, video_ns);

  /* nothing */
  wocky_xmpp_node_iter_init (&iter, stanza->node, "badgers", NULL);
  do_test_iteration (&iter, nothing);

  wocky_xmpp_node_iter_init (&iter, stanza->node, NULL, "snakes");
  do_test_iteration (&iter, nothing);

  g_object_unref (stanza);
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
  g_test_add_func ("/xmpp-node/node-iterator", test_node_iteration);

  result = g_test_run ();
  test_deinit ();
  return result;
}
