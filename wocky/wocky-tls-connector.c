/*
 * wocky-tls-connector.h - Header for WockyTLSConnector
 * Copyright (C) 2010 Collabora Ltd.
 * @author Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 * @author Vivek Dasmohapatra <vivek@collabora.co.uk>
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

#include <config.h>

#include <glib-object.h>

#include "wocky-tls-connector.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_TLS
#include "wocky-debug-internal.h"

#include "wocky-namespaces.h"
#include "wocky-connector.h"
#include "wocky-tls.h"
#include "wocky-tls-handler.h"
#include "wocky-utils.h"
#include "wocky-xmpp-connection.h"

struct _WockyTLSConnectorPrivate {
  gboolean legacy_ssl;
  gchar *peername;
  GStrv extra_identities;

  WockyTLSHandler *handler;
  WockyTLSSession *session;
  WockyXmppConnection *connection;
  WockyXmppConnection *tls_connection;

  GSimpleAsyncResult *secure_result;
  GCancellable *cancellable;
};

enum {
  PROP_HANDLER = 1,
  LAST_PROPERTY,
};

static void
session_handshake_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);

G_DEFINE_TYPE (WockyTLSConnector, wocky_tls_connector, G_TYPE_OBJECT);

static void
wocky_tls_connector_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyTLSConnector *self = WOCKY_TLS_CONNECTOR (object);

  switch (property_id)
    {
      case PROP_HANDLER:
        g_value_set_object (value, self->priv->handler);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_tls_connector_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyTLSConnector *self = WOCKY_TLS_CONNECTOR (object);

  switch (property_id)
    {
      case PROP_HANDLER:
        if (g_value_get_object (value) == NULL)
          self->priv->handler = wocky_tls_handler_new (FALSE);
        else
          self->priv->handler = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_tls_connector_finalize (GObject *object)
{
  WockyTLSConnector *self = WOCKY_TLS_CONNECTOR (object);

  g_free (self->priv->peername);
  g_strfreev (self->priv->extra_identities);

  if (self->priv->session != NULL)
    {
      g_object_unref (self->priv->session);
      self->priv->session = NULL;
    }

  if (self->priv->handler != NULL)
    {
      g_object_unref (self->priv->handler);
      self->priv->handler = NULL;
    }

  if (self->priv->tls_connection != NULL)
    {
      g_object_unref (self->priv->tls_connection);
      self->priv->tls_connection = NULL;
    }

  G_OBJECT_CLASS (wocky_tls_connector_parent_class)->finalize (object);
}

static void
wocky_tls_connector_class_init (WockyTLSConnectorClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (WockyTLSConnectorPrivate));

  oclass->get_property = wocky_tls_connector_get_property;
  oclass->set_property = wocky_tls_connector_set_property;
  oclass->finalize = wocky_tls_connector_finalize;

  /**
   * WockyTLSConnector:tls-handler:
   *
   * The #WockyTLSHandler object used for the TLS handshake.
   */
  pspec = g_param_spec_object ("tls-handler",
      "TLS Handler", "Handler for the TLS handshake",
      WOCKY_TYPE_TLS_HANDLER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_HANDLER, pspec);
}

static void
wocky_tls_connector_init (WockyTLSConnector *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_TLS_CONNECTOR,
      WockyTLSConnectorPrivate);

  self->priv->secure_result = NULL;
}

static void
add_ca (gpointer data,
    gpointer user_data)
{
  WockyTLSSession *session = user_data;
  const gchar *path = data;

  wocky_tls_session_add_ca (session, path);
}

static void
add_crl (gpointer data,
    gpointer user_data)
{
  WockyTLSSession *session = user_data;
  const gchar *path = data;

  wocky_tls_session_add_crl (session, path);
}



static void
prepare_session (WockyTLSConnector *self)
{
  GSList *cas;
  GSList *crl;

  cas = wocky_tls_handler_get_cas (self->priv->handler);
  crl = wocky_tls_handler_get_crl (self->priv->handler);

  g_slist_foreach (cas, add_ca, self->priv->session);
  g_slist_foreach (crl, add_crl, self->priv->session);
}

