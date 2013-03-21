/*
 * dump-certificates.c - Dump all Certificates from TLS Handshake
 * Copyright (C) 2011 Collabora Ltd.
 * @author Stef Walter <stefw@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <glib.h>

#include <gio/gio.h>
#include <wocky/wocky.h>

#include <gnutls/x509.h>

static GMainLoop *mainloop;

typedef struct {
  WockyTLSHandler parent;
} DumpTLSHandler;

typedef struct {
  WockyTLSHandlerClass parent_class;
} DumpTLSHandlerClass;

GType dump_tls_handler_get_type (void);

G_DEFINE_TYPE (DumpTLSHandler, dump_tls_handler, WOCKY_TYPE_TLS_HANDLER)

static void
dump_tls_handler_init (DumpTLSHandler *self)
{

}

static void
dump_tls_handler_verify_async (WockyTLSHandler *self,
    WockyTLSSession *tls_session,
    const gchar *peername,
    GStrv extra_identities,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *res;
  GPtrArray *chain;
  gnutls_x509_crt_t cert;
  gnutls_datum_t datum;
  gchar buffer[1024 * 20];
  gsize length;
  guint i;

  chain = wocky_tls_session_get_peers_certificate (tls_session, NULL);

  for (i = 0; i < chain->len; i++)
    {
      GArray *cert_data = g_ptr_array_index (chain, i);
      datum.data = (gpointer)cert_data->data;
      datum.size = cert_data->len;

      if (gnutls_x509_crt_init (&cert) < 0)
        g_assert_not_reached ();
      if (gnutls_x509_crt_import (cert, &datum, GNUTLS_X509_FMT_DER) < 0)
        {
          g_warning ("couldn't parse certificate %u in chain", i);
          gnutls_x509_crt_deinit (cert);
          continue;
        }

      length = sizeof (buffer);
      gnutls_x509_crt_get_dn (cert, buffer, &length);
      g_print ("Subject: %.*s\n", (gint) length, buffer);

      length = sizeof (buffer);
      gnutls_x509_crt_get_issuer_dn (cert, buffer, &length);
      g_print ("Issuer: %.*s\n", (gint) length, buffer);

      length = sizeof (buffer);
      if (gnutls_x509_crt_export (cert, GNUTLS_X509_FMT_PEM, buffer, &length) < 0)
        {
          g_warning ("couldn't export certificate %u in chain", i);
          gnutls_x509_crt_deinit (cert);
          continue;
        }
      g_print ("%.*s\n", (gint) length, buffer);

      gnutls_x509_crt_deinit (cert);
    }

  g_ptr_array_unref (chain);

  res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      dump_tls_handler_verify_async);
  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}

static gboolean
dump_tls_handler_verify_finish (WockyTLSHandler *self,
    GAsyncResult *result,
    GError **error)
{
  return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error);
}

static void
dump_tls_handler_class_init (DumpTLSHandlerClass *klass)
{
  WockyTLSHandlerClass *handler_class = WOCKY_TLS_HANDLER_CLASS (klass);
  handler_class->verify_async_func = dump_tls_handler_verify_async;
  handler_class->verify_finish_func = dump_tls_handler_verify_finish;
}

static void
connected_cb (
    GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  g_main_loop_quit (mainloop);
}

int
main (int argc,
    char **argv)
{
  char *jid, *password;
  WockyConnector *connector;
  WockyTLSHandler *handler;

  g_type_init ();
  wocky_init ();

  if (argc != 2)
    {
      g_printerr ("Usage: %s <jid>\n", argv[0]);
      return -1;
    }

  jid = argv[1];
  /* This example doesn't use your real password because it does not actually
   * validate certificates: it just dumps them then declares them valid.
   */
  password = "not a chance";

  mainloop = g_main_loop_new (NULL, FALSE);
  handler = g_object_new (dump_tls_handler_get_type (), NULL);
  connector = wocky_connector_new (jid, password, NULL, NULL, handler);
  wocky_connector_connect_async (connector, NULL, connected_cb, NULL);

  g_main_loop_run (mainloop);

  g_object_unref (connector);
  g_main_loop_unref (mainloop);
  return 0;
}
