#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-helper.h"

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

              cond = wocky_xmpp_node_unpack_error (error, &type, &ut, &uo, &um);

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

              cond = wocky_xmpp_node_unpack_error (error,
                  NULL, NULL, NULL, NULL);

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

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-node/node-equal", test_node_equal);
  g_test_add_func ("/xmpp-node/set-attribute", test_set_attribute);
  g_test_add_func ("/xmpp-node/unpack-error", test_unpack_error);
  g_test_add_func ("/xmpp-node/append-content-n", test_append_content_n);

  result = g_test_run ();
  test_deinit ();
  return result;
}
