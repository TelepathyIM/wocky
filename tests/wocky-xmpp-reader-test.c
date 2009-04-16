#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-reader.h>
#include <wocky/wocky-utils.h>

#define HEADER \
"<?xml version='1.0' encoding='UTF-8'?>                                    " \
"<stream:stream xmlns='jabber:client'                                      " \
"  xmlns:stream='http://etherx.jabber.org/streams'>                        "

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

  g_assert (error != NULL);
  g_assert (g_error_matches (error, WOCKY_XMPP_READER_ERROR,
    WOCKY_XMPP_READER_ERROR_INVALID_STREAM_START));

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

  g_assert (error != NULL);
  g_assert (g_error_matches (error, WOCKY_XMPP_READER_ERROR,
    WOCKY_XMPP_READER_ERROR_PARSE_ERROR));

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

int
main (int argc,
    char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-reader/stream-open-error", test_stream_open_error);
  g_test_add_func ("/xmpp-reader/parse-error", test_parse_error);
  g_test_add_func ("/xmpp-reader/no-stream-hunks", test_no_stream_hunks);

  return g_test_run ();
}
