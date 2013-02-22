#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <string.h>

#include <glib.h>

#include <gio/gio.h>
#include <wocky/wocky.h>

GMainLoop *mainloop;

static void
porter_closed_cb (
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

static gboolean
message_received_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  WockyNode *body_node = wocky_node_get_child (
      wocky_stanza_get_top_node (stanza),
      "body");
  const gchar *from = wocky_stanza_get_from (stanza);
  const gchar *message;

  /* We told the porter only to give us messages which contained a <body/>
   * element.
   */
  g_assert (body_node != NULL);
  message = body_node->content;

  if (message == NULL)
    message = "";

  g_print ("Message received from %s: “%s”\n", from, message);

  if (g_ascii_strcasecmp (message, "sign out") == 0)
    {
      WockyStanza *reply = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NORMAL, NULL, from,
          '(', "body",
            '$', "Say please! Didn’t your parents teach you any manners‽",
          ')', NULL);

      wocky_porter_send (porter, reply);
      g_object_unref (reply);
    }

  /* Tell the porter that we've handled this stanza; if there were any
   * lower-priority handlers, they would not be called for this stanza.
   */
  return TRUE;
}

static gboolean
sign_out_message_received_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  WockySession *session = WOCKY_SESSION (user_data);
  const gchar *from = wocky_stanza_get_from (stanza);

  g_print ("%s asked us nicely to sign out\n", from);
  wocky_porter_close_async (porter, NULL, porter_closed_cb, session);

  /* Returning FALSE tells the porter that other, lower-priority handlers that
   * match the stanza should be invoked — in this example, that means
   * message_received_cb().
   */
  return FALSE;
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
      WockyStanza *stanza;

      g_print ("Connected as %s\n", jid);

      /* Register a callback for incoming <message type='chat'/> stanzas from
       * anyone, but only if it contains a <body/> element (the element that
       * actually contains the text of IMs). */
      wocky_porter_register_handler_from_anyone (porter,
          WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_CHAT,
          WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
          message_received_cb, session,
          '(', "body", ')', NULL);

      /* Register a higher-priority handler for incoming <message type='chat'/>
       * stanzas which contain a <body/> element containing the text
       * "please sign out".
       */
      wocky_porter_register_handler_from_anyone (porter,
          WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_CHAT,
          WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
          sign_out_message_received_cb, session,
          '(', "body",
            '$', "please sign out",
          ')', NULL);

      wocky_porter_start (porter);

      /* Broadcast presence for ourself, so our contacts can see us online,
       * with a status message. */
      stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_PRESENCE,
          WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
          '(', "show",
            '$', "chat",
          ')',
          '(', "status",
            '$', "talk to me! (or tell me to “sign out”)",
          ')', NULL);

      wocky_porter_send (porter, stanza);

      g_object_unref (stanza);
      g_free (jid);
    }
}

static gboolean ignore_ssl_errors = FALSE;
static gchar *resource = NULL;

static GOptionEntry entries[] =
{
  { "ignore-ssl-errors", 0, 0, G_OPTION_ARG_NONE, &ignore_ssl_errors,
    "Continue connecting, even if the server provides an invalid SSL certificate",
    NULL },
  { "resource", 'r', 0, G_OPTION_ARG_STRING, &resource,
    "Connect as a specific resource (default: let the server decide)",
    "RESOURCE" },
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

  context = g_option_context_new ("<jid> <password> - signs in as <jid> and prints incoming messages");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error) ||
      argc != 3)
    {
      if (error != NULL)
        printf ("option parsing failed: %s\n", error->message);

      printf ("%s", g_option_context_get_help (context, FALSE, NULL));
      return -1;
    }

  jid = argv[1];
  password = argv[2];

  if (ignore_ssl_errors)
    tls_handler = wocky_tls_handler_new (TRUE);

  mainloop = g_main_loop_new (NULL, FALSE);
  connector = wocky_connector_new (jid, password, resource, NULL, tls_handler);
  wocky_connector_connect_async (connector, NULL, connected_cb, NULL);

  g_main_loop_run (mainloop);

  g_object_unref (connector);
  g_clear_object (&tls_handler);
  g_main_loop_unref (mainloop);
  return 0;
}
