/*
 * wocky-porter.c - Source for WockyPorter
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
 * SECTION: wocky-porter
 * @title: WockyPorter
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

#include "wocky-porter.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"
#include "wocky-namespaces.h"
#include "wocky-contact-factory.h"

#define DEBUG_FLAG DEBUG_PORTER
#include "wocky-debug.h"

G_DEFINE_TYPE(WockyPorter, wocky_porter, G_TYPE_OBJECT)

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
    CLOSING,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _WockyPorterPrivate WockyPorterPrivate;

struct _WockyPorterPrivate
{
  gboolean dispose_has_run;
  gboolean forced_shutdown;

  /* Queue of (sending_queue_elem *) */
  GQueue *sending_queue;
  GCancellable *receive_cancellable;

  GSimpleAsyncResult *close_result;
  gboolean remote_closed;
  gboolean local_closed;
  GCancellable *close_cancellable;
  GSimpleAsyncResult *force_close_result;
  GCancellable *force_close_cancellable;

  /* guint => owned (StanzaHandler *) */
  GHashTable *handlers_by_id;
  /* Sort listed (by decreasing priority) of borrowed (StanzaHandler *) */
  GList *handlers;
  guint next_handler_id;
  /* (const gchar *) => owned (StanzaIqHandler *)
   * This key is the ID of the IQ */
  GHashTable *iq_reply_handlers;

  WockyXmppConnection *connection;
};

/**
 * wocky_porter_error_quark:
 *
 * Get the error quark used by the porter.
 *
 * Returns: the quark for porter errors.
 */
GQuark
wocky_porter_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-porter-error");

  return quark;
}

#define WOCKY_PORTER_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_PORTER, \
    WockyPorterPrivate))

typedef struct
{
  WockyPorter *self;
  WockyXmppStanza *stanza;
  GCancellable *cancellable;
  GSimpleAsyncResult *result;
  gulong cancelled_sig_id;
} sending_queue_elem;

static sending_queue_elem *
sending_queue_elem_new (WockyPorter *self,
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
    callback, user_data, wocky_porter_send_finish);

  return elem;
}

static void
sending_queue_elem_free (sending_queue_elem *elem)
{
  g_object_unref (elem->stanza);
  if (elem->cancellable != NULL)
    {
      g_object_unref (elem->cancellable);
      if (elem->cancelled_sig_id != 0)
        g_signal_handler_disconnect (elem->cancellable, elem->cancelled_sig_id);
      /* FIXME: we should use g_cancellable_disconnect but it raises a dead
       * lock (#587300) */
    }
  g_object_unref (elem->result);

  g_slice_free (sending_queue_elem, elem);
}

typedef struct
{
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  gchar *node;
  gchar *domain;
  gchar *resource;
  guint priority;
  WockyXmppStanza *match;
  WockyPorterHandlerFunc callback;
  gpointer user_data;
} StanzaHandler;

static StanzaHandler *
stanza_handler_new (
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyXmppStanza *stanza,
    WockyPorterHandlerFunc callback,
    gpointer user_data)
{
  StanzaHandler *result = g_slice_new0 (StanzaHandler);

  result->type = type;
  result->sub_type = sub_type;
  result->priority = priority;
  result->callback = callback;
  result->user_data = user_data;
  result->match = g_object_ref (stanza);

  if (from != NULL)
    {
      wocky_decode_jid (from, &(result->node),
          &(result->domain), &(result->resource));
    }

  return result;
}

static void
stanza_handler_free (StanzaHandler *handler)
{
  g_free (handler->node);
  g_free (handler->domain);
  g_free (handler->resource);
  g_object_unref (handler->match);
  g_slice_free (StanzaHandler, handler);
}

typedef struct
{
  WockyPorter *self;
  GSimpleAsyncResult *result;
  GCancellable *cancellable;
  gulong cancelled_sig_id;
  gchar *recipient;
  gchar *id;
  gboolean sent;
} StanzaIqHandler;

static StanzaIqHandler *
stanza_iq_handler_new (WockyPorter *self,
    gchar *id,
    GSimpleAsyncResult *result,
    GCancellable *cancellable,
    const gchar *recipient)
{
  StanzaIqHandler *handler = g_slice_new0 (StanzaIqHandler);
  gchar *to = NULL;

  if (recipient != NULL)
    {
      to = wocky_normalise_jid (recipient);

      if (to == NULL)
        {
          DEBUG ("Failed to normalise stanza recipient '%s'", recipient);
          to = g_strdup (recipient);
        }
    }

  handler->self = self;
  handler->result = result;
  handler->id = id;
  if (cancellable != NULL)
    handler->cancellable = g_object_ref (cancellable);
  handler->recipient = to;

  return handler;
}

