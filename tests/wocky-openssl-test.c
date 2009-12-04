#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "config.h"

#include <glib.h>
#if USING_OPENSSL
#include <wocky/wocky-openssl.h>
#else
#error OpenSSL specific test binary: do not build for GnuTLS
#endif

#include <wocky/wocky-utils.h>
#include "wocky-test-stream.h"
#include "wocky-test-helper.h"
#include "wocky-test-openssl-server.h"

#define BUF_SIZE 8192

typedef struct {
  test_data_t *test;
  char cli_buf[BUF_SIZE];
  char srv_buf[BUF_SIZE];
  WockyTLSConnection *client;
  TestTLSConnection *server;
  GString *cli_data;
  GString *srv_data;
  guint read_op_count;
  guint read_byte_count;
} ssl_test_t;

/* ************************************************************************ */
/* client callbacks */

static void
client_write_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GOutputStream *output = G_OUTPUT_STREAM (source);
  ssl_test_t *ssl_test = data;
  g_output_stream_write_finish (output, result, NULL);

  ssl_test->test->outstanding--;

  if (ssl_test->test->outstanding == 0)
    g_main_loop_quit (ssl_test->test->loop);
}

static void
client_read_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GInputStream *input = G_INPUT_STREAM (source);
  ssl_test_t *ssl_test = data;
  gssize count = g_input_stream_read_finish (input, result, NULL);

  ssl_test->read_byte_count += count;

  ssl_test->test->outstanding--;
  ssl_test->read_op_count++;

  g_string_append_len (ssl_test->cli_data, ssl_test->cli_buf, count);

  if (ssl_test->read_op_count >= 3)
    {
      GIOStream *io = G_IO_STREAM (ssl_test->client);
      GOutputStream *output = g_io_stream_get_output_stream (io);
      g_output_stream_write_async (output,
          TEST_SSL_DATA, TEST_SSL_DATA_LEN, G_PRIORITY_DEFAULT,
          ssl_test->test->cancellable, client_write_cb, data);
      return;
    }

  g_input_stream_read_async (input, ssl_test->cli_buf, BUF_SIZE,
      G_PRIORITY_DEFAULT, ssl_test->test->cancellable, client_read_cb, data);
}

static void
client_handshake_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GInputStream *input;
  WockyTLSSession *session = WOCKY_TLS_SESSION (source);
  ssl_test_t *ssl_test = data;

  ssl_test->client = wocky_tls_session_handshake_finish (session, result, NULL);
  input = g_io_stream_get_input_stream (G_IO_STREAM (ssl_test->client));

  ssl_test->test->outstanding--;
  ssl_test->read_op_count = 0;

  g_input_stream_read_async (input, ssl_test->cli_buf, BUF_SIZE,
      G_PRIORITY_DEFAULT, ssl_test->test->cancellable, client_read_cb, data);
}

/* ************************************************************************ */
/* server callbacks */
static void
server_read_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GInputStream *input = G_INPUT_STREAM (source);
  ssl_test_t *ssl_test = data;
  gssize count = g_input_stream_read_finish (input, result, NULL);

  ssl_test->test->outstanding--;

  g_string_append_len (ssl_test->srv_data, ssl_test->srv_buf, count);

  if (ssl_test->test->outstanding == 0)
    g_main_loop_quit (ssl_test->test->loop);
}


static void
server_write_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  ssl_test_t *ssl_test = data;
  GOutputStream *output = G_OUTPUT_STREAM (source);
  GIOStream *io = G_IO_STREAM (ssl_test->server);
  GInputStream *input = g_io_stream_get_input_stream (io);

  g_output_stream_write_finish (output, result, NULL);
  ssl_test->test->outstanding--;

  g_input_stream_read_async (input, ssl_test->srv_buf, BUF_SIZE,
      G_PRIORITY_DEFAULT, ssl_test->test->cancellable, server_read_cb, data);
}

static void
server_handshake_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GOutputStream *output;
  TestTLSSession *session = TEST_TLS_SESSION (source);
  ssl_test_t *ssl_test = data;

  ssl_test->server = test_tls_session_handshake_finish (session, result, NULL);
  output = g_io_stream_get_output_stream (G_IO_STREAM (ssl_test->server));
  g_output_stream_write_async (output, TEST_SSL_DATA, TEST_SSL_DATA_LEN,
      G_PRIORITY_LOW, ssl_test->test->cancellable, server_write_cb, data);

  ssl_test->test->outstanding--;
}

/* ************************************************************************ */
/* test(s) */

static void
setup_ssl_test (ssl_test_t *ssl_test, test_data_t *test)
{
  ssl_test->test = test;
  ssl_test->cli_data = g_string_new ("");
  ssl_test->srv_data = g_string_new ("");
}

static void
teardown_ssl_test (ssl_test_t *ssl_test)
{
  g_string_free (ssl_test->cli_data, TRUE);
  g_string_free (ssl_test->srv_data, TRUE);
  g_io_stream_close (G_IO_STREAM (ssl_test->server), NULL, NULL);
  g_io_stream_close (G_IO_STREAM (ssl_test->client), NULL, NULL);
  g_object_unref (ssl_test->server);
  g_object_unref (ssl_test->client);
}

static void
test_openssl_handshake_rw (void)
{
  ssl_test_t ssl_test;
  test_data_t *test = setup_test();
  WockyTLSSession *client = wocky_tls_session_new (test->stream->stream0);
  TestTLSSession *server = test_tls_session_server_new (test->stream->stream1,
      1024, TLS_SERVER_KEY_FILE, TLS_SERVER_CRT_FILE);
  gsize expected = (TEST_SSL_RECORD_MARKER_LEN * 2) + TEST_SSL_DATA_LEN;
  gchar *target =
    TEST_SSL_RECORD_MARKER "\0" TEST_SSL_DATA "\0" TEST_SSL_RECORD_MARKER;

  setup_ssl_test (&ssl_test, test);

  wocky_tls_session_handshake_async (client, G_PRIORITY_DEFAULT,
      test->cancellable, client_handshake_cb, &ssl_test);
  test->outstanding += 5; /* handshake + 3 reads + 1 write */

  test_tls_session_handshake_async (server, G_PRIORITY_DEFAULT,
      test->cancellable, server_handshake_cb, &ssl_test);
  test->outstanding += 3; /* handshake + 1 write + 1 read */

  test_wait_pending (test);

  g_assert (test->outstanding == 0);
  g_assert (ssl_test.read_op_count == 3);
  g_assert (ssl_test.cli_data->len == expected);
  g_assert (ssl_test.srv_data->len == TEST_SSL_DATA_LEN);
  g_assert (!memcmp (ssl_test.cli_data->str, target, expected));
  g_assert (!memcmp (ssl_test.srv_data->str, TEST_SSL_DATA, TEST_SSL_DATA_LEN));

  teardown_test (test);
  teardown_ssl_test (&ssl_test);
  g_object_unref (client);
  g_object_unref (server);
}


int
main (int argc, char **argv)
{
  int result;

  test_init (argc, argv);
  g_test_add_func ("/openssl/handshake+rw", test_openssl_handshake_rw);
  result = g_test_run ();
  test_deinit ();

  return result;
}
