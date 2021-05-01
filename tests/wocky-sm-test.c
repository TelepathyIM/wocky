#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef G_OS_UNIX
#include <netinet/tcp.h>
#endif

#include <wocky/wocky.h>

#include "wocky-test-connector-server.h"
#include "test-resolver.h"
#include "wocky-test-helper.h"
#include "config.h"

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "wocky-sm-test"

#define SASL_DB_NAME "sasl-test.db"

#define INVISIBLE_HOST "unreachable.host"
#define VISIBLE_HOST   "reachable.host"
#define REACHABLE      "127.0.0.1"
#define UNREACHABLE    "127.255.255.255"
#define DUFF_H0ST      "no_such_host.at.all"

#define OLD_SSL    TRUE
#define OLD_JABBER TRUE
#define XMPP_V1    FALSE
#define STARTTLS   FALSE

#define CERT_CHECK_STRICT  FALSE
#define CERT_CHECK_LENIENT TRUE

#define TLS_REQUIRED TRUE
#define PLAINTEXT_OK FALSE

#define QUIET TRUE
#define NOISY FALSE

#define TLS   TRUE
#define NOTLS FALSE

#define PLAIN  FALSE
#define DIGEST TRUE

#define DEFAULT_SASL_MECH "SCRAM-SHA-256"

#define PORT_XMPP 5222
#define PORT_NONE 0

#define OK 0
#define CONNECTOR_OK { OK, OK, OK, OK, OK, OK, OK }
#define SM_PROBLEM(x) { OK, OK, OK, OK, OK, OK, SM_PROBLEM_##x }


static GError *error = NULL;
static GResolver *original;
static GResolver *kludged;
static GMainLoop *mainloop;

enum {
  S_NO_ERROR = 0,
  S_WOCKY_AUTH_ERROR,
  S_WOCKY_CONNECTOR_ERROR,
  S_WOCKY_XMPP_CONNECTION_ERROR,
  S_WOCKY_TLS_CERT_ERROR,
  S_WOCKY_XMPP_STREAM_ERROR,
  S_G_IO_ERROR,
  S_G_RESOLVER_ERROR,
  S_ANY_ERROR = 0xff
};

#define MAP(x)  case S_##x: return x
static GQuark
map_static_domain (gint domain)
{
  switch (domain)
    {
      MAP (WOCKY_AUTH_ERROR);
      MAP (WOCKY_CONNECTOR_ERROR);
      MAP (WOCKY_XMPP_CONNECTION_ERROR);
      MAP (WOCKY_TLS_CERT_ERROR);
      MAP (WOCKY_XMPP_STREAM_ERROR);
      MAP (G_IO_ERROR);
      MAP (G_RESOLVER_ERROR);
      default:
        g_assert_not_reached ();
    }
}
#undef MAP


typedef void (*test_setup) (gpointer);

typedef struct _ServerParameters ServerParameters;
struct _ServerParameters {
  struct { gboolean tls; gchar *auth_mech; gchar *version; } features;
  struct { ServerProblem sasl; ConnectorProblem conn; } problem;
  struct { gchar *user; gchar *pass; } auth;
  guint port;
  CertSet cert;

  /* Extra server for see-other-host problem */
  ServerParameters *extra_server;

  /* Runtime */
  TestConnectorServer *server;
  GIOChannel *channel;
  guint watch;
};

typedef struct {
  gchar *desc;
  gboolean quiet;
  struct { int domain;
           int code;
           int fallback_code;
           gchar *mech;
           gchar *used_mech;
           gpointer xmpp;
           gchar *jid;
           gchar *sid;
  } result;
  ServerParameters server_parameters;
  struct { char *srv; guint port; char *host; char *addr; char *srvhost; } dns;
  struct {
    gboolean require_tls;
    struct { gchar *jid; gchar *pass; gboolean secure; gboolean tls; } auth;
    struct { gchar *host; guint port; gboolean jabber; gboolean ssl; gboolean lax_ssl; const gchar *ca; } options;
    int op;
    test_setup setup;
  } client;

  /* Runtime */
  WockyConnector *connector;
  WockySession *session;
  WockyPorter *porter;
  gboolean ok;
} test_t;

