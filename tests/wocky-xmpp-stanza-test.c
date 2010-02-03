#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-stanza.h>
#include <wocky/wocky-xmpp-error.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-namespaces.h>

#include "wocky-test-helper.h"

static void
test_build_iq_result (void)
{
  WockyXmppStanza *iq, *reply, *expected;

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  /* Send a simple ACK */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_result (iq, WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);

  /* Send a more complex reply */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "streamhostproxy.example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Bytestreams Proxy",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_result (iq,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
        WOCKY_NODE, "item",
          WOCKY_NODE_ATTRIBUTE, "jid", "streamhostproxy.example.net",
          WOCKY_NODE_ATTRIBUTE, "name", "Bytestreams Proxy",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);

  /* Send a reply to an IQ with no "to" attribute. */
  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", NULL,
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  /* Send a simple ACK */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_RESULT, NULL, "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_result (iq, WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
test_build_iq_error (void)
{
  WockyXmppStanza *iq, *reply, *expected;

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_GET, "juliet@example.com", "romeo@example.net",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  /* Send a simple error */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_ERROR, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_error (iq, WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);

  /* Send a more complex reply */
  expected = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
    WOCKY_STANZA_SUB_TYPE_ERROR, "romeo@example.net", "juliet@example.com",
    WOCKY_NODE_ATTRIBUTE, "id", "one",
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "code", "403",
          WOCKY_NODE_ATTRIBUTE, "type", "auth",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  reply = wocky_xmpp_stanza_build_iq_error (iq,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, "http://jabber.org/protocol/disco#items",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "code", "403",
          WOCKY_NODE_ATTRIBUTE, "type", "auth",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  g_assert (reply != NULL);
  g_assert (wocky_xmpp_node_equal (reply->node, expected->node));

  g_object_unref (reply);
  g_object_unref (expected);
  g_object_unref (iq);
}

static void
check_error (WockyXmppStanza *stanza,
    GQuark domain,
    gint code,
    const gchar *msg)
{
  GError *error = NULL;

  g_assert (wocky_xmpp_stanza_extract_stream_error (stanza, &error));

  g_assert_error (error, domain, code);
  g_assert_cmpstr (error->message, ==, msg);
  g_error_free (error);
}

static void
test_extract_stanza_error (void)
{
  WockyXmppStanza *stanza;
  GError *error = NULL;

  /* Valid stream error without message */
  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAM,
    WOCKY_NODE, "conflict",
      WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAMS,
    WOCKY_NODE_END,
    WOCKY_STANZA_END);

  check_error (stanza, WOCKY_XMPP_STREAM_ERROR,
      WOCKY_XMPP_STREAM_ERROR_CONFLICT, "");
  g_object_unref (stanza);

  /* Valid stream error with message */
  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAM,
    WOCKY_NODE, "system-shutdown",
      WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAMS,
    WOCKY_NODE_END,
    WOCKY_NODE, "text",
      WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAMS,
      WOCKY_NODE_TEXT, "bye bye",
    WOCKY_NODE_END,
    WOCKY_STANZA_END);

  check_error (stanza, WOCKY_XMPP_STREAM_ERROR,
     WOCKY_XMPP_STREAM_ERROR_SYSTEM_SHUTDOWN, "bye bye");
  g_object_unref (stanza);

  /* Unknown stream error */
  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAM,
    WOCKY_NODE, "badger",
      WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAMS,
    WOCKY_NODE_END,
    WOCKY_STANZA_END);

  check_error (stanza, WOCKY_XMPP_STREAM_ERROR,
     WOCKY_XMPP_STREAM_ERROR_UNKNOWN, "");
  g_object_unref (stanza);

  /* Not an error */
  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAM,
    WOCKY_STANZA_END);

  g_assert (!wocky_xmpp_stanza_extract_stream_error (stanza, &error));
  g_assert_no_error (error);
  g_object_unref (stanza);
}

