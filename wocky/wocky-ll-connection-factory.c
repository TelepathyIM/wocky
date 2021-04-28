/*
 * wocky-ll-connection-factory.c - Source for WockyLLConnectionFactory
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

#include "wocky-ll-connection-factory.h"

#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_CONNECTION_FACTORY
#include "wocky-debug-internal.h"

/* private structure */
struct _WockyLLConnectionFactoryPrivate
{
  GSocketClient *client;
};

G_DEFINE_TYPE_WITH_CODE (WockyLLConnectionFactory,
                         wocky_ll_connection_factory, G_TYPE_OBJECT,
                         G_ADD_PRIVATE(WockyLLConnectionFactory))

GQuark
wocky_ll_connection_factory_error_quark (void)
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string (
        "wocky_ll_connection_factory_error");

  return quark;
}

static void
wocky_ll_connection_factory_init (WockyLLConnectionFactory *self)
{
  WockyLLConnectionFactoryPrivate *priv;

  priv = self->priv;

  priv->client = g_socket_client_new ();
}

static void
wocky_ll_connection_factory_dispose (GObject *object)
{
  WockyLLConnectionFactory *self = WOCKY_LL_CONNECTION_FACTORY (object);

  g_object_unref (self->priv->client);

  if (G_OBJECT_CLASS (wocky_ll_connection_factory_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_ll_connection_factory_parent_class)->dispose (object);
}

static void
wocky_ll_connection_factory_class_init (
    WockyLLConnectionFactoryClass *wocky_ll_connection_factory_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_ll_connection_factory_class);

  object_class->dispose = wocky_ll_connection_factory_dispose;
}

/**
 * wocky_ll_connection_factory_new:
 *
 * Convenience function to create a new #WockyLLConnectionFactory object.
 *
 * Returns: a newly created instance of #WockyLLConnectionFactory
 */
WockyLLConnectionFactory *
wocky_ll_connection_factory_new (void)
{
  return g_object_new (WOCKY_TYPE_LL_CONNECTION_FACTORY,
      NULL);
}

typedef struct
{
  /* the simple async result will hold a ref to this */
  WockyLLConnectionFactory *self;

  GTask *task;
  GCancellable *cancellable;

  GQueue *addresses;
} NewConnectionData;

static void
free_new_connection_data (NewConnectionData *data)
{
  g_queue_foreach (data->addresses, (GFunc) g_object_unref, NULL);
  g_queue_free (data->addresses);

  if (data->cancellable != NULL)
    g_object_unref (data->cancellable);

  g_object_unref (data->task);
  g_slice_free (NewConnectionData, data);
}

static void process_one_address (NewConnectionData *data);

static void
connect_to_host_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  GSocketClient *client = G_SOCKET_CLIENT (source_object);
  NewConnectionData *data = user_data;
  GSocketConnection *conn;
  GError *error = NULL;
  WockyXmppConnection *connection;

  conn = g_socket_client_connect_to_host_finish (client, result, &error);

  if (conn == NULL)
    {
      DEBUG ("failed to connect: %s", error->message);
      g_clear_error (&error);

      /* shame, well let's move on */
      process_one_address (data);
      return;
    }

  connection = wocky_xmpp_connection_new (G_IO_STREAM (conn));

  DEBUG ("made connection");

  g_task_return_pointer (data->task, connection, NULL);
  free_new_connection_data (data);
}

static void
process_one_address (NewConnectionData *data)
{
  GInetSocketAddress *addr;
  gchar *host;

  if (g_task_return_error_if_cancelled (data->task))
    {
      free_new_connection_data (data);
      return;
    }

  addr = g_queue_pop_head (data->addresses);

  /* check we haven't gotten to the end of the list */
  if (addr == NULL)
    {
      g_task_return_new_error (data->task, WOCKY_LL_CONNECTION_FACTORY_ERROR,
          WOCKY_LL_CONNECTION_FACTORY_ERROR_NO_CONTACT_ADDRESS_CAN_BE_CONNECTED_TO,
          "Failed to connect to any of the contact's addresses");
      free_new_connection_data (data);
      return;
    }

  host = g_inet_address_to_string (g_inet_socket_address_get_address (addr));

  DEBUG ("connecting to %s (port %" G_GUINT16_FORMAT ")", host,
      g_inet_socket_address_get_port (addr));

  g_socket_client_connect_to_host_async (data->self->priv->client,
      host, g_inet_socket_address_get_port (addr),
      data->cancellable, connect_to_host_cb, data);

  g_free (host);

  g_object_unref (addr);
}

static void
add_to_queue (gpointer data,
    gpointer user_data)
{
  GQueue *queue = user_data;

  g_queue_push_tail (queue, data);
}

/**
 * wocky_ll_connection_factory_make_connection_async:
 * @factory: a #WockyLLConnectionFactory
 * @contact: the #WockyLLContact to connect to
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request a connection to @contact.  When the connection has been
 * made, @callback will be called and the caller can then call
 * #wocky_ll_connection_factorymake_connection_finish to get the
 * connection.
 */
void
wocky_ll_connection_factory_make_connection_async (
    WockyLLConnectionFactory *self,
    WockyLLContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  NewConnectionData *data;
  GList *addr;

  g_return_if_fail (WOCKY_IS_LL_CONNECTION_FACTORY (self));
  g_return_if_fail (WOCKY_IS_LL_CONTACT (contact));
  g_return_if_fail (callback != NULL);

  data = g_slice_new0 (NewConnectionData);
  data->self = self;

  if (cancellable != NULL)
    data->cancellable = g_object_ref (cancellable);

  data->task = g_task_new (G_OBJECT (self), NULL, callback, user_data);

  data->addresses = g_queue_new ();

  addr = wocky_ll_contact_get_addresses (contact);
  g_list_foreach (addr, add_to_queue, data->addresses);
  g_list_free (addr);

  if (data->addresses == NULL)
    {
      g_task_return_new_error (data->task,
          WOCKY_LL_CONNECTION_FACTORY_ERROR,
          WOCKY_LL_CONNECTION_FACTORY_ERROR_NO_CONTACT_ADDRESSES,
          "No addresses available for contact");
      free_new_connection_data (data);
      return;
    }

  /* go go go */
  process_one_address (data);
}

/**
 * wocky_ll_connection_factory_make_connection_finish:
 * @factory: a #WockyLLConnectionFactory
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore
 *
 * Gets the connection that's been created.
 *
 * Returns: (transfer full): the new #WockyXmppConnection on success, %NULL on error
 */
WockyXmppConnection *
wocky_ll_connection_factory_make_connection_finish (
    WockyLLConnectionFactory *self,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

