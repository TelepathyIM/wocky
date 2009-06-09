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

#define DEBUG_FLAG DEBUG_XMPP_SCHEDULER
#include "wocky-debug.h"

G_DEFINE_TYPE(WockyXmppScheduler, wocky_xmpp_scheduler, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_CONNECTION = 1,
};

/* signal enum */
enum
{
    REMOTE_CLOSED,
    REMOTE_ERROR,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _WockyXmppSchedulerPrivate WockyXmppSchedulerPrivate;

struct _WockyXmppSchedulerPrivate
{
  gboolean dispose_has_run;

  /* Queue of (sending_queue_elem *) */
  GQueue *sending_queue;
  gboolean sending;
  GCancellable *receive_cancellable;
  /* List of (StanzaFilter *) */
  GSList *stanza_filters;

  GSimpleAsyncResult *close_result;
  gboolean remote_closed;
  gboolean local_closed;
  GCancellable *close_cancellable;

  WockyXmppConnection *connection;
};

/**
 * wocky_xmpp_scheduler_error_quark
 *
 * Get the error quark used by the scheduler.
 *
 * Returns: the quark for scheduler errors.
 */
GQuark
wocky_xmpp_scheduler_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-xmpp-scheduler-error");

  return quark;
}

#define WOCKY_XMPP_SCHEDULER_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_SCHEDULER, \
    WockyXmppSchedulerPrivate))

typedef struct
{
  WockyXmppScheduler *self;
  WockyXmppStanza *stanza;
  GCancellable *cancellable;
  GSimpleAsyncResult *result;
  gulong cancelled_sig_id;
} sending_queue_elem;

static sending_queue_elem *
sending_queue_elem_new (WockyXmppScheduler *self,
  WockyXmppStanza *stanza,
  GCancellable *cancellable,
  GAsyncReadyCallback callback,
  gpointer user_data)
{
  sending_queue_elem *elem = g_slice_new0 (sending_queue_elem);

  elem->self = self;
  elem->stanza = g_object_ref (stanza);
  if (cancellable != NULL)
    elem->cancellable = g_object_ref (cancellable);

  elem->result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_xmpp_scheduler_send_full_finish);

  return elem;
}

static void
sending_queue_elem_free (sending_queue_elem *elem)
{
  g_object_unref (elem->stanza);
  if (elem->cancellable != NULL)
    {
      g_object_unref (elem->cancellable);
      g_signal_handler_disconnect (elem->cancellable, elem->cancelled_sig_id);
    }
  g_object_unref (elem->result);

  g_slice_free (sending_queue_elem, elem);
}

typedef struct
{
  WockyXmppSchedulerStanzaFilterFunc filter_func;
  WockyXmppSchedulerStanzaCallbackFunc callback;
  gpointer user_data;
} StanzaFilter;

static StanzaFilter *
stanza_filter_new (WockyXmppSchedulerStanzaFilterFunc filter_func,
  WockyXmppSchedulerStanzaCallbackFunc callback,
  gpointer user_data)
{
  StanzaFilter *result = g_slice_new0 (StanzaFilter);

  result->filter_func = filter_func;
  result->callback = callback;
  result->user_data = user_data;
  return result;
}

static void
stanza_filter_free (StanzaFilter *filter)
{
  g_slice_free (StanzaFilter, filter);
}

static void send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);
static void send_close (WockyXmppScheduler *self);

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
wocky_xmpp_scheduler_constructed (GObject *object)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  g_assert (priv->connection != NULL);
}

