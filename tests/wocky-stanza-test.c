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
test_copy (void)
{
  WockyStanza *iq, *copy, *expected;

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '@', "id", "one",
        '(', "query",
          ':', "http://jabber.org/protocol/disco#items",
        ')',
      NULL);

  /* just to make sure */
  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
      '@', "id", "one",
        '(', "query",
          ':', "http://jabber.org/protocol/disco#items",
        ')',
      NULL);

  copy = wocky_stanza_copy (iq);
  g_assert (copy != NULL);

  test_assert_stanzas_equal (iq, copy);
  test_assert_stanzas_equal (expected, copy);

  g_object_unref (iq);
  g_object_unref (copy);
  g_object_unref (expected);
}

static void
test_build_iq_result_simple_ack (void)
{
  WockyStanza *iq, *reply, *expected;

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
      ')',
    NULL);

  /* Send a simple ACK */
  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    '@', "id", "one",
    NULL);

  reply = wocky_stanza_build_iq_result (iq, NULL);

  g_assert (reply != NULL);
  test_assert_stanzas_equal (reply, expected);

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
test_build_iq_result_complex_reply (void)
{
  WockyStanza *iq, *reply, *expected;

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
      ')',
    NULL);

  /* Send a more complex reply */
  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
        '(', "item",
          '@', "jid", "streamhostproxy.example.net",
          '@', "name", "Bytestreams Proxy",
        ')',
      ')',
    NULL);

  reply = wocky_stanza_build_iq_result (iq,
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
        '(', "item",
          '@', "jid", "streamhostproxy.example.net",
          '@', "name", "Bytestreams Proxy",
        ')',
      ')',
      NULL);

  g_assert (reply != NULL);
  test_assert_stanzas_equal (reply, expected);

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
test_build_iq_result_no_to_attr (void)
{
  WockyStanza *iq, *reply, *expected;

  /* Send a reply to an IQ with no "to" attribute. */
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", NULL,
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
      ')',
    NULL);

  /* Send a simple ACK */
  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, NULL, "juliet@example.com",
    '@', "id", "one",
    NULL);

  reply = wocky_stanza_build_iq_result (iq, NULL);

  g_assert (reply != NULL);
  test_assert_stanzas_equal (reply, expected);

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
test_build_iq_error_simple_error (void)
{
  WockyStanza *iq, *reply, *expected;

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
      ')',
    NULL);

  /* Send a simple error */
  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_ERROR, "romeo@example.net", "juliet@example.com",
    '@', "id", "one",
    '(', "query",
      ':', "http://jabber.org/protocol/disco#items",
    ')',
    NULL);

  reply = wocky_stanza_build_iq_error (iq, NULL);

  g_assert (reply != NULL);
  test_assert_stanzas_equal (reply, expected);

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
test_build_iq_error_complex (void)
{
  WockyStanza *iq, *reply, *expected;

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
      ')',
    NULL);

  /* Send a more complex reply */
  expected = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_ERROR, "romeo@example.net", "juliet@example.com",
    '@', "id", "one",
      '(', "query",
        ':', "http://jabber.org/protocol/disco#items",
      ')',
      '(', "error",
        '@', "code", "403",
        '@', "type", "auth",
      ')',
    NULL);

  reply = wocky_stanza_build_iq_error (iq,
      '(', "error",
        '@', "code", "403",
        '@', "type", "auth",
      ')',
      NULL);

  g_assert (reply != NULL);
  test_assert_stanzas_equal (reply, expected);

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
check_error (WockyStanza *stanza,
    GQuark domain,
    gint code,
    const gchar *msg)
{
  GError *error = NULL;

  g_assert (wocky_stanza_extract_stream_error (stanza, &error));

  g_assert_error (error, domain, code);
  g_assert_cmpstr (error->message, ==, msg);
  g_error_free (error);
}

static void
test_extract_stanza_error (void)
{
  WockyStanza *stanza;
  GError *error = NULL;

  /* Valid stream error without message */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    ':', WOCKY_XMPP_NS_STREAM,
    '(', "conflict",
      ':', WOCKY_XMPP_NS_STREAMS,
    ')',
    NULL);

  check_error (stanza, WOCKY_XMPP_STREAM_ERROR,
      WOCKY_XMPP_STREAM_ERROR_CONFLICT, "");
  g_object_unref (stanza);

  /* Valid stream error with message */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    ':', WOCKY_XMPP_NS_STREAM,
    '(', "system-shutdown",
      ':', WOCKY_XMPP_NS_STREAMS,
    ')',
    '(', "text",
      ':', WOCKY_XMPP_NS_STREAMS,
      '$', "bye bye",
    ')',
    NULL);

  check_error (stanza, WOCKY_XMPP_STREAM_ERROR,
     WOCKY_XMPP_STREAM_ERROR_SYSTEM_SHUTDOWN, "bye bye");
  g_object_unref (stanza);

  /* Unknown stream error */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    ':', WOCKY_XMPP_NS_STREAM,
    '(', "badger",
      ':', WOCKY_XMPP_NS_STREAMS,
    ')',
    NULL);

  check_error (stanza, WOCKY_XMPP_STREAM_ERROR,
     WOCKY_XMPP_STREAM_ERROR_UNKNOWN, "");
  g_object_unref (stanza);

  /* Not an error */
  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    ':', WOCKY_XMPP_NS_STREAM,
    NULL);

  g_assert (!wocky_stanza_extract_stream_error (stanza, &error));
  g_assert_no_error (error);
  g_object_unref (stanza);
}

