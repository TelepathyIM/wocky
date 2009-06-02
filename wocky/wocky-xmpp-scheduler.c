/*
 * wocky-xmpp-scheduler.c - Source for WockyXmppScheduler
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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

/**
 * SECTION: wocky-xmpp-scheduler
 * @title: WockyXmppScheduler
 * @short_description: Wrapper around a #WockyXmppConnection providing a
 * higher level API.
 *
 * Sends and receives #WockyXmppStanza from an underlying
 * #WockyXmppConnection.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "wocky-xmpp-scheduler.h"
#include "wocky-signals-marshal.h"

G_DEFINE_TYPE(WockyXmppScheduler, wocky_xmpp_scheduler, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_CONNECTION = 1,
};

/* private structure */
typedef struct _WockyXmppSchedulerPrivate WockyXmppSchedulerPrivate;

struct _WockyXmppSchedulerPrivate
{
  gboolean dispose_has_run;

  /* Queue of (sending_queue_elt *) */
  GQueue *sending_queue;
  gboolean sending;

  WockyXmppConnection *connection;
};

#define WOCKY_XMPP_SCHEDULER_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_SCHEDULER, \
    WockyXmppSchedulerPrivate))

typedef struct
{
  WockyXmppStanza *stanza;
  GCancellable *cancellable;
  GSimpleAsyncResult *result;
  gulong cancelled_sig_id;
} sending_queue_elt;

static sending_queue_elt *
sending_queue_elt_new (WockyXmppScheduler *self,
  WockyXmppStanza *stanza,
  GCancellable *cancellable,
  GAsyncReadyCallback callback,
  gpointer user_data)
{
  sending_queue_elt *elt = g_slice_new0 (sending_queue_elt);

  elt->stanza = g_object_ref (stanza);
  if (cancellable != NULL)
    elt->cancellable = g_object_ref (cancellable);

  elt->result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_xmpp_scheduler_send_full_finish);

  return elt;
}

static void
sending_queue_elt_free (sending_queue_elt *elt)
{
  g_object_unref (elt->stanza);
  if (elt->cancellable != NULL)
    {
      g_object_unref (elt->cancellable);
      g_signal_handler_disconnect (elt->cancellable, elt->cancelled_sig_id);
    }
  g_object_unref (elt->result);

  g_slice_free (sending_queue_elt, elt);
}

static void send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);

static void
wocky_xmpp_scheduler_init (WockyXmppScheduler *obj)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (obj);
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  priv->sending_queue = g_queue_new ();
}

static void wocky_xmpp_scheduler_dispose (GObject *object);
static void wocky_xmpp_scheduler_finalize (GObject *object);

static void
wocky_xmpp_scheduler_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyXmppScheduler *connection = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (connection);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_assert (priv->connection == NULL);
        priv->connection = g_value_dup_object (value);
        g_assert (priv->connection != NULL);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_scheduler_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyXmppScheduler *connection = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (connection);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_scheduler_class_init (
    WockyXmppSchedulerClass *wocky_xmpp_scheduler_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_scheduler_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_xmpp_scheduler_class,
      sizeof (WockyXmppSchedulerPrivate));

  object_class->set_property = wocky_xmpp_scheduler_set_property;
  object_class->get_property = wocky_xmpp_scheduler_get_property;
  object_class->dispose = wocky_xmpp_scheduler_dispose;
  object_class->finalize = wocky_xmpp_scheduler_finalize;

  spec = g_param_spec_object ("connection", "XMPP connection",
    "the XMPP connection used by this scheduler",
    WOCKY_TYPE_XMPP_CONNECTION,
    G_PARAM_READWRITE |
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_CONNECTION, spec);
}

void
wocky_xmpp_scheduler_dispose (GObject *object)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->connection != NULL)
    {
      g_object_unref (priv->connection);
      priv->connection = NULL;
    }

  if (G_OBJECT_CLASS (wocky_xmpp_scheduler_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_scheduler_parent_class)->dispose (object);
}

