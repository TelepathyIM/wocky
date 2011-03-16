/*
 * wocky-meta-porter.c - Source for WockyMetaPorter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the tubesplied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "wocky-meta-porter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wocky-ll-connection-factory.h"
#include "wocky-contact-factory.h"
#include "wocky-c2s-porter.h"
#include "wocky-utils.h"
#include "wocky-ll-contact.h"
#include "wocky-ll-connector.h"
#include "wocky-loopback-stream.h"

#define DEBUG_FLAG DEBUG_PORTER
#include "wocky-debug.h"

static void wocky_porter_iface_init (gpointer g_iface,
    gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (WockyMetaPorter, wocky_meta_porter, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_PORTER,
        wocky_porter_iface_init));

/* properties */
enum
{
  PROP_JID = 1,
  PROP_CONTACT_FACTORY,
  PROP_CONNECTION,
  PROP_RESOURCE,
};

/* private structure */
struct _WockyMetaPorterPrivate
{
  gchar *jid;
  WockyContactFactory *contact_factory;
  WockyLLConnectionFactory *connection_factory;

  /* owned (gchar *) jid => owned (PorterData *) */
  GHashTable *porters;

  /* guint handler id => owned (StanzaHandler *) */
  GHashTable *handlers;

  GSocketService *listener;

  guint16 port;

  guint next_handler_id;
};

typedef struct
{
  WockyMetaPorter *self;
  WockyContact *contact;
  /* owned */
  WockyPorter *porter;
  /* also owned, for convenience */
  gchar *jid;
  guint refcount;
  guint timeout_id;
} PorterData;

typedef struct
{
  WockyMetaPorter *self;
  WockyContact *contact;

  /* weak reffed WockyPorter* => handler ID */
  GHashTable *porters;

  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  guint priority;
  WockyPorterHandlerFunc callback;
  gpointer user_data;
  WockyStanza *stanza;
} StanzaHandler;

GQuark
wocky_meta_porter_error_quark (void)
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string (
        "wocky_meta_porter_error");

  return quark;
}

static void register_porter_handlers (WockyMetaPorter *self,
    WockyPorter *porter, WockyContact *contact);

static void
porter_data_free (gpointer data)
{
  PorterData *p = data;

  if (p->porter != NULL)
    g_object_unref (p->porter);

  if (p->timeout_id > 0)
    g_source_remove (p->timeout_id);

  g_free (p->jid);

  g_slice_free (PorterData, data);
}

static void
porter_closed_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source_object);
  GError *error = NULL;
  PorterData *data = user_data;

  if (!wocky_porter_close_finish (porter, result, &error))
    {
      DEBUG ("Failed to close porter to '%s': %s", data->jid, error->message);
      g_clear_error (&error);
    }
  else
    {
      DEBUG ("Closed porter to '%s'", data->jid);
    }

  porter_data_free (data);
}

static gboolean
porter_timeout_cb (gpointer d)
{
  PorterData *data = d;
  WockyMetaPorterPrivate *priv = data->self->priv;

  data->timeout_id = 0;

  g_hash_table_steal (priv->porters, data->contact);

  /* we need to unref this ourselves as we just stole it from the hash
   * table */
  g_object_unref (data->contact);

  if (data->porter != NULL)
    wocky_porter_close_async (data->porter, NULL, porter_closed_cb, data);
  else
    porter_data_free (data);

  return FALSE;
}

static void porter_remote_closed_cb (WockyPorter *porter, PorterData *data);

static void
porter_closing_cb (WockyPorter *porter,
    PorterData *data)
{
  DEBUG ("porter to '%s' closing, remove it from our records", data->jid);

  if (data->timeout_id > 0)
    g_source_remove (data->timeout_id);

  data->timeout_id = 0;

  g_signal_handlers_disconnect_by_func (porter,
      porter_remote_closed_cb, data);
  g_signal_handlers_disconnect_by_func (porter,
      porter_closing_cb, data);

  if (data->porter != NULL)
    g_object_unref (data->porter);
  data->porter = NULL;
}

static void
porter_remote_closed_cb (WockyPorter *porter,
    PorterData *data)
{
  DEBUG ("porter closed by remote, remove it from our records");

  if (data->timeout_id > 0)
    g_source_remove (data->timeout_id);

  data->timeout_id = 0;

  g_signal_handlers_disconnect_by_func (porter,
      porter_remote_closed_cb, data);
  g_signal_handlers_disconnect_by_func (porter,
      porter_closing_cb, data);

  if (data->porter != NULL)
    g_object_unref (data->porter);
  data->porter = NULL;
}

static void
maybe_start_timeout (PorterData *data)
{
  if (data->refcount == 0)
    {
      DEBUG ("Started porter timeout...");
      data->timeout_id = g_timeout_add_seconds (5, porter_timeout_cb, data);
    }
}