static void
test_extract_errors_not_error (void)
{
  WockyStanza *stanza;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;
  gboolean ret;

  /* As a prelude, check that it does the right thing for non-errors. */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      "from", "to",
        '(', "hello-thar",
        ')',
      NULL);

  ret = wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert (!ret);
  g_assert_no_error (core);
  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);
}

static void
test_extract_errors_without_description (void)
{
  WockyStanza *stanza;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;
  gboolean ret;

  /* Test a boring error with no description */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "modify",
          '(', "bad-request",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
        ')',
      NULL);

  ret = wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert (ret);
  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_MODIFY);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_BAD_REQUEST);
  g_assert_cmpstr (core->message, ==, "");
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);
}

static void
test_extract_errors_with_text (void)
{
  WockyStanza *stanza;
  const gchar *description = "I am a sentence.";
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* Now a different error with some text */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "cancel",
          '(', "item-not-found",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          '(', "text",
            ':', WOCKY_XMPP_NS_STANZAS,
            '$', description,
          ')',
        ')',
      NULL);

  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_ITEM_NOT_FOUND);
  g_assert_cmpstr (core->message, ==, description);
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);
}

static void
test_extract_errors_application_specific_unknown (void)
{
  WockyStanza *stanza;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* Another error, with an application-specific element we don't understand */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "cancel",
          '(', "subscription-required",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          '(', "buy-a-private-cloud",
            ':', "http://example.com/angry-cloud",
          ')',
        ')',
      NULL);

  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR,
      WOCKY_XMPP_ERROR_SUBSCRIPTION_REQUIRED);
  g_assert_cmpstr (core->message, ==, "");
  g_clear_error (&core);

  g_assert_no_error (specialized);

  /* The namespace is not registered, @specialized is not set. However we
   * assume that any other namespace element is a specialized error, and it
   * should get returned in @specialized_node
   */
  g_assert (specialized_node);
  g_assert_cmpstr (specialized_node->name, ==, "buy-a-private-cloud");
  g_assert_cmpstr (wocky_node_get_ns (specialized_node), ==,
      "http://example.com/angry-cloud");

  g_object_unref (stanza);
}

static void
test_extract_errors_jingle_error (void)
{
  WockyStanza *stanza;
  const gchar *description = "I am a sentence.";
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* A Jingle error! With the child nodes in an erratic order */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "cancel",
          '(', "tie-break",
            ':', WOCKY_XMPP_NS_JINGLE_ERRORS,
          ')',
          '(', "text",
            ':', WOCKY_XMPP_NS_STANZAS,
            '$', description,
          ')',
          '(', "conflict",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
        ')',
      NULL);

  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_CONFLICT);
  g_assert_cmpstr (core->message, ==, description);
  g_clear_error (&core);

  g_assert_error (specialized, WOCKY_JINGLE_ERROR,
      WOCKY_JINGLE_ERROR_TIE_BREAK);
  g_assert_cmpstr (specialized->message, ==, description);
  g_clear_error (&specialized);

  g_assert (specialized_node != NULL);
  g_assert_cmpstr (specialized_node->name, ==, "tie-break");

  /* With the same stanza, let's try ignoring all out params: */
  wocky_stanza_extract_errors (stanza, NULL, NULL, NULL, NULL);

  g_object_unref (stanza);
}

static void
test_extract_errors_extra_application_specific (void)
{
  WockyStanza *stanza;
  const gchar *description = "I am a sentence.";
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* Jingle error! + Bogus extra app specific error, which should be ignored */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "cancel",
          '(', "tie-break",
            ':', WOCKY_XMPP_NS_JINGLE_ERRORS,
          ')',
          '(', "out-of-order",
            ':', WOCKY_XMPP_NS_JINGLE_ERRORS,
          ')',
          '(', "text",
            ':', WOCKY_XMPP_NS_STANZAS,
            '$', description,
          ')',
          '(', "conflict",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
        ')',
      NULL);

  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_CONFLICT);
  g_assert_cmpstr (core->message, ==, description);
  g_clear_error (&core);

  g_assert_error (specialized, WOCKY_JINGLE_ERROR,
      WOCKY_JINGLE_ERROR_TIE_BREAK);
  g_assert_cmpstr (specialized->message, ==, description);
  g_clear_error (&specialized);

  g_assert (specialized_node != NULL);
  g_assert_cmpstr (specialized_node->name, ==, "tie-break");

  g_object_unref (stanza);
}

