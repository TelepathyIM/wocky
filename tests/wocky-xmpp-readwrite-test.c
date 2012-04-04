#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky.h>

#include "wocky-test-helper.h"

#define TO "example.net"
#define FROM "julliet@example.com"
#define XMPP_VERSION "1.0"
#define LANG "en"
#define DUMMY_NS "urn:wocky:test:blah:blah:blah"

static WockyStanza *
create_stanza (void)
{
  WockyStanza *stanza;
  WockyNode *html;
  WockyNode *head;

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
      '(', "html", ':', "http://www.w3.org/1999/xhtml",
        '(', "body",
          '$', "Art thou not Romeo, and a Montague?",
        ')',
      ')',
    NULL);

  html = wocky_node_get_child (wocky_stanza_get_top_node (stanza), "html");
  head = wocky_node_add_child (html, "head");
  wocky_node_set_attribute_ns (head, "rev", "0xbad1dea", DUMMY_NS);

  return stanza;
}

static void
test_readwrite (void)
{
  WockyXmppReader *reader;
  WockyXmppWriter *writer;
  WockyStanza *received = NULL, *sent;
  const guint8 *data;
  gsize length;
  gchar *to, *from, *version, *lang;
  int i;

  writer = wocky_xmpp_writer_new ();
  reader = wocky_xmpp_reader_new ();

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_INITIAL);

  wocky_xmpp_writer_stream_open (writer, TO, FROM, XMPP_VERSION, LANG, NULL,
      &data, &length);
  wocky_xmpp_reader_push (reader, data, length);

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_OPENED);

  g_object_get (reader,
      "to", &to,
      "from", &from,
      "version", &version,
      "lang", &lang,
      NULL);

  g_assert (!wocky_strdiff (to, TO));
  g_assert (!wocky_strdiff (from, FROM));
  g_assert (!wocky_strdiff (version, XMPP_VERSION));
  g_assert (!wocky_strdiff (lang, LANG));

  g_free (to);
  g_free (from);
  g_free (version);
  g_free (lang);

  sent = create_stanza ();

  for (i = 0; i < 3 ; i++)
    {
      WockyNode *html;
      WockyNode *head;
      const gchar *attr_recv = NULL;
      const gchar *attr_send = NULL;
      const gchar *attr_none = NULL;

      g_assert (wocky_xmpp_reader_get_state (reader)
        == WOCKY_XMPP_READER_STATE_OPENED);

      wocky_xmpp_writer_write_stanza (writer, sent, &data, &length);
      wocky_xmpp_reader_push (reader, data, length);

      received = wocky_xmpp_reader_pop_stanza (reader);

      g_assert (received != NULL);
      test_assert_stanzas_equal (sent, received);

      html = wocky_node_get_child (wocky_stanza_get_top_node (received),
          "html");
      head = wocky_node_get_child (html, "head");
      attr_recv = wocky_node_get_attribute_ns (head, "rev", DUMMY_NS);
      attr_none = wocky_node_get_attribute_ns (head, "rev",
        DUMMY_NS ":x");

      html = wocky_node_get_child (wocky_stanza_get_top_node (sent),
          "html");
      head = wocky_node_get_child (html, "head");
      attr_send = wocky_node_get_attribute_ns (head, "rev", DUMMY_NS);

      g_assert (attr_none == NULL);
      g_assert (attr_recv != NULL);
      g_assert (attr_send != NULL);
      g_assert (!strcmp (attr_send, attr_recv));


      g_object_unref (received);

      /* No more stanzas in the queue */
      received = wocky_xmpp_reader_pop_stanza (reader);
      g_assert (received == NULL);
    }

  wocky_xmpp_writer_write_stanza (writer, sent, &data, &length);
  wocky_xmpp_reader_push (reader, data, length);

  wocky_xmpp_writer_stream_close (writer, &data, &length);
  wocky_xmpp_reader_push (reader, data, length);

  /*  Stream state should stay open until we popped the last stanza */
  g_assert (wocky_xmpp_reader_get_state (reader)
     == WOCKY_XMPP_READER_STATE_OPENED);

  received = wocky_xmpp_reader_pop_stanza (reader);
  g_assert (received != NULL);
  test_assert_stanzas_equal (sent, received);

  g_object_unref (received);

  /* Last stanza pop, stream should be closed */
  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_CLOSED);

  /* No more stanzas in the queue */
  received = wocky_xmpp_reader_pop_stanza (reader);
  g_assert (received == NULL);

  g_object_unref (sent);
  g_object_unref (reader);
  g_object_unref (writer);
}

static void
test_readwrite_nostream (void)
{
  WockyXmppReader *reader;
  WockyXmppWriter *writer;
  WockyStanza *received = NULL, *sent;
  const guint8 *data;
  gsize length;
  int i;

  writer = wocky_xmpp_writer_new_no_stream ();
  reader = wocky_xmpp_reader_new_no_stream ();

  sent = create_stanza ();


  for (i = 0 ; i < 3 ; i++)
    {
      g_assert (wocky_xmpp_reader_get_state (reader)
        == WOCKY_XMPP_READER_STATE_OPENED);

      wocky_xmpp_writer_write_stanza (writer, sent, &data, &length);
      wocky_xmpp_reader_push (reader, data, length);

      g_assert (wocky_xmpp_reader_get_state (reader)
        == WOCKY_XMPP_READER_STATE_OPENED);

      received = wocky_xmpp_reader_pop_stanza (reader);

      g_assert (received != NULL);
      test_assert_stanzas_equal (sent, received);

      g_assert (wocky_xmpp_reader_get_state (reader) ==
        WOCKY_XMPP_READER_STATE_CLOSED);

      wocky_xmpp_reader_reset (reader);

      g_object_unref (received);
    }

  g_object_unref (sent);
  g_object_unref (reader);
  g_object_unref (writer);
}

int
main (int argc,
    char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/xmpp-readwrite/readwrite", test_readwrite);
  g_test_add_func ("/xmpp-readwrite/readwrite-nostream",
    test_readwrite_nostream);

  result = g_test_run ();
  test_deinit ();
  return result;
}