static WockyPorter *
create_porter (WockyMetaPorter *self,
    WockyXmppConnection *connection,
    WockyContact *contact)
{
  WockyMetaPorterPrivate *priv = self->priv;
  PorterData *data;

  data = g_hash_table_lookup (priv->porters, contact);

  if (data != NULL)
    {
      g_assert (data->porter == NULL);

      data->porter = wocky_c2s_porter_new (connection, priv->jid);
    }
  else
    {
      data = g_slice_new0 (PorterData);

      data->self = self;
      data->contact = contact; /* already will be reffed as the key */
      data->jid = wocky_contact_dup_jid (contact);
      data->porter = wocky_c2s_porter_new (connection, priv->jid);
      data->refcount = 0;
      data->timeout_id = 0;

      g_hash_table_insert (priv->porters, g_object_ref (contact), data);
    }

  g_signal_connect (data->porter, "closing", G_CALLBACK (porter_closing_cb),
      data);
  g_signal_connect (data->porter, "remote-closed",
      G_CALLBACK (porter_remote_closed_cb), data);

  register_porter_handlers (self, data->porter, contact);
  wocky_porter_start (data->porter);

  /* maybe start the timeout */
  maybe_start_timeout (data);

  return data->porter;
}

/**
 * wocky_meta_porter_hold:
 * @porter: a #WockyMetaPorter
 * @contact: a #WockyContact
 *
 * Increases the hold count of the porter to @contact by
 * one. This means that if there is a connection open to @contact then
 * it will not disconnected after a timeout. Note that calling this
 * function does not mean a connection will be opened. The hold
 * count on a contact survives across connections.
 *
 * To decrement the hold count of the porter to @contact, one
 * must call wocky_meta_porter_unhold().
 */
void
wocky_meta_porter_hold (WockyMetaPorter *self,
    WockyContact *contact)
{
  WockyMetaPorterPrivate *priv = self->priv;
  PorterData *data;

  g_return_if_fail (WOCKY_IS_META_PORTER (self));

  data = g_hash_table_lookup (priv->porters, contact);

  if (data == NULL)
    {
      data = g_slice_new0 (PorterData);
      data->self = self;
      data->contact = contact;
      data->jid = wocky_contact_dup_jid (contact);
      data->porter = NULL;
      data->refcount = 0;
      data->timeout_id = 0;

      g_hash_table_insert (priv->porters, g_object_ref (contact), data);
    }

  DEBUG ("Porter to '%s' refcount %u --> %u", data->jid,
      data->refcount, data->refcount + 1);

  data->refcount++;

  if (data->timeout_id > 0)
    {
      g_source_remove (data->timeout_id);
      data->timeout_id = 0;
    }
}

/**
 * wocky_meta_porter_unhold:
 * @porter: a #WockyMetaPorter
 * @contact: a #WockyContact
 *
 * Decreases the hold count of the porter to @contact by
 * one. This means that if there is a connection open to @contact and
 * the hold count is zero, a connection timeout will be
 * started.
 */
void
wocky_meta_porter_unhold (WockyMetaPorter *self,
    WockyContact *contact)
{
  WockyMetaPorterPrivate *priv;
  PorterData *data;

  g_return_if_fail (WOCKY_IS_META_PORTER (self));

  priv = self->priv;

  data = g_hash_table_lookup (priv->porters, contact);

  if (data == NULL)
    return;

  DEBUG ("Porter to '%s' refcount %u --> %u", data->jid,
      data->refcount, data->refcount - 1);

  data->refcount--;

  maybe_start_timeout (data);
}

static void
wocky_meta_porter_init (WockyMetaPorter *self)
{
  WockyMetaPorterPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      WOCKY_TYPE_META_PORTER, WockyMetaPorterPrivate);

  self->priv = priv;
}

static void
new_connection_connect_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyLLConnector *connector = WOCKY_LL_CONNECTOR (source);
  WockyXmppConnection *connection;
  GError *error = NULL;
  WockyMetaPorter *self = user_data;
  WockyMetaPorterPrivate *priv = self->priv;
  GList *contacts, *l;
  WockyLLContact *contact = NULL;
  gchar *from;

  connection = wocky_ll_connector_finish (connector, result,
      &from, &error);

  if (connection == NULL)
    {
      DEBUG ("connection error: %s", error->message);
      g_clear_error (&error);
      return;
    }

  if (from != NULL)
    {
      contact = wocky_contact_factory_ensure_ll_contact (priv->contact_factory,
          from);
    }

  if (contact == NULL)
    {
      GSocketConnection *socket_connection;
      GSocketAddress *socket_address;
      GInetAddress *addr;

      /* we didn't get a from attribute in the stream open */

      g_object_get (connection,
          "stream", &socket_connection,
          NULL);

      socket_address = g_socket_connection_get_remote_address (
          socket_connection, NULL);

      addr = g_inet_socket_address_get_address (
          G_INET_SOCKET_ADDRESS (socket_address));

      contacts = wocky_contact_factory_get_ll_contacts (priv->contact_factory);

      for (l = contacts; l != NULL; l = l->next)
        {
          WockyLLContact *c = l->data;

          if (wocky_ll_contact_has_address (c, addr))
            {
              contact = g_object_ref (c);
              break;
            }
        }

      g_list_free (contacts);
      g_object_unref (socket_address);
      g_object_unref (socket_connection);
    }

  if (contact != NULL)
    {
      create_porter (self, connection, WOCKY_CONTACT (contact));
    }
  else
    {
      DEBUG ("Failed to find contact for new connection, let it close");
    }

  g_object_unref (connection);
}

