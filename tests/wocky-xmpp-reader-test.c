#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

#define HEADER \
"<?xml version='1.0' encoding='UTF-8'?>                                    " \
"<stream:stream xmlns='jabber:client'                                      " \
"  xmlns:stream='http://etherx.jabber.org/streams'>                        "

#define FOOTER "</stream:stream> "

#define BROKEN_HEADER \
"<?xml version='1.0' encoding='UTF-8'?>                                    " \
"<stream:streamsss xmlns='jabber:client'                                   " \
"  xmlns:stream='http://etherx.jabber.org/streams'>                        "

#define HEADER_WITH_UNQUALIFIED_LANG \
"<?xml version='1.0' encoding='UTF-8'?>                                    " \
"<stream:stream xmlns='jabber:client'                                      " \
"  xmlns:stream='http://etherx.jabber.org/streams'                         " \
"  lang='fi'>                                                              "

#define BROKEN_MESSAGE \
"  <message to='juliet@example.com' from='romeo@example.net' xml:lang='en' " \
"   id=\"0\">                                                              " \
"    <body>Art thou not Romeo, and a Montague?</ody>                       " \
"  </essage>                                                               "

#define MESSAGE_CHUNK0 \
"  <message to='juliet@example.com' from='romeo@example.net' xml:lang='en' " \
"   id=\"0\">                                                              " \
"    <body>Art thou not Romeo,                                             "

#define MESSAGE_CHUNK1 \
"       and a Montague?</body>                                             " \
"  </message>                                                              "

#define VCARD_MESSAGE \
" <iq id='v1'                                                    " \
"     to='stpeter@jabber.org/roundabout'                         " \
"     type='result'>                                             " \
"   <vCard xmlns='vcard-temp'>                                   " \
"     <FN>Peter Saint-Andre</FN>                                 " \
"     <N>                                                        " \
"      <FAMILY>Saint-Andre</FAMILY>                              " \
"      <GIVEN>Peter</GIVEN>                                      " \
"      <MIDDLE/>                                                 " \
"    </N>                                                        " \
"    <NICKNAME>stpeter</NICKNAME>                                " \
"    <URL>http://www.xmpp.org/xsf/people/stpeter.shtml</URL>     " \
"    <BDAY>1966-08-06</BDAY>                                     " \
"    <ORG>                                                       " \
"      <ORGNAME>XMPP Standards Foundation</ORGNAME>              " \
"      <ORGUNIT/>                                                " \
"    </ORG>                                                      " \
"    <TITLE>Executive Director</TITLE>                           " \
"    <ROLE>Patron Saint</ROLE>                                   " \
"    <JABBERID>stpeter@jabber.org</JABBERID>                     " \
"  </vCard>                                                      " \
"</iq>                                                           "

#define VALID_NAMESPACE "http://garden.with.spaces"
#define INVALID_NAMESPACE_MESSAGE \
"<iq id='badger0' to='plant@collabora.cbg'  type='result'> "\
" <branch xmlns='  "VALID_NAMESPACE"                '>     " \
"   <leaf colour='green' />                                " \
" </branch>                                                " \
"</iq>                                                     "

#define WHITESPACE_PADDED_BODY "  The Wench is Dead!  "

#define MESSAGE_WITH_WHITESPACE_PADDED_BODY \
"  <message to='morse@thamesvalley.police.uk' " \
"           from='lewis@thamesvalley.police.uk'> " \
"    <body>" WHITESPACE_PADDED_BODY "</body>" \
"  </message>"


#define WHITESPACE_ONLY_BODY "    "

#define MESSAGE_WITH_WHITESPACE_ONLY_BODY \
"  <message to='morse@thamesvalley.police.uk' " \
"           from='lewis@thamesvalley.police.uk'> " \
"    <body>" WHITESPACE_ONLY_BODY "</body>" \
"  </message>"

#define U_FDEF "\xe7\xb7\xaf"     /* a non-character */
#define REPLACE "\xef\xbf\xbd"    /* U+FFFD REPLACEMENT CHARACTER */
#define MONKEY "\xf0\x9f\x99\x88" /* U+1F648 SEE-NO-EVIL MONKEY */

