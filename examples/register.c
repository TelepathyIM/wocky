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

static void
connector_callback (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  gchar *jid = NULL;
  gchar *sid = NULL;
  WockyConnector *wcon = WOCKY_CONNECTOR (source);
  WockyXmppConnection *connection =
    wocky_connector_register_finish (wcon, res, &jid, &sid, &error);

  if (connection != NULL)
    {
      printf ("connected (%s) [%s]!\n", jid, sid);
      g_free (sid);
      g_free (jid);
    }
  else
    {
      if (error)
        g_warning ("%s: %d: %s\n",
            g_quark_to_string (error->domain), error->code, error->message);
    }
  g_main_loop_quit (mainloop);
}

int
main (int argc,
    char **argv)
{
  gchar *jid = NULL;
  gchar *user = NULL;
  gchar *host = NULL;
  gchar *pass = NULL;
  gchar *email = NULL;
  WockyConnector *wcon = NULL;

  g_type_init ();

  if ((argc < 4) || (argc > 5))
    {
      printf ("Usage: %s <jid> <password> <email> [host]\n", argv[0]);
      return -1;
    }

  jid = argv[1];
  pass = argv[2];
  email = argv[3];
  wocky_decode_jid (jid, &user, NULL, NULL);

  if (argc == 5)
    host = argv[4];

  mainloop = g_main_loop_new (NULL, FALSE);

  if ((host != NULL) && (*host != '\0'))
    wcon = wocky_connector_new (jid, pass, NULL, NULL, NULL);
  else
    wcon = g_object_new (WOCKY_TYPE_CONNECTOR,
        "jid"        , jid ,
        "password"   , pass,
        "xmpp-server", host, NULL);

  g_object_set (G_OBJECT (wcon), "email", email, NULL);
  wocky_connector_register_async (wcon, NULL, connector_callback, NULL);
  g_main_loop_run (mainloop);

  return 0;
}
