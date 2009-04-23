#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <glib.h>

#include <gio/gnio.h>
#include <wocky/wocky-xmpp-connection.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-sasl-auth.h>

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

static gchar *
return_str (WockySaslAuth *auth,
    gpointer user_data)
{
  return g_strdup (user_data);
}

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
auth_success(WockySaslAuth *auth,
    gpointer user_data)
{
  printf("Authentication successfull!!\n");

  /* Reopen the connection */
  wocky_xmpp_connection_reset (conn);
  wocky_xmpp_connection_send_open_async (conn,
      server, NULL, "1.0", NULL,
      NULL, post_auth_open_sent_cb, NULL);
}

static void
auth_failed(WockySaslAuth *auth,
    GQuark domain,
    int code,
    gchar *message,
    gpointer user_data)
{
  printf ("Authentication failed: %s\n", message);
  g_main_loop_quit (mainloop);
}

static void
ssl_features_received_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  WockyXmppStanza *stanza;
  GError *error;

  stanza = wocky_xmpp_connection_recv_stanza_finish (conn, result, NULL);

  g_assert (stanza != NULL);

  if (strcmp (stanza->node->name, "features")
      || strcmp (wocky_xmpp_node_get_ns (stanza->node), WOCKY_XMPP_NS_STREAM))
    {
      printf ("Didn't receive features stanza\n");
      g_main_loop_quit (mainloop);
      return;
    }

  sasl = wocky_sasl_auth_new ();
  g_signal_connect (sasl, "username-requested",
      G_CALLBACK (return_str), (gpointer)username);
  g_signal_connect (sasl, "password-requested",
      G_CALLBACK (return_str), (gpointer)password);
  g_signal_connect (sasl, "authentication-succeeded",
      G_CALLBACK (auth_success), NULL);
  g_signal_connect (sasl, "authentication-failed",
      G_CALLBACK (auth_failed), NULL);

  if (!wocky_sasl_auth_authenticate (sasl, server, conn, stanza, TRUE, &error))
    {
      printf ("Sasl auth start failed: %s\n", error->message);
      g_main_loop_quit (mainloop);
    }
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
      || strcmp(wocky_xmpp_node_get_ns (stanza->node),WOCKY_XMPP_NS_STREAM))
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
tcp_do_connect(void)
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
      tcp_do_connect ();
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
      tcp_do_connect ();
    }
}

int
main(int argc,
    char **argv)
{
  g_type_init ();

  g_assert (argc == 4);

  mainloop = g_main_loop_new (NULL, FALSE);

  client =  g_tcp_client_new ();

  server = argv[1];
  username = argv[2];
  password = argv[3];

  printf ("Connecting to %s\n", server);

  g_tcp_client_connect_to_service_async (client, server,
    "xmpp-client", NULL, tcp_srv_connected, NULL);

  g_main_loop_run (mainloop);
  return 0;
}