static void
report_error_in_idle (WockyTLSConnector *self,
    gint error_code,
    const gchar *format,
    ...)
{
  GError *error = NULL;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (WOCKY_CONNECTOR_ERROR, error_code, format,
      args);
  va_end (args);

  DEBUG ("%s", error->message);

  g_simple_async_result_set_from_error (self->priv->secure_result,
      error);
  g_error_free (error);
  g_simple_async_result_complete_in_idle (self->priv->secure_result);

  g_object_unref (self->priv->secure_result);

  if (self->priv->cancellable != NULL)
    {
      g_object_unref (self->priv->cancellable);
      self->priv->cancellable = NULL;
    }
}

static void
report_error_in_idle_gerror (WockyTLSConnector *self,
    const GError *error)
{
  DEBUG ("Reporting error %s", error->message);

  g_simple_async_result_set_from_error (self->priv->secure_result,
      error);
  g_simple_async_result_complete_in_idle (self->priv->secure_result);

  g_object_unref (self->priv->secure_result);

  if (self->priv->cancellable != NULL)
    {
      g_object_unref (self->priv->cancellable);
      self->priv->cancellable = NULL;
    }
}

static void
do_handshake (WockyTLSConnector *self)
{
  GIOStream *base_stream = NULL;

  g_object_get (self->priv->connection, "base-stream", &base_stream, NULL);
  g_assert (base_stream != NULL);

  self->priv->session = wocky_tls_session_new (base_stream);

  g_object_unref (base_stream);

  if (self->priv->session == NULL)
    {
      report_error_in_idle (self, WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
          "%s", "SSL session failed");
      return;
    }

  prepare_session (self);

  wocky_tls_session_handshake_async (self->priv->session,
      G_PRIORITY_DEFAULT, self->priv->cancellable, session_handshake_cb, self);
}

static void
tls_handler_verify_async_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyTLSConnector *self = user_data;
  WockyTLSHandler *handler = WOCKY_TLS_HANDLER (source);
  GError *error = NULL;

  wocky_tls_handler_verify_finish (handler, res, &error);

  if (error != NULL)
    {
      /* forward the GError as we got it in this case */
      report_error_in_idle_gerror (self, error);
      g_error_free (error);

      return;
    }

  g_simple_async_result_set_op_res_gpointer (self->priv->secure_result,
      self->priv->tls_connection, (GDestroyNotify) g_object_unref);
  self->priv->tls_connection = NULL;
  g_simple_async_result_complete_in_idle (self->priv->secure_result);

  g_object_unref (self->priv->secure_result);

  if (self->priv->cancellable != NULL)
    {
      g_object_unref (self->priv->cancellable);
      self->priv->cancellable = NULL;
    }
}

static void
session_handshake_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  WockyTLSConnection *tls_conn;
  WockyTLSConnector *self = user_data;
  const gchar *tls_type;

  tls_type = self->priv->legacy_ssl ? "SSL" : "TLS";
  tls_conn = wocky_tls_session_handshake_finish (self->priv->session,
      res, &error);

  if (tls_conn == NULL)
    {
      report_error_in_idle (self, WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
          "%s handshake error: %s", tls_type, error->message);
      g_error_free (error);

      return;
    }

  DEBUG ("Completed %s handshake", tls_type);

  self->priv->tls_connection = wocky_xmpp_connection_new (
      G_IO_STREAM (tls_conn));
  g_object_unref (tls_conn);

  wocky_tls_handler_verify_async (self->priv->handler,
      self->priv->session, self->priv->peername,
      self->priv->extra_identities, tls_handler_verify_async_cb, self);
}