static gboolean
_new_connection (GSocketService *service,
    GSocketConnection *socket,
    GObject *source_object,
    gpointer user_data)
{
  WockyMetaPorter *self = user_data;

  DEBUG ("new connection!");

  wocky_ll_connector_incoming_async (G_IO_STREAM (socket),
      NULL, new_connection_connect_cb, self);

  return TRUE;
}

static void stanza_handler_porter_disposed_cb (gpointer data, GObject *porter);

static void
free_handler (gpointer data)
{
  StanzaHandler *handler = data;
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, handler->porters);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      WockyPorter *porter = key;
      guint id = GPOINTER_TO_UINT (value);

      wocky_porter_unregister_handler (porter, id);

      g_object_weak_unref (G_OBJECT (porter),
          stanza_handler_porter_disposed_cb, handler);
    }

  g_hash_table_destroy (handler->porters);
  if (handler->contact != NULL)
    g_object_unref (handler->contact);
  g_object_unref (handler->stanza);
  g_slice_free (StanzaHandler, handler);
}

static void
loopback_recv_open_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source_object);
  WockyMetaPorter *self = user_data;
  WockyMetaPorterPrivate *priv = self->priv;
  WockyLLContact *contact;
  GError *error = NULL;

  if (!wocky_xmpp_connection_recv_open_finish (connection, result,
          NULL, NULL, NULL, NULL, NULL, &error))
    {
      DEBUG ("Failed to receive stream open from loopback stream: %s", error->message);
      g_clear_error (&error);
      g_object_unref (connection);
      return;
    }

  contact = wocky_contact_factory_ensure_ll_contact (
      priv->contact_factory, priv->jid);

  /* the ref, the porter and the connection will all be freed when the
   * meta porter is freed */
  create_porter (self, connection, WOCKY_CONTACT (contact));
  wocky_meta_porter_hold (self, WOCKY_CONTACT (contact));

  g_object_unref (contact);
  g_object_unref (connection);
}

static void
loopback_sent_open_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source_object);
  WockyMetaPorter *self = user_data;
  GError *error = NULL;

  if (!wocky_xmpp_connection_send_open_finish (connection, result, &error))
    {
      DEBUG ("Failed to send stream open to loopback stream: %s", error->message);
      g_clear_error (&error);
      g_object_unref (connection);
      return;
    }

  wocky_xmpp_connection_recv_open_async (connection, NULL,
      loopback_recv_open_cb, self);
}

static void
create_loopback_porter (WockyMetaPorter *self)
{
  WockyMetaPorterPrivate *priv = self->priv;
  GIOStream *stream;
  WockyXmppConnection *connection;

  if (priv->jid == NULL)
    return;

  stream = wocky_loopback_stream_new ();
  connection = wocky_xmpp_connection_new (stream);

  /* really simple connector */
  wocky_xmpp_connection_send_open_async (connection, NULL, NULL, NULL,
      NULL, NULL, NULL, loopback_sent_open_cb, self);

  g_object_unref (stream);
}

static void
wocky_meta_porter_constructed (GObject *obj)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (obj);
  WockyMetaPorterPrivate *priv = self->priv;

  priv->listener = g_socket_service_new ();
  g_signal_connect (priv->listener, "incoming",
      G_CALLBACK (_new_connection), self);

  priv->next_handler_id = 1;

  priv->connection_factory = wocky_ll_connection_factory_new ();

  priv->porters = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      g_object_unref, porter_data_free);

  priv->handlers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, free_handler);

  /* Create the loopback porter */
  if (priv->jid != NULL)
    create_loopback_porter (self);
}

static void
wocky_meta_porter_finalize (GObject *object)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (object);
  WockyMetaPorterPrivate *priv = self->priv;

  g_free (priv->jid);
  priv->jid = NULL;

  if (G_OBJECT_CLASS (wocky_meta_porter_parent_class)->finalize)
    G_OBJECT_CLASS (wocky_meta_porter_parent_class)->finalize (object);
}

static void
wocky_meta_porter_dispose (GObject *object)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (object);
  WockyMetaPorterPrivate *priv = self->priv;

  g_object_unref (priv->contact_factory);
  g_object_unref (priv->connection_factory);

  g_socket_service_stop (priv->listener);
  g_object_unref (priv->listener);

  g_hash_table_destroy (priv->porters);
  g_hash_table_destroy (priv->handlers);

  if (G_OBJECT_CLASS (wocky_meta_porter_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_meta_porter_parent_class)->dispose (object);
}