static void
stanza_iq_handler_remove_cancellable (StanzaIqHandler *handler)
{
  if (handler->cancellable != NULL)
    {
      /* FIXME: we should use g_cancellable_disconnect but it raises a dead
       * lock (#587300) */
      g_signal_handler_disconnect (handler->cancellable, handler->cancelled_sig_id);
      g_object_unref (handler->cancellable);
      handler->cancelled_sig_id = 0;
      handler->cancellable = NULL;
    }
}

static void
stanza_iq_handler_free (StanzaIqHandler *handler)
{
  if (handler->result != NULL)
    g_object_unref (handler->result);

  stanza_iq_handler_remove_cancellable (handler);

  g_free (handler->id);
  g_free (handler->recipient);
  g_slice_free (StanzaIqHandler, handler);
}

static void
stanza_iq_handler_maybe_remove (StanzaIqHandler *handler)
{
  /* Always wait till the iq sent operation has finished and something
   * completed the operation from the perspective of the API user */
  if (handler->sent && handler->result == NULL)
    {
      WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (handler->self);
      g_hash_table_remove (priv->iq_reply_handlers, handler->id);
    }
}

static void send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);

static void send_close (WockyPorter *self);

static gboolean handle_iq_reply (WockyPorter *self,
    WockyXmppStanza *reply,
    gpointer user_data);

static void remote_connection_closed (WockyPorter *self,
    GError *error);

static void
wocky_porter_init (WockyPorter *obj)
{
  WockyPorter *self = WOCKY_PORTER (obj);
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);

  priv->sending_queue = g_queue_new ();

  priv->handlers_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) stanza_handler_free);
  /* these are guints, reserve 0 for "not a valid handler" */
  priv->next_handler_id = 1;
  priv->handlers = NULL;

  priv->iq_reply_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) stanza_iq_handler_free);
}

static void wocky_porter_dispose (GObject *object);
static void wocky_porter_finalize (GObject *object);

static void
wocky_porter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyPorter *connection = WOCKY_PORTER (object);
  WockyPorterPrivate *priv =
      WOCKY_PORTER_GET_PRIVATE (connection);

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
wocky_porter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyPorter *connection = WOCKY_PORTER (object);
  WockyPorterPrivate *priv =
      WOCKY_PORTER_GET_PRIVATE (connection);

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

static gboolean
handle_stream_error (WockyPorter *self,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  GError *error = NULL;

  DEBUG ("Received stream error; consider the remote connection as closed");
  wocky_xmpp_stanza_extract_stream_error (stanza, &error);
  remote_connection_closed (self, error);
  g_error_free (error);
  return TRUE;
}

static void
wocky_porter_constructed (GObject *object)
{
  WockyPorter *self = WOCKY_PORTER (object);
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);

  g_assert (priv->connection != NULL);

  /* Register the IQ reply handler */
  wocky_porter_register_handler (self,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      handle_iq_reply, self, WOCKY_STANZA_END);

  wocky_porter_register_handler (self,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      handle_iq_reply, self, WOCKY_STANZA_END);

  /* Register the stream error handler */
  wocky_porter_register_handler (self,
      WOCKY_STANZA_TYPE_STREAM_ERROR, WOCKY_STANZA_SUB_TYPE_NONE, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      handle_stream_error, self, WOCKY_STANZA_END);
}

