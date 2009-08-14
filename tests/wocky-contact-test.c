#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-contact.h>
#include <wocky/wocky-utils.h>

#include "wocky-test-helper.h"

static void
test_contact_equal (void)
{
  WockyContact *a, *b, *c, *d, *e, *f, *g;
  const gchar *groups[] = { "Friends", "Badger", NULL };
  const gchar *groups2[] = { "Friends", "Snake", NULL };
  const gchar *groups3[] = { "Badger", "Friends", NULL };

  a = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
  g_assert (wocky_contact_equal (a, a));
  g_assert (!wocky_contact_equal (a, NULL));
  g_assert (!wocky_contact_equal (NULL, a));

  /* Different jid */
  b = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.org",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
  g_assert (!wocky_contact_equal (a, b));

  /* Different name */
  c = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Juliet",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups,
      NULL);
  g_assert (!wocky_contact_equal (a, c));

  /* Different subscription */
  d = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO,
      "groups", groups,
      NULL);
  g_assert (!wocky_contact_equal (a, d));

  /* Different groups */
  e = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups2,
      NULL);
  g_assert (!wocky_contact_equal (a, e));

  /* Same groups but in a different order */
  f = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      "groups", groups3,
      NULL);
  g_assert (wocky_contact_equal (a, f));

  /* No group defined */
  g = g_object_new (WOCKY_TYPE_CONTACT,
      "jid", "romeo@example.net",
      "name", "Romeo",
      "subscription", WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH,
      NULL);
  g_assert (wocky_contact_equal (g, g));
  g_assert (!wocky_contact_equal (a, g));

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
  g_object_unref (d);
  g_object_unref (e);
  g_object_unref (f);
  g_object_unref (g);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/contact/contact-equal", test_contact_equal);

  result = g_test_run ();
  test_deinit ();
  return result;
}
