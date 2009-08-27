#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-resource-contact.h>
#include <wocky/wocky-utils.h>

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

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/resource-contact/instantiation", test_instantiation);
  g_test_add_func ("/resource-contact/get-resource", test_get_resource);

  result = g_test_run ();
  test_deinit ();
  return result;
}