static void
wocky_porter_class_init (
    WockyPorterClass *wocky_porter_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_porter_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_porter_class,
      sizeof (WockyPorterPrivate));

  object_class->constructed = wocky_porter_constructed;
  object_class->set_property = wocky_porter_set_property;
  object_class->get_property = wocky_porter_get_property;
  object_class->dispose = wocky_porter_dispose;
  object_class->finalize = wocky_porter_finalize;

  /**
   * WockyPorter::remote-closed:
   * @porter: the object on which the signal is emitted
   *
   * The ::remote-closed signal is emitted when the other side closed the XMPP
   * stream.
   */
  signals[REMOTE_CLOSED] = g_signal_new ("remote-closed",
      G_OBJECT_CLASS_TYPE (wocky_porter_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * WockyPorter::remote-error:
   * @porter: the object on which the signal is emitted
   * @domain: error domain (a #GQuark)
   * @code: error code
   * @message: human-readable informative error message
   *
   * The ::remote-error signal is emitted when an error has been detected
   * on the XMPP stream.
   */
  signals[REMOTE_ERROR] = g_signal_new ("remote-error",
      G_OBJECT_CLASS_TYPE (wocky_porter_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__UINT_INT_STRING,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);

  /**
   * WockyPorter::closing:
   * @porter: the object on which the signal is emitted
   *
   * The ::closing signal is emitted when the #WockyPorter starts to close its
   * XMPP connection. Once this signal has been emitted, the #WockyPorter
   * can't be used to send stanzas any more.
   */
  signals[CLOSING] = g_signal_new ("closing",
      G_OBJECT_CLASS_TYPE (wocky_porter_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * WockyPorter:connection:
   *
   * The underlying #WockyXmppConnection wrapped by the #WockyPorter
   */
  spec = g_param_spec_object ("connection", "XMPP connection",
    "the XMPP connection used by this porter",
    WOCKY_TYPE_XMPP_CONNECTION,
    G_PARAM_READWRITE |
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, spec);
}

void
wocky_porter_dispose (GObject *object)
{
  WockyPorter *self = WOCKY_PORTER (object);
  WockyPorterPrivate *priv =
      WOCKY_PORTER_GET_PRIVATE (self);

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
      g_warning ("Disposing an open XMPP porter");
      g_cancellable_cancel (priv->receive_cancellable);
      g_object_unref (priv->receive_cancellable);
      priv->receive_cancellable = NULL;
    }

  if (priv->close_result != NULL)
    {
      g_object_unref (priv->close_result);
      priv->close_result = NULL;
    }

  if (priv->force_close_result != NULL)
    {
      g_object_unref (priv->force_close_result);
      priv->force_close_result = NULL;
    }

  if (G_OBJECT_CLASS (wocky_porter_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_porter_parent_class)->dispose (object);
}

void
wocky_porter_finalize (GObject *object)
{
  WockyPorter *self = WOCKY_PORTER (object);
  WockyPorterPrivate *priv =
      WOCKY_PORTER_GET_PRIVATE (self);

  DEBUG ("finalize porter %p", self);

  /* sending_queue_elem keeps a ref on the Porter (through the
   * GSimpleAsyncResult) so it shouldn't be destroyed while there are
   * elements in the queue. */
  g_assert_cmpuint (g_queue_get_length (priv->sending_queue), ==, 0);
  g_queue_free (priv->sending_queue);

  g_hash_table_destroy (priv->handlers_by_id);
  g_list_free (priv->handlers);
  g_hash_table_destroy (priv->iq_reply_handlers);

  G_OBJECT_CLASS (wocky_porter_parent_class)->finalize (object);
}

/**
 * wocky_porter_new:
 * @connection: #WockyXmppConnection which will be used to receive and send
 * #WockyXmppStanza
 *
 * Convenience function to create a new #WockyPorter.
 *
 * Returns: a new #WockyPorter.
 */
WockyPorter *
wocky_porter_new (WockyXmppConnection *connection)
{
  WockyPorter *result;

  result = g_object_new (WOCKY_TYPE_PORTER,
    "connection", connection,
    NULL);

  return result;
}

static void
send_head_stanza (WockyPorter *self)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  sending_queue_elem *elem;

  elem = g_queue_peek_head (priv->sending_queue);
  if (elem == NULL)
    /* Nothing to send */
    return;

  if (elem->cancelled_sig_id != 0)
    {
      /* We are going to start sending the stanza. Lower layers are now
       * responsible of handling the cancellable. */
      g_signal_handler_disconnect (elem->cancellable, elem->cancelled_sig_id);
      elem->cancelled_sig_id = 0;
    }

  wocky_xmpp_connection_send_stanza_async (priv->connection,
      elem->stanza, elem->cancellable, send_stanza_cb, self);
}

static void
send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *self = WOCKY_PORTER (user_data);
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  sending_queue_elem *elem;
  GError *error = NULL;

  elem = g_queue_pop_head (priv->sending_queue);

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
      if (elem == NULL)
        /* The elem could have been removed from the queue if its sending
         * operation has already been completed (for example by forcing to
         * close the connection). */
        return;

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
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (
      elem->self);
  GError error = { G_IO_ERROR, G_IO_ERROR_CANCELLED, "Sending was cancelled" };

  g_simple_async_result_set_from_error (elem->result, &error);
  g_simple_async_result_complete_in_idle (elem->result);

  g_queue_remove (priv->sending_queue, elem);
  sending_queue_elem_free (elem);
}

/**
 * wocky_porter_send_async:
 * @porter: a #WockyPorter
 * @stanza: the #WockyXmppStanza to send
 * @cancellable: optional #GCancellable object, %NULL <!-- --> to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request asynchronous sending of a #WockyXmppStanza.
 * When the stanza has been sent callback will be called.
 * You can then call wocky_porter_send_finish() to get the result
 * of the operation.
 */
void
wocky_porter_send_async (WockyPorter *self,
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  sending_queue_elem *elem;

  if (priv->close_result != NULL || priv->force_close_result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_CLOSING,
          "Porter is closing");
      return;
    }

  elem = sending_queue_elem_new (self, stanza, cancellable, callback,
      user_data);
  g_queue_push_tail (priv->sending_queue, elem);

  if (g_queue_get_length (priv->sending_queue) == 1)
    {
      send_head_stanza (self);
    }
  else if (cancellable != NULL)
    {
      elem->cancelled_sig_id = g_cancellable_connect (cancellable,
          G_CALLBACK (send_cancelled_cb), elem, NULL);
    }
}

/**
 * wocky_porter_send_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL <!-- -->to
 * ignore.
 *
 * Finishes sending a #WockyXmppStanza.
 *
 * Returns: %TRUE on success or %FALSE on error.
 */
gboolean
wocky_porter_send_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_porter_send_finish), FALSE);

  return TRUE;
}

