#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-reader.h>
#include <wocky/wocky-xmpp-writer.h>
#include <wocky/wocky-xmpp-writer.h>

#define TO "example.net"
#define FROM "julliet@example.com"
#define VERSION "1.0"

static void
not_emitted (void)
{
  g_critical ("Signal should not have been emitted");
}

static void
stanza_received_cb (WockyXmppReader *reader,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  WockyXmppStanza **received = (WockyXmppStanza **)user_data;

  g_assert (*received == NULL);
  *received = g_object_ref (stanza);
}

static void
stream_opened_cb (WockyXmppReader *reader,
    const gchar *to,
    const gchar *from,
    const gchar *version,
    gpointer user_data)
{
  gboolean *opened = (gboolean *)user_data;

  g_assert (strcmp (to, TO) == 0);
  g_assert (strcmp (from, FROM) == 0);
  g_assert (strcmp (version, VERSION) == 0);

  g_assert (*opened == FALSE);
  *opened = TRUE;
}

static void
stream_closed_cb (WockyXmppReader *reader,
    gpointer user_data)
{
  gboolean *closed = (gboolean *)user_data;

  g_assert (*closed == FALSE);
  *closed = TRUE;
}

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
  gboolean opened = FALSE, closed = FALSE;
  const guint8 *data;
  gsize length;

  writer = wocky_xmpp_writer_new ();
  reader = wocky_xmpp_reader_new ();

  g_signal_connect (reader, "stream-opened",
    G_CALLBACK (stream_opened_cb), &opened);
  g_signal_connect (reader, "stream-closed",
    G_CALLBACK (stream_closed_cb), &closed);
  g_signal_connect (reader, "received-stanza",
    G_CALLBACK (stanza_received_cb), &received);

  wocky_xmpp_writer_stream_open (writer, TO, FROM, VERSION,
      &data, &length);

  g_assert (wocky_xmpp_reader_push (reader, data, length, NULL));
  g_assert (opened && !closed);

  sent = create_stanza ();
  g_assert (wocky_xmpp_writer_write_stanza (writer, sent,
      &data, &length, NULL));
  g_assert (wocky_xmpp_reader_push (reader, data, length, NULL));

  g_assert (!closed);
  g_assert (received != NULL);
  g_assert (wocky_xmpp_node_equal (sent->node, received->node));

  wocky_xmpp_writer_stream_close (writer, &data, &length);
  g_assert (wocky_xmpp_reader_push (reader, data, length, NULL));
  g_assert (closed);

  g_object_unref (reader);
  g_object_unref (writer);
  g_object_unref (received);
}

static void
test_readwrite_nostream (void)
{
  WockyXmppReader *reader;
  WockyXmppWriter *writer;
  WockyXmppStanza *received = NULL, *sent;
  const guint8 *data;
  gsize length;

  writer = wocky_xmpp_writer_new_no_stream ();
  reader = wocky_xmpp_reader_new_no_stream ();

  g_signal_connect (reader, "stream-opened", not_emitted, NULL);
  g_signal_connect (reader, "stream-closed", not_emitted, NULL);
  g_signal_connect (reader, "received-stanza",
      G_CALLBACK (stanza_received_cb), &received);

  sent = create_stanza ();

  g_assert (wocky_xmpp_writer_write_stanza (writer, sent,
      &data, &length, NULL));
  g_assert (wocky_xmpp_reader_push (reader, data, length, NULL));
  g_assert (received != NULL);
  g_assert (wocky_xmpp_node_equal (sent->node, received->node));

  g_object_unref (reader);
  g_object_unref (writer);
  g_object_unref (received);
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
