#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wocky/wocky-connector.h>
#include "wocky-test-connector-server.h"
#include "test-resolver.h"

static GError *error = NULL;
static gboolean finished = FALSE;
static GResolver *original;
static GResolver *kludged;
static GMainLoop *mainloop;

typedef struct {
  gchar *desc;
  struct { GQuark domain; int code; gpointer xmpp; } result;
  struct {
    struct { gboolean tls; gchar *auth_mech; } features;
    struct { ServerProblem sasl; ConnectorProblem conn; } problem;
    struct { gchar *user; gchar *pass; } auth;
    guint port;
  } server;
  struct { char *srv; guint srv_port; char *host; char *addr; } dns;
  struct {
    gboolean require_tls;
    struct { gchar *jid; gchar *pass; gboolean secure; gboolean digest; } auth;
    struct { gchar *host; guint port; } options;
  } client;
} test_t;

test_t tests[] =
  { /* basic connection test, no SRV record, no host or port supplied: */
    { "/connector/basic/noserv/nohost/noport",
      { 0, 0, NULL },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { NULL, 0, "weasel-juice.org", "127.0.0.1" },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 0 } } },
/* we are done, cap the list: */
    { NULL }
  };

/* ************************************************************************* */
#define STRING_OK(x) (((x) != NULL) && (*x))

static void
setup_dummy_dns_entries (const test_t *test)
{
  TestResolver *tr = TEST_RESOLVER (kludged);
  guint port = test->dns.srv_port ? test->dns.srv_port : 5222;
  const char *domain = test->dns.srv;
  const char *host = test->dns.host;
  const char *addr = test->dns.addr;

  test_resolver_reset (tr);

  if (STRING_OK (domain) && STRING_OK (host))
    test_resolver_add_SRV (tr, "xmpp-client", "tcp", domain, host, port);

  if (STRING_OK (host) && STRING_OK (addr))
    test_resolver_add_A (tr, host, addr);
}

/* ************************************************************************* */
/* Dummy XMPP server */
static gboolean
client_connected (GIOChannel *channel,
    GIOCondition cond,
    gpointer data)
{
  struct sockaddr_in client;
  socklen_t clen = sizeof (client);
  int ssock = g_io_channel_unix_get_fd (channel);
  int csock = accept (ssock, (struct sockaddr *)&client, &clen);
  GSocket *gsock = g_socket_new_from_fd (csock, NULL);
  test_t *test = data;

  GSocketConnection *gconn;
  TestConnectorServer *server;
  long flags;

  if (csock < 0)
    {
      perror ("accept() failed");
      g_warning ("accept() failed on socket that should have been ready.");
      return TRUE;
    }

  fprintf (stderr, "XMPP CLIENT CONNECTION\n");

  while (g_source_remove_by_user_data (test));
  g_io_channel_close (channel);

  flags = fcntl (csock, F_GETFL );
  flags = flags & ~O_NONBLOCK;
  fcntl (csock, F_SETFL, flags);
  gconn = g_object_new (G_TYPE_SOCKET_CONNECTION, "socket", gsock, NULL);
  server = test_connector_server_new (G_IO_STREAM (gconn),
      test->server.features.auth_mech,
      test->server.auth.user,
      test->server.auth.pass,
      test->server.problem.sasl,
      test->server.problem.conn);
  test_connector_server_start (G_OBJECT (server));
  return FALSE;
}

static void
start_dummy_xmpp_server (const test_t *test)
{
  int ssock;
  int reuse = 1;
  struct sockaddr_in server;
  GIOChannel *channel;
  pid_t server_pid;

  /* setenv ("WOCKY_DEBUG","all",TRUE); */
  memset (&server, 0, sizeof (server));

  server.sin_family = AF_INET;
  inet_aton ("127.0.0.1", &server.sin_addr);
  server.sin_port = htons (test->server.port);
  ssock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  setsockopt (ssock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));
  bind (ssock, (struct sockaddr *)&server, sizeof (server));
  listen (ssock, 1024);

  server_pid = fork ();

  if (server_pid < 0)
    {
      perror ("fork() of dummy server failed\n");
      g_main_loop_quit (mainloop);
      exit (server_pid);
    }

  if (server_pid)
    {
      close (ssock);
      return;
    }

  channel = g_io_channel_unix_new (ssock);
  g_io_add_watch (channel, G_IO_IN|G_IO_PRI, client_connected, (gpointer)test);
  g_main_loop_run (mainloop);
}
/* ************************************************************************* */
static void
test_done (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  test_t *test = data;
  test->result.xmpp = wocky_connector_connect_finish (source, res, &error);
  finished = TRUE;
  g_main_loop_quit (mainloop);
}

static void
run_test (gconstpointer data)
{
  WockyConnector *wcon = NULL;
  const test_t *test = data;

  start_dummy_xmpp_server (test);
  setup_dummy_dns_entries (test);

  wcon = wocky_connector_new_full (test->client.auth.jid,
      test->client.auth.pass,
      NULL,
      test->client.options.host,
      test->client.options.port,
      test->client.require_tls,
      FALSE, /* insecure tls cert/etc not yet implemented */
      !test->client.auth.secure,
      !test->client.auth.digest);

  wocky_connector_connect_async (G_OBJECT (wcon), test_done, (gpointer)test);

  if (!finished)
    g_main_loop_run (mainloop);

  if (wcon != NULL)
    g_object_unref (wcon);

  if (test->result.domain == 0)
    {
      if (error)
        fprintf (stderr, "Error: %d.%d: %s\n",
            error->domain,
            error->code,
            error->message);
      g_assert (error == NULL);
      g_assert (test->result.xmpp != NULL);
    }
  else
    g_assert (g_error_matches (error, test->result.domain, test->result.code));

  if (error != NULL)
    g_error_free (error);

  if (test->result.xmpp != NULL)
    g_object_unref (test->result.xmpp);

  error = NULL;
}

int
main (int argc,
    char **argv)
{
  int i;
  g_thread_init (NULL);
  g_type_init ();
  g_test_init (&argc, &argv, NULL);
  original = g_resolver_get_default ();
  kludged = g_object_new (TEST_TYPE_RESOLVER, "real-resolver", original, NULL);
  g_resolver_set_default (kludged);

  mainloop = g_main_loop_new (NULL, FALSE);

  for (i = 0; tests[i].desc != NULL; i++)
    g_test_add_data_func (tests[i].desc, &tests[i], run_test);

  return g_test_run ();
}