static void
test_extract_errors_legacy_code (void)
{
  WockyStanza *stanza;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* How about a legacy error code? */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "code", "408",
        ')',
      NULL);

  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  /* XEP-0086 §3 says that 408 maps to remote-server-timeout, type=wait */
  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_WAIT);

  g_assert_error (core, WOCKY_XMPP_ERROR,
      WOCKY_XMPP_ERROR_REMOTE_SERVER_TIMEOUT);
  /* No assertion about the message. As an implementation detail, it's probably
   * the definition of r-s-t from XMPP Core.
   */
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);
}


static void
test_extract_errors_unrecognised_condition (void)
{
  WockyNodeTree *error_tree;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;
  const gchar *text = "The room is currently overactive, please try again later";

  /* I got a <policy-violation> error back from prosody with type='wait', but
   * Wocky didn't know about policy-violation (which was introduced in
   * RFC6120). Not only did it ignore <policy-violation>, it also ignored
   * type='wait' and returned the default, WOCKY_XMPP_ERROR_TYPE_CANCEL.
   */
  g_test_bug ("43166#c9");

  error_tree = wocky_node_tree_new ("error", WOCKY_XMPP_NS_JABBER_CLIENT,
      '@', "type", "wait",
      '(', "typo-violation", ':', WOCKY_XMPP_NS_STANZAS, ')',
      '(', "text", ':', WOCKY_XMPP_NS_STANZAS,
        '$', text,
      ')', NULL);

  wocky_xmpp_error_extract (wocky_node_tree_get_top_node (error_tree),
      &type, &core, &specialized, &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_WAIT);

  /* Wocky should default to undefined-condition when the server returns an
   * unknown core error element.
   */
  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_UNDEFINED_CONDITION);
  g_assert_cmpstr (core->message, ==, text);
  g_clear_error (&core);

  /* The unrecognised error element was in the :xmpp-stanzas namespace, so it
   * shouldn't be returned as a specialized error.
   */
  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (error_tree);
}


static void
test_extract_errors_no_sense (void)
{
  WockyStanza *stanza;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* An error that makes no sense */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "aoeu", "snth",
          '(', "hoobily-lala-whee",
          ')',
          '(', "møøse",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
        ')',
      NULL);

  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  /* 'cancel' is the most sensible default if we have no idea. */
  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_UNDEFINED_CONDITION);
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node != NULL);
  g_assert_cmpstr (specialized_node->name, ==, "hoobily-lala-whee");

  g_object_unref (stanza);
}

static void
test_extract_errors_not_really (void)
{
  WockyStanza *stanza;
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyNode *specialized_node = NULL;

  /* And finally, a stanza with type='error' but no <error/> at all... */
  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to", NULL);
  wocky_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  /* 'cancel' is the most sensible default if we have no idea. */
  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_UNDEFINED_CONDITION);
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);
}

#define assert_cmperr(e1, e2) \
  G_STMT_START { \
    g_assert_error (e1, e2->domain, e2->code); \
    g_assert_cmpstr (e1->message, ==, e2->message); \
  } G_STMT_END

static void
test_stanza_error_to_node_core (void)
{
  GError *e = NULL;
  GError *core = NULL, *specialized = NULL;
  const gchar *description = "bzzzt";
  WockyStanza *stanza, *expected;

  /* An XMPP Core stanza error */
  g_set_error_literal (&e, WOCKY_XMPP_ERROR,
      WOCKY_XMPP_ERROR_REMOTE_SERVER_TIMEOUT, description);

  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
      NULL);

  wocky_stanza_error_to_node (e, wocky_stanza_get_top_node (stanza));

  expected = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "wait",
          '@', "code", "504", /* Per XEP-0086 */
          '(', "remote-server-timeout",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          '(', "text",
            ':', WOCKY_XMPP_NS_STANZAS,
            '$', description,
          ')',
        ')',
      NULL);
  test_assert_stanzas_equal (stanza, expected);

  /* Let's see how it roundtrips: */
  wocky_stanza_extract_errors (stanza, NULL, &core, &specialized, NULL);

  assert_cmperr (e, core);
  g_assert_no_error (specialized);

  g_object_unref (stanza);
  g_object_unref (expected);
  g_clear_error (&e);
  g_clear_error (&core);
}

