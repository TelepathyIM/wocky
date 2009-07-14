#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <glib.h>

#include <gio/gnio.h>
#include <wocky/wocky-connector.h>
#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-sasl-auth.h>

GMainLoop *mainloop;
WockyXmppConnection *conn;
const gchar *server;
const gchar *username;
const gchar *password;
WockySaslAuth *sasl = NULL;
GSocketConnection *tcp;

GSocketClient *client;
GTLSConnection *ssl;
GTLSSession *ssl_session;

static void
post_auth_open_sent_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  if (!wocky_xmpp_connection_send_open_finish (conn, result, NULL))
    {
      printf ("Sending open failed\n");
      g_main_loop_quit (mainloop);
    }
}

static void
auth_done_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;

  if (!wocky_sasl_auth_authenticate_finish (WOCKY_SASL_AUTH (source),
      result, &error))
    {
      printf ("Authentication failed: %s\n", error->message);
      g_error_free (error);
      g_main_loop_quit (mainloop);
      return;
    }

  printf ("Authentication successfull!!\n");

  /* Reopen the connection */
  wocky_xmpp_connection_reset (conn);
  wocky_xmpp_connection_send_open_async (conn,
      server, NULL, "1.0", NULL,
      NULL, post_auth_open_sent_cb, NULL);
}

static void
ssl_features_received_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  WockyXmppStanza *stanza;

  stanza = wocky_xmpp_connection_recv_stanza_finish (conn, result, NULL);

  g_assert (stanza != NULL);

  if (strcmp (stanza->node->name, "features")
      || strcmp (wocky_xmpp_node_get_ns (stanza->node), WOCKY_XMPP_NS_STREAM))
    {
      printf ("Didn't receive features stanza\n");
      g_main_loop_quit (mainloop);
      return;
    }

  sasl = wocky_sasl_auth_new (server, username, password, conn);

  wocky_sasl_auth_authenticate_async (sasl, stanza, TRUE,
    NULL, auth_done_cb, NULL);
}


static void
ssl_received_open_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  gchar *version;
  gchar *from;

  if (!wocky_xmpp_connection_recv_open_finish (conn, result,
      NULL, &from, &version, NULL, NULL))
    {
      printf ("Didn't receive open\n");
      g_main_loop_quit (mainloop);
      return;
    }

  printf ("Stream opened -- from: %s version: %s\n", from, version);
  if (version == NULL || strcmp (version, "1.0"))
    {
      printf ("Server is not xmpp compliant\n");
      g_main_loop_quit (mainloop);
    }

  /* waiting for features */
  wocky_xmpp_connection_recv_stanza_async (conn,
    NULL, ssl_features_received_cb, NULL);

  g_free (version);
  g_free (from);
}

static void
ssl_open_sent_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  if (!wocky_xmpp_connection_send_open_finish (conn, result, NULL))
    {
      printf ("Sending open failed\n");
      g_main_loop_quit (mainloop);
      return;
    }

  wocky_xmpp_connection_recv_open_async (conn, NULL,
    ssl_received_open_cb, NULL);
}

static void
tcp_start_tls_recv_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  WockyXmppStanza *stanza;
  GError *error = NULL;

  stanza = wocky_xmpp_connection_recv_stanza_finish (conn, result, NULL);

  g_assert (stanza != NULL);

  if (strcmp (stanza->node->name, "proceed")
      || strcmp (wocky_xmpp_node_get_ns (stanza->node), WOCKY_XMPP_NS_TLS))
    {
      printf ("Server doesn't want to start tls");
      g_main_loop_quit (mainloop);
      return;
    }

  g_object_unref (conn);

  ssl_session = g_tls_session_new (G_IO_STREAM (tcp));
  ssl = g_tls_session_handshake (ssl_session, NULL, &error);

  if (ssl == NULL)
    g_error ("connect: %s: %d, %s", g_quark_to_string (error->domain),
      error->code, error->message);

  conn = wocky_xmpp_connection_new (G_IO_STREAM (ssl));
  wocky_xmpp_connection_send_open_async (conn,
      server, NULL, "1.0", NULL,
      NULL, ssl_open_sent_cb, NULL);
}

static void
tcp_start_tls_send_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  g_assert (wocky_xmpp_connection_send_stanza_finish (conn, result, NULL));

  wocky_xmpp_connection_recv_stanza_async (conn,
      NULL, tcp_start_tls_recv_cb, NULL);
}