/**
 * wocky_porter_send:
 * @porter: a #WockyPorter
 * @stanza: the #WockyXmppStanza to send
 *
 * Send a #WockyXmppStanza.
 * This is a convenient function to not have to call
 * wocky_porter_send_async() with lot of %NULL arguments if you don't care to
 * know when the stanza has been actually sent.
 */
void
wocky_porter_send (WockyPorter *self,
    WockyXmppStanza *stanza)
{
  wocky_porter_send_async (self, stanza, NULL, NULL, NULL);
}

static void receive_stanza (WockyPorter *self);

static void
complete_close (WockyPorter *self)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  GSimpleAsyncResult *tmp;

  if (g_cancellable_is_cancelled (priv->close_cancellable))
    {
      g_simple_async_result_set_error (priv->close_result, G_IO_ERROR,
          G_IO_ERROR_CANCELLED, "closing operation was cancelled");
    }

  g_simple_async_result_complete (priv->close_result);

  tmp = priv->close_result;
  priv->close_result = NULL;
  priv->close_cancellable = NULL;

  g_object_unref (tmp);
}

static gboolean
handle_iq_reply (WockyPorter *self,
    WockyXmppStanza *reply,
    gpointer user_data)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  const gchar *id, *from;
  StanzaIqHandler *handler;
  gboolean ret = FALSE;

  id = wocky_xmpp_node_get_attribute (reply->node, "id");
  if (id == NULL)
    {
      DEBUG ("Ignoring reply without IQ id");
      return FALSE;
    }

  handler = g_hash_table_lookup (priv->iq_reply_handlers, id);

  if (handler == NULL)
    {
      DEBUG ("Ignored IQ reply");
      return FALSE;
    }

  from = wocky_xmpp_node_get_attribute (reply->node, "from");
  /* FIXME: If handler->recipient is NULL, we should check if the 'from' is
   * either NULL, our bare jid or our full jid. */
  if (handler->recipient != NULL &&
      wocky_strdiff (from, handler->recipient))
    {
      gchar *nfrom = wocky_normalise_jid (from);

      if (wocky_strdiff (nfrom, handler->recipient))
        {
          DEBUG ("'%s' (normal: '%s') attempts to spoof an IQ reply from '%s'",
              from, nfrom, handler->recipient);
          g_free (nfrom);
          return FALSE;
        }

      g_free (nfrom);
    }

  if (handler->result != NULL)
    {
      GSimpleAsyncResult *r = handler->result;

      handler->result = NULL;

      /* Don't want to get cancelled during completion */
      stanza_iq_handler_remove_cancellable (handler);

      g_simple_async_result_set_op_res_gpointer (r, reply, NULL);
      g_simple_async_result_complete (r);
      g_object_unref (r);

      ret = TRUE;
    }

  stanza_iq_handler_maybe_remove (handler);
  return ret;
}

static void
handle_stanza (WockyPorter *self,
    WockyXmppStanza *stanza)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  GList *l;
  const gchar *from;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  gchar *node = NULL, *domain = NULL, *resource = NULL;

  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  /* The from attribute of the stanza need not always be present, for example
   * when receiving roster items, so don't enforce it. */
  from = wocky_xmpp_node_get_attribute (stanza->node, "from");

  if (from != NULL)
    wocky_decode_jid (from, &node, &domain, &resource);

  for (l = priv->handlers; l != NULL; l = g_list_next (l))
    {
      StanzaHandler *handler = (StanzaHandler *) l->data;

      if (type != handler->type)
        continue;

      if (sub_type != handler->sub_type &&
          handler->sub_type != WOCKY_STANZA_SUB_TYPE_NONE)
        continue;

      if (handler->node != NULL)
        {
          g_assert (handler->domain != NULL);

          if (wocky_strdiff (node, handler->node))
            continue;

          if (wocky_strdiff (domain, handler->domain))
            continue;

          if (handler->resource != NULL)
            {
              /* A ressource is defined so we want to match exactly the same
               * JID */

              if (wocky_strdiff (resource, handler->resource))
                continue;
            }
        }

      /* Check if the stanza matches the pattern */
      if (!wocky_xmpp_node_is_superset (stanza->node, handler->match->node))
        continue;

      if (handler->callback (self, stanza, handler->user_data))
        goto out;
    }

  DEBUG ("Stanza not handled");
out:
  g_free (node);
  g_free (domain);
  g_free (resource);
}

static void
abort_pending_iqs (WockyPorter *self,
    GError *error)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, priv->iq_reply_handlers);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      StanzaIqHandler *handler = value;

      if (handler->result == NULL)
        continue;

      /* Don't want to get cancelled during completion */
      stanza_iq_handler_remove_cancellable (handler);

      g_simple_async_result_set_from_error (handler->result, error);
      g_simple_async_result_complete_in_idle (handler->result);

      g_object_unref (handler->result);
      handler->result = NULL;

      if (handler->sent)
        g_hash_table_iter_remove (&iter);
    }
}

