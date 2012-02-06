#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "config.h"

#include <glib.h>

#include <wocky/wocky.h>
#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

#define BUF_SIZE 8192

#define TEST_SSL_DATA_A "badgerbadgerbadger"
#define TEST_SSL_DATA_B "mushroom, mushroom"
#define TEST_SSL_DATA_LEN sizeof (TEST_SSL_DATA_A)

static const gchar *test_data[] = { TEST_SSL_DATA_A, TEST_SSL_DATA_B, NULL };

typedef struct {
  test_data_t *test;
  char cli_buf[BUF_SIZE];
  char srv_buf[BUF_SIZE];

  char *cli_send;
  gsize cli_send_len;
  gsize cli_sent;

  WockyTLSConnection *client;
  WockyTLSConnection *server;
  GString *cli_data;
  GString *srv_data;
  guint read_op_count;
  guint write_op_count;
  gboolean in_read;
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
  GError *error = NULL;
  gsize ret;

  ret = g_output_stream_write_finish (output, result, &error);
  g_assert_cmpint (ret, <=, ssl_test->cli_send_len - ssl_test->cli_sent);
  g_assert_no_error (error);

  ssl_test->cli_sent += ret;

  if (ssl_test->cli_sent == ssl_test->cli_send_len)
    {
      ssl_test->test->outstanding--;
      g_main_loop_quit (ssl_test->test->loop);
      return;
    }

  g_output_stream_write_async (output,
    ssl_test->cli_send + ssl_test->cli_sent,
    ssl_test->cli_send_len - ssl_test->cli_sent,
    G_PRIORITY_DEFAULT, ssl_test->test->cancellable, client_write_cb, data);
}

static void
client_read_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GInputStream *input = G_INPUT_STREAM (source);
  ssl_test_t *ssl_test = data;
  GError *error = NULL;
  gssize count = g_input_stream_read_finish (input, result, &error);

  g_assert (!ssl_test->in_read);
  g_assert_no_error (error);
  g_assert_cmpint (count, ==, TEST_SSL_DATA_LEN);

  ssl_test->read_op_count++;

  g_string_append_len (ssl_test->cli_data, ssl_test->cli_buf, count);

  switch (ssl_test->read_op_count)
    {
      case 1:
        /* we've read at least one ssl record at this point, combine the others
         */
        wocky_test_stream_set_mode (ssl_test->test->stream->stream0_input,
         WOCK_TEST_STREAM_READ_COMBINE);
        break;
      case 2:
#ifdef USING_OPENSSL
        /* Our openssl backend should have read all ssl records by now and
         * thus shouldn't request more data from the input stream. The GnuTLS
         * backend requests more granularily what it needs so will still read
         * from the input stream */
        wocky_test_input_stream_set_read_error (
            ssl_test->test->stream->stream0_input);
#endif /* USING_OPENSSL */
        break;
      case 3:
      case 4:
        break;
      case 5:
        {
          GIOStream *io = G_IO_STREAM (ssl_test->client);
          GOutputStream *output = g_io_stream_get_output_stream (io);

          g_output_stream_write_async (output,
              ssl_test->cli_send, ssl_test->cli_send_len, G_PRIORITY_DEFAULT,
              ssl_test->test->cancellable, client_write_cb, data);
          return;
        }
      default:
        g_error ("Read too many records: test broken?");
    }

  ssl_test->in_read = TRUE;
  g_input_stream_read_async (input, ssl_test->cli_buf, BUF_SIZE,
      G_PRIORITY_DEFAULT, ssl_test->test->cancellable, client_read_cb, data);
  ssl_test->in_read = FALSE;
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

  ssl_test->in_read = TRUE;
  g_input_stream_read_async (input, ssl_test->cli_buf, BUF_SIZE,
      G_PRIORITY_DEFAULT, ssl_test->test->cancellable, client_read_cb, data);
  ssl_test->in_read = FALSE;
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

  g_string_append_len (ssl_test->srv_data, ssl_test->srv_buf, count);

  if (ssl_test->srv_data->len < ssl_test->cli_send_len)
    {
      g_input_stream_read_async (input, ssl_test->srv_buf, BUF_SIZE,
       G_PRIORITY_DEFAULT, ssl_test->test->cancellable, server_read_cb, data);
    }
  else
    {
      ssl_test->test->outstanding--;
      g_main_loop_quit (ssl_test->test->loop);
    }
}


