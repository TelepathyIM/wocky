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
  WockyContactFactory *factory;

  factory = wocky_contact_factory_new ();

  g_object_unref (factory);
}

gboolean add_signal_received = FALSE;

static void
bare_contact_added (WockyContactFactory *factory,
    WockyBareContact *contact,
    gpointer data)
{
  add_signal_received = TRUE;
}

static void
test_ensure_bare_contact (void)
{
  WockyContactFactory *factory;
  WockyBareContact *juliet, *a, *b;

  factory = wocky_contact_factory_new ();
  juliet = wocky_bare_contact_new ("juliet@example.org");

  add_signal_received = FALSE;
  g_signal_connect (factory, "bare-contact-added",
      G_CALLBACK (bare_contact_added), NULL);

  a = wocky_contact_factory_ensure_bare_contact (factory, "juliet@example.org");
  g_assert (wocky_bare_contact_equal (a, juliet));
  g_assert (add_signal_received);

  b = wocky_contact_factory_ensure_bare_contact (factory, "juliet@example.org");
  g_assert (a == b);

  g_object_unref (factory);
  g_object_unref (juliet);
  g_object_unref (a);
  g_object_unref (b);
}

static void
test_lookup_bare_contact (void)
{
  WockyContactFactory *factory;
  WockyBareContact *contact;

  factory = wocky_contact_factory_new ();

  g_assert (wocky_contact_factory_lookup_bare_contact (factory,
        "juliet@example.org") == NULL);

  contact = wocky_contact_factory_ensure_bare_contact (factory,
      "juliet@example.org");

  g_assert (wocky_contact_factory_lookup_bare_contact (factory,
        "juliet@example.org") != NULL);

  g_object_unref (contact);

  /* contact has been disposed and so is not in the factory anymore */
  g_assert (wocky_contact_factory_lookup_bare_contact (factory,
        "juliet@example.org") == NULL);

  g_object_unref (factory);
}

static void
resource_contact_added (WockyContactFactory *factory,
    WockyBareContact *contact,
    gpointer data)
{
  add_signal_received = TRUE;
}

static void
test_ensure_resource_contact (void)
{
  WockyContactFactory *factory;
  WockyBareContact *juliet, *bare;
  WockyResourceContact *juliet_balcony, *juliet_pub, *a, *b;
  GSList *resources;

  factory = wocky_contact_factory_new ();
  juliet = wocky_bare_contact_new ("juliet@example.org");
  juliet_balcony = wocky_resource_contact_new (juliet, "Balcony");
  juliet_pub = wocky_resource_contact_new (juliet, "Pub");

  add_signal_received = FALSE;
  g_signal_connect (factory, "resource-contact-added",
      G_CALLBACK (resource_contact_added), NULL);

  /* Bare contact isn't in the factory yet */
  a = wocky_contact_factory_ensure_resource_contact (factory,
      "juliet@example.org/Balcony");
  g_assert (wocky_resource_contact_equal (a, juliet_balcony));
  g_assert (add_signal_received);

  bare = wocky_contact_factory_lookup_bare_contact (factory,
        "juliet@example.org");
  g_assert (bare != NULL);

  /* Resource has been added to the bare contact */
  resources = wocky_bare_contact_get_resources (bare);
  g_assert_cmpuint (g_slist_length (resources), ==, 1);
  g_assert (g_slist_find (resources, a) != NULL);
  g_slist_free (resources);

  /* Bare contact is already in the factory */
  b = wocky_contact_factory_ensure_resource_contact (factory,
      "juliet@example.org/Pub");
  g_assert (wocky_resource_contact_equal (b, juliet_pub));

  g_object_unref (factory);
  g_object_unref (juliet);
  g_object_unref (juliet_balcony);
  g_object_unref (juliet_pub);
  g_object_unref (a);
  g_object_unref (b);
}

static void
test_lookup_resource_contact (void)
{
  WockyContactFactory *factory;
  WockyResourceContact *contact;

  factory = wocky_contact_factory_new ();

  g_assert (wocky_contact_factory_lookup_resource_contact (factory,
        "juliet@example.org/Balcony") == NULL);
  contact  = wocky_contact_factory_ensure_resource_contact (factory,
      "juliet@example.org/Balcony");
  g_assert (wocky_contact_factory_lookup_resource_contact (factory,
        "juliet@example.org/Balcony") != NULL);

  g_object_unref (factory);
  g_object_unref (contact);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/contact-factory/instantiation", test_instantiation);
  g_test_add_func ("/contact-factory/ensure-bare-contact",
      test_ensure_bare_contact);
  g_test_add_func ("/contact-factory/lookup-bare-contact",
      test_lookup_bare_contact);
  g_test_add_func ("/contact-factory/ensure-resource-contact",
      test_ensure_resource_contact);
  g_test_add_func ("/contact-factory/lookup-resource-contact",
      test_lookup_resource_contact);

  result = g_test_run ();
  test_deinit ();
  return result;
}
