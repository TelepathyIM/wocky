#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <glib.h>

#include <gio/gio.h>
#include <wocky/wocky.h>

GMainLoop *mainloop;
char *recipient;
char *message;

static void
closed_cb (
    GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source);
  WockySession *session = WOCKY_SESSION (user_data);
  GError *error = NULL;

  if (wocky_porter_close_finish (porter, res, &error))
    {
      g_print ("Signed out\n");
    }
  else
    {
      g_warning ("Couldn't sign out cleanly: %s\n", error->message);
      g_clear_error (&error);
    }

  /* Either way, we're done. */
  g_object_unref (session);
  g_main_loop_quit (mainloop);
}

static void
message_sent_cb (
    GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source);
  WockySession *session = WOCKY_SESSION (user_data);
  GError *error = NULL;

  if (wocky_porter_send_finish (porter, res, &error))
    {
      g_print ("Sent '%s' to %s\n", message, recipient);
    }
  else
    {
      g_warning ("Couldn't send message: %s\n", error->message);
      g_clear_error (&error);
    }

  /* Sign out. */
  wocky_porter_close_async (porter, NULL, closed_cb, session);
}

static void
connected_cb (
    GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyConnector *connector = WOCKY_CONNECTOR (source);
  WockyXmppConnection *connection;
  gchar *jid = NULL;
  GError *error = NULL;

  connection = wocky_connector_connect_finish (connector, res, &jid, NULL,
      &error);

  if (connection == NULL)
    {
      g_warning ("Couldn't connect: %s", error->message);
      g_clear_error (&error);
      g_main_loop_quit (mainloop);
    }
  else
    {
      WockySession *session = wocky_session_new_with_connection (connection, jid);
      WockyPorter *porter = wocky_session_get_porter (session);
      WockyStanza *stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NONE, NULL, recipient,
          '(', "body",
            '$', message,
          ')', NULL);

      g_print ("Connected as %s\n", jid);

      wocky_porter_start (porter);
      wocky_porter_send_async (porter, stanza, NULL, message_sent_cb, session);

      g_object_unref (stanza);
      g_free (jid);
    }
}

static gboolean ignore_ssl_errors = FALSE;

static GOptionEntry entries[] =
{
  { "ignore-ssl-errors", 0, 0, G_OPTION_ARG_NONE, &ignore_ssl_errors,
    "Continue connecting, even if the server provides an invalid SSL certificate",
    NULL },
  { NULL }
};

int
main (int argc,
    char **argv)
{
  GError *error = NULL;
  GOptionContext *context;
  char *jid, *password;
  WockyTLSHandler *tls_handler = NULL;
  WockyConnector *connector;

  g_type_init ();
  wocky_init ();

  context = g_option_context_new ("<jid> <password> <receipient> <message> - signs in as <jid> and sends <message> to <recipient>");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error) ||
      argc != 5)
    {
      if (error != NULL)
        printf ("option parsing failed: %s\n", error->message);

      printf ("%s", g_option_context_get_help (context, FALSE, NULL));
      return -1;
    }

  jid = argv[1];
  password = argv[2];
  recipient = argv[3];
  message = argv[4];

  if (ignore_ssl_errors)
    tls_handler = wocky_tls_handler_new (TRUE);

  mainloop = g_main_loop_new (NULL, FALSE);
  connector = wocky_connector_new (jid, password, NULL, NULL, tls_handler);
  wocky_connector_connect_async (connector, NULL, connected_cb, NULL);

  g_main_loop_run (mainloop);

  g_object_unref (connector);
  g_clear_object (&tls_handler);
  g_main_loop_unref (mainloop);
  return 0;
}
