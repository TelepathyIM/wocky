#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-test-stream.h"
#include "wocky-test-helper.h"

#include <string.h>

#include <wocky/wocky.h>

/* WockyHttpProxy isn't public API, so we need to be a bit sneaky to get the
 * header.
 */
#define WOCKY_COMPILATION
#include <wocky/wocky-http-proxy.h>
#undef WOCKY_COMPILATION

typedef enum
{
  DOMAIN_NONE = 0,
  DOMAIN_G_IO_ERROR
} HttpErrorDomain;

typedef struct
{
  const gchar *path;
  const gchar *reply;
  HttpErrorDomain domain;
  gint code;
  const gchar *username;
  const gchar *password;
} HttpTestCase;

typedef struct
{
  GMainLoop *mainloop;
  GMainLoop *thread_mainloop;
  GCancellable *cancellable;
  GCancellable *thread_cancellable;
  GThread *thread;
  GSocketListener *listener;
  guint16 port;
  const HttpTestCase *test_case;
} HttpTestData;

static HttpTestCase test_cases[] = {
    { "/http-proxy/close-by-peer",
      "", DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_FAILED },
    { "/http-proxy/bad-reply",
      "BAD REPLY", DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_FAILED },
    { "/http-proxy/very-short-reply",
      "HTTP/1.\r\n\r\n", DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_FAILED },
    { "/http-proxy/short-reply",
      "HTTP/1.0\r\n\r\n", DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_FAILED },
    { "/http-proxy/http-404",
      "HTTP/1.0 404 Not Found\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "<html><h1>Hello</h1></html>",
      DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_FAILED },
    { "/http-proxy/need-authentication",
      "HTTP/1.0 407 Proxy Authentication Required\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "<html><h1>Hello</h1></html>",
      DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_NEED_AUTH },
    { "/http-proxy/success",
      "HTTP/1.0 200 OK\r\n"
        "\r\n",
      DOMAIN_NONE, 0 },
    { "/http-proxy/authentication-failed",
      "HTTP/1.0 407 Proxy Authentication Required\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "<html><h1>Hello</h1></html>",
      DOMAIN_G_IO_ERROR, G_IO_ERROR_PROXY_AUTH_FAILED,
      "username", "bad-password" },
    { "/http-proxy/authenticated",
      "HTTP/1.0 200 OK\r\n"
        "\r\n",
      DOMAIN_NONE, 0,
      "Aladdin", "open sesame"},
};

static HttpTestData *
tearup (const HttpTestCase *test_case)
{
  HttpTestData *data;

  data = g_new0 (HttpTestData, 1);

  data->mainloop = g_main_loop_new (NULL, FALSE);

  data->cancellable = g_cancellable_new ();
  data->thread_cancellable = g_cancellable_new ();

  data->listener = g_socket_listener_new ();
  data->port = g_socket_listener_add_any_inet_port (data->listener, NULL, NULL);
  g_assert_cmpuint (data->port, !=, 0);

  data->test_case = test_case;

  return data;
}

static void
run_in_thread (HttpTestData *data,
    GThreadFunc func)
{
  data->thread = g_thread_new ("server_thread", func, data);
  g_assert (data->thread != NULL);
}

static gboolean
tear_down_idle_cb (gpointer user_data)
{
  HttpTestData *data = user_data;
  g_main_loop_quit (data->mainloop);
  return FALSE;
}

static void
teardown (HttpTestData *data)
{
  if (!g_cancellable_is_cancelled (data->cancellable))
    g_cancellable_cancel (data->cancellable);

  if (!g_cancellable_is_cancelled (data->thread_cancellable))
    g_cancellable_cancel (data->thread_cancellable);

  if (data->thread)
    g_thread_join (data->thread);

  if (g_main_loop_is_running (data->mainloop))
    g_main_loop_quit (data->mainloop);

  g_idle_add_full (G_PRIORITY_LOW, tear_down_idle_cb, data, NULL);

  g_main_loop_run (data->mainloop);

  g_object_unref (data->cancellable);
  g_object_unref (data->thread_cancellable);
  g_object_unref (data->listener);
  g_main_loop_unref (data->mainloop);

  g_free (data);
}

static gboolean
str_has_prefix_case (const gchar *str,
    const gchar *prefix)
{
  return g_ascii_strncasecmp (prefix, str, strlen (prefix)) == 0;
}

static void
test_http_proxy_instantiation (void)
{
  GProxy *proxy;

  proxy = g_proxy_get_default_for_protocol ("http");
  g_assert (G_IS_PROXY (proxy));
  g_assert (WOCKY_IS_HTTP_PROXY (proxy));
  g_object_unref (proxy);
}

