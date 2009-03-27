#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <wocky/wocky-xmpp-connection.h>
#include "wocky-test-stream.h"

#define SIMPLE_MESSAGE \
"<?xml version='1.0' encoding='UTF-8'?>                                    " \
"<stream:stream xmlns='jabber:client'                                      " \
"  xmlns:stream='http://etherx.jabber.org/streams'>                        " \
"  <message to='juliet@example.com' from='romeo@example.net' xml:lang='en' " \
"   id=\"0\">                                                              " \
"    <body>Art thou not Romeo, and a Montague?</body>                      " \
"  </message>                                                              " \
"</stream:stream>"

static void
test_instantiation (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;;

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);

  g_assert (connection != NULL);

  connection = wocky_xmpp_connection_new (NULL);

  g_assert (connection != NULL);

  g_object_unref (connection);
}

static void
stanza_received_cb (WockyXmppConnection *connection,
  WockyXmppStanza *stanza, gpointer user_data)
{
  gboolean *message_parsed = user_data;

  *message_parsed = TRUE;
}

static void
parse_error_cb (WockyXmppConnection *connection, gpointer user_data)
{
  gboolean *parse_error_found = user_data;

  *parse_error_found = TRUE;
}

#define CHUNK_SIZE 13

static gboolean
timeout_cb (gpointer data)
{
  g_test_message ("Timeout reached :(");
  g_assert_not_reached ();

  return FALSE;
}

static void
test_simple_message (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  gsize len;
  gsize offset = 0;
  gboolean parse_error_found = FALSE;
  gboolean message_parsed = FALSE;
  gchar message[] = SIMPLE_MESSAGE;
  GMainLoop *loop = NULL;

  loop = g_main_loop_new (NULL, FALSE);

  len = strlen (message);

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);

  g_signal_connect (connection, "received-stanza",
      G_CALLBACK(stanza_received_cb), &message_parsed);

  g_signal_connect (connection, "parse-error",
      G_CALLBACK(parse_error_cb), &parse_error_found);

  while (!parse_error_found && offset < len)
    {
      guint l = MIN (len - offset, CHUNK_SIZE);
      g_output_stream_write_all (stream->stream1_output,
        message + offset, l, NULL, NULL, NULL);
      offset += l;
    }

  g_timeout_add (1000, timeout_cb, NULL);

  while (g_main_context_iteration (NULL, TRUE) && !message_parsed)
    ;

  g_assert (!parse_error_found);
  g_assert (message_parsed);

  g_main_loop_unref (loop);

  g_object_unref (stream);
  g_object_unref (connection);
}

int
main (int argc, char **argv)
{
  g_thread_init (NULL);

  g_test_init (&argc, &argv, NULL);
  g_type_init ();

  g_test_add_func ("/xmpp-connection/initiation", test_instantiation);
  g_test_add_func ("/xmpp-connection/simpe-message",
    test_simple_message);

  return g_test_run ();
}