static void
remote_connection_closed (WockyPorter *self,
    GError *error)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  gboolean error_occured = TRUE;

  /* Completing a close operation, firing the remote-error signal could make the
   * user unref the porter. Ref it so, in such case, it would stay alive until
   * we have finished to threat the error. */
  g_object_ref (self);

  /* Complete pending send IQ operations as we won't be able to receive their
   * IQ replies */
  abort_pending_iqs (self, error);

  if (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
            WOCKY_XMPP_CONNECTION_ERROR_CLOSED))
    error_occured = FALSE;

  /* This flag MUST be set before we emit the remote-* signals: If it is not *
   * some very subtle and hard to debug problems are created, which can in   *
   * turn conceal further problems in the code. You have been warned.        */
  priv->remote_closed = TRUE;

  if (error_occured)
    {
      g_signal_emit (self, signals[REMOTE_ERROR], 0, error->domain,
          error->code, error->message);
    }
  else
    {
      g_signal_emit (self, signals[REMOTE_CLOSED], 0);
    }

  if (priv->close_result != NULL && priv->local_closed)
    {
      if (error_occured)
        {
          /* We sent our close but something went wrong with the connection
           * so we won't be able to receive close from the other side.
           * Complete the close operation. */
          g_simple_async_result_set_from_error (priv->close_result, error);
        }

       complete_close (self);
    }

  if (priv->receive_cancellable != NULL)
    {
      g_object_unref (priv->receive_cancellable);
      priv->receive_cancellable = NULL;
    }

  g_object_unref (self);
}

static void
connection_force_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *self = WOCKY_PORTER (user_data);
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  GSimpleAsyncResult *r = priv->force_close_result;
  GError *error = NULL;

  /* null out the result so no-one else can use it after us   *
   * this should never happen, but nullifying it lets us trap *
   * that internal inconsistency if it arises                 */
  priv->force_close_result = NULL;
  priv->local_closed = TRUE;

  /* This can fail if the porter has put two                *
   * wocky_xmpp_connection_force_close_async ops in flight  *
   * at the same time: this is bad and should never happen: */
  g_assert (r != NULL);

  if (!wocky_xmpp_connection_force_close_finish (WOCKY_XMPP_CONNECTION (source),
        res, &error))
    {
      g_simple_async_result_set_from_error (r, error);
      g_error_free (error);
    }

  if (priv->receive_cancellable != NULL)
    {
      g_object_unref (priv->receive_cancellable);
      priv->receive_cancellable = NULL;
    }

  DEBUG ("XMPP connection has been closed; complete the force close operation");
  g_simple_async_result_complete (r);
  g_object_unref (r);

  g_object_unref (self);
}

static void
stanza_received_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *self = WOCKY_PORTER (user_data);
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  GError *error = NULL;

  stanza = wocky_xmpp_connection_recv_stanza_finish (
      WOCKY_XMPP_CONNECTION (source), res, &error);
  if (stanza == NULL)
    {
      if (g_error_matches (error, WOCKY_XMPP_CONNECTION_ERROR,
            WOCKY_XMPP_CONNECTION_ERROR_CLOSED))
        {
          DEBUG ("Remote connection has been closed");
        }
      else
        {
          DEBUG ("Error receiving stanza: %s", error->message);
        }

      if (priv->force_close_result != NULL)
        {
          DEBUG ("Receive operation has been cancelled; ");
          if (!priv->forced_shutdown)
            {
              /* We are forcing the closing. Actually close the connection. */
              DEBUG ("force shutdown of the XMPP connection");
              g_object_ref (self);
              priv->forced_shutdown = TRUE;
              wocky_xmpp_connection_force_close_async (priv->connection,
                  priv->force_close_cancellable,
                  connection_force_close_cb, self);
            }
          else
            {
              DEBUG ("forced shutdown of XMPP connection already in progress");
            }
        }
      else
        {
          remote_connection_closed (self, error);
        }

      g_error_free (error);
      return;
    }

  /* Handling a stanza could make the user unref the porter.
   * Ref it so, in such case, it would stay alive until we have finished to
   * threat the stanza. */
  g_object_ref (self);

  handle_stanza (self, stanza);
  g_object_unref (stanza);

  if (!priv->remote_closed)
    {
      /* We didn't detect any error on the stream, wait for next stanza */
      receive_stanza (self);
    }
  else
    {
      DEBUG ("Remote connection has been closed, don't wait for next stanza");
      DEBUG ("Remote connection has been closed; ");

      if (priv->forced_shutdown)
        {
          DEBUG ("forced shutdown of the XMPP connection already in progress");
        }
      else if (priv->force_close_result != NULL)
        {
          DEBUG ("force shutdown of the XMPP connection");
          g_object_ref (self);
          priv->forced_shutdown = TRUE;
          wocky_xmpp_connection_force_close_async (priv->connection,
              priv->force_close_cancellable, connection_force_close_cb, self);
        }
    }

  g_object_unref (self);
}

