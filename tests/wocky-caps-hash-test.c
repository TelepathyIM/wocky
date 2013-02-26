#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

static gboolean
check_hash (WockyStanza *stanza,
  const gchar *expected)
{
  gchar *hash;

  hash = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_assert_cmpstr (hash, ==, expected);
  g_object_unref (stanza);
  g_free (hash);
  return TRUE;
}

static void
test_simple (void)
{
  /* Simple example from XEP-0115 */
  WockyStanza *stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_RESULT, NULL, NULL,
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Exodus 0.9.1",
        '@', "type", "pc",
      ')',
      '(', "feature",
          '@', "var", "http://jabber.org/protocol/disco#info", ')',
      '(', "feature",
          '@', "var", "http://jabber.org/protocol/disco#items", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/muc", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/caps", ')',
      NULL);

  check_hash (stanza, "QgayPKawpkPSDYmwT/WM94uAlu0=");
}

static void
test_complex (void)
{
  /* Complex example from XEP-0115 */
  WockyStanza *stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#info", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#items", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/muc", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/caps", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "ip_version",
          '(', "value", '$', "ipv4", ')',
          '(', "value", '$', "ipv6", ')',
        ')',
        '(', "field",
          '@', "var", "os",
          '(', "value", '$', "Mac", ')',
        ')',
        '(', "field",
          '@', "var", "os_version",
          '(', "value", '$', "10.5.1", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '(', "value", '$', "Psi", ')',
        ')',
        '(', "field",
          '@', "var", "software_version",
          '(', "value", '$', "0.11", ')',
        ')',
      ')',
      NULL);

  check_hash (stanza, "q07IKJEyjvHSyhy//CH0CxmKi8w=");
}

static void
test_sorting_simple (void)
{
  WockyStanza *stanza;
  gchar *one, *two;

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      NULL);

  one = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      NULL);

  two = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert_cmpstr (one, ==, two);

  g_free (one);
  g_free (two);
}

static void
test_sorting_complex (void)
{
  WockyStanza *stanza;
  gchar *one, *two;

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#info", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#items", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/muc", ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/caps", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "ip_version",
          '(', "value", '$', "ipv4", ')',
          '(', "value", '$', "ipv6", ')',
        ')',
        '(', "field",
          '@', "var", "os",
          '(', "value", '$', "Mac", ')',
        ')',
        '(', "field",
          '@', "var", "os_version",
          '(', "value", '$', "10.5.1", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '(', "value", '$', "Psi", ')',
        ')',
        '(', "field",
          '@', "var", "software_version",
          '(', "value", '$', "0.11", ')',
        ')',
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:somethingelse", ')',
        ')',
        '(', "field",
          '@', "var", "foo",
          '(', "value", '$', "bananas", ')',
          '(', "value", '$', "cheese", ')',
        ')',
        '(', "field",
          '@', "var", "wheeeeee",
          '(', "value", '$', "I'm on a rollercoster", ')',
        ')',
        '(', "field",
          '@', "var", "loldongz",
          '@', "type", "boolean",
          '(', "value", '$', "1", ')',
        ')',
      ')',
      NULL);

  one = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#items", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:somethingelse", ')',
        ')',
        '(', "field",
          '@', "var", "wheeeeee",
          '(', "value", '$', "I'm on a rollercoster", ')',
        ')',
        '(', "field",
          '@', "var", "loldongz",
          '@', "type", "boolean",
          '(', "value", '$', "1", ')',
        ')',
        '(', "field",
          '@', "var", "foo",
          '(', "value", '$', "cheese", ')',
          '(', "value", '$', "bananas", ')',
        ')',
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/muc", ')',
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#info", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '(', "value", '$', "Psi", ')',
        ')',
        '(', "field",
          '@', "var", "os",
          '(', "value", '$', "Mac", ')',
        ')',
        '(', "field",
          '@', "var", "os_version",
          '(', "value", '$', "10.5.1", ')',
        ')',
        '(', "field",
          '@', "var", "software_version",
          '(', "value", '$', "0.11", ')',
        ')',
        '(', "field",
          '@', "var", "ip_version",
          '(', "value", '$', "ipv4", ')',
          '(', "value", '$', "ipv6", ')',
        ')',
      ')',
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/caps", ')',
      NULL);

  two = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert_cmpstr (one, ==, two);

  g_free (one);
  g_free (two);
}

