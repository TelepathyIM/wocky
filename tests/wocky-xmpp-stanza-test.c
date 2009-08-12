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

/* test wocky_xmpp_stanza_to_gerror */
static void
check_error (WockyXmppStanza *stanza,
    GQuark domain,
    gint code,
    const gchar *msg)
{
  GError *error = wocky_xmpp_stanza_to_gerror (stanza);

  g_assert_error (error, domain, code);
  g_assert (!wocky_strdiff (error->message, msg));
  g_error_free (error);
}

static void
test_stream_error_to_gerror (void)
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
      WOCKY_XMPP_STREAM_ERROR_CONFLICT, "a stream error occurred");
  g_object_unref (stanza);

  /* Valid stream error with message */
  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_STREAM_ERROR,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAM,
    WOCKY_NODE, "system-shutdown",
      WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAMS,
    WOCKY_NODE_END,
    WOCKY_NODE, "text",
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
     WOCKY_XMPP_STREAM_ERROR_UNKNOWN, "a stream error occurred");
  g_object_unref (stanza);

  /* Not an error */
  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
    WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_STREAM,
    WOCKY_STANZA_END);

  error = wocky_xmpp_stanza_to_gerror (stanza);
  g_assert_no_error (error);
  g_object_unref (stanza);
}

static void
test_xmpp_error_to_gerror (void)
{
  WockyXmppError xmpp_error;

  for (xmpp_error = 1; xmpp_error < NUM_WOCKY_XMPP_ERRORS; xmpp_error++)
    {
      WockyXmppStanza *stanza;
      GError *error = NULL;

      stanza = wocky_xmpp_stanza_build (
          WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
          "from", "to",
          WOCKY_STANZA_END);
      wocky_xmpp_error_to_node (xmpp_error, stanza->node, NULL);

      error = wocky_xmpp_stanza_to_gerror (stanza);
      g_assert_error (error, WOCKY_XMPP_ERROR, (gint) xmpp_error);
      g_assert (!wocky_strdiff (error->message,
            wocky_xmpp_error_description (xmpp_error)));

      g_object_unref (stanza);
      g_error_free (error);
    }
}

int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);
  g_test_add_func ("/xmpp-stanza/build-iq-result", test_build_iq_result);
  g_test_add_func ("/xmpp-stanza/build-iq-error", test_build_iq_error);
  g_test_add_func ("/xmpp-stanza/stream-error-to-gerror",
      test_stream_error_to_gerror);
  g_test_add_func ("/xmpp-stanza/xmpp-error-to-gerror",
      test_xmpp_error_to_gerror);

  result =  g_test_run ();
  test_deinit ();
  return result;
}
