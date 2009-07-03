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

#define INVISIBLE_HOST "unreachable.host"
#define VISIBLE_HOST   "reachable.host"
#define REACHABLE      "127.0.0.1"
#define UNREACHABLE    "127.255.255.255"
#define DUFF_H0ST      "no_such_host.at.all"

static GError *error = NULL;
static GResolver *original;
static GResolver *kludged;
static GMainLoop *mainloop;

typedef struct {
  gchar *desc;
  struct { gchar *domain; int code; gpointer xmpp; } result;
  struct {
    struct { gboolean tls; gchar *auth_mech; } features;
    struct { ServerProblem sasl; ConnectorProblem conn; } problem;
    struct { gchar *user; gchar *pass; } auth;
    guint port;
  } server;
  struct { char *srv; guint port; char *host; char *addr; char *srvhost; } dns;
  struct {
    gboolean require_tls;
    struct { gchar *jid; gchar *pass; gboolean secure; gboolean digest; } auth;
    struct { gchar *host; guint port; } options;
  } client;
  pid_t  server_pid;
} test_t;

test_t tests[] =
  { /* basic connection test, no SRV record, no host or port supplied: */
    /*
    { "/name/of/test",
      // result of test:
      { DOMAIN, CODE, XMPP_CONNECTION_PLACEHOLDER },

      // Derver Details:
      { { TLS_SUPPORT, AUTH_MECH_OR_NULL_FOR_ALL  },
        { SERVER_PROBLEM..., CONNECTOR_PROBLEM... },
        { USERNAME, PASSWORD },
        SERVER_LISTEN_PORT },

      // Fake DNS Record:
      // SRV_HOSTs SRV record → { HOSTNAME, PORT }
      // HOSTs     A   record → IP_ADDRESS
      // SRV_HOSTs A   record → IP_ADDR_OF_SRV_HOST
      { SRV_HOST, PORT, HOSTNAME, IP_ADDRESS, IP_ADDR_OF_SRV_HOST },

      // Client Details
      { TLS_REQUIRED,
        { BARE_JID, PASSWORD, MUST_BE_SECURE, MUST_BE_DIGEST_AUTH },
        { XMPP_HOSTNAME_OR_NULL, XMPP_PORT_OR_ZERO } } }, */

    { "/connector/basic/noserv/nohost/noport",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 0 } } },

    { "/connector/basic/noserv/nohost/port",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        8222 },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 8222 } } },

    { "/connector/basic/noserv/nohost/duffport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        8222 },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 8221 } } },

    { "/connector/basic/noserv/host/noport",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { NULL, 0, "schadenfreude.org", REACHABLE, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { "schadenfreude.org", 0 } } },

    { "/connector/basic/noserv/host/port",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5555 },
      { NULL, 0, "meerkats.net", REACHABLE, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { "meerkats.net", 5555 } } },

    { "/connector/basic/noserv/host/duffport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5555 },
      { NULL, 0, "meerkats.net", REACHABLE, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { "meerkats.net", 5554 } } },

    { "/connector/basic/noserv/duffhost/noport",
      { "g-resolver-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { NULL, NULL },
        0 },
      { NULL, 0, NULL, NULL, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { DUFF_H0ST, 0 } } },

    { "/connector/basic/noserv/duffhost/port",
      { "g-resolver-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { NULL, NULL },
        0 },
      { NULL, 0, NULL, NULL, NULL },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { "still.no_such_host.at.all", 23 } } },

    { "/connector/basic/serv/nohost/noport",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5050 },
      { "weasel-juice.org", 5050, "thud.org", REACHABLE, UNREACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 0 } } },

    { "/connector/basic/serv/nohost/port",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5051 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 5051 } } },

    { "/connector/basic/serv/nohost/duffport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5051 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 5050 } } },

    { "/connector/basic/serv/host/noport",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { VISIBLE_HOST, 0 } } },

    { "/connector/basic/serv/host/port",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5656 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { VISIBLE_HOST, 5656 } } },

    { "/connector/basic/serv/host/duffport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5656 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { VISIBLE_HOST, 5655 } } },

    { "/connector/basic/serv/duffhost/noport",
      { "g-resolver-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { "weasel-juice.org", 5222, "thud.org", REACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { DUFF_H0ST, 0 } } },

    { "/connector/basic/serv/duffhost/port",
      { "g-resolver-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { "weasel-juice.org", 5222, "thud.org", REACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { DUFF_H0ST, 5222 } } },

    { "/connector/basic/duffserv/nohost/noport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { "weasel-juice.org", 5222, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 0 } } },

    { "/connector/basic/duffserv/nohost/port",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5050 },
      { "weasel-juice.org", 5222, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 5050 } } },

    { "/connector/basic/duffserv/nohost/duffport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5050 },
      { "weasel-juice.org", 5222, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { NULL, 5049 } } },

    { "/connector/basic/duffserv/host/noport",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { VISIBLE_HOST, 0 } } },

    { "/connector/basic/duffserv/host/port",
      { NULL, 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5151 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { VISIBLE_HOST, 5151 } } },

    { "/connector/basic/duffserv/host/duffport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5151 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { VISIBLE_HOST, 5149 } } },

    { "/connector/basic/duffserv/duffhost/noport",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5222 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { INVISIBLE_HOST, 0 } } },

    { "/connector/basic/duffserv/duffhost/port",
      { "g-io-error-quark", 0 },
      { { TRUE, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_PROBLEM_NO_PROBLEM },
        { "moose", "something" },
        5151 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { FALSE,
        { "moose@weasel-juice.org", "something", FALSE, FALSE },
        { INVISIBLE_HOST, 5151 } } },

    /* we are done, cap the list: */
    { NULL }
  };

/* ************************************************************************* */
#define STRING_OK(x) (((x) != NULL) && (*x != '\0'))

static void
setup_dummy_dns_entries (const test_t *test)
{
  TestResolver *tr = TEST_RESOLVER (kludged);
  guint port = test->dns.port ? test->dns.port : 5222;
  const char *domain = test->dns.srv;
  const char *host = test->dns.host;
  const char *addr = test->dns.addr;
  const char *s_ip = test->dns.srvhost;

  test_resolver_reset (tr);

  if (STRING_OK (domain) && STRING_OK (host))
    test_resolver_add_SRV (tr, "xmpp-client", "tcp", domain, host, port);

  if (STRING_OK (domain) && STRING_OK (s_ip))
    test_resolver_add_A (tr, domain, s_ip);

  if (STRING_OK (host) && STRING_OK (addr))
    test_resolver_add_A (tr, host, addr);

  test_resolver_add_A (tr, INVISIBLE_HOST, UNREACHABLE);
  test_resolver_add_A (tr, VISIBLE_HOST, REACHABLE);
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
start_dummy_xmpp_server (test_t *test)
{
  int ssock;
  int reuse = 1;
  struct sockaddr_in server;
  GIOChannel *channel;
  pid_t server_pid;

  if (test->server.port == 0)
    return;

  /* setenv ("WOCKY_DEBUG","all",TRUE); */
  memset (&server, 0, sizeof (server));

  server.sin_family = AF_INET;
  inet_aton (REACHABLE, &server.sin_addr);
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
      test->server_pid = server_pid;
      close (ssock);
      return;
    }

  channel = g_io_channel_unix_new (ssock);
  g_io_add_watch (channel, G_IO_IN|G_IO_PRI, client_connected, (gpointer)test);
  g_main_loop_run (mainloop);
  exit(0);
}
/* ************************************************************************* */
static void
test_done (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  gchar *jid = NULL;
  test_t *test = data;
  WockyConnector *wcon = WOCKY_CONNECTOR (source);
  WockyXmppConnection *conn = NULL;

  conn = wocky_connector_connect_finish (wcon, res, &error, &jid);
  if (conn != NULL)
    test->result.xmpp = g_object_ref (conn);

  if (test->server_pid > 0)
    kill (test->server_pid, SIGKILL);

  g_main_loop_quit (mainloop);
}

static void
run_test (gconstpointer data)
{
  WockyConnector *wcon = NULL;
  test_t *test = data;

  start_dummy_xmpp_server (test);
  setup_dummy_dns_entries (test);

  wcon = g_object_new ( WOCKY_TYPE_CONNECTOR,
      "jid"                     , test->client.auth.jid,
      "password"                , test->client.auth.pass,
      "xmpp-server"             , test->client.options.host,
      "xmpp-port"               , test->client.options.port,
      "tls-required"            , test->client.require_tls,
      "encrypted-plain-auth-ok" , !test->client.auth.secure,
      "plaintext-auth-allowed"  , !test->client.auth.digest,
      /* insecure tls cert/etc not yet implemented */
      "ignore-ssl-errors"       , FALSE,
      NULL);

  wocky_connector_connect_async (wcon, test_done, (gpointer)test);

  g_main_loop_run (mainloop);

  if (wcon != NULL)
    g_object_unref (wcon);

  if (test->result.domain == NULL)
    {
      if (error)
        fprintf (stderr, "Error: %s.%d: %s\n",
            g_quark_to_string (error->domain),
            error->code,
            error->message);
      g_assert (error == NULL);
      g_assert (test->result.xmpp != NULL);
    }
  else
    {
      GQuark domain = g_quark_from_string (test->result.domain);
      if (!g_error_matches (error, domain, test->result.code))
        fprintf (stderr, "ERROR: %s.%d: %s\n",
            g_quark_to_string (error->domain),
            error->code,
            error->message);
      g_assert_error (error, domain, test->result.code);
    }

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