#define NON_CHARACTER_CODEPOINTS U_FDEF MONKEY U_FDEF
#define NON_CHARACTER_CODEPOINTS_REPLACEMENT REPLACE MONKEY REPLACE

#define MESSAGE_WITH_NON_CHARACTER_CODEPOINTS \
"  <message to='morse@thamesvalley.police.uk' " \
"           from='lewis@thamesvalley.police.uk'> " \
"    <body>" NON_CHARACTER_CODEPOINTS "</body>" \
"  </message>"



static void
test_stream_no_stanzas (void)
{
  WockyXmppReader *reader;
  GError *error = NULL;

  reader = wocky_xmpp_reader_new ();

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_INITIAL);

  wocky_xmpp_reader_push (reader,
    (guint8 *) HEADER FOOTER, strlen (HEADER FOOTER));

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);
  g_assert (wocky_xmpp_reader_peek_stanza (reader) == NULL);

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);
  g_assert (wocky_xmpp_reader_pop_stanza (reader) == NULL);

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);

  error = wocky_xmpp_reader_get_error (reader);

  g_assert_no_error (error);

  g_object_unref (reader);
}

static void
test_stream_open_error (void)
{
  WockyXmppReader *reader;
  GError *error = NULL;

  reader = wocky_xmpp_reader_new ();

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_INITIAL);

  wocky_xmpp_reader_push (reader,
    (guint8 *) BROKEN_HEADER, strlen (BROKEN_HEADER));

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_ERROR);

  error = wocky_xmpp_reader_get_error (reader);

  g_assert_error (error, WOCKY_XMPP_READER_ERROR,
      WOCKY_XMPP_READER_ERROR_INVALID_STREAM_START);

  g_error_free (error);

  g_object_unref (reader);
}

static void
test_stream_open_unqualified_lang (void)
{
  WockyXmppReader *reader = wocky_xmpp_reader_new ();

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_INITIAL);

  wocky_xmpp_reader_push (reader,
    (guint8 *) HEADER_WITH_UNQUALIFIED_LANG,
    strlen (HEADER_WITH_UNQUALIFIED_LANG));

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);

  g_object_unref (reader);
}

static void
test_parse_error (void)
{
  WockyXmppReader *reader;
  GError *error = NULL;

  reader = wocky_xmpp_reader_new ();

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_INITIAL);

  wocky_xmpp_reader_push (reader,
    (guint8 *) HEADER, strlen (HEADER));

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);

  wocky_xmpp_reader_push (reader,
    (guint8 *) BROKEN_MESSAGE, strlen (BROKEN_MESSAGE));

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_ERROR);

  g_assert (wocky_xmpp_reader_peek_stanza (reader) == NULL);
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_ERROR);

  g_assert (wocky_xmpp_reader_pop_stanza (reader) == NULL);
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_ERROR);

  error = wocky_xmpp_reader_get_error (reader);

  g_assert_error (error, WOCKY_XMPP_READER_ERROR,
    WOCKY_XMPP_READER_ERROR_PARSE_ERROR);

  g_error_free (error);
  g_object_unref (reader);
}

static void
test_no_stream_parse_message (WockyXmppReader *reader)
{
  WockyStanza *stanza;

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);

  wocky_xmpp_reader_push (reader,
    (guint8 *) MESSAGE_CHUNK0, strlen (MESSAGE_CHUNK0));

  g_assert (wocky_xmpp_reader_pop_stanza (reader) == NULL);

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);

  wocky_xmpp_reader_push (reader,
    (guint8 *) MESSAGE_CHUNK1, strlen (MESSAGE_CHUNK1));

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);

  g_assert ((stanza = wocky_xmpp_reader_peek_stanza (reader)) != NULL);
  g_assert ((stanza = wocky_xmpp_reader_pop_stanza (reader)) != NULL);
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);

  g_object_unref (stanza);

}

