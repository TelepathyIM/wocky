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
  WockyBareContact *bare_contact;
  WockyResourceContact *resource_contact;

  bare_contact = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "juliet@example.net",
      NULL);

  resource_contact = g_object_new (WOCKY_TYPE_RESOURCE_CONTACT,
      "resource", "Balcony",
      "bare-contact", bare_contact,
      NULL);

  g_assert (WOCKY_IS_RESOURCE_CONTACT (resource_contact));
  /* WockyResourceContact is a sub-class of WockyContact */
  g_assert (WOCKY_IS_CONTACT (resource_contact));
  g_object_unref (resource_contact);

  resource_contact = wocky_resource_contact_new (bare_contact, "Balcony");
  g_assert (WOCKY_IS_RESOURCE_CONTACT (resource_contact));
  g_assert (WOCKY_IS_CONTACT (resource_contact));
  g_object_unref (resource_contact);

  g_object_unref (bare_contact);
}

static void
test_get_resource (void)
{
  WockyBareContact *bare_contact;
  WockyResourceContact *resource_contact;

  bare_contact = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "juliet@example.net",
      NULL);

  resource_contact = wocky_resource_contact_new (bare_contact, "Balcony");

  g_assert (!wocky_strdiff (
        wocky_resource_contact_get_resource (resource_contact), "Balcony"));

  g_object_unref (bare_contact);
  g_object_unref (resource_contact);
}

static void
test_get_bare_contact (void)
{
  WockyBareContact *bare_contact;
  WockyResourceContact *resource_contact;

  bare_contact = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", "juliet@example.net",
      NULL);

  resource_contact = wocky_resource_contact_new (bare_contact, "Balcony");

  g_assert (wocky_resource_contact_get_bare_contact (resource_contact) ==
      bare_contact);

  g_object_unref (bare_contact);
  g_object_unref (resource_contact);
}

static void
test_equal (void)
{
  WockyBareContact *a, *b;
  WockyResourceContact *a_1, *a_2, *b_1;

  a = wocky_bare_contact_new ("a@example.net");
  a_1 = wocky_resource_contact_new (a, "Resource1");

  g_assert (wocky_resource_contact_equal (a_1, a_1));
  g_assert (!wocky_resource_contact_equal (a_1, NULL));
  g_assert (!wocky_resource_contact_equal (NULL, a_1));

  a_2 = wocky_resource_contact_new (a, "Resource2");
  g_assert (!wocky_resource_contact_equal (a_1, a_2));

  b = wocky_bare_contact_new ("b@example.net");
  b_1 = wocky_resource_contact_new (b, "Resource1");
  g_assert (!wocky_resource_contact_equal (b_1, a_1));
  g_assert (!wocky_resource_contact_equal (b_1, a_2));

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (a_1);
  g_object_unref (a_2);
  g_object_unref (b_1);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/resource-contact/instantiation", test_instantiation);
  g_test_add_func ("/resource-contact/get-resource", test_get_resource);
  g_test_add_func ("/resource-contact/get-bare-contact", test_get_bare_contact);
  g_test_add_func ("/resource-contact/equal", test_equal);

  result = g_test_run ();
  test_deinit ();
  return result;
}
