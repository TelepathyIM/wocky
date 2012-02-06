/*
 * wocky-ll-connector.c - Source for WockyLLConnector
 * Copyright (C) 2011 Collabora Ltd.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>

#include "wocky-ll-connector.h"

#include "wocky-utils.h"
#include "wocky-namespaces.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_CONNECTOR
#include "wocky-debug-internal.h"

static void initable_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (WockyLLConnector, wocky_ll_connector, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, initable_iface_init))

enum
{
  PROP_STREAM = 1,
  PROP_CONNECTION,
  PROP_LOCAL_JID,
  PROP_REMOTE_JID,
  PROP_INCOMING,
};

/* private structure */
struct _WockyLLConnectorPrivate
{
  GIOStream *stream;
  WockyXmppConnection *connection;
  gchar *local_jid;
  gchar *remote_jid;
  gboolean incoming;

  gchar *from;

  GSimpleAsyncResult *simple;
  GCancellable *cancellable;
};

GQuark
wocky_ll_connector_error_quark (void)
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string (
        "wocky_ll_connector_error");

  return quark;
}

static void
wocky_ll_connector_init (WockyLLConnector *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_LL_CONNECTOR,
      WockyLLConnectorPrivate);
}

static void
wocky_ll_connector_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyLLConnector *connector = WOCKY_LL_CONNECTOR (object);
  WockyLLConnectorPrivate *priv = connector->priv;

  switch (property_id)
    {
      case PROP_STREAM:
       priv->stream = g_value_get_object (value);
        break;
      case PROP_CONNECTION:
       priv->connection = g_value_get_object (value);
        break;
      case PROP_LOCAL_JID:
        priv->local_jid = g_value_dup_string (value);
        break;
      case PROP_REMOTE_JID:
        priv->remote_jid = g_value_dup_string (value);
        break;
      case PROP_INCOMING:
        priv->incoming = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_ll_connector_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyLLConnector *connector = WOCKY_LL_CONNECTOR (object);
  WockyLLConnectorPrivate *priv = connector->priv;

  switch (property_id)
    {
      case PROP_STREAM:
        g_value_set_object (value, priv->stream);
        break;
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;
      case PROP_LOCAL_JID:
        g_value_set_string (value, priv->local_jid);
        break;
      case PROP_REMOTE_JID:
        g_value_set_string (value, priv->remote_jid);
        break;
      case PROP_INCOMING:
        g_value_set_boolean (value, priv->incoming);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_ll_connector_dispose (GObject *object)
{
  WockyLLConnector *self = WOCKY_LL_CONNECTOR (object);
  WockyLLConnectorPrivate *priv = self->priv;

  DEBUG ("dispose called");

  g_object_unref (priv->connection);
  priv->connection = NULL;

  g_free (priv->local_jid);
  priv->local_jid = NULL;

  g_free (priv->remote_jid);
  priv->remote_jid = NULL;

  g_free (priv->from);
  priv->from = NULL;

  if (priv->cancellable != NULL)
    {
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  if (G_OBJECT_CLASS (wocky_ll_connector_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_ll_connector_parent_class)->dispose (object);
}

static void
wocky_ll_connector_constructed (GObject *object)
{
  WockyLLConnector *self = WOCKY_LL_CONNECTOR (object);
  WockyLLConnectorPrivate *priv = self->priv;

  if (G_OBJECT_CLASS (wocky_ll_connector_parent_class)->constructed)
    G_OBJECT_CLASS (wocky_ll_connector_parent_class)->constructed (object);

  if (priv->connection == NULL)
    priv->connection = wocky_xmpp_connection_new (priv->stream);
}
static void
wocky_ll_connector_class_init (
    WockyLLConnectorClass *wocky_ll_connector_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_ll_connector_class);
  GParamSpec *spec;

  object_class->get_property = wocky_ll_connector_get_property;
  object_class->set_property = wocky_ll_connector_set_property;
  object_class->dispose = wocky_ll_connector_dispose;
  object_class->constructed = wocky_ll_connector_constructed;

  spec = g_param_spec_object ("stream", "XMPP stream",
      "The XMPP stream", G_TYPE_IO_STREAM,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STREAM, spec);

  spec = g_param_spec_object ("connection", "XMPP connection",
      "The XMPP connection", WOCKY_TYPE_XMPP_CONNECTION,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, spec);

  spec = g_param_spec_string ("local-jid", "User's JID",
      "Local user's XMPP JID",
      "",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOCAL_JID, spec);

  spec = g_param_spec_string ("remote-jid", "Contact's JID",
      "Remote contact's XMPP JID",
      "",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REMOTE_JID, spec);

  spec = g_param_spec_boolean ("incoming", "Incoming",
      "Whether the connection is incoming",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INCOMING, spec);

  g_type_class_add_private (wocky_ll_connector_class,
      sizeof (WockyLLConnectorPrivate));
}

/**
 * wocky_ll_connector_incoming_async:
 * @stream: a #GIOStream
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: a function to call when the operation is complete
 * @user_data: data to pass to @callback
 *
 * Start an asychronous connect operation with an incoming link-local
 * connection by negotiating the stream open stanzas and sending
 * stream features.
 *
 * The ownership of @stream is taken by the connector.
 *
 * When the operation is complete, @callback will be called and it
 * should call wocky_ll_connector_finish().
 */
void
wocky_ll_connector_incoming_async (
    GIOStream *stream,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_async_initable_new_async (WOCKY_TYPE_LL_CONNECTOR,
      G_PRIORITY_DEFAULT, cancellable, callback, user_data,
      "stream", stream,
      "incoming", TRUE,
      NULL);
}

/**
 * wocky_ll_connector_outgoing_async:
 * @connection: a #WockyXmppConnection
 * @local_jid: the JID of the local user
 * @remote_jid: the JID of the remote contact
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: a function to call when the operation is complete
 * @user_data: data to pass to @callback
 *
 * Start an asychronous connect operation with an outgoing link-local
 * connection by negotiating the stream open stanzas and sending
 * stream features.
 *
 * The ownership of @connection is taken by the connector.
 *
 * When the operation is complete, @callback will be called and it
 * should call wocky_ll_connector_finish().
 */
void
wocky_ll_connector_outgoing_async (
    WockyXmppConnection *connection,
    const gchar *local_jid,
    const gchar *remote_jid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_async_initable_new_async (WOCKY_TYPE_LL_CONNECTOR,
      G_PRIORITY_DEFAULT, cancellable, callback, user_data,
      "connection", connection,
      "local-jid", local_jid,
      "remote-jid", remote_jid,
      "incoming", FALSE,
      NULL);
}

static void
features_sent_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source_object);
  WockyLLConnector *self = user_data;
  WockyLLConnectorPrivate *priv = self->priv;
  GError *error = NULL;

  if (!wocky_xmpp_connection_send_stanza_finish (connection, result, &error))
    {
      DEBUG ("Failed to send stream features: %s", error->message);

      g_simple_async_result_set_error (priv->simple, WOCKY_LL_CONNECTOR_ERROR,
          WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_SEND_STANZA,
          "Failed to send stream features: %s", error->message);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (priv->simple);
  g_object_unref (priv->simple);
  priv->simple = NULL;

  g_object_unref (self);
}

static void send_open_cb (GObject *source_object,
    GAsyncResult *result, gpointer user_data);

static void
recv_open_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source_object);
  GError *error = NULL;
  WockyLLConnector *self = user_data;
  WockyLLConnectorPrivate *priv = self->priv;
  gchar *from = NULL;

  if (!wocky_xmpp_connection_recv_open_finish (connection, result,
          NULL, &from, NULL, NULL, NULL, &error))
    {
      DEBUG ("Failed to receive stream open: %s", error->message);

      g_simple_async_result_set_error (priv->simple, WOCKY_LL_CONNECTOR_ERROR,
          WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_RECEIVE_STANZA,
          "Failed to receive stream open: %s", error->message);
      g_clear_error (&error);

      g_simple_async_result_complete (priv->simple);
      g_object_unref (priv->simple);
      priv->simple = NULL;

      return;
    }

  if (!priv->incoming)
    {
      WockyStanza *features;

      DEBUG ("connected, sending stream features but not "
          "expecting anything back");

      features = wocky_stanza_new ("features", WOCKY_XMPP_NS_STREAM);
      wocky_xmpp_connection_send_stanza_async (connection,
          features, NULL, features_sent_cb, self);
      g_object_unref (features);
    }
  else
    {
      DEBUG ("stream opened from %s, sending open back",
          from != NULL ? from : "<no from attribute>");

      wocky_xmpp_connection_send_open_async (connection, from,
          priv->local_jid, "1.0", NULL, NULL, priv->cancellable,
          send_open_cb, self);
    }

  priv->from = from;
}

