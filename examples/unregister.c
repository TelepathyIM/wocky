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
int rval = -1;

static void
unregister_callback (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  WockyConnector *wcon = WOCKY_CONNECTOR (source);
  gboolean done = wocky_connector_unregister_finish (wcon, res, &error);

  if (done)
    {
      printf ("Unregistered!\n");
    }
  else
    {
      if (error)
        g_warning ("Unregistration error: %s: %d: %s\n",
            g_quark_to_string (error->domain), error->code, error->message);
      else
        g_warning ("Unregister failed: No error supplied\n");
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
  WockyConnector *wcon = NULL;

  g_type_init ();

  if ((argc < 3) || (argc > 4))
    {
      printf ("Usage: %s <jid> <password> [host]\n", argv[0]);
      return -1;
    }

  jid = argv[1];
  pass = argv[2];
  wocky_decode_jid (jid, &user, NULL, NULL);

  if (argc == 4)
    host = argv[3];

  mainloop = g_main_loop_new (NULL, FALSE);

  if ((host != NULL) && (*host != '\0'))
    wcon = wocky_connector_new (jid, pass, NULL, NULL, NULL);
  else
    wcon = g_object_new (WOCKY_TYPE_CONNECTOR,
        "jid"        , jid ,
        "password"   , pass,
        "xmpp-server", host, NULL);

  wocky_connector_unregister_async (wcon, NULL, unregister_callback, NULL);
  g_main_loop_run (mainloop);

  return rval;
}