static void
wocky_meta_porter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (object);
  WockyMetaPorterPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_JID:
        g_value_set_string (value, priv->jid);
        break;
      case PROP_CONTACT_FACTORY:
        g_value_set_object (value, priv->contact_factory);
        break;
      case PROP_CONNECTION:
        /* nothing; just here to implement WockyPorter */
        g_value_set_object (value, NULL);
        break;
      case PROP_RESOURCE:
        /* nothing; just here to implement WockyPorter */
        g_value_set_string (value, NULL);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_meta_porter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (object);
  WockyMetaPorterPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_JID:
        priv->jid = g_value_dup_string (value);
        break;
      case PROP_CONTACT_FACTORY:
        priv->contact_factory = g_value_dup_object (value);
        break;
      case PROP_CONNECTION:
        /* nothing; just here to implement WockyPorter */
        break;
      case PROP_RESOURCE:
        /* nothing; just here to implement WockyPorter */
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_meta_porter_class_init (
    WockyMetaPorterClass *wocky_meta_porter_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_meta_porter_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_meta_porter_class,
      sizeof (WockyMetaPorterPrivate));

  object_class->dispose = wocky_meta_porter_dispose;
  object_class->finalize = wocky_meta_porter_finalize;
  object_class->constructed = wocky_meta_porter_constructed;

  object_class->get_property = wocky_meta_porter_get_property;
  object_class->set_property = wocky_meta_porter_set_property;

  /**
   * WockyMetaPorter:contact-factory:
   *
   * The #WockyContactFactory object in use by this meta porter.
   */
  param_spec = g_param_spec_object ("contact-factory",
      "Contact factory", "WockyContactFactory object in use",
      WOCKY_TYPE_CONTACT_FACTORY,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT_FACTORY,
      param_spec);

  g_object_class_override_property (object_class,
      PROP_CONNECTION, "connection");
  g_object_class_override_property (object_class,
      PROP_JID, "full-jid");
  g_object_class_override_property (object_class,
      PROP_JID, "bare-jid");
  g_object_class_override_property (object_class,
      PROP_RESOURCE, "resource");
}

/**
 * wocky_meta_porter_new:
 * @jid: the JID of the local user, or %NULL
 * @contact_factory: a #WockyContactFactory object
 *
 * Convenience function to create a new #WockyMetaPorter object. The
 * JID can be set later by using wocky_meta_porter_set_jid().
 *
 * Returns: a new #WockyMetaPorter
 */
WockyPorter *
wocky_meta_porter_new (const gchar *jid,
    WockyContactFactory *contact_factory)
{
  g_return_val_if_fail (WOCKY_IS_CONTACT_FACTORY (contact_factory), NULL);

  return g_object_new (WOCKY_TYPE_META_PORTER,
      "full-jid", jid,
      "contact-factory", contact_factory,
      NULL);
}

static const gchar *
wocky_meta_porter_get_jid (WockyPorter *porter)
{
  WockyMetaPorter *self;

  g_return_val_if_fail (WOCKY_IS_META_PORTER (porter), NULL);

  self = (WockyMetaPorter *) porter;

  return self->priv->jid;
}

static const gchar *
wocky_meta_porter_get_resource (WockyPorter *porter)
{
  return NULL;
}

typedef void (*OpenPorterIfNecessaryFunc) (WockyMetaPorter *self,
    WockyPorter *porter,
    GCancellable *cancellable,
    const GError *error,
    GSimpleAsyncResult *simple,
    gpointer user_data);

typedef struct
{
  WockyMetaPorter *self;
  WockyLLContact *contact;
  OpenPorterIfNecessaryFunc callback;
  GCancellable *cancellable;
  GSimpleAsyncResult *simple;
  gpointer user_data;
} OpenPorterData;

static void
made_connection_connect_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyLLConnector *connector = WOCKY_LL_CONNECTOR (source_object);
  WockyXmppConnection *connection;
  GError *error = NULL;
  OpenPorterData *data = user_data;
  WockyPorter *porter;

  connection = wocky_ll_connector_finish (connector,
      result, NULL, &error);

  if (connection == NULL)
    {
      DEBUG ("failed to connect: %s", error->message);
      data->callback (data->self, NULL, NULL, error,
          data->simple, data->user_data);
      g_clear_error (&error);
      goto out;
    }

  DEBUG ("connected");

  porter = create_porter (data->self, connection, WOCKY_CONTACT (data->contact));

  data->callback (data->self, porter, data->cancellable, NULL,
      data->simple, data->user_data);

  g_object_unref (connection);

out:
  g_object_unref (data->contact);
  g_slice_free (OpenPorterData, data);
}

static void
make_connection_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyLLConnectionFactory *factory = WOCKY_LL_CONNECTION_FACTORY (source_object);
  WockyXmppConnection *connection;
  GError *error = NULL;
  OpenPorterData *data = user_data;
  WockyMetaPorterPrivate *priv = data->self->priv;
  gchar *jid;

  connection = wocky_ll_connection_factory_make_connection_finish (factory, result, &error);

  if (connection == NULL)
    {
      DEBUG ("making connection failed: %s", error->message);

      data->callback (data->self, NULL, NULL, error,
          data->simple, data->user_data);

      g_clear_error (&error);

      g_object_unref (data->contact);
      g_slice_free (OpenPorterData, data);
      return;
    }

  jid = wocky_contact_dup_jid (WOCKY_CONTACT (data->contact));

  wocky_ll_connector_outgoing_async (connection, priv->jid,
      jid, data->cancellable, made_connection_connect_cb, data);

  g_free (jid);
}

/* Convenience function to call @callback with a porter and do all the
 * handling the creating a porter if necessary. */