static void
wocky_xmpp_scheduler_class_init (
    WockyXmppSchedulerClass *wocky_xmpp_scheduler_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_scheduler_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_xmpp_scheduler_class,
      sizeof (WockyXmppSchedulerPrivate));

  object_class->constructed = wocky_xmpp_scheduler_constructed;
  object_class->set_property = wocky_xmpp_scheduler_set_property;
  object_class->get_property = wocky_xmpp_scheduler_get_property;
  object_class->dispose = wocky_xmpp_scheduler_dispose;
  object_class->finalize = wocky_xmpp_scheduler_finalize;

  signals[REMOTE_CLOSED] = g_signal_new ("remote-closed",
      G_OBJECT_CLASS_TYPE (wocky_xmpp_scheduler_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[REMOTE_ERROR] = g_signal_new ("remote-error",
      G_OBJECT_CLASS_TYPE (wocky_xmpp_scheduler_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__UINT_INT_STRING,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);

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

  if (priv->receive_cancellable != NULL)
    {
      g_warning ("Disposing a not closed XMPP scheduler");
      g_cancellable_cancel (priv->receive_cancellable);
      g_object_unref (priv->receive_cancellable);
      priv->receive_cancellable = NULL;
    }

  if (priv->close_result != NULL)
    {
      g_object_unref (priv->close_result);
      priv->close_result = NULL;
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
  GSList *l;

  /* sending_queue_elem keeps a ref on the Scheduler (through the
   * GSimpleAsyncResult) so it shouldn't be destroyed while there are
   * elements in the queue. */
  g_assert_cmpuint (g_queue_get_length (priv->sending_queue), ==, 0);
  g_queue_free (priv->sending_queue);

  for (l = priv->stanza_filters; l != NULL; l = g_slist_next (l))
    {
      stanza_filter_free ((StanzaFilter *) l->data);
    }
  g_slist_free (priv->stanza_filters);
  priv->stanza_filters = NULL;

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
  sending_queue_elem *elem;

  elem = g_queue_peek_head (priv->sending_queue);
  if (elem == NULL)
    /* Nothing to send */
    return;

  wocky_xmpp_connection_send_stanza_async (priv->connection,
      elem->stanza, elem->cancellable, send_stanza_cb, self);
}

static void
send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (user_data);
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  sending_queue_elem *elem;
  GError *error = NULL;

  elem = g_queue_pop_head (priv->sending_queue);
  g_assert (elem != NULL);

  if (!wocky_xmpp_connection_send_stanza_finish (
        WOCKY_XMPP_CONNECTION (source), res, &error))
    {
      /* Sending failed. Cancel this sending operation and all the others
       * pending ones as we won't be able to send any more stanza. */

      while (elem != NULL)
        {
          g_simple_async_result_set_from_error (elem->result, error);
          g_simple_async_result_complete (elem->result);
          sending_queue_elem_free (elem);
          elem = g_queue_pop_head (priv->sending_queue);
        }

      g_error_free (error);
    }
  else
    {

      g_simple_async_result_complete (elem->result);

      sending_queue_elem_free (elem);

      if (g_queue_get_length (priv->sending_queue) > 0)
        {
          /* Send next stanza */
          send_head_stanza (self);
        }
    }

  if (priv->close_result != NULL &&
      g_queue_get_length (priv->sending_queue) == 0)
    {
      /* Queue is empty and we are waiting to close the connection. */
      DEBUG ("Queue has been flushed. Closing the connection.");
      send_close (self);
    }
}

static void
send_cancelled_cb (GCancellable *cancellable,
    gpointer user_data)
{
  sending_queue_elem *elem = (sending_queue_elem *) user_data;
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (
      elem->self);
  GError error = { G_IO_ERROR, G_IO_ERROR_CANCELLED, "Sending was cancelled" };

  g_simple_async_result_set_from_error (elem->result, &error);
  g_simple_async_result_complete (elem->result);

  g_queue_remove (priv->sending_queue, elem);
  sending_queue_elem_free (elem);
}

void
wocky_xmpp_scheduler_send_full (WockyXmppScheduler *self,
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  sending_queue_elem *elem;

  elem = sending_queue_elem_new (self, stanza, cancellable, callback,
      user_data);
  g_queue_push_tail (priv->sending_queue, elem);

  if (g_queue_get_length (priv->sending_queue) == 1)
    {
      send_head_stanza (self);
    }

  if (cancellable != NULL)
    {
      elem->cancelled_sig_id = g_signal_connect (cancellable, "cancelled",
          G_CALLBACK (send_cancelled_cb), elem);
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

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_xmpp_scheduler_send_full_finish), FALSE);

  return TRUE;
}

void
wocky_xmpp_scheduler_send (WockyXmppScheduler *self,
    WockyXmppStanza *stanza)
{
  wocky_xmpp_scheduler_send_full (self, stanza, NULL, NULL, NULL);
}

static void receive_stanza (WockyXmppScheduler *self);

static void
apply_filter (WockyXmppScheduler *self,
    StanzaFilter *filter,
    WockyXmppStanza *stanza)
{
  if (filter->filter_func == NULL)
    /* No filter function, match every stanza */
    goto call_cb;

  if (!filter->filter_func (self, stanza, filter->user_data))
    return;

call_cb:
  filter->callback (self, stanza, filter->user_data);
}

static void
stanza_received_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (user_data);
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  GError *error = NULL;
  GSList *l;

  stanza = wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), res, &error);
  if (stanza == NULL)
    {
      if (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
            WOCKY_XMPP_CONNECTION_ERROR_CLOSED))
        {
          if (priv->close_result != NULL)
            {
              /* Close completed */
              g_simple_async_result_complete_in_idle (priv->close_result);
              g_object_unref (priv->close_result);
              priv->close_result = NULL;
            }

          DEBUG ("Remote connection has been closed");
          g_signal_emit (self, signals[REMOTE_CLOSED], 0);
        }
      else
        {
          DEBUG ("Error receiving stanza: %s\n", error->message);
          g_signal_emit (self, signals[REMOTE_ERROR], 0, error->domain,
              error->code, error->message);
        }

      if (priv->receive_cancellable != NULL)
        {
          g_object_unref (priv->receive_cancellable);
          priv->receive_cancellable = NULL;
        }

      priv->remote_closed = TRUE;
      g_error_free (error);
      return;
    }

  for (l = priv->stanza_filters; l != NULL; l = g_slist_next (l))
    {
      StanzaFilter *filter = (StanzaFilter *) l->data;

      apply_filter (self, filter, stanza);
    }
  g_object_unref (stanza);

  /* wait for next stanza */
  receive_stanza (self);
}