static void
receive_stanza (WockyPorter *self)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);

  wocky_xmpp_connection_recv_stanza_async (priv->connection,
      priv->receive_cancellable, stanza_received_cb, self);
}

/**
 * wocky_porter_start:
 * @porter: a #WockyPorter
 *
 * Start a #WockyPorter to make it read and dispatch incoming stanzas.
 */
void
wocky_porter_start (WockyPorter *self)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);

  if (priv->receive_cancellable != NULL)
    /* Porter has already been started */
    return;

  priv->receive_cancellable = g_cancellable_new ();

  receive_stanza (self);
}

static void
close_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *self = WOCKY_PORTER (user_data);
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  GError *error = NULL;

  priv->local_closed = TRUE;

  if (!wocky_xmpp_connection_send_close_finish (WOCKY_XMPP_CONNECTION (source),
        res, &error))
    {
      g_simple_async_result_set_from_error (priv->close_result, error);
      g_error_free (error);

      goto out;
    }

  if (!g_cancellable_is_cancelled (priv->close_cancellable)
      && !priv->remote_closed)
    {
      /* we'll complete the close operation once the remote side closes it's
       * connection */
       return;
    }

out:
  if (priv->close_result != NULL)
    {
      /* close operation could already be completed if the other side closes
       * before we send our close */
      complete_close (self);
    }
}

static void
send_close (WockyPorter *self)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);

  wocky_xmpp_connection_send_close_async (priv->connection,
      NULL, close_sent_cb, self);
}

/**
 * wocky_porter_close_async:
 * @porter: a #WockyPorter
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request asynchronous closing of a #WockyPorter. This fires the
 * WockyPorter::closing signal, flushes the sending queue, closes the XMPP
 * stream and waits that the other side closes the XMPP stream as well.
 * When this is done, @callback is called.
 * You can then call wocky_porter_close_finish() to get the result of
 * the operation.
 */
void
wocky_porter_close_async (WockyPorter *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);

  if (priv->local_closed)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_CLOSED,
          "Porter has already been closed");
      return;
    }

  if (priv->receive_cancellable == NULL && !priv->remote_closed)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_NOT_STARTED,
          "Porter has not been started");
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

  if (priv->force_close_result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, G_IO_ERROR, G_IO_ERROR_PENDING,
          "A force close operation is pending");
      return;
    }

  priv->close_result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_porter_close_finish);

  priv->close_cancellable = cancellable;

  g_signal_emit (self, signals[CLOSING], 0);

  if (g_queue_get_length (priv->sending_queue) > 0)
    {
      DEBUG ("Sending queue is not empty. Flushing it before "
          "closing the connection.");
      return;
    }

  send_close (self);
}

/**
 * wocky_porter_close_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore.
 *
 * Finishes a close operation.
 *
 * Returns: %TRUE on success or %FALSE on error.
 */
gboolean
wocky_porter_close_finish (
    WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_porter_close_finish), FALSE);

  return TRUE;
}

static gint
compare_handler (StanzaHandler *a,
    StanzaHandler *b)
{
  /* List is sorted by decreasing priority */
  if (a->priority < b->priority)
    return 1;
  else if (a->priority > b->priority)
    return -1;
  else
    return 0;
}

/**
 * wocky_porter_register_handler:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @from: the JID whose messages this handler is intended for, or %NULL to
 *  match messages from any sender.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @spec: The start of a wocky_xmpp_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  built with @spec and its subsequent arguments, as per
 *  wocky_xmpp_node_is_superset().
 * @Varargs: the rest of the args to wocky_xmpp_stanza_build(),
 *  terminated by %WOCKY_STANZA_END
 *
 * Register a new stanza handler.
 * Stanza handlers are called when the Porter receives a new stanza matching
 * the rules of the handler. Matching handlers are sorted by priority and are
 * called until one claims to have handled the stanza (by returning %TRUE).
 *
 * If @from is a bare JID, then the resource of the JID in the from attribute
 * will be ignored: In other words, a handler registered against a bare JID
 * will match _all_ stanzas from a JID with the same node and domain:
 * "foo@<!-- -->bar.org" will match
 * "foo@<!-- -->bar.org", "foo@<!-- -->bar.org/moose" and so forth.
 *
 * To register a handler matching all message stanzas received from anyone:
 *
 * |[
 * id = wocky_porter_register_handler (porter,
 *   WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE, NULL,
 *   WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, message_received_cb, NULL,
 *   WOCKY_STANZA_END);
 * ]|
 *
 * To register an IQ handler from Juliet for all the Jingle stanzas related
 * to one Jingle session:
 *
 * |[
 * id = wocky_porter_register_handler (porter,
 *   WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE, NULL,
 *   WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, jingle_cb,
 *   "juliet@example.com/Balcony",
 *   WOCKY_NODE, "jingle",
 *     WOCKY_NODE_XMLNS, "urn:xmpp:jingle:1",
 *     WOCKY_NODE_ATTRIBUTE, "sid", "my_sid",
 *   WOCKY_NODE_END, WOCKY_STANZA_END);
 * ]|
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyBuildTag spec,
    ...)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  StanzaHandler *handler;
  WockyXmppStanza *stanza;
  va_list ap;

  va_start (ap, spec);
  stanza = wocky_xmpp_stanza_build_va (type, WOCKY_STANZA_SUB_TYPE_NONE,
      NULL, NULL, spec, ap);
  g_assert (stanza != NULL);
  va_end (ap);

  handler = stanza_handler_new (type, sub_type, from, priority, stanza,
      callback, user_data);
  g_object_unref (stanza);

  g_hash_table_insert (priv->handlers_by_id,
      GUINT_TO_POINTER (priv->next_handler_id), handler);
  priv->handlers = g_list_insert_sorted (priv->handlers, handler,
      (GCompareFunc) compare_handler);

  return priv->next_handler_id++;
}

/**
 * wocky_porter_unregister_handler:
 * @porter: a #WockyPorter
 * @id: the id of the handler to unregister
 *
 * Unregister a registered handler. This handler won't be called when
 * receiving stanzas anymore.
 */
