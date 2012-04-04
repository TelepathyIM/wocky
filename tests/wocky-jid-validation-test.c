#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

static void
valid (gconstpointer data)
{
  const gchar *jid = data;

  g_assert (wocky_decode_jid (jid, NULL, NULL, NULL));
}

static void
invalid (gconstpointer data)
{
  const gchar *jid = data;

  g_assert (!wocky_decode_jid (jid, NULL, NULL, NULL));
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_data_func ("/jid/valid/simple", "foo@bar.com/baz", valid);
  g_test_add_data_func ("/jid/valid/no-resource", "foo@bar.com", valid);
  g_test_add_data_func ("/jid/valid/no-node", "bar.com/baz", valid);
  g_test_add_data_func ("/jid/valid/just-domain", "bar.com", valid);
  g_test_add_data_func ("/jid/valid/ip", "foo@127.0.0.1", valid);
  g_test_add_data_func ("/jid/valid/ipv6", "foo@2:0:1cfe:face:b00c::3", valid);
  g_test_add_data_func ("/jid/valid/russian", "Анна@Каренина.ru", valid);

  g_test_add_data_func ("/jid/invalid/empty", "", invalid);
  g_test_add_data_func ("/jid/invalid/just-node", "foo@", invalid);
  g_test_add_data_func ("/jid/invalid/node-and-resource", "foo@/lol", invalid);
  g_test_add_data_func ("/jid/invalid/just-resource", "/lol", invalid);
  g_test_add_data_func ("/jid/invalid/garbage", "(*&$(*)", invalid);
  g_test_add_data_func ("/jid/invalid/space-domain", "foo bar", invalid);
  g_test_add_data_func ("/jid/invalid/two-ats", "squid@cat@battle", invalid);

#if 0
  /* These are improperly accepted. */
  g_test_add_data_func ("/jid/invalid/space-node", "i am a@fish", invalid);
  /* U+00A0 NO-BREAK SPACE is in Table C.1.2, which is forbidden in domains. */
  g_test_add_data_func ("/jid/invalid/nbsp-domain", "foo bar", invalid);
#endif

  result = g_test_run ();
  test_deinit ();
  return result;
}
