#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-reader.h>
#include <wocky/wocky-xmpp-writer.h>
#include <wocky/wocky-utils.h>

#define TO "example.net"
#define FROM "julliet@example.com"
#define VERSION "1.0"
#define LANG "en"

static WockyXmppStanza *
create_stanza (void)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
    WOCKY_STANZA_SUB_TYPE_CHAT, "juliet@example.com", "romeo@example.net",
      WOCKY_NODE, "html", WOCKY_NODE_XMLNS, "http://www.w3.org/1999/xhtml",
        WOCKY_NODE, "body",
          WOCKY_NODE_TEXT, "Art thou not Romeo, and a Montague?",
        WOCKY_NODE_END,
      WOCKY_NODE_END,
    WOCKY_STANZA_END);

  return stanza;
}

static void
test_readwrite (void)
{
  WockyXmppReader *reader;
  WockyXmppWriter *writer;
  WockyXmppStanza *received = NULL, *sent;
  const guint8 *data;
  gsize length;
  gchar *to, *from, *version, *lang;
  int i;

  writer = wocky_xmpp_writer_new ();
  reader = wocky_xmpp_reader_new ();

  g_assert (wocky_xmpp_reader_get_state (reader)
    == WOCKY_XMPP_READER_STATE_INITIAL);

  wocky_xmpp_writer_stream_open (writer, TO, FROM, VERSION, LANG,
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
  g_assert (!wocky_strdiff (version, VERSION));
  g_assert (!wocky_strdiff (lang, LANG));

  g_free (to);
  g_free (from);
  g_free (version);
  g_free (lang);

  sent = create_stanza ();

  for (i = 0; i < 3 ; i ++)
    {
      g_assert (wocky_xmpp_reader_get_state (reader)
        == WOCKY_XMPP_READER_STATE_OPENED);

      wocky_xmpp_writer_write_stanza (writer, sent, &data, &length);
      wocky_xmpp_reader_push (reader, data, length);

      received = wocky_xmpp_reader_pop_stanza (reader);

      g_assert (received != NULL);
      g_assert (wocky_xmpp_node_equal (sent->node, received->node));

      g_object_unref (received);

      /* No more stanzas in the queue */
      received = wocky_xmpp_reader_pop_stanza (reader);
      g_assert (received == NULL);
    }

  wocky_xmpp_writer_write_stanza (writer, sent, &data, &length);
  wocky_xmpp_reader_push (reader, data, length);

  wocky_xmpp_writer_stream_close (writer, &data, &length);
  wocky_xmpp_reader_push (reader, data, length);

  /*  Stream state should stay open untill we popped the last stanza */
  g_assert (wocky_xmpp_reader_get_state (reader)
     == WOCKY_XMPP_READER_STATE_OPENED);

  received = wocky_xmpp_reader_pop_stanza (reader);
  g_assert (received != NULL);
  g_assert (wocky_xmpp_node_equal (sent->node, received->node));

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
  WockyXmppStanza *received = NULL, *sent;
  const guint8 *data;
  gsize length;
  int i;

  writer = wocky_xmpp_writer_new_no_stream ();
  reader = wocky_xmpp_reader_new_no_stream ();

  sent = create_stanza ();


  for (i = 0 ; i < 3 ; i++ )
    {
      g_assert (wocky_xmpp_reader_get_state (reader)
        == WOCKY_XMPP_READER_STATE_OPENED);

      wocky_xmpp_writer_write_stanza (writer, sent, &data, &length);
      wocky_xmpp_reader_push (reader, data, length);

      g_assert (wocky_xmpp_reader_get_state (reader)
        == WOCKY_XMPP_READER_STATE_OPENED);

      received = wocky_xmpp_reader_pop_stanza (reader);

      g_assert (received != NULL);
      g_assert (wocky_xmpp_node_equal (sent->node, received->node));

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
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-readwrite/readwrite", test_readwrite);
  g_test_add_func ("/xmpp-readwrite/readwrite-nostream",
    test_readwrite_nostream);

  return g_test_run ();
}