static void
open_porter_if_necessary (WockyMetaPorter *self,
    WockyLLContact *contact,
    GCancellable *cancellable,
    OpenPorterIfNecessaryFunc callback,
    GSimpleAsyncResult *simple,
    gpointer user_data)
{
  WockyMetaPorterPrivate *priv = self->priv;
  PorterData *porter_data = g_hash_table_lookup (priv->porters, contact);
  OpenPorterData *data;

  if (porter_data != NULL && porter_data->porter != NULL)
    {
      callback (self, porter_data->porter, cancellable, NULL, simple, user_data);
      return;
    }

  data = g_slice_new0 (OpenPorterData);
  data->self = self;
  data->contact = g_object_ref (contact);
  data->callback = callback;
  data->cancellable = cancellable;
  data->simple = simple;
  data->user_data = user_data;

  wocky_ll_connection_factory_make_connection_async (priv->connection_factory,
      contact, cancellable, make_connection_cb, data);
}

static void
meta_porter_send_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = user_data;
  GError *error = NULL;

  if (!wocky_porter_send_finish (WOCKY_PORTER (source_object), result, &error))
    g_simple_async_result_take_error (simple, error);

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static void
meta_porter_send_got_porter_cb (WockyMetaPorter *self,
    WockyPorter *porter,
    GCancellable *cancellable,
    const GError *error,
    GSimpleAsyncResult *simple,
    gpointer user_data)
{
  WockyStanza *stanza = user_data;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
  else
    {
      wocky_porter_send_async (porter, stanza, cancellable,
          meta_porter_send_cb, simple);
    }

  g_object_unref (stanza);
}

static void
wocky_meta_porter_send_async (WockyPorter *porter,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);
  WockyMetaPorterPrivate *priv = self->priv;
  GSimpleAsyncResult *simple;
  WockyContact *to;

  simple = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      wocky_meta_porter_send_async);

  to = wocky_stanza_get_to_contact (stanza);

  g_return_if_fail (WOCKY_IS_LL_CONTACT (to));

  /* stamp on from if there is none */
  if (wocky_stanza_get_from (stanza) == NULL)
    {
      wocky_node_set_attribute (wocky_stanza_get_top_node (stanza),
          "from", priv->jid);
    }

  open_porter_if_necessary (self, WOCKY_LL_CONTACT (to), cancellable,
      meta_porter_send_got_porter_cb, simple, g_object_ref (stanza));
}

static gboolean
wocky_meta_porter_send_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (WOCKY_IS_META_PORTER (self), FALSE);

  wocky_implement_finish_void (self, wocky_meta_porter_send_async);
}

static guint16
wocky_meta_porter_listen (WockyMetaPorter *self,
    GError **error)
{
  WockyMetaPorterPrivate *priv = self->priv;
  guint16 port;

  /* The port 5298 is preferred to remain compatible with old versions of
   * iChat. Try a few close to it, and if those fail, use a random port. */
  for (port = 5298; port < 5300; port++)
    {
      GError *e = NULL;

      if (g_socket_listener_add_inet_port (G_SOCKET_LISTENER (priv->listener),
              port, NULL, &e))
        break;

      if (!g_error_matches (e, G_IO_ERROR,
              G_IO_ERROR_ADDRESS_IN_USE))
        {
          g_propagate_error (error, e);
          return 0;
        }

      g_error_free (e);
      e = NULL;
    }

  if (port < 5300)
    return port;

  return g_socket_listener_add_any_inet_port (G_SOCKET_LISTENER (priv->listener),
      NULL, error);
}

static void
wocky_meta_porter_start (WockyPorter *porter)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);
  WockyMetaPorterPrivate *priv = self->priv;
  GError *error = NULL;
  guint16 port;

  port = wocky_meta_porter_listen (self, &error);

  if (error != NULL)
    {
      DEBUG ("Failed to listen: %s", error->message);
      g_clear_error (&error);
    }

  DEBUG ("listening on port %u", port);

  g_socket_service_start (G_SOCKET_SERVICE (priv->listener));

  priv->port = port;
}

static gboolean
porter_handler_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  StanzaHandler *handler = user_data;
  WockyMetaPorter *self = handler->self;
  WockyMetaPorterPrivate *priv = self->priv;
  WockyLLContact *contact;
  const gchar *from;

  from = wocky_stanza_get_from (stanza);

  contact = wocky_contact_factory_ensure_ll_contact (
      priv->contact_factory, from);

  wocky_stanza_set_from_contact (stanza, WOCKY_CONTACT (contact));
  g_object_unref (contact);

  return handler->callback (WOCKY_PORTER (handler->self),
      stanza, handler->user_data);
}

static void
stanza_handler_porter_disposed_cb (gpointer data,
    GObject *porter)
{
  StanzaHandler *handler = data;

  g_hash_table_remove (handler->porters, porter);
}

static void
register_porter_handler (StanzaHandler *handler,
    WockyPorter *porter)
{
  guint id;

  g_assert (g_hash_table_lookup (handler->porters, porter) == NULL);

  if (handler->contact != NULL)
    {
      gchar *jid = wocky_contact_dup_jid (handler->contact);

      id = wocky_porter_register_handler_from_by_stanza (porter,
          handler->type, handler->sub_type, jid,
          handler->priority, porter_handler_cb, handler,
          handler->stanza);

      g_free (jid);
    }
  else
    {
      id = wocky_porter_register_handler_from_anyone_by_stanza (porter,
          handler->type, handler->sub_type,
          handler->priority, porter_handler_cb, handler,
          handler->stanza);
    }

  g_hash_table_insert (handler->porters, porter, GUINT_TO_POINTER (id));

  g_object_weak_ref (G_OBJECT (porter),
      stanza_handler_porter_disposed_cb, handler);
}