static void
send_open_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source_object);
  GError *error = NULL;
  WockyLLConnector *self = user_data;
  WockyLLConnectorPrivate *priv = self->priv;

  if (!wocky_xmpp_connection_send_open_finish (connection, result, &error))
    {
      DEBUG ("Failed to send stream open: %s", error->message);

      g_simple_async_result_set_error (priv->simple, WOCKY_LL_CONNECTOR_ERROR,
          WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_SEND_STANZA,
          "Failed to send stream open: %s", error->message);
      g_clear_error (&error);

      g_simple_async_result_complete (priv->simple);
      g_object_unref (priv->simple);
      priv->simple = NULL;
      return;
    }

  if (!priv->incoming)
    {
      DEBUG ("successfully sent stream open, now waiting for other side to too");

      wocky_xmpp_connection_recv_open_async (connection, priv->cancellable,
          recv_open_cb, self);
    }
  else
    {
      WockyStanza *features;

      DEBUG ("connected, sending stream features but not "
          "expecting anything back");

      features = wocky_stanza_new ("features", WOCKY_XMPP_NS_STREAM);
      wocky_xmpp_connection_send_stanza_async (connection,
          features, NULL, features_sent_cb, self);
      g_object_unref (features);
    }
}

/**
 * wocky_ll_connector_finish:
 * @connector: a #WockyLLConnector
 * @result: a #GAsyncResult
 * @from: a location to store the remote user's JID, or %NULL
 * @error: a location to save errors to, or %NULL to ignore
 *
 * Gets the result of the asynchronous connect request.
 *
 * Returns: the connected #WockyXmppConnection which should be freed
 *   using g_object_unref(), or %NULL on error
 */
