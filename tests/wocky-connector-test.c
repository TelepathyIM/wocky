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
#define G_LOG_DOMAIN "wocky-connector-test"

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

#define PORT_XMPP 5222
#define PORT_NONE 0

#define CONNECTOR_INTERNALS_TEST "/connector/basic/internals"

#define OK 0
#define CONNECTOR_OK { OK, OK, OK, OK, OK, OK }

static GError *error = NULL;
static GResolver *original;
static GResolver *kludged;
static GMainLoop *mainloop;

enum {
  OP_CONNECT = 0,
  OP_REGISTER,
  OP_CANCEL,
};

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
  gboolean ok;
} test_t;

static void _set_connector_email_prop (test_t *test)
{
  g_object_set (G_OBJECT (test->connector), "email", "foo@bar.org", NULL);
}

ServerParameters see_other_host_extra_server =
  { { TLS, NULL },
    { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
    { "moose", "something" },
    8222 };

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

    { CONNECTOR_INTERNALS_TEST,
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", FALSE, NOTLS },
        { NULL, 0 } } },

    { "/connector/see-other-host",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_SEE_OTHER_HOST, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_STANDARD, &see_other_host_extra_server },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* No SRV or connect host specified */
    { "/connector/basic/noserv/nohost/noport",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* No SRV or connect host specified, connect port specified */
    { "/connector/basic/noserv/nohost/port",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        8222 },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 8222 } } },

    /* No SRV or connect host specified, bad port specified: FAIL */
    { "/connector/basic/noserv/nohost/duffport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        8222 },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 8221 } } },

    /* No SRV record, connect host specified */
    { "/connector/basic/noserv/host/noport",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "schadenfreude.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { "schadenfreude.org", 0 } } },

    /* No SRV record, connect host and port specified */
    { "/connector/basic/noserv/host/port",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5555 },
      { NULL, 0, "meerkats.net", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { "meerkats.net", 5555 } } },

    /* No SRV record, connect host and bad port specified: FAIL */
    { "/connector/basic/noserv/host/duffport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5555 },
      { NULL, 0, "meerkats.net", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { "meerkats.net", 5554 } } },

    /* No SRV record, bad connect host: FAIL */
    { "/connector/basic/noserv/duffhost/noport",
      NOISY,
      { S_G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { NULL, NULL },
        PORT_NONE },
      { NULL, 0, NULL, NULL, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { DUFF_H0ST, 0 } } },

    /* No SRV record, bad connect host, port specified: FAIL */
    { "/connector/basic/noserv/duffhost/port",
      NOISY,
      { S_G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { NULL, NULL },
        PORT_NONE },
      { NULL, 0, NULL, NULL, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { "still.no_such_host.at.all", 23 } } },

    /* SRV record specified */
    { "/connector/basic/serv/nohost/noport",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5050 },
      { "weasel-juice.org", 5050, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* SRV record specified, port specified: ignore SRV and connect */
    { "/connector/basic/serv/nohost/port",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5051 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 5051 } } },

    /* SRV record specified, bad port: ignore SRV and FAIL */
    { "/connector/basic/serv/nohost/duffport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5051 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 5050 } } },

    /* SRV record, connect host specified: use connect host */
    { "/connector/basic/serv/host/noport",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { VISIBLE_HOST, 0 } } },

    /* SRV, connect host and port specified: use host and port */
    { "/connector/basic/serv/host/port",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5656 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { VISIBLE_HOST, 5656 } } },

    /* SRV record, connect host, bad port: ignore SRV, FAIL */
    { "/connector/basic/serv/host/duffport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5656 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { VISIBLE_HOST, 5655 } } },

    /* SRV record, bad connect host: use bad host and FAIL */
    { "/connector/basic/serv/duffhost/noport",
      NOISY,
      { S_G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { DUFF_H0ST, 0 } } },

    /* SRV record, bad connect host, connect port: use bad host and FAIL */
    { "/connector/basic/serv/duffhost/port",
      NOISY,
      { S_G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { DUFF_H0ST, PORT_XMPP } } },

    /* Facebook Chat has a broken SRV record: you ask for
     * _xmpp-client._tcp.chat.facebook.com, and it gives you back a CNAME! So
     * g_socket_client_connect_to_service() fails. But as it happens the real
     * result should have just been chat.facebook.com anyway, so Wocky tries to
     * fall back to that.
     *
     * So this test has a fake SRV record for an unreachable server, but
     * expects to succeed because it's listening on the default XMPP port on
     * our hypothetical 'weasel-juice.org'.
     */
    { "/connector/basic/facebook-chat-srv-workaround",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* Further to the above test, this one tests the case where the fallback
     * doesn't work either. The server isn't listening anywhere (that's the
     * PORT_NONE in the server_parameters sub-struct), and thud.org (the result
     * of the SRV lookup) is unreachable. So the connection should fail.
     */
    { "/connector/basic/duffserv/nohost/noport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_NETWORK_UNREACHABLE, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_NONE },
      { "not.an.xmpp.server", PORT_XMPP, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@not.an.xmpp.server", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* Bad SRV record, port specified, ignore SRV and connect to domain host */
    { "/connector/basic/duffserv/nohost/port",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5050 },
      { "weasel-juice.org", PORT_XMPP, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 5050 } } },

    /* Bad SRV record, bad port specified, ignore SRV and FAIL */
    { "/connector/basic/duffserv/nohost/duffport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5050 },
      { "weasel-juice.org", PORT_XMPP, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 5049 } } },

    /* Bad SRV record, connect host specified, ignore SRV */
    { "/connector/basic/duffserv/host/noport",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { VISIBLE_HOST, 0 } } },

    /* Bad SRV record, connect host and port given: ignore SRV */
    { "/connector/basic/duffserv/host/port",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5151 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { VISIBLE_HOST, 5151 } } },

    /* Bad SRV record, connect host and bad port, ignore SRV and FAIL */
    { "/connector/basic/duffserv/host/duffport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5151 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { VISIBLE_HOST, 5149 } } },

    /* Bad SRV record, bad host and bad port: Just FAIL */
    { "/connector/basic/duffserv/duffhost/noport",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_NETWORK_UNREACHABLE, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { INVISIBLE_HOST, 0 } } },

    /*Bad SRV and connect host, ignore SRV and FAIL */
    { "/connector/basic/duffserv/duffhost/port",
      NOISY,
      { S_G_IO_ERROR, G_IO_ERROR_NETWORK_UNREACHABLE, G_IO_ERROR_FAILED },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        5151 },
      { "weasel-juice.org", 5050, "thud.org", UNREACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { INVISIBLE_HOST, 5151 } } },

    /* ******************************************************************* *
     * that's it for the basic DNS/connection-logic tests                  *
     * now onto the post-tcp-connect stages:                               */
    { "/connector/auth/secure/no-tlsplain/notls/nodigest",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { NOTLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/connector/auth/secure/no-tlsplain/notls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/no-tlsplain/notls/nodigest",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { NOTLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/no-tlsplain/notls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* *************************************************************** *
     * This block of tests will fail as we don't advertise TLS support */
    { "/connector/auth/insecure/no-tlsplain/notls/any",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE, -1 },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/tlsplain/notls/any",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE, -1 },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/auth/secure/no-tlsplain/notls/any",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE, -1 },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", TRUE, TLS },
        { NULL, 0 } } },

    { "/connector/auth/secure/tlsplain/notls/any",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_TLS_UNAVAILABLE, -1 },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { TLS_REQUIRED,
        { "moose@weasel-juice.org", "something", TRUE, NOTLS },
        { NULL, 0 } } },

    /* **************************************************************** *
     * this will be a mix of failures and sucesses depending on whether *
     * we allow plain auth or not                                       */
    { "/connector/auth/secure/no-tlsplain/tls/plain",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_INVALID_PASSWORD, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/connector/auth/secure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/no-tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/secure/no-tlsplain/tls/plain",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/secure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/insecure/no-tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/insecure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },
    /* **************************************************************** *
     * these should all be digest auth successes                        */
    { "/connector/auth/secure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/connector/auth/secure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0 } } },

    { "/connector/auth/insecure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/secure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/secure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/insecure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0 } } },

    { "/connector/tls+auth/insecure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* ***************************************************************** *
     * SASL problems                                                     */
    { "/connector/problem/sasl/bad-pass",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_INVALID_PASSWORD, CONNECTOR_OK },
        { "foo", "bar" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "foo@weasel-juice.org", "notbar", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/sasl/bad-user",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_INVALID_USERNAME, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "caribou@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/sasl/no-sasl",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_SASL, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/sasl/no-mechanisms",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_MECHANISMS, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/sasl/bad-mechanism",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "omg-poniez" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* ********************************************************************* */
    /* TLS error conditions */
    { "/connector/problem/tls/refused",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_TLS_REFUSED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_TLS_REFUSED, OK, OK, OK, OK} },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* ********************************************************************* *
     * Invalid JID                                                           */
    { "/connector/problem/jid/invalid",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BAD_JID, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_NONE },
      { NULL, 0, "thud.org", REACHABLE },
      { PLAINTEXT_OK,
        { "bla@h@_b&la<>h", "something", PLAIN, NOTLS },
        { "weasel-juice.org", PORT_XMPP } } },

    { "/connector/problem/jid/domainless",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BAD_JID, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_NONE },
      { "weasel-juice.org", 5001, "thud.org", REACHABLE, REACHABLE },
      { PLAINTEXT_OK,
        { "moose@", "something", PLAIN, NOTLS },
        { "weasel-juice.org", 0 } } },

    /* ********************************************************************* *
     * XMPP errors                                                           */
    { "/connector/problem/xmpp/version/0.x",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
          WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER, -1 },
      { { TLS, NULL, "0.9" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* we actually tolerate > 1.0 versions */
    { "/connector/problem/xmpp/version/1.x",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL, "1.1" },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/error/host-unknown",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR,
        WOCKY_XMPP_STREAM_ERROR_HOST_UNKNOWN, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OTHER_HOST, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/error/tls-load",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR,
        WOCKY_XMPP_STREAM_ERROR_RESOURCE_CONSTRAINT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_TLS_LOAD, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/error/bind-conflict",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR, WOCKY_XMPP_STREAM_ERROR_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_CLASH, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/error/session-fail",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR,
        WOCKY_XMPP_STREAM_ERROR_RESOURCE_CONSTRAINT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, SESSION_PROBLEM_NO_SESSION, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/features",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BAD_FEATURES, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_FEATURES, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE */
    { "/connector/problem/xmpp/no-bind",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_CANNOT_BIND, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_INVALID      */
    { "/connector/problem/xmpp/bind/invalid",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_INVALID, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_INVALID, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_DENIED     */
    { "/connector/problem/xmpp/bind/denied",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_DENIED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_DENIED, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_CONFLICT      */
    { "/connector/problem/xmpp/bind/conflict",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_CONFLICT, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_REJECTED    */
    { "/connector/problem/xmpp/bind/rejected",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_REJECTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_REJECTED, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_FAILED    */
    { "/connector/problem/xmpp/bind/failed",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_FAILED, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/bind/nonsense",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_NONSENSE, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/bind/no-jid",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, BIND_PROBLEM_NO_JID, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/session/none",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_NO_SESSION, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_FAILED   */
    { "/connector/problem/xmpp/session/failed",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, SESSION_PROBLEM_FAILED, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_DENIED   */
    { "/connector/problem/xmpp/session/denied",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_DENIED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, SESSION_PROBLEM_DENIED, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT */
    { "/connector/problem/xmpp/session/conflict",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, SESSION_PROBLEM_CONFLICT, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_REJECTED */
    { "/connector/problem/xmpp/session/rejected",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_REJECTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, SESSION_PROBLEM_REJECTED, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/problem/xmpp/session/nonsense",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, SESSION_PROBLEM_NONSENSE, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/econnreset/server-start",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, SERVER_DEATH_SERVER_START, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/econnreset/client-open",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, SERVER_DEATH_CLIENT_OPEN, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/econnreset/server-open",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, SERVER_DEATH_SERVER_OPEN, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/econnreset/features",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, SERVER_DEATH_FEATURES, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/econnreset/tls-negotiate",
      QUIET,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, SERVER_DEATH_TLS_NEG, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },
    /* ******************************************************************** */
    /* quirks                                                               */
    { "/connector/google/domain-discovery/require",
      QUIET,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_REQUIRE_GOOGLE_JDD, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    { "/connector/google/domain-discovery/dislike",
      QUIET,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_DISLIKE_GOOGLE_JDD, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 } } },

    /* ******************************************************************** */
    /* XEP 0077                                                             */
    { "/connector/xep77/register/ok",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/no-args",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_EMPTY, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_NO_ARGS } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/email-missing",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_EMAIL_ARG } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/unknown-arg",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_UNSUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_STRANGE_ARG } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/unknown+email-args",
      NOISY,
      { S_ANY_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK,
            XEP77_PROBLEM_STRANGE_ARG|XEP77_PROBLEM_EMAIL_ARG } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/email-arg-ok",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_EMAIL_ARG } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER,
        (test_setup)_set_connector_email_prop } },

    { "/connector/xep77/register/email-arg-ok/unknown-arg",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_UNSUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK,
            XEP77_PROBLEM_EMAIL_ARG|XEP77_PROBLEM_STRANGE_ARG } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER,
        (test_setup)_set_connector_email_prop } },

    { "/connector/xep77/register/fail/conflict",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_FAIL_CONFLICT } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/fail/other",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_REJECTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_FAIL_REJECTED } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/nonsense",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_QUERY_NONSENSE } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/already/get",
      NOISY,
      { S_NO_ERROR, 0 , 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_QUERY_ALREADY } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/already/set",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_ALREADY } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },

    { "/connector/xep77/register/not-available",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_REGISTRATION_UNAVAILABLE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_NOT_AVAILABLE } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_REGISTER } },
    /* ******************************************************************** */
    { "/connector/xep77/cancel/ok",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_CANCEL } },

    { "/connector/xep77/cancel/denied",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_CANCEL_FAILED } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_CANCEL } },

    { "/connector/xep77/cancel/disabled",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_UNREGISTER_DENIED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_CANCEL_DISABLED } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_CANCEL } },

    { "/connector/xep77/cancel/rejected",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_UNREGISTER_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_CANCEL_REJECTED } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_CANCEL } },

    { "/connector/xep77/cancel/stream-closed",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { OK, OK, OK, OK, OK, XEP77_PROBLEM_CANCEL_STREAM } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0 },
        OP_CANCEL } },

    /* ******************************************************************** */
    /* old school jabber tests (pre XMPP 1.0)                               */
    { "/connector/jabber/no-ssl/auth/digest",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/reject",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, OK } },
        { "moose", "blerg" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/unavailable",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, JABBER_PROBLEM_AUTH_NIH } },
        { "moose", "blerg" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/bind-error",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_RESOURCE_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, JABBER_PROBLEM_AUTH_BIND } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/incomplete",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_CREDENTIALS, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK,
            JABBER_PROBLEM_AUTH_PARTIAL } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/failure",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, JABBER_PROBLEM_AUTH_FAILED } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/bizarre",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_INVALID_REPLY, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK,
            JABBER_PROBLEM_AUTH_STRANGE } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/nonsense",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_INVALID_REPLY, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK,
            JABBER_PROBLEM_AUTH_NONSENSE } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/no-mechs",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "none" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/plain",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, "password" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/plain/rejected",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED, -1 },
      { { TLS, "password" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, JABBER_PROBLEM_AUTH_REJECT } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/digest/rejected",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED, -1 },
      { { TLS, "digest" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER, OK, OK, OK, JABBER_PROBLEM_AUTH_REJECT } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/old+sasl",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_AUTH_FEATURE, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    { "/connector/jabber/no-ssl/auth/old-sasl",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_SASL,
          { XMPP_PROBLEM_OLD_AUTH_FEATURE, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER } } },

    /* ******************************************************************** */
    /* old SSL                                                              */
    { "/connector/jabber/ssl/auth/digest",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/reject",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "blerg" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/unavailable",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
              JABBER_PROBLEM_AUTH_NIH } },
        { "moose", "blerg" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/bind-error",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_RESOURCE_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
              JABBER_PROBLEM_AUTH_BIND } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/incomplete",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_CREDENTIALS, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
            JABBER_PROBLEM_AUTH_PARTIAL } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/failure",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
              JABBER_PROBLEM_AUTH_FAILED } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/bizarre",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_INVALID_REPLY, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
            JABBER_PROBLEM_AUTH_STRANGE } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/nonsense",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_INVALID_REPLY, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
            JABBER_PROBLEM_AUTH_NONSENSE } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/no-mechs",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "none" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/plain",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, "password" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/plain/rejected",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED, -1 },
      { { TLS, "password" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
            JABBER_PROBLEM_AUTH_REJECT } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/digest/rejected",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_AUTHORIZED, -1 },
      { { TLS, "digest" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SERVER|XMPP_PROBLEM_OLD_SSL, OK, OK, OK,
            JABBER_PROBLEM_AUTH_REJECT } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/old+sasl",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
          { SERVER_PROBLEM_NO_PROBLEM,
            { XMPP_PROBLEM_OLD_AUTH_FEATURE|XMPP_PROBLEM_OLD_SSL,
              OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector/jabber/ssl/auth/old-sasl",
      NOISY,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_SASL,
          { XMPP_PROBLEM_OLD_AUTH_FEATURE|XMPP_PROBLEM_OLD_SSL,
            OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    /* ******************************************************************* */
    /* duplicate earlier blocks of tests, but with old SSL                 */
    { "/connector+ssl/auth/secure/no-tlsplain/notls/nodigest",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { NOTLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0, OLD_JABBER, OLD_SSL } } },

    { "/connector+ssl/auth/secure/no-tlsplain/notls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/insecure/no-tlsplain/notls/nodigest",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { NOTLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/insecure/no-tlsplain/notls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { NOTLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* **************************************************************** *
     * this will be a mix of failures and sucesses depending on whether *
     * we allow plain auth or not                                       */
    { "/connector+ssl/auth/secure/no-tlsplain/tls/plain",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_INVALID_PASSWORD,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/secure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/insecure/no-tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/insecure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/secure/no-tlsplain/tls/plain",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/secure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/insecure/no-tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/insecure/tlsplain/tls/plain",
      NOISY,
      { S_NO_ERROR, 0, 0, "PLAIN" },
      { { TLS, "PLAIN" },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* **************************************************************** *
     * these should all be digest auth successes                        */
    { "/connector+ssl/auth/secure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/secure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/insecure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/auth/insecure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/secure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/secure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, TLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/insecure/no-tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", DIGEST, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/tls+auth/insecure/tlsplain/tls/digest",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* ***************************************************************** *
     * SASL problems                                                     */
    { "/connector+ssl/problem/sasl/bad-pass",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_INVALID_PASSWORD,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "foo", "bar" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "foo@weasel-juice.org", "notbar", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/sasl/bad-user",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_INVALID_USERNAME,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "caribou@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/sasl/no-sasl",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_SASL, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/sas/no-mechanisms",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_MECHANISMS,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/sasl/bad-mechanism",
      NOISY,
      { S_WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS, -1 },
      { { TLS, "omg-poniez" },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/version/0.x",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR,
        WOCKY_CONNECTOR_ERROR_NON_XMPP_V1_SERVER, -1 },
      { { TLS, NULL, "0.9" },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* we actually tolerate > 1.0 versions */
    { "/connector+ssl/problem/xmpp/version/1.x",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL, "1.1" },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/error/host-unknown",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR, WOCKY_XMPP_STREAM_ERROR_HOST_UNKNOWN, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OTHER_HOST|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/error/bind-conflict",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR, WOCKY_XMPP_STREAM_ERROR_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_CLASH, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/error/session-fail",
      NOISY,
      { S_WOCKY_XMPP_STREAM_ERROR,
        WOCKY_XMPP_STREAM_ERROR_RESOURCE_CONSTRAINT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, SESSION_PROBLEM_NO_SESSION, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/features",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BAD_FEATURES, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_FEATURES|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE */
    { "/connector+ssl/problem/xmpp/no-bind",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_UNAVAILABLE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_CANNOT_BIND|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_INVALID      */
    { "/connector+ssl/problem/xmpp/bind/invalid",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_INVALID, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
            { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_INVALID, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_DENIED     */
    { "/connector+ssl/problem/xmpp/bind/denied",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_DENIED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_DENIED, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_CONFLICT      */
    { "/connector+ssl/problem/xmpp/bind/conflict",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_CONFLICT, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_REJECTED    */
    { "/connector+ssl/problem/xmpp/bind/rejected",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_REJECTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_REJECTED, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_BIND_FAILED    */
    { "/connector+ssl/problem/xmpp/bind/failed",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_FAILED, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/bind/nonsense",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_BIND_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_NONSENSE, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/bind/no-jid",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, BIND_PROBLEM_NO_JID, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/session/none",
      NOISY,
      { S_NO_ERROR, 0, 0, "DIGEST-MD5" },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_NO_SESSION|XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_FAILED   */
    { "/connector+ssl/problem/xmpp/session/failed",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, SESSION_PROBLEM_FAILED, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_DENIED   */
    { "/connector+ssl/problem/xmpp/session/denied",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_DENIED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, SESSION_PROBLEM_DENIED, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT */
    { "/connector+ssl/problem/xmpp/session/conflict",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_CONFLICT, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, SESSION_PROBLEM_CONFLICT, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* WOCKY_CONNECTOR_ERROR_SESSION_REJECTED */
    { "/connector+ssl/problem/xmpp/session/rejected",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_REJECTED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, SESSION_PROBLEM_REJECTED, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/problem/xmpp/session/nonsense",
      NOISY,
      { S_WOCKY_CONNECTOR_ERROR, WOCKY_CONNECTOR_ERROR_SESSION_FAILED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, SESSION_PROBLEM_NONSENSE, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/econnreset/server-start",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, SERVER_DEATH_SERVER_START, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/econnreset/client-open",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, SERVER_DEATH_CLIENT_OPEN, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/econnreset/server-open",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, SERVER_DEATH_SERVER_OPEN, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/econnreset/features",
      NOISY,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, SERVER_DEATH_FEATURES, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector+ssl/econnreset/ssl-negotiate",
      QUIET,
      { S_ANY_ERROR, 0 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM,
          { XMPP_PROBLEM_OLD_SSL, OK, OK, SERVER_DEATH_TLS_NEG, OK } },
        { "moose", "something" },
        PORT_XMPP },
      { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
      { PLAINTEXT_OK,
        { "moose@weasel-juice.org", "something", PLAIN, NOTLS },
        { NULL, 0, XMPP_V1, OLD_SSL } } },

    /* ********************************************************************* */
    /* certificate verification tests                                        */
    { "/connector/cert-verification/tls/nohost/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/multica-verification/tls/nohost/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/tls/host/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "thud.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { "thud.org", 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/nohost/fail/name-mismatch",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { "tomato-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/host/fail/name-mismatch",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "tomato-juice.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { "tomato-juice.org", 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/expired/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_EXPIRED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_EXPIRED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/inactive/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NOT_ACTIVE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_NOT_YET },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/selfsigned/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_INVALID, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_SELFSIGN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/unknown/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_SIGNER_UNKNOWN, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_UNKNOWN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    /* This is a combination of the above test
     * (/connector/cert-verification/tls/unknown/fail) and
     * /connector/cert-verification/tls/host/fail/name-mismatch. It checks that
     * Wocky considers a hostname mismatch more erroneous than the certificate
     * being broken.
     */
    { "/connector/cert-verification/tls/host/fail/name-mismatch-and-unknown",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_UNKNOWN },
        { "tomato-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/cert-verification/tls/wildcard/ok",
      QUIET,
      { S_NO_ERROR },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_WILDCARD },
        { "foo.weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@foo.weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/tls/wildcard/level-mismatch/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_WILDCARD },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/tls/wildcard/glob-mismatch/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_WILDCARD },
        { "foo.diesel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@foo.diesel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/tls/bad-wildcard/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_BADWILD },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/tls/revoked/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_REVOKED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_REVOKED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/tls/revoked/lenient/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_REVOKED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_REVOKED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT, TLS_CA_DIR } } },

    /* ********************************************************************* */
    /* as above but with legacy ssl                                          */
    { "/connector/cert-verification/ssl/nohost/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/host/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@thud.org", "something", PLAIN, TLS },
          { "weasel-juice.org", PORT_XMPP, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/nohost/fail/name-mismatch",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { "tomato-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/host/fail/name-mismatch",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "tomato-juice.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { "tomato-juice.org", 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/expired/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_EXPIRED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_EXPIRED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/inactive/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NOT_ACTIVE, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_NOT_YET },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/selfsigned/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_INVALID, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_SELFSIGN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/unknown/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_SIGNER_UNKNOWN, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_UNKNOWN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL } } },

    { "/connector/cert-verification/ssl/wildcard/ok",
      QUIET,
      { S_NO_ERROR },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_WILDCARD },
        { "foo.weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@foo.weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/ssl/wildcard/level-mismatch/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_WILDCARD },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/ssl/wildcard/glob-mismatch/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_WILDCARD },
        { "foo.diesel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@foo.diesel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/ssl/bad-wildcard/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_NAME_MISMATCH, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_BADWILD },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/ssl/revoked/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_REVOKED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_REVOKED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_STRICT, TLS_CA_DIR } } },

    { "/connector/cert-verification/ssl/revoked/lenient/fail",
      QUIET,
      { S_WOCKY_TLS_CERT_ERROR, WOCKY_TLS_CERT_REVOKED, -1 },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_REVOKED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { TLS_REQUIRED,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT, TLS_CA_DIR } } },

    /* ********************************************************************* */
    /* certificate non-verification tests                                    */
    { "/connector/cert-nonverification/tls/nohost/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1 } } },

    { "/connector/cert-nonverification/tls/host/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "thud.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { "thud.org", 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/tls/nohost/ok/name-mismatch",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { "tomato-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/tls/host/ok/name-mismatch",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "tomato-juice.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { "tomato-juice.org", 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/tls/expired/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_EXPIRED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/tls/inactive/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_NOT_YET },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/tls/selfsigned/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_SELFSIGN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/tls/unknown/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, CONNECTOR_OK },
        { "moose", "something" },
        PORT_XMPP, CERT_UNKNOWN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, STARTTLS, CERT_CHECK_LENIENT } } },

    /* ********************************************************************* */
    /* as above but with legacy ssl                                          */
    { "/connector/cert-nonverification/ssl/nohost/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/host/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "weasel-juice.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@thud.org", "something", PLAIN, TLS },
          { "weasel-juice.org", PORT_XMPP, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/nohost/ok/name-mismatch",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { "tomato-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/host/ok/name-mismatch",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP },
        { NULL, 0, "tomato-juice.org", REACHABLE, NULL },
        { PLAINTEXT_OK,
          { "moose@tomato-juice.org", "something", PLAIN, TLS },
          { "tomato-juice.org", 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/expired/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_EXPIRED },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/inactive/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_NOT_YET },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/selfsigned/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_SELFSIGN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    { "/connector/cert-nonverification/ssl/unknown/ok",
      QUIET,
      { S_NO_ERROR, },
      { { TLS, NULL },
        { SERVER_PROBLEM_NO_PROBLEM, { XMPP_PROBLEM_OLD_SSL, OK, OK, OK, OK } },
        { "moose", "something" },
        PORT_XMPP, CERT_UNKNOWN },
        { "weasel-juice.org", PORT_XMPP, "thud.org", REACHABLE, UNREACHABLE },
        { PLAINTEXT_OK,
          { "moose@weasel-juice.org", "something", PLAIN, TLS },
          { NULL, 0, XMPP_V1, OLD_SSL, CERT_CHECK_LENIENT } } },

    /* we are done, cap the list: */
    { NULL }
  };

/* ************************************************************************* */
#define STRING_OK(x) (((x) != NULL) && (*x != '\0'))

static void
setup_dummy_dns_entries (const test_t *test)
{
  TestResolver *tr = TEST_RESOLVER (kludged);
  guint port = test->dns.port ? test->dns.port : PORT_XMPP;
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

      /* Run until server is down */
      test_connector_server_teardown (srv->server,
        test_server_teardown_cb, loop);
      g_main_loop_run (loop);

      g_clear_object (&srv->server);
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
  WockyConnector *wcon = WOCKY_CONNECTOR (source);
  WockyXmppConnection *conn = NULL;

  error = NULL;

  switch (test->client.op)
    {
      case OP_CONNECT:
        conn = wocky_connector_connect_finish (wcon, res,
            &test->result.jid, &test->result.sid, &error);
        test->ok = (conn != NULL);
        break;
      case OP_REGISTER:
        conn = wocky_connector_register_finish (wcon, res,
            &test->result.jid, &test->result.sid, &error);
        test->ok = (conn != NULL);
        break;
      case OP_CANCEL:
        test->ok = wocky_connector_unregister_finish (wcon, res, &error);
        break;
    }

  if (conn != NULL)
    test->result.xmpp = g_object_ref (conn);

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

static gboolean
start_test (gpointer data)
{
  test_t *test = data;

  if (test->client.setup != NULL)
    (test->client.setup) (test);

  switch (test->client.op)
    {
      case OP_CONNECT:
        wocky_connector_connect_async (test->connector, NULL,
            test_done, data);
        break;
      case OP_REGISTER:
        wocky_connector_register_async (test->connector, NULL,
            test_done, data);
        break;
      case OP_CANCEL:
        wocky_connector_unregister_async (test->connector, NULL,
            test_done, data);
        break;
    }
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

      if (test->client.op == OP_CANCEL)
        {
          g_assert (test->ok == TRUE);
          g_assert (test->result.xmpp == NULL);
        }
      else
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

      /* property get/set functionality */
      if (!strcmp (test->desc, CONNECTOR_INTERNALS_TEST))
        {
          int i;
          gchar *identity, *session_id, *resource;
          WockyConnector *tmp =
            wocky_connector_new ("foo@bar.org", "abc", "xyz", NULL, NULL);
          WockyStanza *feat = NULL;
          gboolean jabber;
          gboolean oldssl;
          XmppProblem xproblem = test->server_parameters.problem.conn.xmpp;
          const gchar *prop = NULL;
          const gchar *str_prop[] = { "jid", "password",
                                      "xmpp-server", "email", NULL };
          const gchar *str_vals[] = { "abc", "PASSWORD",
                                      "xmpp.server", "e@org", NULL };
          const gchar *boolprop[] = { "plaintext-auth-allowed",
                                      "encrypted-plain-auth-ok",
                                      "tls-required",
                                      NULL };

          g_object_get (wcon, "identity", &identity, "features", &feat, NULL);
          g_assert (identity != NULL);
          g_assert (*identity != '\0');
          g_assert (feat != NULL);
          g_assert (G_OBJECT_TYPE (feat) == WOCKY_TYPE_STANZA);
          g_free (identity);
          g_object_unref (feat);
          identity = NULL;

          g_object_get (wcon, "session-id", &session_id, NULL);
          g_assert (session_id != NULL);
          g_assert (*session_id != '\0');
          g_free (session_id);

          g_object_get (wcon, "resource", &resource, NULL);
          /* TODO: really? :resource gets updated to contain the actual
           * post-bind resource, but perhaps :resource should be updated too?
           */
          g_assert_cmpstr (resource, ==, NULL);
          g_free (resource);

          g_object_get (wcon, "legacy", &jabber, "old-ssl", &oldssl, NULL);
          g_assert (jabber == (gboolean)(xproblem & XMPP_PROBLEM_OLD_SERVER));
          g_assert (oldssl == (gboolean)(xproblem & XMPP_PROBLEM_OLD_SSL));

          for (i = 0, prop = str_prop[0]; prop; prop = str_prop[++i])
            {
              gchar *val = NULL;
              g_object_set (tmp, prop, str_vals[i], NULL);
              g_object_get (tmp, prop, &val, NULL);
              g_assert (!strcmp (val, str_vals[i]));
              g_assert (val != str_vals[i]);
              g_free (val);
            }

          for (i = 0, prop = boolprop[0]; prop; prop = boolprop[++i])
            {
              gboolean val;
              g_object_set (tmp, prop, TRUE, NULL);
              g_object_get (tmp, prop, &val, NULL);
              g_assert (val);
              g_object_set (tmp, prop, FALSE, NULL);
              g_object_get (tmp, prop, &val, NULL);
              g_assert (!val);
            }

          g_object_set (tmp, "xmpp-port", 31415, NULL);
          g_object_get (tmp, "xmpp-port", &i, NULL);
          g_assert (i == 31415);

          g_object_unref (tmp);
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

  g_message ("libsasl2 not found: skipping MD5 SASL tests");
  for (i = 0; tests[i].desc != NULL; i++)
    {
      if (!wocky_strdiff (tests[i].result.mech, "DIGEST-MD5"))
        continue;
      g_test_add_data_func (tests[i].desc, &tests[i], (test_func)run_test);
    }

#endif

  result = g_test_run ();
  test_deinit ();
  return result;
}