static void
receive_stanza (WockyXmppScheduler *self)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  wocky_xmpp_connection_recv_stanza_async (priv->connection,
      priv->receive_cancellable, stanza_received_cb, self);
}

void
wocky_xmpp_scheduler_start (WockyXmppScheduler *self)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  if (priv->receive_cancellable != NULL)
    /* Scheduler has already been started */
    return;

  priv->receive_cancellable = g_cancellable_new ();

  receive_stanza (self);
}

void
wocky_xmpp_scheduler_add_stanza_filter (WockyXmppScheduler *self,
    WockyXmppSchedulerStanzaFilterFunc filter_func,
    WockyXmppSchedulerStanzaCallbackFunc callback,
    gpointer user_data)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  StanzaFilter *filter;

  filter = stanza_filter_new (filter_func, callback, user_data);

  priv->stanza_filters = g_slist_append (priv->stanza_filters, filter);
}

static void
close_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (user_data);
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);
  GError *error = NULL;

  if (!g_cancellable_is_cancelled (priv->close_cancellable))
    priv->local_closed = TRUE;

  if (!wocky_xmpp_connection_send_close_finish (WOCKY_XMPP_CONNECTION (source),
        res, &error))
    {
      g_simple_async_result_set_from_error (priv->close_result, error);
      g_simple_async_result_complete_in_idle (priv->close_result);

      g_object_unref (priv->close_result);
      priv->close_result = NULL;
      g_error_free (error);
    }

  if (priv->close_result != NULL && priv->remote_closed)
    {
      /* Remote connection is already closed. Finish the close operation */
      g_simple_async_result_complete_in_idle (priv->close_result);

      g_object_unref (priv->close_result);
      priv->close_result = NULL;
    }

  priv->close_cancellable = NULL;
}

static void
send_close (WockyXmppScheduler *self)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  wocky_xmpp_connection_send_close_async (priv->connection,
      priv->close_cancellable, close_sent_cb, self);
}

void
wocky_xmpp_scheduler_close (WockyXmppScheduler *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppSchedulerPrivate *priv = WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  if (priv->local_closed)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_XMPP_SCHEDULER_ERROR,
          WOCKY_XMPP_SCHEDULER_ERROR_CLOSED,
          "Scheduler has already been closed");
      return;
    }

  if (priv->receive_cancellable == NULL && !priv->remote_closed)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_XMPP_SCHEDULER_ERROR,
          WOCKY_XMPP_SCHEDULER_ERROR_NOT_STARTED,
          "Scheduler has not been started");
      return;
    }

  if (priv->close_result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, G_IO_ERROR,
          G_IO_ERROR_PENDING,
          "Another close operation is pending");
      return;
    }

  priv->close_result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_xmpp_scheduler_close_finish);

  priv->close_cancellable = cancellable;

  if (g_queue_get_length (priv->sending_queue) > 0)
    {
      DEBUG ("Sending queue is not empty. Flushing it before "
          "closing the connection.");
      return;
    }

  send_close (self);
}

gboolean
wocky_xmpp_scheduler_close_finish (
    WockyXmppScheduler *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_xmpp_scheduler_close_finish), FALSE);

  return TRUE;
}
