#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

static void
test_build_simple_tree (void)
{
  WockyNodeTree *tree;
  WockyNode *n;

  tree = wocky_node_tree_new ("lions", "animals",
    '(', "distribution",
      '$', "Only in kenya",
    ')',
    NULL);

  g_assert (tree != NULL);

  n = wocky_node_tree_get_top_node (tree);
  g_assert (n != NULL);
  g_assert_cmpstr (n->name, ==, "lions");
  g_assert_cmpstr (wocky_node_get_ns (n), ==, "animals");

  g_assert_cmpint (g_slist_length (n->children), ==, 1);
  n = wocky_node_get_first_child (n);
  g_assert (n != NULL);
  g_assert_cmpstr (n->name, ==, "distribution");
  g_assert_cmpstr (wocky_node_get_ns (n), ==, "animals");
  g_assert_cmpstr (n->content, ==, "Only in kenya");

  g_object_unref (tree);
}

static void
test_tree_from_node (void)
{
  WockyNodeTree *a, *b;

  a = wocky_node_tree_new ("noms", "foodstocks",
    '(', "item", '@', "origin", "Italy",
      '$', "Plum cake",
    ')',
    NULL);

  b = wocky_node_tree_new_from_node (wocky_node_tree_get_top_node (a));

  test_assert_nodes_equal (wocky_node_tree_get_top_node (a),
    wocky_node_tree_get_top_node (b));

  g_object_unref (a);
  g_object_unref (b);
}

static void
test_node_add_tree (void)
{
  WockyNodeTree *origin, *ashes, *destination;
  WockyNode *ash, *top;
  WockyNode *ash_copy;

  origin = wocky_node_tree_new ("Eyjafjallajökull", "urn:wocky:lol:ísland",
    '(', "æsc",
      '*', &ash,
      '@', "type", "vulcanic",
      '(', "description", '$', "Black and smokey", ')',
    ')',
    NULL);

  ashes = wocky_node_tree_new_from_node (ash);

  destination = wocky_node_tree_new ("europe", "urn:wocky:lol:noplanesforyou",
      '*', &top, NULL);

  ash_copy = wocky_node_add_node_tree (top, ashes);

  test_assert_nodes_equal (ash_copy, ash);

  test_assert_nodes_equal (
    wocky_node_get_first_child (wocky_node_tree_get_top_node (destination)),
    wocky_node_get_first_child (wocky_node_tree_get_top_node (origin)));

  g_object_unref (origin);
  g_object_unref (ashes);
  g_object_unref (destination);
}


int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-node-tree/simple-tree",
    test_build_simple_tree);
  g_test_add_func ("/xmpp-node-tree/tree-from-node",
    test_tree_from_node);
  g_test_add_func ("/xmpp-node-tree/node-add-tree",
    test_node_add_tree);

  result =  g_test_run ();
  test_deinit ();
  return result;
}