static void
server_write_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  ssl_test_t *ssl_test = data;
  gsize written;
  GOutputStream *output = G_OUTPUT_STREAM (source);
  GIOStream *io = G_IO_STREAM (ssl_test->server);
  GInputStream *input = g_io_stream_get_input_stream (io);

  written = g_output_stream_write_finish (output, result, NULL);

  g_assert_cmpint (written, ==, TEST_SSL_DATA_LEN);

  ssl_test->write_op_count++;
  if (ssl_test->write_op_count < 5)
    {
      const char *payload = test_data[ssl_test->write_op_count & 1];
      g_output_stream_write_async (output, payload, TEST_SSL_DATA_LEN,
        G_PRIORITY_LOW, ssl_test->test->cancellable, server_write_cb, data);
    }
  else
    {
      wocky_test_stream_cork (ssl_test->test->stream->stream0_input, FALSE);
      g_input_stream_read_async (input, ssl_test->srv_buf, BUF_SIZE,
       G_PRIORITY_DEFAULT, ssl_test->test->cancellable, server_read_cb, data);
    }
}

static void
server_handshake_cb (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  GOutputStream *output;
  WockyTLSSession *session = WOCKY_TLS_SESSION (source);
  ssl_test_t *ssl_test = data;
  GError *error = NULL;

  ssl_test->server = wocky_tls_session_handshake_finish (session,
    result, &error);

  g_assert_no_error (error);
  g_assert (ssl_test->server != NULL);

  output = g_io_stream_get_output_stream (G_IO_STREAM (ssl_test->server));

  /* cork the client reading stream */
  wocky_test_stream_cork (ssl_test->test->stream->stream0_input, TRUE);

  g_output_stream_write_async (output, TEST_SSL_DATA_A, TEST_SSL_DATA_LEN,
      G_PRIORITY_LOW, ssl_test->test->cancellable, server_write_cb, data);
}

/* ************************************************************************ */
/* test(s) */

static void
setup_ssl_test (ssl_test_t *ssl_test, test_data_t *test)
{
  guint x;

  /* the tests currently rely on all the chunks being the same size */
  /* note that we include the terminating \0 in the payload         */
  for (x = 0; test_data[x] != NULL; x++)
    g_assert (strlen (test_data[x]) == (TEST_SSL_DATA_LEN - 1));

  ssl_test->test = test;
  ssl_test->cli_data = g_string_new ("");
  ssl_test->srv_data = g_string_new ("");
}

static void
teardown_ssl_test (ssl_test_t *ssl_test)
{
  g_free (ssl_test->cli_send);
  g_string_free (ssl_test->cli_data, TRUE);
  g_string_free (ssl_test->srv_data, TRUE);
  g_io_stream_close (G_IO_STREAM (ssl_test->server), NULL, NULL);
  g_io_stream_close (G_IO_STREAM (ssl_test->client), NULL, NULL);
  g_object_unref (ssl_test->server);
  g_object_unref (ssl_test->client);
}

static void
test_tls_handshake_rw (void)
{
  ssl_test_t ssl_test = { NULL, } ;
  test_data_t *test = setup_test ();
  WockyTLSSession *client = wocky_tls_session_new (test->stream->stream0);
  WockyTLSSession *server = wocky_tls_session_server_new (
    test->stream->stream1, 1024, TLS_SERVER_KEY_FILE, TLS_SERVER_CRT_FILE);
  gsize expected = TEST_SSL_DATA_LEN * 5;
  gchar *target =
    TEST_SSL_DATA_A "\0" TEST_SSL_DATA_B "\0"
    TEST_SSL_DATA_A "\0" TEST_SSL_DATA_B "\0" TEST_SSL_DATA_A;

  ssl_test.cli_send = g_malloc0 (32 * 1024);

  while (ssl_test.cli_send_len + 4 < 32 * 1024)
    {
      guint32 r = g_random_int ();
      memcpy (ssl_test.cli_send + ssl_test.cli_send_len, &r, 4);
      ssl_test.cli_send_len += 4;
    }

  setup_ssl_test (&ssl_test, test);

  wocky_tls_session_handshake_async (client, G_PRIORITY_DEFAULT,
      test->cancellable, client_handshake_cb, &ssl_test);
  test->outstanding += 1;

  wocky_tls_session_handshake_async (server, G_PRIORITY_DEFAULT,
      test->cancellable, server_handshake_cb, &ssl_test);
  test->outstanding += 1;

  test_wait_pending (test);

  g_assert_cmpint (test->outstanding, ==, 0);
  g_assert_cmpint (ssl_test.cli_data->len, ==, expected);
  g_assert_cmpint (ssl_test.srv_data->len, ==, ssl_test.cli_send_len);
  g_assert (!memcmp (ssl_test.cli_data->str, target, expected));
  g_assert (!memcmp (ssl_test.srv_data->str,
    ssl_test.cli_send, ssl_test.cli_send_len));

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
  g_test_add_func ("/tls/handshake+rw", test_tls_handshake_rw);
  result = g_test_run ();
  test_deinit ();

  return result;
}