void
wocky_xmpp_scheduler_finalize (GObject *object)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  sending_queue_elt *elt;

  elt = g_queue_pop_head (priv->sending_queue);
  while (elt != NULL)
    {
      /* FIXME: call cb? */
      sending_queue_elt_free (elt);
      elt = g_queue_pop_head (priv->sending_queue);
    }

  g_queue_free (priv->sending_queue);

  G_OBJECT_CLASS (wocky_xmpp_scheduler_parent_class)->finalize (object);
}

/**
 * wocky_xmpp_scheduler_new:
 * @connection: #WockyXmppConnection which will be used to receive and send
 * #WockyXmppStanza
 *
 * Convenience function to create a new #WockyXmppScheduler.
 *
 * Returns: a new #WockyXmppScheduler.
 */
WockyXmppScheduler *
wocky_xmpp_scheduler_new (WockyXmppConnection *connection)
{
  WockyXmppScheduler *result;

  result = g_object_new (WOCKY_TYPE_XMPP_SCHEDULER,
    "connection", connection,
    NULL);

  return result;
}

static void
send_head_stanza (WockyXmppScheduler *self)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  sending_queue_elt *elt;

  elt = g_queue_peek_head (priv->sending_queue);
  if (elt == NULL)
    /* Nothing to send */
    return;

  wocky_xmpp_connection_send_stanza_async (priv->connection,
      elt->stanza, elt->cancellable, send_stanza_cb, self);
}

static void
send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (user_data);
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  sending_queue_elt *elt;

  elt = g_queue_pop_head (priv->sending_queue);
  g_assert (elt != NULL);

  g_simple_async_result_complete (elt->result);

  sending_queue_elt_free (elt);

  if (g_queue_get_length (priv->sending_queue) > 0)
    {
      /* Send next stanza */
      send_head_stanza (self);
    }
}

typedef struct
{
  WockyXmppScheduler *self;
  sending_queue_elt *elt;
} send_cancelled_cb_data;

static send_cancelled_cb_data *
send_cancelled_cb_data_new (WockyXmppScheduler *self,
    sending_queue_elt *elt)
{
  send_cancelled_cb_data *data = g_slice_new0 (send_cancelled_cb_data);
  data->self = self;
  data->elt = elt;

  return data;
}

static void
send_cancelled_cb_data_free (gpointer user_data,
    GClosure *closure)
{
  g_slice_free (send_cancelled_cb_data, user_data);
}

static void
send_cancelled_cb (GCancellable *cancellable,
    gpointer user_data)
{
  send_cancelled_cb_data *d = (send_cancelled_cb_data *) user_data;
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (d->self);
  GError error = { G_IO_ERROR, G_IO_ERROR_CANCELLED, "Sending was cancelled" };

  g_simple_async_result_set_from_error (d->elt->result, &error);
  g_simple_async_result_complete (d->elt->result);

  g_queue_remove_all (priv->sending_queue, d->elt);
  sending_queue_elt_free (d->elt);
}

void
wocky_xmpp_scheduler_send_full (WockyXmppScheduler *self,
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  sending_queue_elt *elt;

  elt = sending_queue_elt_new (self, stanza, cancellable, callback, user_data);
  g_queue_push_tail (priv->sending_queue, elt);

  if (g_queue_get_length (priv->sending_queue) == 1)
    {
      send_head_stanza (self);
    }

  if (cancellable != NULL)
    {
      send_cancelled_cb_data *data = send_cancelled_cb_data_new (self, elt);

      elt->cancelled_sig_id = g_signal_connect_data (cancellable, "cancelled",
          G_CALLBACK (send_cancelled_cb), data, send_cancelled_cb_data_free, 0);
    }
}

gboolean
wocky_xmpp_scheduler_send_full_finish (WockyXmppScheduler *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  return TRUE;
}

void
wocky_xmpp_scheduler_send (WockyXmppScheduler *self,
    WockyXmppStanza *stanza)
{
  wocky_xmpp_scheduler_send_full (self, stanza, NULL, NULL, NULL);
}
