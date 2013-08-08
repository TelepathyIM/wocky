#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "wocky-test-connector-server.h"

GMainLoop *loop;

static gboolean
server_quit (GIOChannel *channel,
    GIOCondition cond,
    gpointer data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

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
  ConnectorProblem cproblem = { 0, };

  GSocketConnection *gconn;
  pid_t pid = 0;
  TestConnectorServer *server;

  if (csock < 0)
    {
      perror ("accept() failed");
      g_warning ("accept() failed on socket that should have been ready.");
      return TRUE;
    }

  switch ((pid = fork ()))
    {
    case -1:
      perror ("Failed to spawn child process");
      g_main_loop_quit (loop);
      break;
    case 0:
      while (g_source_remove_by_user_data (loop));
      g_io_channel_shutdown (channel, TRUE, NULL);
      gconn = g_object_new (G_TYPE_SOCKET_CONNECTION, "socket", gsock, NULL);
      server = test_connector_server_new (G_IO_STREAM (gconn),
          NULL, "foo", "bar", "1.0",
          &cproblem,
          SERVER_PROBLEM_NO_PROBLEM,
          CERT_STANDARD);
      test_connector_server_start (server);
      return FALSE;
    default:
      g_socket_close (gsock, NULL);
      return TRUE;
    }
  return FALSE;
}

int
main (int argc,
    char **argv)
{
  int ssock;
  int reuse = 1;
  struct sockaddr_in server;
  GIOChannel *channel;

  memset (&server, 0, sizeof (server));

  g_type_init ();

  loop = g_main_loop_new (NULL, FALSE);

  server.sin_family = AF_INET;
  inet_aton ("127.0.0.1", &server.sin_addr);
  server.sin_port = htons (5222);
  ssock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

  setsockopt (ssock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));
  bind (ssock, (struct sockaddr *)&server, sizeof (server));
  listen (ssock, 1024);
  channel = g_io_channel_unix_new (ssock);
  g_io_add_watch (channel, G_IO_IN|G_IO_PRI, client_connected, loop);
  g_io_add_watch (channel, G_IO_ERR|G_IO_NVAL, server_quit, loop);

  g_main_loop_run (loop);
}

