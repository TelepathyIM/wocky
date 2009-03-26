#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#include <wocky/wocky-xmpp-connection.h>
#include "wocky-test-stream.h"

struct _FileChunker {
  gchar *contents;
  gsize length;
  gsize size;
  gsize offset;
};
typedef struct _FileChunker FileChunker;

static void
file_chunker_destroy (FileChunker *fc) {
  g_free (fc->contents);
  g_free (fc);
}


static FileChunker *
file_chunker_new (const gchar *filename, gsize chunk_size) {
  FileChunker *fc;
  fc = g_new0 (FileChunker, 1);

  fc->size = chunk_size;
  if (!g_file_get_contents (filename, &fc->contents, &fc->length, NULL)) {
    file_chunker_destroy (fc);
    return NULL;
  }
  return fc;
}

static gboolean
file_chunker_get_chunk (FileChunker *fc,
                        gchar **chunk,
                        gsize *chunk_size) {
  if (fc->offset < fc->length) {
    *chunk_size = MIN (fc->length - fc->offset, fc->size);
    *chunk = fc->contents + fc->offset;
    fc->offset += *chunk_size;
    return TRUE;
  }
  return FALSE;
}


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

static void
test_simple_message (void)
{
  WockyXmppConnection *connection;
  WockyTestStream *stream;
  gchar *chunk;
  gsize chunk_length;
  gboolean parse_error_found = FALSE;
  gboolean message_parsed = FALSE;
  const gchar *srcdir;
  gchar *file;
  FileChunker *fc;

  srcdir = g_getenv ("srcdir");
  if (srcdir == NULL)
    {
      file = g_strdup ("inputs/simple-message.input");
    }
  else
    {
      file = g_strdup_printf ("%s/inputs/simple-message.input", srcdir);
    }

  fc = file_chunker_new (file, 10);
  g_assert (fc != NULL);

  stream = g_object_new (WOCKY_TYPE_TEST_STREAM, NULL);
  connection = wocky_xmpp_connection_new (stream->stream0);

  g_signal_connect (connection, "received-stanza",
      G_CALLBACK(stanza_received_cb), &message_parsed);

  g_signal_connect (connection, "parse-error",
      G_CALLBACK(parse_error_cb), &parse_error_found);

  while (!parse_error_found &&
      file_chunker_get_chunk (fc, &chunk, &chunk_length))
    {
      g_output_stream_write_all (stream->stream1_output,
        chunk, chunk_length, NULL, NULL, NULL);
    }

  g_assert (!parse_error_found);
  g_assert (message_parsed);

  g_free (file);
  file_chunker_destroy (fc);
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
