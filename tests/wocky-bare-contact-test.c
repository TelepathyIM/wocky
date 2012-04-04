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
test_instantiation (void)
{
  WockyBareContact *a, *b;

  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      NULL);

  g_assert (WOCKY_IS_BARE_CONTACT (a));
  /* WockyBareContact is a sub-class of WockyContact */
  g_assert (WOCKY_IS_CONTACT (a));

  b = wocky_bare_contact_new ("romeo@example.net");
  g_assert (wocky_bare_contact_equal (a, b));

  g_object_unref (a);
  g_object_unref (b);
}

static void
test_contact_equal (void)
{
  WockyBareContact *a, *b, *c, *d, *e, *f, *g, *h, *i;
  const gchar *groups[] = { "Friends", "Badger", NULL };
  const gchar *groups2[] = { "Friends", "Snake", NULL };
  const gchar *groups3[] = { "Badger", "Friends", NULL };
  const gchar *groups4[] = { "aa", "bb", NULL };
  const gchar *groups5[] = { "ab", "ba", NULL };

  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
  g_assert (wocky_bare_contact_equal (a, a));
  g_assert (!wocky_bare_contact_equal (a, NULL));
  g_assert (!wocky_bare_contact_equal (NULL, a));

  /* Different jid */
  b = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.org",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
  g_assert (!wocky_bare_contact_equal (a, b));

  /* Different name */
  c = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Juliet",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
  g_assert (!wocky_bare_contact_equal (a, c));

  /* Different subscription */
  d = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO,
      "groups", groups,
      NULL);
  g_assert (!wocky_bare_contact_equal (a, d));

  /* Different groups */
  e = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups2,
      NULL);
  g_assert (!wocky_bare_contact_equal (a, e));

  /* Same groups but in a different order */
  f = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups3,
      NULL);
  g_assert (wocky_bare_contact_equal (a, f));

  /* No group defined */
  g = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      NULL);
  g_assert (wocky_bare_contact_equal (g, g));
  g_assert (!wocky_bare_contact_equal (a, g));

  /* regression test: used to fail with old group comparison algorithm */
  h = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "groups", groups4,
      NULL);

  i = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "groups", groups5,
      NULL);

  g_assert (!wocky_bare_contact_equal (h, i));

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
  g_object_unref (d);
  g_object_unref (e);
  g_object_unref (f);
  g_object_unref (g);
  g_object_unref (h);
  g_object_unref (i);
}

/* test wocky_bare_contact_add_group */
static void
test_add_group (void)
{
  WockyBareContact *a, *b, *c, *d;
  const gchar *groups[] = { "Friends", "Badger", NULL };
  const gchar *groups2[] = { "Friends", "Badger", "Snake", NULL };
  const gchar *groups3[] = { "Friends", NULL };

  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);

  /* same as 'a' but with one more group */
  b = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups2,
      NULL);

  g_assert (!wocky_bare_contact_equal (a, b));

  wocky_bare_contact_add_group (a, "Snake");
  g_assert (wocky_bare_contact_equal (a, b));

  /* try to add an already present group is no-op */
  wocky_bare_contact_add_group (a, "Snake");
  g_assert (wocky_bare_contact_equal (a, b));

  /* No group */
  c = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      NULL);

  d = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups3,
      NULL);

  g_assert (!wocky_bare_contact_equal (c, d));

  wocky_bare_contact_add_group (c, "Friends");
  g_assert (wocky_bare_contact_equal (c, d));

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
  g_object_unref (d);
}

/* test wocky_bare_contact_in_group */
static void
test_in_group (void)
{
  WockyBareContact *a, *b;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);

  g_assert (wocky_bare_contact_in_group (a, "Friends"));
  g_assert (wocky_bare_contact_in_group (a, "Badger"));
  g_assert (!wocky_bare_contact_in_group (a, "Snake"));

  /* no group defined */
  b = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      NULL);

  g_assert (!wocky_bare_contact_in_group (b, "Snake"));

  g_object_unref (a);
  g_object_unref (b);
}

