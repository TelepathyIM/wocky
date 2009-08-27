#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-contact-factory.h>
#include <wocky/wocky-utils.h>

#include "wocky-test-helper.h"

static void
test_instantiation (void)
{
  WockyContactFactory *factory;

  factory = wocky_contact_factory_new ();

  g_object_unref (factory);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/contact-factory/instantiation", test_instantiation);

  result = g_test_run ();
  test_deinit ();
  return result;
}