static void
test_dataforms_invalid (void)
{
  gchar *out;
  /* this stanza has a bad data form type */
  WockyStanza *stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "lol",
      ')',
      NULL);

  out = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  /* should be NULL because of type="lol" */
  g_assert (out == NULL);

  /* now with no FORM_TYPE field but fine otherwise */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "ip_version",
          '(', "value", '$', "ipv4", ')',
          '(', "value", '$', "ipv6", ')',
        ')',
      ')',
      NULL);

  /* should just be ignoring the invalid data form as XEP-0115 section
   * "5.4 Processing Method", bullet point 3.6 tells us */
  check_hash (stanza, "9LXnSGAOqGkjoewMq7WHTF4wK/U=");

  /* now with a FORM_TYPE but not type="hidden" */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
      ')',
      NULL);

  /* should just be ignoring the invalid data form as XEP-0115 section
   * "5.4 Processing Method", bullet point 3.6 tells us */
  check_hash (stanza, "9LXnSGAOqGkjoewMq7WHTF4wK/U=");

  /* now with <field var='blah'><value/></field> but fine otherwise;
   * this will fail because we have everything about the field but the
   * actual value */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "lol_version",
          '(', "value", ')',
        ')',
      ')',
      NULL);

  out = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert (out == NULL);

  /* now with <field var='blah' /> but fine otherwise; this will fail
   * because we have everything about the field but the actual
   * value */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "lol_version",
        ')',
      ')',
      NULL);

  out = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert (out == NULL);

  /* now with <field /> but fine otherwise; the data form parser will
   * ignore fields with no var='' attribute so this will succeed */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
        ')',
      ')',
      NULL);

  out = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert_cmpstr (out, ==, "wMFSetHbIiscGZgVgx4CZMaYIBQ=");
  g_free (out);
}


static void
test_dataforms_fixed_and_no_var (void)
{
  WockyStanza *stanza;
  gchar *out;

  g_test_bug ("61433");

  /* <field type='fixed'> with no var='' attribute is legal in data forms in
   * general, but the hashing algorithm doesn't specify what to do in this
   * case. We choose to make it fail; previously, we crashed.
   */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "type", "fixed",
          '(', "value", '$', "trolololol", ')',
        ')',
      ')',
      NULL);

  out = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert_cmpstr (out, ==, NULL);
  g_free (out);
}


static void
test_dataforms_form_type_wrong_type (void)
{
  WockyStanza *stanza;

  /* 6. If the response includes an extended service discovery information
   *    form where the FORM_TYPE field is not of type "hidden" or the form
   *    does not include a FORM_TYPE field, ignore the form but continue
   *    processing.
   *
   * This form's FORM_TYPE is of type boolean. This used to crash the hashing
   * code.
   */
  g_test_bug ("61433");
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "boolean",
          '(', "value", '$', "true", ')',
        ')',
      ')',
      NULL);

  check_hash (stanza, "9LXnSGAOqGkjoewMq7WHTF4wK/U=");
}


static void
test_dataforms_form_type_without_value (void)
{
  WockyStanza *stanza;

  /* XEP-0115 says:
   *
   *     5. If the response includes more than one extended service discovery
   *        information form with the same FORM_TYPE or the FORM_TYPE field
   *        contains more than one <value/> element with different XML
   *        character data, consider the entire response to be ill-formed.
   *     6. If the response includes an extended service discovery information
   *        form where the FORM_TYPE field is not of type "hidden" or the form
   *        does not include a FORM_TYPE field, ignore the form but continue
   *        processing.
   *
   * Neither item covers the case where there is a FORM_TYPE field of type
   * "hidden" with no <value/>. I choose to treat it as 5 above.
   */
  g_test_bug ("61433");
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
        ')',
      ')',
      NULL);

  check_hash (stanza, NULL);
}