/* test wocky_bare_contact_remove_group */
static void
test_remove_group (void)
{
  WockyBareContact *a, *b, *c;
  const gchar *groups[] = { "Friends", "Badger", NULL };
  const gchar *groups2[] = { "Badger", NULL };

  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);

  /* same as 'a' but with one more group */
  b = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups2,
      NULL);

  g_assert (!wocky_bare_contact_equal (a, b));

  wocky_bare_contact_remove_group (a, "Friends");
  g_assert (wocky_bare_contact_equal (a, b));

  /* try to remove an already not present group is no-op */
  wocky_bare_contact_remove_group (a, "Friends");
  g_assert (wocky_bare_contact_equal (a, b));

  /* no group defined */
  c = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      NULL);

  g_assert (wocky_bare_contact_equal (c, c));
  wocky_bare_contact_remove_group (c, "Misc");
  g_assert (wocky_bare_contact_equal (c, c));

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
}

static void
test_contact_copy (void)
{
  WockyBareContact *a, *copy;
  const gchar *groups[] = { "Friends", "Badger", NULL };

  /* Full contact */
  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);

  copy = wocky_bare_contact_copy (a);
  g_assert (wocky_bare_contact_equal (a, copy));
  g_object_unref (copy);
  g_object_unref (a);

  /* No name */
  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);

  copy = wocky_bare_contact_copy (a);
  g_assert (wocky_bare_contact_equal (a, copy));
  g_object_unref (copy);
  g_object_unref (a);

  /* No subscription */
  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "groups", groups,
      NULL);

  copy = wocky_bare_contact_copy (a);
  g_assert (wocky_bare_contact_equal (a, copy));
  g_object_unref (copy);
  g_object_unref (a);

  /* No group */
  a = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "romeo@example.net",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      NULL);

  copy = wocky_bare_contact_copy (a);
  g_assert (wocky_bare_contact_equal (a, copy));
  g_object_unref (copy);
  g_object_unref (a);
}

static void
test_add_get_resource (void)
{
  WockyBareContact *bare;
  WockyResourceContact *resource_1, *resource_2;
  GSList *resources;

  bare = wocky_bare_contact_new ("juliet@example.org");
  resources = wocky_bare_contact_get_resources (bare);
  g_assert_cmpuint (g_slist_length (resources), ==, 0);

  /* add a ressource */
  resource_1 = wocky_resource_contact_new (bare, "Resource1");
  wocky_bare_contact_add_resource (bare, resource_1);

  resources = wocky_bare_contact_get_resources (bare);
  g_assert_cmpuint (g_slist_length (resources), ==, 1);
  g_assert (g_slist_find (resources, resource_1) != NULL);
  g_slist_free (resources);

  /* add another resource */
  resource_2 = wocky_resource_contact_new (bare, "Resource2");
  wocky_bare_contact_add_resource (bare, resource_2);

  resources = wocky_bare_contact_get_resources (bare);
  g_assert_cmpuint (g_slist_length (resources), ==, 2);
  g_assert (g_slist_find (resources, resource_1) != NULL);
  g_assert (g_slist_find (resources, resource_2) != NULL);
  g_slist_free (resources);

  /* first resource is disposed and so automatically removed */
  g_object_unref (resource_1);

  resources = wocky_bare_contact_get_resources (bare);
  g_assert_cmpuint (g_slist_length (resources), ==, 1);
  g_assert (g_slist_find (resources, resource_2) != NULL);
  g_slist_free (resources);

  g_object_unref (bare);
  g_object_unref (resource_2);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/bare-contact/instantiation", test_instantiation);
  g_test_add_func ("/bare-contact/contact-equal", test_contact_equal);
  g_test_add_func ("/bare-contact/add-group", test_add_group);
  g_test_add_func ("/bare-contact/in-group", test_in_group);
  g_test_add_func ("/bare-contact/remove-group", test_remove_group);
  g_test_add_func ("/bare-contact/contact-copy", test_contact_copy);
  g_test_add_func ("/bare-contact/add-get-resource", test_add_get_resource);

  result = g_test_run ();
  test_deinit ();
  return result;
}