void
wocky_porter_unregister_handler (WockyPorter *self,
    guint id)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  StanzaHandler *handler;

  handler = g_hash_table_lookup (priv->handlers_by_id, GUINT_TO_POINTER (id));
  if (handler == NULL)
    {
      g_warning ("Trying to remove an unregistered handler: %u", id);
      return;
    }

  priv->handlers = g_list_remove (priv->handlers, handler);
  g_hash_table_remove (priv->handlers_by_id, GUINT_TO_POINTER (id));
}

static void
send_iq_cancelled_cb (GCancellable *cancellable,
    gpointer user_data)
{
  StanzaIqHandler *handler = (StanzaIqHandler *) user_data;
  GError error = { G_IO_ERROR, G_IO_ERROR_CANCELLED,
      "IQ sending was cancelled" };

  /* The disconnect should always be disconnected if the result has been
   * finished */
  g_assert (handler->result != NULL);

  g_simple_async_result_set_from_error (handler->result, &error);
  g_simple_async_result_complete_in_idle (handler->result);
  g_object_unref (handler->result);
  handler->result = NULL;

  stanza_iq_handler_maybe_remove (handler);
}

static void
iq_sent_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyPorter *self = WOCKY_PORTER (source);
  StanzaIqHandler *handler = (StanzaIqHandler *) user_data;
  GError *error = NULL;

  handler->sent = TRUE;

  if (wocky_porter_send_finish (self, res, &error))
    goto finished;

  /* Raise an error */
  if (handler->result != NULL)
    {
      GSimpleAsyncResult *r = handler->result;
      handler->result = NULL;

      /* Don't want to get cancelled during completion */
      stanza_iq_handler_remove_cancellable (handler);

      g_simple_async_result_set_from_error (r, error);
      g_simple_async_result_complete (r);
      g_object_unref (r);
    }
  g_error_free (error);

finished:
  stanza_iq_handler_maybe_remove (handler);
}

/**
 * wocky_porter_send_iq_async:
 * @porter: a #WockyPorter
 * @stanza: the #WockyXmppStanza to send
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request asynchronous sending of a #WockyXmppStanza of type
 * %WOCKY_STANZA_TYPE_IQ and sub-type %WOCKY_STANZA_SUB_TYPE_GET or
 * %WOCKY_STANZA_SUB_TYPE_SET.
 * When the reply to this IQ has been received callback will be called.
 * You can then call #wocky_porter_send_iq_finish to get the reply stanza.
 */
void
wocky_porter_send_iq_async (WockyPorter *self,
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  StanzaIqHandler *handler;
  const gchar *recipient;
  gchar *id = NULL;
  GSimpleAsyncResult *result;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  if (priv->close_result != NULL || priv->force_close_result != NULL)
    {
      gchar *node = NULL;

      g_assert (stanza != NULL && stanza->node != NULL);

      node = wocky_xmpp_node_to_string (stanza->node);
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_CLOSING,
          "Porter is closing: iq '%s' aborted", node);
      g_free (node);

      return;
    }

  wocky_xmpp_stanza_get_type_info (stanza, &type, &sub_type);

  if (type != WOCKY_STANZA_TYPE_IQ)
    goto wrong_stanza;

  if (sub_type != WOCKY_STANZA_SUB_TYPE_GET &&
      sub_type != WOCKY_STANZA_SUB_TYPE_SET)
    goto wrong_stanza;

  recipient = wocky_xmpp_node_get_attribute (stanza->node, "to");

  /* Set an unique ID */
  do
    {
      g_free (id);
      id = wocky_xmpp_connection_new_id (priv->connection);
    }
  while (g_hash_table_lookup (priv->iq_reply_handlers, id) != NULL);

  wocky_xmpp_node_set_attribute (stanza->node, "id", id);

  result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_porter_send_iq_finish);

  handler = stanza_iq_handler_new (self, id, result, cancellable,
      recipient);

  if (cancellable != NULL)
    {
      handler->cancelled_sig_id = g_cancellable_connect (cancellable,
          G_CALLBACK (send_iq_cancelled_cb), handler, NULL);
    }

  g_hash_table_insert (priv->iq_reply_handlers, id, handler);

  wocky_porter_send_async (self, stanza, cancellable, iq_sent_cb,
      handler);
  return;