static gpointer
server_thread (gpointer user_data)
{
  HttpTestData *data = user_data;
  GSocketConnection *conn;
  GDataInputStream *data_in;
  GOutputStream *out;
  gchar *buffer;
  gint has_host = 0;
  gint has_user_agent = 0;
  gint has_cred = 0;

  conn = g_socket_listener_accept (data->listener, NULL,
      data->thread_cancellable, NULL);
  g_assert (conn != NULL);

  data_in = g_data_input_stream_new (
      g_io_stream_get_input_stream (G_IO_STREAM (conn)));
  g_data_input_stream_set_newline_type (data_in,
      G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

  buffer = g_data_input_stream_read_line (data_in, NULL,
      data->thread_cancellable, NULL);
  g_assert_cmpstr (buffer, ==, "CONNECT to:443 HTTP/1.0");

  do {
      g_free (buffer);
      buffer = g_data_input_stream_read_line (data_in, NULL,
          data->thread_cancellable, NULL);
      g_assert (buffer != NULL);

      if (str_has_prefix_case (buffer, "Host:"))
        {
          has_host++;
          g_assert_cmpstr (buffer, ==, "Host: to:443");
        }
      else if (str_has_prefix_case (buffer, "User-Agent:"))
        has_user_agent++;
      else if (str_has_prefix_case (buffer, "Proxy-Authorization:"))
        {
          gchar *cred;
          gchar *base64_cred;
          const gchar *received_cred;

          has_cred++;

          g_assert (data->test_case->username != NULL);
          g_assert (data->test_case->password != NULL);

          cred = g_strdup_printf ("%s:%s",
              data->test_case->username, data->test_case->password);
          base64_cred = g_base64_encode ((guchar *) cred, strlen (cred));
          g_free (cred);

          received_cred = buffer + 20;
          while (*received_cred == ' ')
            received_cred++;

          g_assert_cmpstr (base64_cred, ==, received_cred);
          g_free (base64_cred);
        }
  } while (buffer[0] != '\0');

  g_assert_cmpuint (has_host, ==, 1);
  g_assert_cmpuint (has_user_agent, ==, 1);

  if (data->test_case->username != NULL)
    g_assert_cmpuint (has_cred, ==, 1);
  else
    g_assert_cmpuint (has_cred, ==, 0);

  g_free (buffer);

  out = g_io_stream_get_output_stream (G_IO_STREAM (conn));
  g_assert (g_output_stream_write_all (out,
        data->test_case->reply, strlen (data->test_case->reply),
        NULL, data->thread_cancellable, NULL));
  g_object_unref (data_in);
  g_object_unref (conn);

  return NULL;
}

static GQuark
get_error_domain (HttpErrorDomain id)
{
  GQuark domain = 0;

  switch (id)
    {
    case DOMAIN_G_IO_ERROR:
      domain = G_IO_ERROR;
      break;
    default:
      g_assert_not_reached ();
    }

  return domain;
}

static GSocketAddress *
create_proxy_address (HttpTestData *data)
{
  GSocketAddress *proxy_address;
  GInetAddress *inet_address;

  inet_address = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  proxy_address = g_proxy_address_new (inet_address, data->port, "http",
      "to", 443, data->test_case->username, data->test_case->password);
  g_object_unref (inet_address);

  return proxy_address;
}

static void
check_result (const HttpTestCase *test_case,
    GSocketConnection *connection,
    GError *error)
{
  if (test_case->domain != DOMAIN_NONE)
    {
      g_assert_error (error, get_error_domain (test_case->domain),
          test_case->code);
      g_error_free (error);
    }
  else
    {
      g_assert_no_error (error);
      g_object_unref (connection);
    }
}

static void
test_http_proxy_with_data (gconstpointer user_data)
{
  const HttpTestCase *test_case = user_data;
  HttpTestData *data;
  GSocketClient *client;
  GSocketAddress *proxy_address;
  GSocketConnection *connection;
  GError *error = NULL;

  data = tearup (test_case);

  run_in_thread (data, server_thread);

  client = g_socket_client_new ();
  proxy_address = create_proxy_address (data);
  connection = g_socket_client_connect (client,
      G_SOCKET_CONNECTABLE (proxy_address), data->cancellable, &error);

  g_object_unref (proxy_address);
  g_object_unref (client);

  check_result (test_case, connection, error);

  teardown (data);
}

static void
connect_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  HttpTestData *data = user_data;
  GSocketConnection *connection;
  GError *error = NULL;

  connection = g_socket_client_connect_finish (G_SOCKET_CLIENT (source),
      result, &error);

  check_result (data->test_case, connection, error);

  g_main_loop_quit (data->mainloop);
}

static void
test_http_proxy_with_data_async (gconstpointer user_data)
{
  const HttpTestCase *test_case = user_data;
  HttpTestData *data;
  GSocketClient *client;
  GSocketAddress *proxy_address;

  data = tearup (test_case);

  run_in_thread (data, server_thread);

  client = g_socket_client_new ();
  proxy_address = create_proxy_address (data);
  g_socket_client_connect_async (client, G_SOCKET_CONNECTABLE (proxy_address),
      data->cancellable, connect_cb, data);

  g_object_unref (client);
  g_object_unref (proxy_address);

  g_main_loop_run (data->mainloop);

  teardown (data);
}

int main (int argc,
    char **argv)
{
  int result;
  guint i;

  test_init (argc, argv);

  _wocky_http_proxy_get_type ();

  g_test_add_func ("/http-proxy/instantiation",
      test_http_proxy_instantiation);

  for (i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
      gchar *async_path;

      g_test_add_data_func (test_cases[i].path,
          test_cases + i, test_http_proxy_with_data);

      async_path = g_strdup_printf ("%s-async", test_cases[i].path);
      g_test_add_data_func (async_path,
          test_cases + i, test_http_proxy_with_data_async);
      g_free (async_path);
    }

  result = g_test_run ();
  test_deinit ();
  return result;
}