static void
test_dataforms_form_type_two_both_with_no_value (void)
{
  WockyStanza *stanza;

  /* This ought to be equivalent to test_dataforms_form_type_without_value but
   * there was a crash in the form-sorting code, which of course doesn't
   * trigger if there's only one form.
   */
  g_test_bug ("61433");
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
        ')',
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
        ')',
      ')',
      NULL);

  check_hash (stanza, NULL);
}


static void
test_dataforms_same_type (void)
{
  gchar *out;
  /* stanza has two data forms both with the same FORM_TYPE value */
  WockyStanza *stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Psi 0.11",
        '@', "type", "pc",
        '#', "en",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/caps", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "ip_version",
          '(', "value", '$', "ipv4", ')',
          '(', "value", '$', "ipv6", ')',
        ')',
        '(', "field",
          '@', "var", "os",
          '(', "value", '$', "Mac", ')',
        ')',
        '(', "field",
          '@', "var", "os_version",
          '(', "value", '$', "10.5.1", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '(', "value", '$', "Psi", ')',
        ')',
        '(', "field",
          '@', "var", "software_version",
          '(', "value", '$', "0.11", ')',
        ')',
      ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "ip_version",
          '(', "value", '$', "ipv4", ')',
          '(', "value", '$', "ipv6", ')',
        ')',
        '(', "field",
          '@', "var", "os",
          '(', "value", '$', "Mac", ')',
        ')',
        '(', "field",
          '@', "var", "os_version",
          '(', "value", '$', "10.5.1", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '(', "value", '$', "Psi", ')',
        ')',
        '(', "field",
          '@', "var", "software_version",
          '(', "value", '$', "0.11", ')',
        ')',
      ')',
      NULL);

  out = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert (out == NULL);
}

static void
test_dataforms_boolean_values (void)
{
  WockyStanza *stanza;
  gchar *one, *two;

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#info", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '@', "type", "boolean",
          '(', "value", '$', "1", ')',
        ')',
      ')',
      NULL);

  one = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL, "badger",
      '(', "identity",
        '@', "category", "client",
        '@', "name", "Ψ 0.11",
        '@', "type", "pc",
        '#', "el",
      ')',
      '(', "feature", '@', "var", "http://jabber.org/protocol/disco#info", ')',
      '(', "x",
        ':', "jabber:x:data",
        '@', "type", "result",
        '(', "field",
          '@', "var", "FORM_TYPE",
          '@', "type", "hidden",
          '(', "value", '$', "urn:xmpp:dataforms:softwareinfo", ')',
        ')',
        '(', "field",
          '@', "var", "software",
          '@', "type", "boolean",
          '(', "value", '$', "true", ')',
        ')',
      ')',
      NULL);

  two = wocky_caps_hash_compute_from_node (
      wocky_stanza_get_top_node (stanza));
  g_object_unref (stanza);

  g_assert (one != NULL);
  g_assert (two != NULL);
  g_assert_cmpstr (one, !=, two);

  g_free (one);
  g_free (two);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);
  g_test_add_func ("/caps-hash/simple", test_simple);
  g_test_add_func ("/caps-hash/complex", test_complex);
  g_test_add_func ("/caps-hash/sorting/simple", test_sorting_simple);
  g_test_add_func ("/caps-hash/sorting/complex", test_sorting_complex);
  g_test_add_func ("/caps-hash/dataforms/invalid", test_dataforms_invalid);
  g_test_add_func ("/caps-hash/dataforms/invalid/fixed-and-no-var",
      test_dataforms_fixed_and_no_var);
  g_test_add_func ("/caps-hash/dataforms/invalid/form_type/wrong-type",
      test_dataforms_form_type_wrong_type);
  g_test_add_func ("/caps-hash/dataforms/invalid/form_type/no-value",
      test_dataforms_form_type_without_value);
  g_test_add_func ("/caps-hash/dataforms/invalid/form_type/two-both-with-no-value",
      test_dataforms_form_type_two_both_with_no_value);

  g_test_add_func ("/caps-hash/dataforms/same-type", test_dataforms_same_type);
  g_test_add_func ("/caps-hash/dataforms/boolean-values",
      test_dataforms_boolean_values);

  result = g_test_run ();
  test_deinit ();

  return result;
}