static void
test_no_stream_hunks (void)
{
  WockyXmppReader *reader;

  reader = wocky_xmpp_reader_new_no_stream ();
  test_no_stream_parse_message (reader);

  g_object_unref (reader);
}

static void
test_no_stream_reset (void)
{
  WockyXmppReader *reader;

  reader = wocky_xmpp_reader_new_no_stream ();

  /* whole message, reset, whole message, reset */
  test_no_stream_parse_message (reader);
  wocky_xmpp_reader_reset (reader);

  test_no_stream_parse_message (reader);
  wocky_xmpp_reader_reset (reader);

  /* push half a message and reset the parser*/
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);
  wocky_xmpp_reader_push (reader,
    (guint8 *) MESSAGE_CHUNK0, strlen (MESSAGE_CHUNK0));
  wocky_xmpp_reader_reset (reader);

  /* And push a whole message through again */
  test_no_stream_parse_message (reader);

  g_object_unref (reader);
}

/* libXML2 doesn't like the vcard-temp namespace test if we can still
   correctly parse it */
static void
test_vcard_namespace (void)
{
  WockyXmppReader *reader;
  WockyStanza *stanza;

  reader = wocky_xmpp_reader_new_no_stream ();

  wocky_xmpp_reader_push (reader,
    (guint8 *) VCARD_MESSAGE, strlen (VCARD_MESSAGE));

  g_assert ((stanza = wocky_xmpp_reader_pop_stanza (reader)) != NULL);
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);

  g_object_unref (stanza);
  g_object_unref (reader);
}

static void
test_invalid_namespace (void)
{
  WockyXmppReader *reader;
  WockyStanza *stanza;

  reader = wocky_xmpp_reader_new_no_stream ();

  wocky_xmpp_reader_push (reader,
    (guint8 *) INVALID_NAMESPACE_MESSAGE, strlen (INVALID_NAMESPACE_MESSAGE));

  g_assert ((stanza = wocky_xmpp_reader_pop_stanza (reader)) != NULL);
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);

  g_assert_cmpstr (VALID_NAMESPACE, ==,
    wocky_node_get_ns (
      wocky_node_get_child (wocky_stanza_get_top_node (stanza), "branch")));

  g_object_unref (stanza);
  g_object_unref (reader);
}

/* Helper function for the whitespace body tests */
static void
test_body_with_alternative (
    const gchar *xml,
    const gchar *expected_body_text,
    const gchar *alt_body_text)
{
  WockyXmppReader *reader = wocky_xmpp_reader_new_no_stream ();
  WockyStanza *stanza;
  WockyNode *body;

  wocky_xmpp_reader_push (reader, (guint8 *) xml, strlen (xml));

  stanza = wocky_xmpp_reader_pop_stanza (reader);
  g_assert (stanza != NULL);

  body = wocky_node_get_child (wocky_stanza_get_top_node (stanza), "body");
  g_assert (body != NULL);

  g_assert (g_utf8_validate (body->content, -1, NULL));

  if (alt_body_text == NULL)
    {
      g_assert_cmpstr (body->content, ==, expected_body_text);
    }
  else
    {
      if (wocky_strdiff (body->content, expected_body_text) &&
          wocky_strdiff (body->content, alt_body_text))
        {
          g_error ("Body text «%s» was neither «%s» nor «%s»",
              body->content, expected_body_text, alt_body_text);
        }
    }

  g_object_unref (stanza);
  g_object_unref (reader);
}

static void
test_body (const gchar *xml,
    const gchar *exp)
{
  test_body_with_alternative (xml, exp, NULL);
}

/* Test that whitespace around the text contents of a message isn't ignored */
static void
test_whitespace_padding (void)
{
  test_body (MESSAGE_WITH_WHITESPACE_PADDED_BODY, WHITESPACE_PADDED_BODY);
}

/* Test that a message body consisting entirely of whitespace isn't ignored */
static void
test_whitespace_only (void)
{
  test_body (MESSAGE_WITH_WHITESPACE_ONLY_BODY, WHITESPACE_ONLY_BODY);
}