WockyXmppConnection *
wocky_ll_connector_finish (WockyLLConnector *self,
    GAsyncResult *result,
    gchar **from,
    GError **error)
{
  WockyLLConnectorPrivate *priv = self->priv;

  if (g_async_initable_new_finish (G_ASYNC_INITABLE (self),
          result, error) == NULL)
    return NULL;

  if (from != NULL)
    *from = g_strdup (priv->from);

  return g_object_ref (priv->connection);
}

static void
wocky_ll_connector_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyLLConnector *self = WOCKY_LL_CONNECTOR (initable);
  WockyLLConnectorPrivate *priv = self->priv;

  g_return_if_fail (priv->simple == NULL);

  priv->simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_ll_connector_init_async);

  if (cancellable != NULL)
    priv->cancellable = g_object_ref (cancellable);

  if (priv->incoming)
    {
      /* we need to wait for stream open first */
      wocky_xmpp_connection_recv_open_async (priv->connection,
          priv->cancellable, recv_open_cb, self);
    }
  else
    {
      /* we need to send stream open first */
      wocky_xmpp_connection_send_open_async (priv->connection,
          priv->remote_jid, priv->local_jid, "1.0", NULL, NULL, priv->cancellable,
          send_open_cb, self);
    }
}

static gboolean
wocky_ll_connector_init_finish (GAsyncInitable *initable,
    GAsyncResult *result,
    GError **error)
{
  WockyLLConnector *self = WOCKY_LL_CONNECTOR (initable);
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  WockyLLConnectorPrivate *priv = self->priv;

  g_return_val_if_fail (priv->simple == simple, FALSE);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), wocky_ll_connector_init_async), FALSE);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface,
    gpointer data)
{
  GAsyncInitableIface *iface = g_iface;

  iface->init_async = wocky_ll_connector_init_async;
  iface->init_finish = wocky_ll_connector_init_finish;
}
