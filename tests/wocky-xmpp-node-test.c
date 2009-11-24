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

static WockyXmppStanza *
_create_error (const gchar *cond,
    const gchar *type,
    const gchar *text,
    WockyXmppNode *orig,
    WockyXmppNode *extra)
{
  WockyXmppStanza *stanza = NULL;
  WockyXmppNode *error = NULL;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_ERROR,
      "server.xmpp.org", "a.user@server.xmpp.org",
      WOCKY_NODE_ATTRIBUTE, "id", "unpack-error-test-001",
      WOCKY_NODE, "error",
      WOCKY_NODE_ATTRIBUTE, "type", type,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  error = wocky_xmpp_node_get_child (stanza->node, "error");

  if (orig != NULL)
    stanza->node->children =
      g_slist_prepend (stanza->node->children, orig);

  wocky_xmpp_node_add_child_ns (error, cond, WOCKY_XMPP_NS_STANZAS);

  if (text != NULL)
    wocky_xmpp_node_add_child_with_content_ns (error, "text", text,
        WOCKY_XMPP_NS_STANZAS);


  if (extra != NULL)
      error->children = g_slist_append (error->children, extra);

  return stanza;
}

static void
_delete_error (WockyXmppStanza *stanza,
    WockyXmppNode *orig,
    WockyXmppNode *extra)
{
  WockyXmppNode *error = wocky_xmpp_node_get_child (stanza->node, "error");

  if (extra != NULL)
    error->children = g_slist_remove (error->children, extra);

  if (orig != NULL)
    stanza->node->children = g_slist_remove (stanza->node->children, orig);

  g_object_unref (stanza);
}

static WockyXmppStanza *
_create_stanza (const gchar *tag)
{
  return
    wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
        WOCKY_STANZA_SUB_TYPE_SET,
        "a.user@server.xmpp.org", "server.xmpp.org",
        WOCKY_NODE, tag,
        WOCKY_NODE_XMLNS, "urn:wocky:test:unpack:error:a:made:up:ns",
        WOCKY_NODE_TEXT, "bah blah blah",
        WOCKY_NODE_END,
        WOCKY_STANZA_END);
}

static void
test_unpack_error (void)
{
  int orig;
  int text;
  int more;

  for (orig = 0; orig < 2; orig++)
    for (text = 0; text < 2; text++)
      for (more = 0; more < 2; more++)
        {
          gboolean of = orig;
          gboolean tf = text;
          gboolean mf = more;
          gchar *label = NULL;
          gchar *etext = NULL;
          WockyXmppStanza *ostanza = NULL;
          WockyXmppStanza *mstanza = NULL;
          WockyXmppStanza *stanza = NULL;
          WockyXmppNode *error = NULL;
          WockyXmppNode *onode = NULL;
          WockyXmppNode *mnode = NULL;

          label = g_strconcat ("unpack-error",
              of ? "-orig" : "",
              tf ? "-text" : "",
              mf ? "-more" : "", NULL);

          if (tf)
            etext = g_strdup_printf ("Error in %s", label);

          if (of)
            {
              gchar *otag = g_strdup_printf("original-%s", label);
              ostanza = _create_stanza (otag);
              onode = wocky_xmpp_node_get_first_child (ostanza->node);
              g_free (otag);
            }

          if (mf)
            {
              gchar *mtag = g_strdup_printf ("extra-%s", label);
              mstanza = _create_stanza (mtag);
              mnode = wocky_xmpp_node_get_first_child (mstanza->node);
              g_free (mtag);
            }

          stanza = _create_error (label, "something", etext, onode, mnode);
          error = stanza->node;

          if (error != NULL)
            {
              const gchar *fake = "moo";
              const gchar *cond = NULL;
              const gchar *type = NULL;
              WockyXmppNode *ut = (WockyXmppNode *) fake;
              WockyXmppNode *uo = (WockyXmppNode *) fake;
              WockyXmppNode *um = (WockyXmppNode *) fake;

              cond = wocky_xmpp_error_unpack_node (error,
                  &type, &ut, &uo, &um, NULL);

              g_assert (cond != NULL);
              g_assert (!strcmp (cond, label));
              g_assert (!strcmp ("something", type));

              if (tf)
                {
                  g_assert (ut != NULL);
                  g_assert (ut->content != NULL);
                  g_assert (!strcmp (ut->content, etext));
                }
              else
                g_assert (ut == NULL);

              if (of)
                {
                  g_assert (uo != NULL);
                  g_assert (wocky_xmpp_node_equal (uo, onode));
                }
              else
                g_assert (uo == NULL);

              if (mf)
                {
                  g_assert (um != NULL);
                  g_assert (wocky_xmpp_node_equal (um, mnode));
                }
              else
                g_assert (um == NULL);

              cond = wocky_xmpp_error_unpack_node (error,
                  NULL, NULL, NULL, NULL, NULL);

              g_assert (cond != NULL);
              g_assert (!strcmp (cond, label));
            }
          else
            g_error ("%s failed - could not create error stanza", label);

          _delete_error (stanza, onode, mnode);

          if (ostanza != NULL)
            g_object_unref (ostanza);

          if (mstanza != NULL)
            g_object_unref (mstanza);

          g_free (etext);
          g_free (label);
        }
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
  g_test_add_func ("/xmpp-node/unpack-error", test_unpack_error);
  g_test_add_func ("/xmpp-node/append-content-n", test_append_content_n);
  g_test_add_func ("/xmpp-node/set-attribute-ns", test_set_attribute_ns);

  result = g_test_run ();
  test_deinit ();
  return result;
}