static void
register_porter_handlers (WockyMetaPorter *self,
    WockyPorter *porter,
    WockyContact *contact)
{
  WockyMetaPorterPrivate *priv = self->priv;
  GList *handlers, *l;

  handlers = g_hash_table_get_values (priv->handlers);

  for (l = handlers; l != NULL; l = l->next)
    {
      StanzaHandler *handler = l->data;

      if (contact == handler->contact || handler->contact == NULL)
        register_porter_handler (handler, porter);
    }

  g_list_free (handlers);
}

static guint
wocky_meta_porter_register_handler_from_by_stanza (WockyPorter *porter,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *jid,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);
  WockyMetaPorterPrivate *priv = self->priv;
  PorterData *porter_data;
  guint id;
  StanzaHandler *handler;
  WockyLLContact *from;

  g_return_val_if_fail (jid != NULL, 0);

  from = wocky_contact_factory_lookup_ll_contact (
      priv->contact_factory, jid);

  g_return_val_if_fail (WOCKY_IS_LL_CONTACT (from), 0);

  handler = g_slice_new0 (StanzaHandler);
  handler->self = self;
  handler->contact = g_object_ref (from);
  handler->porters = g_hash_table_new (g_direct_hash, g_direct_equal);

  handler->type = type;
  handler->sub_type = sub_type;
  handler->priority = priority;
  handler->callback = callback;
  handler->user_data = user_data;
  handler->stanza = g_object_ref (stanza);

  id = priv->next_handler_id++;

  porter_data = g_hash_table_lookup (priv->porters, from);
  if (porter_data != NULL && porter_data->porter != NULL)
    register_porter_handler (handler, porter_data->porter);

  g_hash_table_insert (priv->handlers, GUINT_TO_POINTER (id), handler);

  return id;
}

static guint
wocky_meta_porter_register_handler_from_anyone_by_stanza (WockyPorter *porter,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);
  WockyMetaPorterPrivate *priv = self->priv;
  PorterData *porter_data;
  guint id;
  StanzaHandler *handler;
  GList *porters, *l;

  handler = g_slice_new0 (StanzaHandler);
  handler->self = self;
  handler->porters = g_hash_table_new (g_direct_hash, g_direct_equal);

  handler->type = type;
  handler->sub_type = sub_type;
  handler->priority = priority;
  handler->callback = callback;
  handler->user_data = user_data;
  handler->stanza = g_object_ref (stanza);

  id = priv->next_handler_id++;

  /* register on all porters */
  porters = g_hash_table_get_values (priv->porters);

  for (l = porters; l != NULL; l = l->next)
    {
      porter_data = l->data;

      if (porter_data->porter != NULL)
        register_porter_handler (handler, porter_data->porter);
    }

  g_list_free (porters);

  g_hash_table_insert (priv->handlers, GUINT_TO_POINTER (id), handler);

  return id;
}

static void
wocky_meta_porter_unregister_handler (WockyPorter *porter,
    guint id)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);
  WockyMetaPorterPrivate *priv = self->priv;

  g_hash_table_remove (priv->handlers, GUINT_TO_POINTER (id));
}

typedef gboolean (* ClosePorterFinishFunc) (WockyPorter *,
    GAsyncResult *, GError **);
typedef void (* ClosePorterAsyncFunc) (WockyPorter *,
    GCancellable *, GAsyncReadyCallback, gpointer);


typedef struct
{
  GSimpleAsyncResult *simple;
  guint remaining;
  gboolean failed;
  ClosePorterFinishFunc close_finish;
} ClosePorterData;

static void
porter_close_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyPorter *porter = WOCKY_PORTER (source_object);
  GError *error = NULL;
  ClosePorterData *data = user_data;


  if (!data->close_finish (porter, result, &error))
    {
      DEBUG ("Failed to close porter: %s", error->message);
      g_clear_error (&error);
      data->failed = TRUE;
    }

  data->remaining--;

  if (data->remaining > 0)
    return;

  /* all porters have now replied */

  if (data->failed)
    {
      GError *err = g_error_new (WOCKY_META_PORTER_ERROR,
          WOCKY_META_PORTER_ERROR_FAILED_TO_CLOSE,
          "Failed to close at least one porter");

      g_simple_async_result_take_error (data->simple, err);
    }

  g_simple_async_result_complete (data->simple);

  g_object_unref (data->simple);
  g_slice_free (ClosePorterData, data);
}

static void
close_all_porters (WockyMetaPorter *self,
    ClosePorterAsyncFunc close_async_func,
    ClosePorterFinishFunc close_finish_func,
    gpointer source_tag,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyMetaPorterPrivate *priv = self->priv;
  GSimpleAsyncResult *simple;
  GList *porters, *l;
  ClosePorterData *data;
  guint num;

  porters = g_hash_table_get_values (priv->porters);
  num = g_list_length (porters);

  simple = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, source_tag);

  g_signal_emit_by_name (self, "closing");

  if (num == 0)
    goto out;

  data = g_slice_new0 (ClosePorterData);
  data->close_finish = close_finish_func;
  data->remaining = num;
  data->simple = simple;

  porters = g_hash_table_get_values (priv->porters);

  for (l = porters; l != NULL; l = l->next)
    {
      PorterData *porter_data = l->data;

      /* NULL if there's a refcount but no porter */
      if (porter_data->porter == NULL)
        continue;

      close_async_func (porter_data->porter, cancellable,
          porter_close_cb, data);
    }

