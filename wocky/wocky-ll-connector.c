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

#include "wocky-ll-connector.h"

#include "wocky-utils.h"
#include "wocky-namespaces.h"

#define DEBUG_FLAG DEBUG_CONNECTOR
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyLLConnector, wocky_ll_connector, G_TYPE_OBJECT)

enum
{
  PROP_STREAM = 1,
  PROP_CONNECTION,
  PROP_JID,
  PROP_INCOMING,
};

/* private structure */
struct _WockyLLConnectorPrivate
{
  GIOStream *stream;
  WockyXmppConnection *connection;
  gchar *jid;
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
  WockyLLConnectorPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_LL_CONNECTOR,
      WockyLLConnectorPrivate);
  priv = self->priv;
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
      case PROP_JID:
        priv->jid = g_value_dup_string (value);
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
      case PROP_JID:
        g_value_set_string (value, priv->jid);
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

  g_object_unref (priv->connection);
  priv->connection = NULL;

  g_free (priv->jid);
  priv->jid = NULL;

  g_free (priv->from);
  priv->from = NULL;

  g_object_unref (priv->simple);
  priv->simple = NULL;

  g_object_unref (priv->cancellable);
  priv->cancellable = NULL;

  if (G_OBJECT_CLASS (wocky_ll_connector_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_ll_connector_parent_class)->dispose (object);
}

static void
wocky_ll_connector_constructed (GObject *object)
{
  WockyLLConnector *self = WOCKY_LL_CONNECTOR (object);
  WockyLLConnectorPrivate *priv = self->priv;

  if (priv->connection == NULL)
    priv->connection = wocky_xmpp_connection_new (priv->stream);

  if (G_OBJECT_CLASS (wocky_ll_connector_parent_class)->constructed)
    G_OBJECT_CLASS (wocky_ll_connector_parent_class)->constructed (object);
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

  spec = g_param_spec_string ("jid", "JID",
      "XMPP JID",
      "",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_JID, spec);

  spec = g_param_spec_boolean ("incoming", "Incoming",
      "Whether the connection is incoming",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INCOMING, spec);

  g_type_class_add_private (wocky_ll_connector_class,
      sizeof (WockyLLConnectorPrivate));
}

/**
 * wocky_ll_connector_new:
 * @stream: a #GIOStream
 *
 * Creates a new #WockyLLConnector object.
 *
 * This function should be called if the connection is incoming and
 * the identity of the other side is unknown. For outgoing
 * connections, wocky_ll_connector_new_from_connection() should be
 * used.
 *
 * The ownership of @stream is taken by the connector.
 *
 * The caller should call wocky_ll_connector_connect_async() with the
 * result of this function.
 *
 * Returns: a new #WockyLLConnector object
 */
WockyLLConnector *
wocky_ll_connector_new (GIOStream *stream)
{
  return g_object_new (WOCKY_TYPE_LL_CONNECTOR,
      "stream", stream,
      "incoming", TRUE,
      NULL);
}

/**
 * wocky_ll_connector_new_from_connection:
 * @stream: a #GIOStream
 * @jid: the JID of the local user
 *
 * Creates a new #WockyLLConnector object.
 *
 * This function should be called if the connection is outgoing. For
 * incoming connections, wocky_ll_connector_new() should be used.
 *
 * The ownership of @connection is taken by the connector.
 *
 * The caller should call wocky_ll_connector_connect_async() with the
 * result of this function.
 *
 * Returns: a new #WockyLLConnector object
 */
WockyLLConnector *
wocky_ll_connector_new_from_connection (WockyXmppConnection *connection,
    const gchar *jid)
{
  return g_object_new (WOCKY_TYPE_LL_CONNECTOR,
      "connection", connection,
      "jid", jid,
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
      GError *err = g_error_new (WOCKY_LL_CONNECTOR_ERROR,
          WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_SEND_STANZA,
          "Failed to send stream features: %s", error->message);
      g_clear_error (&error);

      DEBUG ("%s", err->message);

      g_simple_async_result_take_error (priv->simple, err);
    }

  g_simple_async_result_complete (priv->simple);
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
      GError *err = g_error_new (WOCKY_LL_CONNECTOR_ERROR,
          WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_RECEIVE_STANZA,
          "Failed to receive stream open: %s", error->message);
      g_clear_error (&error);

      DEBUG ("%s", err->message);

      g_simple_async_result_take_error (priv->simple, err);
      g_simple_async_result_complete (priv->simple);
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
          priv->jid, "1.0", NULL, NULL, priv->cancellable,
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
      GError *err = g_error_new (WOCKY_LL_CONNECTOR_ERROR,
          WOCKY_LL_CONNECTOR_ERROR_FAILED_TO_SEND_STANZA,
          "Failed to send stream open: %s", error->message);
      g_clear_error (&error);

      DEBUG ("%s", err->message);

      g_simple_async_result_take_error (priv->simple, err);
      g_simple_async_result_complete (priv->simple);
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
      DEBUG ("successfully sent stream open, connection now open");

      g_simple_async_result_complete (priv->simple);
    }
}

/**
 * wocky_ll_connector_connect_async:
 * @connector: the #WockyLLConnector
 * @remote_jid: the JID of the remote user, or %NULL in the incoming case
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback
 * @user_data: some user data to pass to @callback
 *
 * Requests an asynchronous connect using the stream or connection
 * passed to one of the new functions.
 *
 * When the connection has been opened, @callback will be called.
 */
void
wocky_ll_connector_connect_async (WockyLLConnector *self,
    const gchar *remote_jid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyLLConnectorPrivate *priv;

  g_return_if_fail (WOCKY_IS_LL_CONNECTOR (self));

  priv = self->priv;

  g_return_if_fail (priv->simple == NULL);

  priv->simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_ll_connector_connect_async);

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
          remote_jid, priv->jid, "1.0", NULL, NULL, priv->cancellable,
          send_open_cb, self);
    }
}

/**
 * wocky_ll_connector_connect_finish:
 * @connector: the #WockyLLConnector
 * @result: a #GAsyncResult
 * @from: a location to store the remote user's JID, or %NULL
 * @error: a location to save errors to, or %NULL to ignore
 *
 * Gets the result of the asynchronous connect request.
 *
 * After this function is called, @connector can be freed using
 * g_object_unref(). Note that the connection returned from this
 * function must be reffed using g_object_ref() before freeing the
 * connector otherwise the connection will be disposed and will close.
 *
 * Returns: the connected #WockyXmppConnection, or %NULL on error
 */
WockyXmppConnection *
wocky_ll_connector_connect_finish (WockyLLConnector *self,
    GAsyncResult *result,
    gchar **from,
    GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  WockyLLConnectorPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_LL_CONNECTOR (self), NULL);

  priv = self->priv;

  g_return_val_if_fail (priv->simple == simple, NULL);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), wocky_ll_connector_connect_async), NULL);

  if (from != NULL)
    *from = g_strdup (priv->from);

  return priv->connection;
}
