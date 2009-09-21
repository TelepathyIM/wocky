#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-pep-service.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

/* Test to instantiate a WockyPepService object */
static void
test_instantiation (void)
{
  WockyPepService *pep;

  pep = wocky_pep_service_new ("http://test.com/badger", FALSE);
  g_assert (pep != NULL);

  g_object_unref (pep);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/pep-service/instantiation", test_instantiation);

  result = g_test_run ();
  test_deinit ();
  return result;
}