out:
  g_list_free (porters);

  /* there were no porters to close anyway */
  if (num == 0)
    {
      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
}

static void
wocky_meta_porter_close_async (WockyPorter *porter,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);

  close_all_porters (self, wocky_porter_close_async,
      wocky_porter_close_finish, wocky_meta_porter_close_async,
      cancellable, callback, user_data);
}

static gboolean
wocky_meta_porter_close_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self, wocky_meta_porter_close_async);
}

typedef struct
{
  WockyMetaPorter *self; /* already reffed by simple */
  GSimpleAsyncResult *simple;
  WockyContact *contact;
} SendIQData;

static void
meta_porter_send_iq_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  SendIQData *data = user_data;
  GSimpleAsyncResult *simple = data->simple;
  GError *error = NULL;
  WockyStanza *stanza;

  stanza = wocky_porter_send_iq_finish (WOCKY_PORTER (source_object),
      result, &error);

  if (stanza == NULL)
    g_simple_async_result_take_error (simple, error);
  else
    g_simple_async_result_set_op_res_gpointer (simple, stanza, NULL);

  g_simple_async_result_complete (simple);
  g_object_unref (simple);

  wocky_meta_porter_unhold (data->self, data->contact);

  g_object_unref (data->contact);
  g_slice_free (SendIQData, data);
}

static void
meta_porter_send_iq_got_porter_cb (WockyMetaPorter *self,
    WockyPorter *porter,
    GCancellable *cancellable,
    const GError *error,
    GSimpleAsyncResult *simple,
    gpointer user_data)
{
  WockyStanza *stanza = user_data;
  WockyContact *contact;

  contact = wocky_stanza_get_to_contact (stanza);

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_simple_async_result_complete (simple);

      g_object_unref (simple);
      wocky_meta_porter_unhold (self, contact);
    }
  else
    {
      SendIQData *data = g_slice_new0 (SendIQData);
      data->self = self;
      data->simple = simple;
      data->contact = g_object_ref (contact);

      wocky_porter_send_iq_async (porter, stanza, cancellable,
          meta_porter_send_iq_cb, data);
    }

  g_object_unref (stanza);
}

static void
wocky_meta_porter_send_iq_async (WockyPorter *porter,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);
  WockyMetaPorterPrivate *priv = self->priv;
  GSimpleAsyncResult *simple;
  WockyContact *to;

  to = wocky_stanza_get_to_contact (stanza);

  g_return_if_fail (WOCKY_IS_LL_CONTACT (to));

  simple = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      wocky_meta_porter_send_iq_async);

  wocky_meta_porter_hold (self, to);

  /* stamp on from if there is none */
  if (wocky_node_get_attribute (wocky_stanza_get_top_node (stanza),
          "from") == NULL)
    {
      wocky_node_set_attribute (wocky_stanza_get_top_node (stanza),
          "from", priv->jid);
    }

  open_porter_if_necessary (self, WOCKY_LL_CONTACT (to), cancellable,
      meta_porter_send_iq_got_porter_cb, simple, g_object_ref (stanza));
}

static WockyStanza *
wocky_meta_porter_send_iq_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_return_pointer (self, wocky_meta_porter_send_iq_async);
}

static void
wocky_meta_porter_force_close_async (WockyPorter *porter,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyMetaPorter *self = WOCKY_META_PORTER (porter);

  close_all_porters (self, wocky_porter_force_close_async,
      wocky_porter_force_close_finish, wocky_meta_porter_force_close_async,
      cancellable, callback, user_data);
}

static gboolean
wocky_meta_porter_force_close_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self, wocky_meta_porter_force_close_async);
}

/**
 * wocky_meta_porter_get_port:
 * @porter: a #WockyMetaPorter
 *
 * Returns the port @porter is listening in on for new incoming XMPP
 * connections, or 0 if it has not been started yet with
 * wocky_porter_start().
 *
 * Returns: the port @porter is listening in on for new incoming XMPP
 *   connections, or 0 if it has not been started.
 */
guint16
wocky_meta_porter_get_port (WockyMetaPorter *self)
{
  g_return_val_if_fail (WOCKY_IS_META_PORTER (self), 0);

  return self->priv->port;
}

/**
 * wocky_meta_porter_set_jid:
 * @porter: a #WockyMetaPorter
 * @jid: a new JID
 *
 * Changes the local JID according to @porter. Note that this function
 * can only be called once, and only if %NULL was passed to
 * wocky_meta_porter_new() when creating @porter. Calling it again
 * will be a no-op.
 */