static void
starttls_recv_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyTLSConnector *self = user_data;
  WockyStanza *stanza;
  GError *error = NULL;
  WockyNode *node;

  stanza = wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (self->priv->connection), result, &error);

  if (stanza == NULL)
    {
      report_error_in_idle (self, WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
          "STARTTLS reply not received: %s", error->message);
      g_error_free (error);

      goto out;
    }

  if (wocky_stanza_extract_stream_error (stanza, &error))
    {
      /* forward the GError as we got it in this case */
      report_error_in_idle_gerror (self, error);
      g_error_free (error);

      goto out;
    }

  DEBUG ("Received STARTTLS response");
  node = wocky_stanza_get_top_node (stanza);

  if (!wocky_node_matches (node, "proceed", WOCKY_XMPP_NS_TLS))
    {
      report_error_in_idle (self, WOCKY_CONNECTOR_ERROR_TLS_REFUSED,
          "%s", "STARTTLS refused by the server");
      goto out;
    }
  else
    {
      GIOStream *base_stream = NULL;

      g_object_get (self->priv->connection, "base-stream", &base_stream, NULL);
      g_assert (base_stream != NULL);

      self->priv->session = wocky_tls_session_new (base_stream);

      g_object_unref (base_stream);

      if (self->priv->session == NULL)
        {
          report_error_in_idle (self, WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
              "%s", "Unable to create a TLS session");
          goto out;
        }

      prepare_session (self);

      DEBUG ("Starting client TLS handshake %p", self->priv->session);
      wocky_tls_session_handshake_async (self->priv->session,
          G_PRIORITY_HIGH, self->priv->cancellable,
          session_handshake_cb, self);
    }

 out:
  if (stanza != NULL)
    g_object_unref (stanza);
}

static void
starttls_sent_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyTLSConnector *self = user_data;
  GError *error = NULL;

  if (!wocky_xmpp_connection_send_stanza_finish (
          WOCKY_XMPP_CONNECTION (self->priv->connection), result, &error))
    {
      report_error_in_idle (self, WOCKY_CONNECTOR_ERROR_TLS_SESSION_FAILED,
          "Failed to send STARTTLS stanza: %s", error->message);
      g_error_free (error);

      return;
    }

  DEBUG ("Sent STARTTLS stanza");
  wocky_xmpp_connection_recv_stanza_async (
      WOCKY_XMPP_CONNECTION (self->priv->connection), self->priv->cancellable,
      starttls_recv_cb, self);
}

static void
do_starttls (WockyTLSConnector *self)
{
  WockyStanza *starttls;

  starttls = wocky_stanza_new ("starttls", WOCKY_XMPP_NS_TLS);

  DEBUG ("Sending STARTTLS stanza");
  wocky_xmpp_connection_send_stanza_async (
      WOCKY_XMPP_CONNECTION (self->priv->connection), starttls,
      self->priv->cancellable, starttls_sent_cb, self);
  g_object_unref (starttls);
}

WockyTLSConnector *
wocky_tls_connector_new (WockyTLSHandler *handler)
{
  return g_object_new (WOCKY_TYPE_TLS_CONNECTOR,
      "tls-handler", handler, NULL);
}

void
wocky_tls_connector_secure_async (WockyTLSConnector *self,
    WockyXmppConnection *connection,
    gboolean old_style_ssl,
    const gchar *peername,
    GStrv extra_identities,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *async_result;

  g_assert (self->priv->secure_result == NULL);
  g_assert (self->priv->cancellable == NULL);

  async_result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_tls_connector_secure_async);

  if (cancellable != NULL)
    self->priv->cancellable = g_object_ref (cancellable);

  self->priv->connection = connection;
  self->priv->secure_result = async_result;
  self->priv->legacy_ssl = old_style_ssl;
  self->priv->peername = g_strdup (peername);
  self->priv->extra_identities = g_strdupv (extra_identities);

  if (old_style_ssl)
    do_handshake (self);
  else
    do_starttls (self);
}

WockyXmppConnection *
wocky_tls_connector_secure_finish (WockyTLSConnector *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_return_copy_pointer (self,
      wocky_tls_connector_secure_async, g_object_ref);
}