static void
test_extract_errors (void)
{
  WockyXmppStanza *stanza;
  const gchar *description = "I am a sentence.";
  WockyXmppErrorType type;
  GError *core = NULL, *specialized = NULL;
  WockyXmppNode *specialized_node = NULL;
  gboolean ret;

  /* As a prelude, check that it does the right thing for non-errors. */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      "from", "to",
        WOCKY_NODE, "hello-thar",
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  ret = wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert (!ret);
  g_assert_no_error (core);
  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  /* Test a boring error with no description */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "type", "modify",
          WOCKY_NODE, "bad-request",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  ret = wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert (ret);
  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_MODIFY);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_BAD_REQUEST);
  g_assert_cmpstr (core->message, ==, "");
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  /* Now a different error with some text */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "type", "cancel",
          WOCKY_NODE, "item-not-found",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
          WOCKY_NODE, "text",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
            WOCKY_NODE_TEXT, description,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_ITEM_NOT_FOUND);
  g_assert_cmpstr (core->message, ==, description);
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);

  /* Another error, with an application-specific element we don't understand */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "type", "cancel",
          WOCKY_NODE, "subscription-required",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
          WOCKY_NODE, "buy-a-private-cloud",
            WOCKY_NODE_XMLNS, "http://example.com/angry-cloud",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR,
      WOCKY_XMPP_ERROR_SUBSCRIPTION_REQUIRED);
  g_assert_cmpstr (core->message, ==, "");
  g_clear_error (&core);

  g_assert_no_error (specialized);

  /* This is questionable: maybe wocky_xmpp_error_extract() should assume that
   * a child of <error/> in a NS it doesn't understand is a specialized error,
   * rather than requiring the ns to be registered with
   * wocky_xmpp_error_register_domain().
   */
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);

  /* A Jingle error! With the child nodes in an erratic order */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "type", "cancel",
          WOCKY_NODE, "tie-break",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_JINGLE_ERRORS,
          WOCKY_NODE_END,
          WOCKY_NODE, "text",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
            WOCKY_NODE_TEXT, description,
          WOCKY_NODE_END,
          WOCKY_NODE, "conflict",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
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
  wocky_xmpp_stanza_extract_errors (stanza, NULL, NULL, NULL, NULL);

  g_object_unref (stanza);

  /* How about a legacy error code? */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "code", "408",
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
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

  /* And finally, an error that's completely broken. */
  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "aoeu", "snth",
          WOCKY_NODE, "hoobily-lala-whee",
          WOCKY_NODE_END,
          WOCKY_NODE, "møøse",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  wocky_xmpp_stanza_extract_errors (stanza, &type, &core, &specialized,
      &specialized_node);

  /* 'cancel' is the most sensible default if we have no idea. */
  g_assert_cmpuint (type, ==, WOCKY_XMPP_ERROR_TYPE_CANCEL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_UNDEFINED_CONDITION);
  g_clear_error (&core);

  g_assert_no_error (specialized);
  g_assert (specialized_node == NULL);

  g_object_unref (stanza);
}

#define assert_nodes_equal(n1, n2) \
  G_STMT_START { \
    if (!wocky_xmpp_node_equal ((n1), (n2))) \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
          g_strdup_printf ("Nodes not equal:\n%s\n\n%s", \
              wocky_xmpp_node_to_string (n1), \
              wocky_xmpp_node_to_string (n2))); \
  } G_STMT_END

#define assert_cmperr(e1, e2) \
  G_STMT_START { \
    g_assert_error(e1, e2->domain, e2->code); \
    g_assert_cmpstr(e1->message, ==, e2->message); \
  } G_STMT_END

static void
test_stanza_error_to_node (void)
{
  GError *e = NULL;
  GError *core = NULL, *specialized = NULL;
  const gchar *description = "bzzzt";
  WockyXmppStanza *stanza, *expected;

  /* An XMPP Core stanza error */
  g_set_error_literal (&e, WOCKY_XMPP_ERROR,
      WOCKY_XMPP_ERROR_REMOTE_SERVER_TIMEOUT, description);

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
      WOCKY_STANZA_END);

  wocky_stanza_error_to_node (e, stanza->node);

  expected = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "type", "wait",
          WOCKY_NODE_ATTRIBUTE, "code", "504", /* Per XEP-0086 */
          WOCKY_NODE, "remote-server-timeout",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
          WOCKY_NODE, "text",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
            WOCKY_NODE_TEXT, description,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);
  assert_nodes_equal (stanza->node, expected->node);

  /* Let's see how it roundtrips: */
  wocky_xmpp_stanza_extract_errors (stanza, NULL, &core, &specialized, NULL);

  assert_cmperr (e, core);
  g_assert_no_error (specialized);

  g_object_unref (stanza);
  g_object_unref (expected);
  g_clear_error (&e);
  g_clear_error (&core);

  /* How about a nice game of Jingle? */
  g_set_error_literal (&e, WOCKY_JINGLE_ERROR,
      WOCKY_JINGLE_ERROR_UNKNOWN_SESSION, description);

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
      WOCKY_STANZA_END);

  wocky_stanza_error_to_node (e, stanza->node);

  expected = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      "from", "to",
        WOCKY_NODE, "error",
          WOCKY_NODE_ATTRIBUTE, "type", "cancel",
          WOCKY_NODE_ATTRIBUTE, "code", "404", /* Per XEP-0086 */
          WOCKY_NODE, "item-not-found",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
          WOCKY_NODE_END,
          WOCKY_NODE, "unknown-session",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_JINGLE_ERRORS,
          WOCKY_NODE_END,
          WOCKY_NODE, "text",
            WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STANZAS,
            WOCKY_NODE_TEXT, description,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);
  assert_nodes_equal (stanza->node, expected->node);

  /* Let's see how it roundtrips: */
  wocky_xmpp_stanza_extract_errors (stanza, NULL, &core, &specialized, NULL);

  g_assert_error (core, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_ITEM_NOT_FOUND);
  assert_cmperr (e, specialized);

  g_object_unref (stanza);
  g_object_unref (expected);
  g_clear_error (&e);
  g_clear_error (&core);
  g_clear_error (&specialized);
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);
  g_test_add_func ("/xmpp-stanza/build-iq-result", test_build_iq_result);
  g_test_add_func ("/xmpp-stanza/build-iq-error", test_build_iq_error);
  g_test_add_func ("/xmpp-stanza/extract-stanza-error",
      test_extract_stanza_error);
  g_test_add_func ("/xmpp-stanza/extract-errors", test_extract_errors);
  g_test_add_func ("/xmpp-stanza/stanza-error-to-node",
      test_stanza_error_to_node);

  result =  g_test_run ();
  test_deinit ();
  return result;
}