void
wocky_meta_porter_set_jid (WockyMetaPorter *self,
    const gchar *jid)
{
  WockyMetaPorterPrivate *priv;

  g_return_if_fail (WOCKY_IS_META_PORTER (self));

  priv = self->priv;

  if (priv->jid != NULL)
    {
      DEBUG ("You cannot set the meta porter JID again");
      return;
    }

  /* don't try and change existing porter's JIDs */

  priv->jid = g_strdup (jid);

  /* now we can do this */
  create_loopback_porter (self);
}

static void
meta_porter_open_got_porter_cb (WockyMetaPorter *self,
    WockyPorter *porter,
    GCancellable *cancellable,
    const GError *error,
    GSimpleAsyncResult *simple,
    gpointer user_data)
{
  WockyContact *contact = user_data;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (simple, error);
      wocky_meta_porter_unhold (self, contact);
    }

  g_simple_async_result_complete (simple);

  g_object_unref (contact);
  g_object_unref (simple);
}

/**
 * wocky_meta_porter_open_async:
 * @porter: a #WockyMetaPorter
 * @contact: the #WockyLLContact
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: a callback to be called
 * @user_data: data for @callback
 *
 * Make an asynchronous request to open a connection to @contact if
 * one is not already open. The hold count of the porter to
 * @contact will be incrememented and so after completion
 * wocky_meta_porter_unhold() should be called on contact to release
 * the hold.
 *
 * When the request is complete, @callback will be called and the user
 * should call wocky_meta_porter_open_finish() to finish the request.
 */
void
wocky_meta_porter_open_async (WockyMetaPorter *self,
    WockyContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;

  g_return_if_fail (WOCKY_IS_META_PORTER (self));
  g_return_if_fail (WOCKY_IS_LL_CONTACT (contact));
  g_return_if_fail (callback != NULL);

  simple = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      wocky_meta_porter_open_async);

  wocky_meta_porter_hold (self, contact);

  open_porter_if_necessary (self, WOCKY_LL_CONTACT (contact),
      cancellable, meta_porter_open_got_porter_cb, simple,
      g_object_ref (contact));
}

/**
 * wocky_meta_porter_open_finish:
 * @porter: a #WockyMetaPorter
 * @result: the #GAsyncResult
 * @error: an optional #GError location to store an error message
 *
 * Finishes an asynchronous request to open a connection to @contact
 * if one is not already open. See wocky_meta_porter_open_async() for
 * more details.
 *
 * Returns: %TRUE if the operation was a success, otherwise %FALSE
 */
gboolean
wocky_meta_porter_open_finish (WockyMetaPorter *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self, wocky_meta_porter_open_async);
}

/**
 * wocky_meta_porter_borrow_connection:
 * @porter: a #WockyMetaPorter
 * @contact: the #WockyContact
 *
 * Borrow the #GSocketConnection of the porter to @contact, if one
 * exists otherwise %NULL will be returned. Note that the connection
 * returned is not reffed and should not be kept as it still is owned
 * and operated on by the underlying #WockyXmppConnection object.
 *
 * Returns: the #GSocketConnection or %NULL if no connection is open
 */
GSocketConnection *
wocky_meta_porter_borrow_connection (WockyMetaPorter *self,
    WockyContact *contact)
{
  WockyMetaPorterPrivate *priv;
  PorterData *porter_data;
  GSocketConnection *socket_conn;
  WockyXmppConnection *xmpp_conn;

  g_return_val_if_fail (WOCKY_IS_META_PORTER (self), NULL);
  g_return_val_if_fail (WOCKY_IS_LL_CONTACT (contact), NULL);

  priv = self->priv;

  porter_data = g_hash_table_lookup (priv->porters, contact);

  if (porter_data == NULL || porter_data->porter == NULL)
    return NULL;

  /* splendid, the connection is already open */

  g_object_get (porter_data->porter, "connection", &xmpp_conn, NULL);
  /* will give it a new ref */
  g_object_get (xmpp_conn, "base-stream", &socket_conn, NULL);

  /* we take back the ref */
  g_object_unref (socket_conn);
  g_object_unref (xmpp_conn);

  /* but this will still be alive */
  return socket_conn;
}

static void
wocky_porter_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  WockyPorterInterface *iface = g_iface;

  iface->get_full_jid = wocky_meta_porter_get_jid;
  iface->get_bare_jid = wocky_meta_porter_get_jid;
  /* a dummy implementation to return NULL so if someone calls it on
   * us it won't assert */
  iface->get_resource = wocky_meta_porter_get_resource;

  iface->start = wocky_meta_porter_start;

  iface->send_async = wocky_meta_porter_send_async;
  iface->send_finish = wocky_meta_porter_send_finish;

  iface->register_handler_from_by_stanza =
    wocky_meta_porter_register_handler_from_by_stanza;
  iface->register_handler_from_anyone_by_stanza =
    wocky_meta_porter_register_handler_from_anyone_by_stanza;

  iface->unregister_handler = wocky_meta_porter_unregister_handler;

  iface->close_async = wocky_meta_porter_close_async;
  iface->close_finish = wocky_meta_porter_close_finish;

  iface->send_iq_async = wocky_meta_porter_send_iq_async;
  iface->send_iq_finish = wocky_meta_porter_send_iq_finish;

  iface->force_close_async = wocky_meta_porter_force_close_async;
  iface->force_close_finish = wocky_meta_porter_force_close_finish;
}