test_t tests[] =
  { /* basic connection test, no SRV record, no host or port supplied: */
    /*
    { "/name/of/test",
      SUPPRESS_STDERR,
      // result of test:
      { DOMAIN, CODE, FALLBACK_CODE,
        AUTH_MECH_USED, XMPP_CONNECTION_PLACEHOLDER },
      // When an error is expected it should match the domain and either
      // the given CODE or the FALLBACK_CODE (GIO over time became more
      // specific about the error codes it gave in certain conditions)

      // Server Details:
      { { TLS_SUPPORT, AUTH_MECH_OR_NULL_FOR_ALL  },
        { SERVER_PROBLEM..., CONNECTOR_PROBLEM... },
        { USERNAME, PASSWORD },
        SERVER_LISTEN_PORT, SERVER_CERT },

      // Fake DNS Record:
      // SRV_HOSTs SRV record → { HOSTNAME, PORT }
      // HOSTs     A   record → IP_ADDRESS
      // SRV_HOSTs A   record → IP_ADDR_OF_SRV_HOST
      { SRV_HOST, PORT, HOSTNAME, IP_ADDRESS, IP_ADDR_OF_SRV_HOST },

      // Client Details
      { TLS_REQUIRED,
        { BARE_JID, PASSWORD, MUST_BE_DIGEST_AUTH, MUST_BE_SECURE },
        { XMPP_HOSTNAME_OR_NULL, XMPP_PORT_OR_ZERO, OLD_JABBER, OLD_SSL } }
        SERVER_PROCESS_ID }, */

    /* simple connection, followed by checks on all the internal state *
     * and get/set property methods to make sure they work             */

    /* ********************************************************************* */
    /* SM test conditions */
    { "/tls/sasl/sm/enable",
      NOISY,
      { S_NO_ERROR },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (ENABLED) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/tls/sasl/sm/ack0",
      NOISY,
      { S_NO_ERROR },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (ACK0) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/tls/sasl/sm/ack1",
      NOISY,
      { S_NO_ERROR },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (ACK1) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/tls/sasl/sm/ack-overrun",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR, WOCKY_XMPP_STREAM_ERROR_UNDEFINED_CONDITION, },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (ACK0_OVER) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/tls/sasl/sm/ack-wrap0",
      NOISY,
      { S_NO_ERROR },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (WRAP0) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/tls/sasl/sm/ack-wrap1",
      NOISY,
      { S_NO_ERROR },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (WRAP1) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/tls/sasl/sm/wrap-overrun",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR, WOCKY_XMPP_STREAM_ERROR_UNDEFINED_CONDITION, },
      { { TLS, DEFAULT_SASL_MECH },
        { SERVER_PROBLEM_NO_PROBLEM, SM_PROBLEM (WRAP0_OVER) },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    /* we are done, cap the list: */
    { NULL }
  };

/* ************************************************************************* */
#define STRING_OK(x) (((x) != NULL) && (*x != '\0'))

static void
setup_dummy_dns_entries (const test_t *test)
{
  TestResolver *tr = TEST_RESOLVER (kludged);
  guint port = test->dns.port ? test->dns.port : test->server_parameters.port;
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
static void start_dummy_xmpp_server (ServerParameters *srv);

static gboolean
client_connected (GIOChannel *channel,
    GIOCondition cond,
    gpointer data)
{
  ServerParameters *srv = data;
  struct sockaddr_in client;
  socklen_t clen = sizeof (client);
  int ssock = g_io_channel_unix_get_fd (channel);
  int csock = accept (ssock, (struct sockaddr *) &client, &clen);
  GSocket *gsock = g_socket_new_from_fd (csock, NULL);
  ConnectorProblem *cproblem = &srv->problem.conn;

  GSocketConnection *gconn;

  if (csock < 0)
    {
      perror ("accept() failed");
      g_warning ("accept() failed on socket that should have been ready.");
      return TRUE;
    }

  if (!srv->features.tls)
      cproblem->xmpp |= XMPP_PROBLEM_NO_TLS;

  gconn = g_object_new (G_TYPE_SOCKET_CONNECTION, "socket", gsock, NULL);
  g_object_unref (gsock);

  srv->server = test_connector_server_new (G_IO_STREAM (gconn),
      srv->features.auth_mech,
      srv->auth.user,
      srv->auth.pass,
      srv->features.version,
      NULL,
      cproblem,
      srv->problem.sasl,
      srv->cert);
  g_object_unref (gconn);

  /* Recursively start extra servers */
  if (srv->extra_server != NULL)
    {
      test_connector_server_set_other_host (srv->server, REACHABLE,
          srv->extra_server->port);
      start_dummy_xmpp_server (srv->extra_server);
    }

  test_connector_server_start (srv->server);

  srv->watch = 0;
  return FALSE;
}

static void
start_dummy_xmpp_server (ServerParameters *srv)
{
  int ssock;
  int reuse = 1;
  struct sockaddr_in server;
  int res = -1;
  guint port = srv->port;

  if (port == 0)
    return;

  memset (&server, 0, sizeof (server));

  server.sin_family = AF_INET;

  /* mingw doesn't support aton or pton so using more portable inet_addr */
  server.sin_addr.s_addr = inet_addr ((const char * ) REACHABLE);
  server.sin_port = htons (port);
  ssock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  setsockopt (ssock, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof (reuse));
#ifdef G_OS_UNIX
  setsockopt (ssock, IPPROTO_TCP, TCP_NODELAY, (const char *) &reuse, sizeof (reuse));
#endif

  res = bind (ssock, (struct sockaddr *) &server, sizeof (server));

  if (res != 0)
    {
      int code = errno;
      char *err = g_strdup_printf ("bind to " REACHABLE ":%d failed", port);
      perror (err);
      g_free (err);
      exit (code);
    }

  res = listen (ssock, 1024);
  if (res != 0)
    {
      int code = errno;
      char *err = g_strdup_printf ("listen on " REACHABLE ":%d failed", port);
      perror (err);
      g_free (err);
      exit (code);
    }

  srv->channel = g_io_channel_unix_new (ssock);
  g_io_channel_set_flags (srv->channel, G_IO_FLAG_NONBLOCK, NULL);
  srv->watch = g_io_add_watch (srv->channel, G_IO_IN|G_IO_PRI,
    client_connected, srv);
  g_io_channel_set_close_on_unref (srv->channel, TRUE);
  return;
}
/* ************************************************************************* */
static void
test_server_teardown_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  GMainLoop *loop = user_data;

  g_assert (test_connector_server_teardown_finish (
    TEST_CONNECTOR_SERVER (source), result, NULL));

  g_main_loop_quit (loop);
}

static gboolean
test_server_idle_quit_loop_cb (GMainLoop *loop)
{
  static int retries = 0;

  if (retries == 5)
    {
      g_main_loop_quit (loop);
      retries = 0;
      return G_SOURCE_REMOVE;
    }
  else
    {
      retries ++;
      return G_SOURCE_CONTINUE;
    }
}

static void
test_server_teardown (test_t *test,
    ServerParameters *srv)
{
  /* Recursively teardown extra servers */
  if (srv->extra_server != NULL)
    test_server_teardown (test, srv->extra_server);
  srv->extra_server = NULL;

  if (srv->server != NULL)
    {
      GMainLoop *loop = g_main_loop_new (NULL, FALSE);

      if (test->result.used_mech == NULL)
        {
          test->result.used_mech = g_strdup (
            test_connector_server_get_used_mech (srv->server));
        }

      /* let the server dispatch any pending events before
       * forcing it to tear down */
      g_idle_add ((GSourceFunc) test_server_idle_quit_loop_cb, loop);
      g_main_loop_run (loop);

      /* Run until server is down */
      test_connector_server_teardown (srv->server,
        test_server_teardown_cb, loop);
      g_main_loop_run (loop);

      g_clear_object (&srv->server);
      g_main_loop_unref (loop);
    }

  if (srv->watch != 0)
    g_source_remove (srv->watch);
  srv->watch = 0;

  if (srv->channel != NULL)
    g_io_channel_unref (srv->channel);
  srv->channel = NULL;
}

static void
test_done (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  test_t *test = data;

  if (WOCKY_IS_XMPP_CONNECTION (source))
    test->result.xmpp = g_object_ref (source);
  else if (WOCKY_IS_C2S_PORTER (source))
    {
      WockyXmppConnection *conn = NULL;
      g_object_get (source,
          "connection", &conn,
          NULL);
      wocky_porter_close_finish (WOCKY_PORTER (source), res, &error);
      g_object_unref (source);
      test->result.xmpp = g_object_ref (conn);
    }

  test_server_teardown (test, &test->server_parameters);

  g_main_loop_quit (mainloop);
}

typedef void (*test_func) (gconstpointer);

#ifdef G_OS_UNIX
static void
connection_established_cb (WockyConnector *connector,
    GSocketConnection *conn,
    gpointer user_data)
{
  GSocket *sock = g_socket_connection_get_socket (conn);
  gint fd, flag = 1;

  fd = g_socket_get_fd (sock);
  setsockopt (fd, IPPROTO_TCP, TCP_NODELAY,
      (const char *) &flag, sizeof (flag));
}
#endif

static void
iq_result (GObject *source,
    GAsyncResult *res,
    gpointer data);

static gboolean
test_phase (gpointer data)
{
  test_t *test = data;
  SMProblem sm = test->server_parameters.problem.conn.sm;
  const WockyPorterSmCtx *smc;

  if (test->porter == NULL)
    {
      test_done (NULL, NULL, test);
      return FALSE;
    }

  smc = wocky_c2s_porter_get_sm_ctx (WOCKY_C2S_PORTER (test->porter));

  g_debug ("Reqs: %d", smc->reqs);
  g_assert_cmpint (smc->reqs, ==, 0);
  if (sm & SM_PROBLEM_WRAP)
    {
      if (test->client.op == 0)
        g_assert_cmpint (smc->snt_acked, ==, G_MAXUINT32);
      else
        g_assert_cmpint (smc->snt_acked, ==, 0);
    }
  else
    g_assert_cmpint (smc->snt_acked, ==, test->client.op);

  if (sm & SM_PROBLEM_ACK1 && test->client.op == 0)
    {
      WockyStanza *iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
            WOCKY_STANZA_SUB_TYPE_SET,
            NULL, NULL,
            '(', "session", ':', WOCKY_XMPP_NS_SESSION,
            ')',
            NULL);
      wocky_porter_send_iq_async (WOCKY_PORTER (test->porter), iq, NULL,
          iq_result, test);
      test->client.op++;
      g_object_unref (iq);
      return FALSE;
    }

  wocky_porter_close_async (WOCKY_PORTER (test->porter), NULL, test_done,
      test);
  return FALSE;
}

static void
smreqd (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  test_t *test = data;
  WockyC2SPorter *p = WOCKY_C2S_PORTER (source);
  const WockyPorterSmCtx *smc = wocky_c2s_porter_get_sm_ctx (p);

  g_assert_true (
      wocky_c2s_porter_send_whitespace_ping_finish (p, res, &error));
  g_assert_cmpint (smc->reqs, ==, 1);

  /* and wait for `a` in response  */

  g_idle_add (test_phase, test);
}

static void
iq_result (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  test_t *test = data;
  WockyPorter *p = WOCKY_PORTER (source);
  WockyC2SPorter *cp = WOCKY_C2S_PORTER (source);
  const WockyPorterSmCtx *smc = wocky_c2s_porter_get_sm_ctx (cp);
  WockyStanza *iq = wocky_porter_send_iq_finish (p, res, &error);

  g_assert_nonnull (iq);
  g_object_unref (iq);

  g_assert_cmpint (smc->snt_count, ==, 1);
  g_assert_cmpint (smc->rcv_count, ==, 1);

  g_debug ("Acked: %zu", smc->snt_acked);
  wocky_c2s_porter_send_whitespace_ping_async (WOCKY_C2S_PORTER (test->porter),
      NULL, smreqd, test);
}

static void
remote_error_cb (WockyPorter *porter,
    GQuark domain,
    gint code,
    gchar *msg,
    gpointer data)
{
  test_t *test = data;

  g_debug ("Error: %s", msg);
  g_set_error_literal (&error, domain, code, msg);

  if (test->server_parameters.problem.conn.sm & SM_PROBLEM_OVER)
    {
      test->porter = NULL;
      g_object_unref (porter);
    }
}

static void
enabled (GObject *source,
    GAsyncResult *result,
    gpointer data)
{
  test_t *test = data;
  WockyXmppConnection *conn = WOCKY_XMPP_CONNECTION (source);
  SMProblem sm = test->server_parameters.problem.conn.sm;
  const WockyStanza *enabled;
  WockyNode *enn;
  WockyPorterSmCtx *smc;

  enabled = wocky_xmpp_connection_peek_stanza_finish (conn, result, &error);
  g_assert_nonnull (enabled);
  g_assert_null (error);

  enn = wocky_stanza_get_top_node ((WockyStanza *) enabled);
  g_assert_cmpstr (enn->name, ==, "enabled");

  test->session = wocky_session_new_with_connection (conn, test->result.jid);
  test->porter = wocky_session_get_porter (test->session);
  if (sm & SM_PROBLEM_OVER)
    g_signal_connect (test->porter, "remote-error",
        G_CALLBACK (remote_error_cb), test);
  wocky_porter_start (test->porter);

  /* Do NOT EVER do that in real life - context must not me modified */
  smc = (WockyPorterSmCtx *) wocky_c2s_porter_get_sm_ctx (
      WOCKY_C2S_PORTER (test->porter));
  g_assert_nonnull (smc);
  g_assert_true (smc->enabled);

  /* let the world spin - consume the enabled */
  g_main_context_iteration (NULL, FALSE);

  g_assert_true (smc->resumable);
  g_assert_cmpstr (smc->id, ==, "deadbeef");

  if (sm < SM_PROBLEM_ACK0)
    {
      test_done (G_OBJECT (conn), NULL, test);
      return;
    }

  /* let the world spin again - there will be `r` from ... */
  g_main_context_iteration (NULL, FALSE);
  /* ... and `a` to the server */
  g_main_context_iteration (NULL, FALSE);

  /* before we do get the chance to process the `a`nswer let set some tests */
  if (sm & SM_PROBLEM_WRAP)
    {
      g_debug ("unwrapping snt_acked");
      smc->snt_acked = G_MAXUINT32-1;
    }

  /* now send our r there */
  wocky_c2s_porter_send_whitespace_ping_async (WOCKY_C2S_PORTER (test->porter),
      NULL, smreqd, test);
}

static void
connected (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  test_t *test = data;
  WockyConnector *wcon = WOCKY_CONNECTOR (source);
  WockyXmppConnection *conn = NULL;

  error = NULL;
  conn = wocky_connector_connect_finish (wcon, res,
            &test->result.jid, &test->result.sid, &error);
  g_assert_nonnull (conn);
  g_assert_null (error);

  g_debug ("Connected %d", test->server_parameters.problem.conn.sm);
  wocky_xmpp_connection_peek_stanza_async (conn, NULL, enabled, test);
}

static gboolean
start_test (gpointer data)
{
  test_t *test = data;

  if (test->client.setup != NULL)
    (test->client.setup) (test);

  g_debug ("Start %d", test->server_parameters.problem.conn.sm);
  wocky_connector_connect_async (test->connector, NULL, connected, data);
  return FALSE;
}

static void
run_test (gpointer data)
{
  WockyConnector *wcon = NULL;
  WockyTLSHandler *handler;
  test_t *test = data;
  struct stat dummy;
  gchar *base;
  char *path;
  const gchar *ca;

  /* clean up any leftover messes from previous tests     */
  /* unlink the sasl db tmpfile, it will cause a deadlock */
  base = g_get_current_dir ();
  path = g_strdup_printf ("%s/__db.%s", base, SASL_DB_NAME);
  g_free (base);
  g_assert ((g_stat (path, &dummy) != 0) || (g_unlink (path) == 0));
  g_free (path);
  /* end of cleanup block */

  start_dummy_xmpp_server (&test->server_parameters);
  setup_dummy_dns_entries (test);

  ca = test->client.options.ca ? test->client.options.ca : TLS_CA_CRT_FILE;

  if (test->client.options.host && test->client.options.port == 0)
    test->client.options.port = test->server_parameters.port;

  /* insecure tls cert/etc not yet implemented */
  handler = wocky_tls_handler_new (test->client.options.lax_ssl);

  wcon = g_object_new ( WOCKY_TYPE_CONNECTOR,
      "jid"                     , test->client.auth.jid,
      "password"                , test->client.auth.pass,
      "xmpp-server"             , test->client.options.host,
      "xmpp-port"               , test->client.options.port,
      "tls-required"            , test->client.require_tls,
      "encrypted-plain-auth-ok" , !test->client.auth.secure,
      /* this refers to PLAINTEXT vs CRYPT, not PLAIN vs DIGEST */
      "plaintext-auth-allowed"  , !test->client.auth.tls,
      "legacy"                  , test->client.options.jabber,
      "old-ssl"                 , test->client.options.ssl,
      "tls-handler"             , handler,
      NULL);

  /* Make sure we only use the test CAs, not system-wide ones. */
  wocky_tls_handler_forget_cas (handler);
  g_assert (wocky_tls_handler_get_cas (handler) == NULL);

  /* check if the cert paths are valid */
  g_assert (g_file_test (TLS_CA_CRT_FILE, G_FILE_TEST_EXISTS));

  wocky_tls_handler_add_ca (handler, ca);

  /* not having a CRL can expose a bug in the openssl error handling
   * (basically we get 'CRL not fetched' instead of 'Expired'):
   * The bug has been fixed, but we can keep checking for it by
   * dropping the CRLs when the test is for an expired cert */
  if (test->server_parameters.cert != CERT_EXPIRED)
    wocky_tls_handler_add_crl (handler, TLS_CRL_DIR);

  g_object_unref (handler);

  test->connector = wcon;
  g_idle_add (start_test, test);

#ifdef G_OS_UNIX
  /* set TCP_NODELAY as soon as possible */
  g_signal_connect (test->connector, "connection-established",
      G_CALLBACK (connection_established_cb), NULL);
#endif

  g_main_loop_run (mainloop);

  if (test->result.domain == S_NO_ERROR)
    {
      if (error != NULL)
        fprintf (stderr, "Error: %s.%d: %s\n",
            g_quark_to_string (error->domain),
            error->code,
            error->message);
      g_assert_no_error (error);

      if (test->client.op >= 0)
        {
          g_assert (test->result.xmpp != NULL);

          /* make sure we selected the right auth mechanism */
          if (test->result.mech != NULL)
            {
              g_assert_cmpstr (test->result.mech, ==,
                  test->result.used_mech);
            }

          /* we got a JID back, I hope */
          g_assert (test->result.jid != NULL);
          g_assert (*test->result.jid != '\0');
          g_free (test->result.jid);

          /* we got a SID back, I hope */
          g_assert (test->result.sid != NULL);
          g_assert (*test->result.sid != '\0');
          g_free (test->result.sid);
        }
   }
  else
    {
      g_assert (test->result.xmpp == NULL);

      if (test->result.domain != S_ANY_ERROR)
        {
          /* We want the error to match either of result.code or
           * result.fallback_code, but don't care which.
           * The expected error domain is the same for either code.
           */
          if (error->code == test->result.fallback_code)
            g_assert_error (error, map_static_domain (test->result.domain),
              test->result.fallback_code);
          else
            g_assert_error (error, map_static_domain (test->result.domain),
              test->result.code);
        }
    }

  if (wcon != NULL)
    g_object_unref (wcon);

  if (error != NULL)
    g_error_free (error);

  if (test->result.xmpp != NULL)
    g_object_unref (test->result.xmpp);

  g_free (test->result.used_mech);
  error = NULL;
}

int
main (int argc,
    char **argv)
{
  int i;
  gchar *base;
  gchar *path = NULL;
  struct stat dummy;
  int result;

  test_init (argc, argv);

  /* hook up the fake DNS resolver that lets us divert A and SRV queries *
   * into our local cache before asking the real DNS                     */
  original = g_resolver_get_default ();
  kludged = g_object_new (TEST_TYPE_RESOLVER, "real-resolver", original, NULL);
  g_resolver_set_default (kludged);

  /* unlink the sasl db, we want to test against a fresh one */
  base = g_get_current_dir ();
  path = g_strdup_printf ("%s/%s", base, SASL_DB_NAME);
  g_free (base);
  g_assert ((g_stat (path, &dummy) != 0) || (g_unlink (path) == 0));
  g_free (path);

  mainloop = g_main_loop_new (NULL, FALSE);

#ifdef HAVE_LIBSASL2

  for (i = 0; tests[i].desc != NULL; i++)
    g_test_add_data_func (tests[i].desc, &tests[i], (test_func)run_test);

#else

  g_message ("libsasl2 not found: skipping SCRAM SASL tests");
  for (i = 0; tests[i].desc != NULL; i++)
    {
      if (!wocky_strdiff (tests[i].result.mech, DEFAULT_SASL_MECH))
        continue;
      g_test_add_data_func (tests[i].desc, &tests[i], (test_func)run_test);
    }

#endif

  result = g_test_run ();
  test_deinit ();
  return result;
}
