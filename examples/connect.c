#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <glib.h>

#include <gio/gnio.h>
#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-sasl-auth.h>

typedef enum {
  INITIAL,
  SSL,
  SSL_DONE,
  SASL,
  DONE,
} State;


State state = INITIAL;
GMainLoop *mainloop;
WockyXmppConnection *conn;
const gchar *server;
const gchar *username;
const gchar *password;
WockySaslAuth *sasl = NULL;
GTcpConnection *tcp;

GTcpClient *client;
GTLSConnection *ssl;
GTLSSession *ssl_session;

static void
conn_parse_error(WockyXmppConnection *connection, gpointer user_data) {
  fprintf(stderr, "PARSE ERROR\n");
  exit(1);
}

static void
conn_stream_opened(WockyXmppConnection *connection, 
              const gchar *to, const gchar *from, const gchar *version,
              gpointer user_data) {
  printf("Stream opened -- from: %s version: %s\n", from, version);
  if (version == NULL || strcmp(version, "1.0")) {
    printf("Server is not xmpp compliant\n");
    g_main_loop_quit(mainloop);
  }
}

static void
conn_stream_closed(WockyXmppConnection *connection, gpointer user_data) {
  printf("Stream opened\n");
  wocky_xmpp_connection_close(connection);
}

static gchar *
return_str(WockySaslAuth *auth, gpointer user_data) {
  return g_strdup(user_data);
}

static void
auth_success(WockySaslAuth *auth, gpointer user_data) {
  printf("Authentication successfull!!\n");
  state = DONE;
  /* Reopen the connection */
  wocky_xmpp_connection_restart(conn);
  wocky_xmpp_connection_open(conn, server, NULL, "1.0");
}

static void
auth_failed(WockySaslAuth *auth, GQuark domain, 
    int code, gchar *message, gpointer user_data) {
  printf("Authentication failed: %s\n", message);
  g_main_loop_quit(mainloop);
}

static void
start_ssl (WockyXmppConnection *connection, WockyXmppStanza *stanza) {
  GError *error = NULL;

  if (strcmp(stanza->node->name, "proceed") 
    || strcmp(wocky_xmpp_node_get_ns(stanza->node), 
          WOCKY_XMPP_NS_TLS)) {
    printf("Server doesn't want to start tls");
    g_main_loop_quit(mainloop);
  }

  wocky_xmpp_connection_disengage(connection);

  ssl_session = g_tls_session_new (G_IO_STREAM (tcp));
  ssl = g_tls_session_handshake (ssl_session, NULL, &error);

  if (ssl == NULL)
    g_error ("connect: %s: %d, %s", g_quark_to_string (error->domain),
      error->code, error->message);

  wocky_xmpp_connection_restart(connection);
  wocky_xmpp_connection_engage(connection, G_IO_STREAM(ssl));
  wocky_xmpp_connection_open(conn, server, NULL, "1.0");

  state = SSL_DONE;
}

static void
negotiate_ssl(WockyXmppConnection *connection, WockyXmppStanza *stanza) {
  WockyXmppNode *tls;
  WockyXmppStanza *starttls;
  if (strcmp(stanza->node->name, "features") 
    || strcmp(wocky_xmpp_node_get_ns(stanza->node),WOCKY_XMPP_NS_STREAM)) {
    printf("Didn't receive features stanza\n");
    g_main_loop_quit(mainloop);
  }

  tls = wocky_xmpp_node_get_child_ns(stanza->node, "starttls",
                                      WOCKY_XMPP_NS_TLS);
  if (tls == NULL) {
    printf("Server doesn't support tls\n");
    g_main_loop_quit(mainloop);
  }

  starttls = wocky_xmpp_stanza_new("starttls");
  wocky_xmpp_node_set_ns(starttls->node, WOCKY_XMPP_NS_TLS);

  state = SSL;

  g_assert(wocky_xmpp_connection_send(connection, starttls, NULL));
  g_object_unref(starttls);
}

static void
start_sasl_helper(WockyXmppConnection *connection, WockyXmppStanza *stanza) {
  GError *error;

  state = SASL;
  sasl = wocky_sasl_auth_new();
  g_signal_connect(sasl, "username-requested",
                    G_CALLBACK(return_str), (gpointer)username);
  g_signal_connect(sasl, "password-requested",
                    G_CALLBACK(return_str), (gpointer)password);
  g_signal_connect(sasl, "authentication-succeeded",
                    G_CALLBACK(auth_success), NULL);
  g_signal_connect(sasl, "authentication-failed",
                    G_CALLBACK(auth_failed), NULL);

  if (!wocky_sasl_auth_authenticate(sasl, server, 
                                  connection, stanza, TRUE, &error)) {
     printf("Sasl auth start failed: %s\n", error->message);
     g_main_loop_quit(mainloop);
  }
}

static void
conn_received_stanza(WockyXmppConnection *connection,
                WockyXmppStanza *stanza,
                gpointer user_data) {

  switch (state ) {
    case INITIAL:
      negotiate_ssl(connection, stanza);
      break;
    case SSL:
      start_ssl(connection, stanza);
      break;
    case SSL_DONE:
      start_sasl_helper(connection, stanza);
      break;
    case SASL:
    case DONE:
      break;
  }
}

static void
tcp_do_connect(void)
{
  g_assert (tcp != NULL);

  printf ("TCP connection established\n");

  conn = wocky_xmpp_connection_new (G_IO_STREAM (tcp));
  wocky_xmpp_connection_open(conn, server, NULL, "1.0");

  g_signal_connect(conn, "parse-error",
      G_CALLBACK(conn_parse_error), NULL);
  g_signal_connect(conn, "stream-opened",
      G_CALLBACK(conn_stream_opened), NULL);
  g_signal_connect(conn, "stream-closed",
      G_CALLBACK(conn_stream_closed), NULL);
  g_signal_connect(conn, "received-stanza",
      G_CALLBACK(conn_received_stanza), NULL);
}

static void
tcp_host_connected (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;

  tcp = g_tcp_client_connect_to_host_finish (G_TCP_CLIENT (source),
    result, &error);

  if (tcp == NULL)
    {
      g_message ("HOST connect failed: %s: %d, %s\n",
        g_quark_to_string (error->domain),
        error->code, error->message);

      g_error_free (error);
      g_main_loop_quit (mainloop);
    }
  else
    {
      tcp_do_connect();
    }
}

static void
tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;

  tcp = g_tcp_client_connect_to_service_finish (G_TCP_CLIENT (source),
    result, &error);

  if (tcp == NULL)
    {
      g_message ("SRV connect failed: %s: %d, %s",
        g_quark_to_string (error->domain),
        error->code, error->message);

      g_message ("Falling back to direct connection");
      g_error_free (error);
      g_tcp_client_connect_to_host_async (client, server, "5222", NULL,
          tcp_host_connected, NULL);
    }
  else
    {
      tcp_do_connect();
    }
}

int
main(int argc,
    char **argv)
{
  g_type_init();

  g_assert(argc == 4);

  mainloop = g_main_loop_new(NULL, FALSE);

  client =  g_tcp_client_new ();

  server = argv[1];
  username = argv[2];
  password = argv[3];

  printf ("Connecting to %s\n", server);

  g_tcp_client_connect_to_service_async (client, server,
    "xmpp-client", NULL, tcp_srv_connected, NULL);

  g_main_loop_run(mainloop);
  return 0;
}