/* Test that a message body containing non-character codepoints is
 * handled "appropriately". Older GLib replaces them with U+FFFD,
 * newer GLib keeps them as-is. */
static void
test_non_character_codepoints (void)
{
  test_body_with_alternative (MESSAGE_WITH_NON_CHARACTER_CODEPOINTS,
    NON_CHARACTER_CODEPOINTS,
    NON_CHARACTER_CODEPOINTS_REPLACEMENT);
}

static void
check_namespaces (
    WockyXmppReader *reader,
    const gchar *xml,
    const gchar *expected_namespace)
{
  WockyStanza *stanza;
  WockyNode *top_node, *body_node;

  wocky_xmpp_reader_push (reader, (const guint8 *) xml, strlen (xml));
  stanza = wocky_xmpp_reader_pop_stanza (reader);
  g_assert (stanza != NULL);
  top_node = wocky_stanza_get_top_node (stanza);
  g_assert (top_node != NULL);
  g_assert_cmpstr (expected_namespace, ==, wocky_node_get_ns (top_node));
  body_node = wocky_node_get_first_child (top_node);
  g_assert (body_node != NULL);
  g_assert_cmpstr (expected_namespace, ==, wocky_node_get_ns (body_node));

  g_object_unref (stanza);
  wocky_xmpp_reader_reset (reader);
}

static void
test_no_stream_default_namespace (
    WockyXmppReader *reader,
    const gchar *expected_default_namespace)
{
  /* Regardless of the reader's default namespace, a root node with an
   * explicitly-specified namespace should get that namespace.
   */
  check_namespaces (reader,
      "<message xmlns='" WOCKY_XMPP_NS_JABBER_CLIENT "'>"
      "<body>hai</body></message>",
      WOCKY_XMPP_NS_JABBER_CLIENT);

  /* What namespace the nodes here end up in depends on the reader's default.
   */
  check_namespaces (reader, "<message><body>hai</body></message>",
      expected_default_namespace);
}

static void
test_no_stream_default_default_namespace (void)
{
  WockyXmppReader *reader = wocky_xmpp_reader_new_no_stream ();

  /* WockyXmppReader defaults to the empty namespace. */
  test_no_stream_default_namespace (reader, "");

  g_object_unref (reader);
}

static void
test_no_stream_specified_default_namespace (void)
{
#define WEIRD "wocky:weird:namespace"
  WockyXmppReader *reader = wocky_xmpp_reader_new_no_stream_ns (WEIRD);

  test_no_stream_default_namespace (reader, WEIRD);

  g_object_unref (reader);
#undef WEIRD
}

int
main (int argc,
    char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-reader/stream-no-stanzas", test_stream_no_stanzas);
  g_test_add_func ("/xmpp-reader/stream-open-error", test_stream_open_error);
  g_test_add_func ("/xmpp-reader/stream-open-unqualified-lang",
      test_stream_open_unqualified_lang);
  g_test_add_func ("/xmpp-reader/parse-error", test_parse_error);
  g_test_add_func ("/xmpp-reader/no-stream-hunks", test_no_stream_hunks);
  g_test_add_func ("/xmpp-reader/no-stream-resetting", test_no_stream_reset);
  g_test_add_func ("/xmpp-reader/vcard-namespace", test_vcard_namespace);
  g_test_add_func ("/xmpp-reader/invalid-namespace", test_invalid_namespace);
  g_test_add_func ("/xmpp-reader/whitespace-padding", test_whitespace_padding);
  g_test_add_func ("/xmpp-reader/whitespace-only", test_whitespace_only);
  g_test_add_func ("/xmpp-reader/utf-non-character-codepoints",
    test_non_character_codepoints);
  g_test_add_func ("/xmpp-reader/no-stream-default-default-namespace",
      test_no_stream_default_default_namespace);
  g_test_add_func ("/xmpp-reader/no-stream-specified-default-namespace",
      test_no_stream_specified_default_namespace);

  result = g_test_run ();
  test_deinit ();
  return result;
}
