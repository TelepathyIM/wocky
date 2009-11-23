#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>
#include <wocky/wocky-xmpp-reader.h>
#include <wocky/wocky-utils.h>

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
test_no_stream_hunks (void)
{
  WockyXmppReader *reader;
  WockyXmppStanza *stanza;

  reader = wocky_xmpp_reader_new_no_stream ();

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
  g_object_unref (reader);
}

/* libXML2 doesn't like the vcard-temp namespace test if we can still
   correctly parse it */
static void
test_vcard_namespace (void)
{
  WockyXmppReader *reader;
  WockyXmppStanza *stanza;

  reader = wocky_xmpp_reader_new_no_stream ();

  wocky_xmpp_reader_push (reader,
    (guint8 *) VCARD_MESSAGE, strlen (VCARD_MESSAGE));

  g_assert ((stanza = wocky_xmpp_reader_pop_stanza (reader)) != NULL);
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);

  g_object_unref (stanza);
  g_object_unref (reader);
}

int
main (int argc,
    char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-reader/stream-no-stanzas", test_stream_no_stanzas);
  g_test_add_func ("/xmpp-reader/stream-open-error", test_stream_open_error);
  g_test_add_func ("/xmpp-reader/parse-error", test_parse_error);
  g_test_add_func ("/xmpp-reader/no-stream-hunks", test_no_stream_hunks);
  g_test_add_func ("/xmpp-reader/vcard-namespace", test_vcard_namespace);

  result = g_test_run ();
  test_deinit ();
  return result;
}