static void
test_stanza_error_to_node_jingle (void)
{
  GError *e = NULL;
  GError *core = NULL, *specialized = NULL;
  const gchar *description = "bzzzt";
  WockyStanza *stanza, *expected;

  /* How about a nice game of Jingle? */
  g_set_error_literal (&e, WOCKY_JINGLE_ERROR,
      WOCKY_JINGLE_ERROR_UNKNOWN_SESSION, description);

  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
      NULL);

  wocky_stanza_error_to_node (e, wocky_stanza_get_top_node (stanza));

  expected = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        '(', "error",
          '@', "type", "cancel",
          '@', "code", "404", /* Per XEP-0086 */
          '(', "item-not-found",
            ':', WOCKY_XMPP_NS_STANZAS,
          ')',
          '(', "unknown-session",
            ':', WOCKY_XMPP_NS_JINGLE_ERRORS,
          ')',
          '(', "text",
            ':', WOCKY_XMPP_NS_STANZAS,
            '$', description,
          ')',
        ')',
      NULL);
  test_assert_stanzas_equal (stanza, expected);

  /* Let's see how it roundtrips: */
  wocky_stanza_extract_errors (stanza, NULL, &core, &specialized, NULL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_ITEM_NOT_FOUND);
  assert_cmperr (e, specialized);

  g_object_unref (stanza);
  g_object_unref (expected);
  g_clear_error (&e);
  g_clear_error (&core);
  g_clear_error (&specialized);
}

static void
test_unknown (
    gconstpointer name_null_ns)
{
  const gchar *name = name_null_ns;
  const gchar *ns = name + strlen (name) + 1;
  WockyStanza *stanza = wocky_stanza_new (name, ns);
  WockyStanzaType type;

  wocky_stanza_get_type_info (stanza, &type, NULL);
  g_assert_cmpuint (type, ==, WOCKY_STANZA_TYPE_UNKNOWN);

  g_object_unref (stanza);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);
  g_test_add_func ("/xmpp-stanza/copy", test_copy);
  g_test_add_func ("/xmpp-stanza/iq-result/build-simple-ack",
      test_build_iq_result_simple_ack);
  g_test_add_func ("/xmpp-stanza/iq-result/build-complex-reply",
      test_build_iq_result_complex_reply);
  g_test_add_func ("/xmpp-stanza/iq-result/build-no-to-attr",
      test_build_iq_result_no_to_attr);
  g_test_add_func ("/xmpp-stanza/errors/build-simple",
      test_build_iq_error_simple_error);
  g_test_add_func ("/xmpp-stanza/errors/build-complex",
      test_build_iq_error_complex);
  g_test_add_func ("/xmpp-stanza/errors/extract-stanza",
      test_extract_stanza_error);
  g_test_add_func ("/xmpp-stanza/errors/not-error",
      test_extract_errors_not_error);
  g_test_add_func ("/xmpp-stanza/errors/without-description",
      test_extract_errors_without_description);
  g_test_add_func ("/xmpp-stanza/errors/with-text",
      test_extract_errors_with_text);
  g_test_add_func ("/xmpp-stanza/errors/application-specific-unknown",
      test_extract_errors_application_specific_unknown);
  g_test_add_func ("/xmpp-stanza/errors/jingle-error",
      test_extract_errors_jingle_error);
  g_test_add_func ("/xmpp-stanza/errors/extra-application-specific",
      test_extract_errors_extra_application_specific);
  g_test_add_func ("/xmpp-stanza/errors/legacy-code",
      test_extract_errors_legacy_code);
  g_test_add_func ("/xmpp-stanza/errors/unrecognised-condition",
      test_extract_errors_unrecognised_condition);
  g_test_add_func ("/xmpp-stanza/errors/no-sense",
      test_extract_errors_no_sense);
  g_test_add_func ("/xmpp-stanza/errors/not-really",
      test_extract_errors_not_really);
  g_test_add_func ("/xmpp-stanza/errors/stanza-to-node",
      test_stanza_error_to_node_core);
  g_test_add_func ("/xmpp-stanza/errors/stanza-to-node-jingle",
      test_stanza_error_to_node_jingle);

  g_test_add_data_func ("/xmpp-stanza/types/unknown-stanza-type",
      "this-will-never-be-real\0" WOCKY_XMPP_NS_JABBER_CLIENT,
      test_unknown);
  g_test_add_data_func ("/xmpp-stanza/types/wrong-namespaces",
      "challenge\0this:is:not:the:sasl:namespace",
      test_unknown);

  result =  g_test_run ();
  test_deinit ();
  return result;
}