wrong_stanza:
  g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
      user_data, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_NOT_IQ,
      "Stanza is not an IQ query");
}

/**
 * wocky_porter_send_iq_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore.
 *
 * Get the reply of an IQ query.
 *
 * Returns: a reffed #WockyXmppStanza on success, %NULL on error
 */
WockyXmppStanza * wocky_porter_send_iq_finish (
    WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  WockyXmppStanza *reply;

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_porter_send_iq_finish), NULL);

  reply = g_simple_async_result_get_op_res_gpointer (
      G_SIMPLE_ASYNC_RESULT (result));

  return g_object_ref (reply);
}

/**
 * wocky_porter_force_close_async:
 * @porter: a #WockyPorter
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Force the #WockyPorter to close the TCP connection of the underlying
 * #WockyXmppConnection.
 * If a close operation is pending, it will be completed with the
 * %WOCKY_PORTER_ERROR_FORCE_CLOSING error.
 * When the connection has been closed, @callback will be called.
 * You can then call wocky_porter_force_close_finish() to get the result of
 * the operation.
 */
void
wocky_porter_force_close_async (WockyPorter *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterPrivate *priv = WOCKY_PORTER_GET_PRIVATE (self);
  sending_queue_elem *elem;
  GError err = { WOCKY_PORTER_ERROR, WOCKY_PORTER_ERROR_FORCIBLY_CLOSED,
      "Porter was closed forcibly" };

  if (priv->force_close_result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, G_IO_ERROR, G_IO_ERROR_PENDING,
          "Another force close operation is pending");
      return;
    }

  if (priv->receive_cancellable == NULL && priv->local_closed)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_CLOSED,
          "Porter has already been closed");
      return;
    }

  if (priv->receive_cancellable == NULL && !priv->remote_closed)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_NOT_STARTED,
          "Porter has not been started");
      return;
    }

  /* Ensure to keep us alive during the closing */
  g_object_ref (self);

  if (priv->close_result != NULL)
    {
      /* Finish pending close operation */
      g_simple_async_result_set_from_error (priv->close_result, &err);
      g_simple_async_result_complete_in_idle (priv->close_result);
      g_object_unref (priv->close_result);
      priv->close_result = NULL;
    }
  else
    {
      /* the "closing" signal has already been fired when _close_async has
       * been called */
      g_signal_emit (self, signals[CLOSING], 0);
    }

  priv->force_close_result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_porter_force_close_finish);
  priv->force_close_cancellable = cancellable;

  /* force_close_result now keeps a ref on ourself so we can release the ref
   * without risking to destroy the object */
  g_object_unref (self);

  /* Terminate all the pending sending operations */
  elem = g_queue_pop_head (priv->sending_queue);
  while (elem != NULL)
    {
      g_simple_async_result_set_from_error (elem->result, &err);
      g_simple_async_result_complete_in_idle (elem->result);
      sending_queue_elem_free (elem);
      elem = g_queue_pop_head (priv->sending_queue);
    }

  /* Terminate all the pending send IQ operations */
  abort_pending_iqs (self, &err);

  if (priv->remote_closed)
    {
      /* forced shutdown in progress. noop */
      if (priv->forced_shutdown)
        {
          g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
              user_data, WOCKY_PORTER_ERROR,
              WOCKY_PORTER_ERROR_FORCIBLY_CLOSED,
              "Porter is already executing a forced-shutdown");
          return;
        }
      /* No need to wait, close connection right now */
      DEBUG ("remote is already closed, close the XMPP connection");
      g_object_ref (self);
      priv->forced_shutdown = TRUE;
      wocky_xmpp_connection_force_close_async (priv->connection,
          priv->force_close_cancellable, connection_force_close_cb, self);
      return;
    }

  /* The operation will be completed when:
   * - the receive operation has been cancelled
   * - the XMPP connection has been closed
   */

  g_cancellable_cancel (priv->receive_cancellable);
}

/**
 * wocky_porter_force_close_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore.
 *
 * Finishes a force close operation.
 *
 * Returns: %TRUE on success or %FALSE on error.
 */
gboolean
wocky_porter_force_close_finish (
    WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_porter_force_close_finish), FALSE);

  return TRUE;
}