static void
tcp_features_received_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  WockyXmppStanza *stanza;
  WockyXmppNode *tls;
  WockyXmppStanza *starttls;

  stanza = wocky_xmpp_connection_recv_stanza_finish (conn, result, NULL);

  g_assert (stanza != NULL);

  if (strcmp (stanza->node->name, "features")
      || strcmp (wocky_xmpp_node_get_ns (stanza->node), WOCKY_XMPP_NS_STREAM))
    {
      printf ("Didn't receive features stanza\n");
      g_main_loop_quit (mainloop);
      return;
    }

  tls = wocky_xmpp_node_get_child_ns (stanza->node, "starttls",
      WOCKY_XMPP_NS_TLS);

  if (tls == NULL)
    {
      printf ("Server doesn't support tls\n");
      g_main_loop_quit (mainloop);
    }

  starttls = wocky_xmpp_stanza_new ("starttls");
  wocky_xmpp_node_set_ns (starttls->node, WOCKY_XMPP_NS_TLS);

  wocky_xmpp_connection_send_stanza_async (conn, starttls,
    NULL, tcp_start_tls_send_cb, NULL);

  g_object_unref (stanza);
  g_object_unref (starttls);
}

static void
tcp_received_open_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  gchar *version;
  gchar *from;

  if (!wocky_xmpp_connection_recv_open_finish (conn, result,
      NULL, &from, &version, NULL, NULL))
    {
      printf ("Didn't receive open\n");
      g_main_loop_quit (mainloop);
      return;
    }

  printf ("Stream opened -- from: %s version: %s\n", from, version);

  if (version == NULL || strcmp (version, "1.0"))
    {
      printf ("Server is not xmpp compliant\n");
      g_main_loop_quit (mainloop);
    }

  /* waiting for features */
  wocky_xmpp_connection_recv_stanza_async (conn,
    NULL, tcp_features_received_cb, NULL);

  g_free (version);
  g_free (from);
}

static void
tcp_sent_open_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  if (!wocky_xmpp_connection_send_open_finish (conn, result, NULL))
    {
      printf ("Sending open failed\n");
      g_main_loop_quit (mainloop);
      return;
    }

  wocky_xmpp_connection_recv_open_async (conn, NULL,
    tcp_received_open_cb, NULL);
}

static void
tcp_do_connect (void)
{
  g_assert (tcp != NULL);

  printf ("TCP connection established\n");

  conn = wocky_xmpp_connection_new (G_IO_STREAM (tcp));

  wocky_xmpp_connection_send_open_async (conn,
      server, NULL, "1.0", NULL,
      NULL, tcp_sent_open_cb, NULL);
}

static void
tcp_host_connected (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;

  tcp = g_socket_client_connect_to_host_finish (G_SOCKET_CLIENT (source),
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
      tcp_do_connect ();
    }
}

static void
tcp_srv_connected (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;

  tcp = g_socket_client_connect_to_service_finish (G_SOCKET_CLIENT (source),
    result, &error);

  if (tcp == NULL)
    {
      g_message ("SRV connect failed: %s: %d, %s",
        g_quark_to_string (error->domain),
          error->code, error->message);

      g_message ("Falling back to direct connection");
      g_error_free (error);
      g_socket_client_connect_to_host_async (client, server, 5222, NULL,
          tcp_host_connected, NULL);
    }
  else
    {
      tcp_do_connect ();
    }
}

static void
connector_callback (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  WockyXmppConnection *connection =
    wocky_connector_connect_finish (source, res, &error);

  if (connection != NULL)
    {
      printf ("connected!\n");
    }
  else
    {
      if (error)
        fprintf (stderr, "%s: %d: %s\n",
            g_quark_to_string (error->domain), error->code, error->message);
      g_main_loop_quit (mainloop);
    }
}

int
main (int argc,
    char **argv)
{
  gchar *jid;
  char *c;
  const char *type = "raw";

  g_type_init ();

  if ((argc < 3) || (argc > 4))
    {
      printf ("Usage: %s <jid> <password> [connection-type]\n", argv[0]);
      printf ("    connection-type is 'raw' or 'connector' (default raw)\n");
      return -1;
    }

  if (argc == 4)
    type = argv[3];

  mainloop = g_main_loop_new (NULL, FALSE);

  if (!strcmp ("connector",type))
    {
      WockyConnector *wcon = NULL;

      wcon = wocky_connector_new (argv[1], "password", argv[2]);

      if (wocky_connector_connect_async (G_OBJECT (wcon),
              connector_callback, NULL))
        {
          printf ("Instantiated connector and made successful connect call\n");
          g_main_loop_run (mainloop);
        }

      return 0;
    }

  jid = g_strdup (argv[1]);
  password = argv[2];
  c = rindex (jid, '@');
  if (c == NULL)
    {
      printf ("JID should contain an @ sign\n");
      return -1;
    }
  *c = '\0';
  server = c + 1;
  username = jid;

  client = g_socket_client_new ();

  printf ("Connecting to %s\n", server);

  g_socket_client_connect_to_service_async (client, server,
      "xmpp-client", NULL, tcp_srv_connected, NULL);

  g_main_loop_run (mainloop);
  g_free (jid);
  return 0;
}
