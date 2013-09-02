/*
 * wocky-c2s-porter.c - Source for WockyC2SPorter
 * Copyright (C) 2009-2011 Collabora Ltd.
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
 * SECTION: wocky-c2s-porter
 * @title: WockyC2SPorter
 * @short_description: Wrapper around a #WockyXmppConnection providing a
 * higher level API.
 *
 * Sends and receives #WockyStanza from an underlying
 * #WockyXmppConnection.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-c2s-porter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <gio/gio.h>

#include "wocky-porter.h"
#include "wocky-utils.h"
#include "wocky-namespaces.h"
#include "wocky-contact-factory.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_PORTER
#include "wocky-debug-internal.h"

static void wocky_porter_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (WockyC2SPorter, wocky_c2s_porter, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_PORTER,
        wocky_porter_iface_init));

/* properties */
enum
{
  PROP_CONNECTION = 1,
  PROP_FULL_JID,
  PROP_BARE_JID,
  PROP_RESOURCE,
};

/* private structure */
struct _WockyC2SPorterPrivate
{
  gboolean dispose_has_run;
  gboolean forced_shutdown;

  gchar *full_jid;
  gchar *bare_jid;
  gchar *resource;
  gchar *domain;

  /* Queue of (sending_queue_elem *) */
  GQueue *sending_queue;
  GCancellable *receive_cancellable;
  gboolean sending_whitespace_ping;

  GSimpleAsyncResult *close_result;
  gboolean waiting_to_close;
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

  gboolean power_saving_mode;
  /* Queue of (owned WockyStanza *) */
  GQueue *unimportant_queue;
  /* List of (owned WockyStanza *) */
  GQueue queueable_stanza_patterns;

  WockyXmppConnection *connection;
};

typedef struct
{
  WockyC2SPorter *self;
  WockyStanza *stanza;
  GCancellable *cancellable;
  GSimpleAsyncResult *result;
  gulong cancelled_sig_id;
} sending_queue_elem;

static void wocky_c2s_porter_send_async (WockyPorter *porter,
    WockyStanza *stanza, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data);

static sending_queue_elem *
sending_queue_elem_new (WockyC2SPorter *self,
  WockyStanza *stanza,
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
    callback, user_data, wocky_c2s_porter_send_async);

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

typedef enum {
    MATCH_ANYONE,
    MATCH_SERVER,
    MATCH_JID
} SenderMatch;

typedef struct {
    gchar *node;
    gchar *domain;
    gchar *resource;
} JidTriple;

typedef struct
{
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  SenderMatch sender_match;
  JidTriple jid;
  guint priority;
  WockyStanza *match;
  WockyPorterHandlerFunc callback;
  gpointer user_data;
} StanzaHandler;

static StanzaHandler *
stanza_handler_new (
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    SenderMatch sender_match,
    JidTriple *jid,
    guint priority,
    WockyStanza *stanza,
    WockyPorterHandlerFunc callback,
    gpointer user_data)
{
  StanzaHandler *result = g_slice_new0 (StanzaHandler);

  result->type = type;
  result->sub_type = sub_type;
  result->priority = priority;
  result->callback = callback;
  result->user_data = user_data;
  result->sender_match = sender_match;

  if (stanza != NULL)
    result->match = g_object_ref (stanza);

  if (sender_match == MATCH_JID)
    {
      g_assert (jid != NULL);

      result->jid = *jid;
    }
  else
    {
      g_assert (jid == NULL);
    }

  return result;
}

static void
stanza_handler_free (StanzaHandler *handler)
{
  g_free (handler->jid.node);
  g_free (handler->jid.domain);
  g_free (handler->jid.resource);

  if (handler->match != NULL)
    g_object_unref (handler->match);

  g_slice_free (StanzaHandler, handler);
}

typedef struct
{
  WockyC2SPorter *self;
  GSimpleAsyncResult *result;
  GCancellable *cancellable;
  gulong cancelled_sig_id;
  gchar *recipient;
  gchar *id;
  gboolean sent;
} StanzaIqHandler;

static StanzaIqHandler *
stanza_iq_handler_new (WockyC2SPorter *self,
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
      /* We might have already have disconnected the signal handler
       * from send_head_stanza(), so check whether it's still connected. */
      if (handler->cancelled_sig_id > 0)
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
      WockyC2SPorterPrivate *priv = handler->self->priv;
      g_hash_table_remove (priv->iq_reply_handlers, handler->id);
    }
}

static void send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);

static void send_close (WockyC2SPorter *self);

static gboolean handle_iq_reply (WockyPorter *porter,
    WockyStanza *reply,
    gpointer user_data);

static void remote_connection_closed (WockyC2SPorter *self,
    GError *error);

static void
wocky_c2s_porter_init (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_C2S_PORTER,
      WockyC2SPorterPrivate);
  priv = self->priv;

  priv->sending_queue = g_queue_new ();

  priv->handlers_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) stanza_handler_free);
  /* these are guints, reserve 0 for "not a valid handler" */
  priv->next_handler_id = 1;
  priv->handlers = NULL;
  priv->power_saving_mode = FALSE;
  priv->unimportant_queue = g_queue_new ();

  priv->iq_reply_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) stanza_iq_handler_free);
}

static void wocky_c2s_porter_dispose (GObject *object);
static void wocky_c2s_porter_finalize (GObject *object);

static void
wocky_c2s_porter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyC2SPorter *connection = WOCKY_C2S_PORTER (object);
  WockyC2SPorterPrivate *priv =
      connection->priv;

  switch (property_id)
    {
      gchar *node;

      case PROP_CONNECTION:
        g_assert (priv->connection == NULL);
        priv->connection = g_value_dup_object (value);
        g_assert (priv->connection != NULL);
        break;

      case PROP_FULL_JID:
        g_assert (priv->full_jid == NULL);    /* construct-only */
        g_assert (priv->bare_jid == NULL);
        g_assert (priv->resource == NULL);

        priv->full_jid = g_value_dup_string (value);
        g_assert (priv->full_jid != NULL);
        wocky_decode_jid (priv->full_jid, &node, &priv->domain, &priv->resource);
        priv->bare_jid = wocky_compose_jid (node, priv->domain, NULL);
        g_free (node);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_c2s_porter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyC2SPorter *connection = WOCKY_C2S_PORTER (object);
  WockyC2SPorterPrivate *priv =
      connection->priv;

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;

      case PROP_FULL_JID:
        g_value_set_string (value, priv->full_jid);
        break;

      case PROP_BARE_JID:
        g_value_set_string (value, priv->bare_jid);
        break;

      case PROP_RESOURCE:
        g_value_set_string (value, priv->resource);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
handle_stream_error (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  GError *error = NULL;
  gboolean ret = wocky_stanza_extract_stream_error (stanza, &error);

  /* If wocky_stanza_extract_stream_error() failed, @stanza wasn't a stream
   * error, in which case we are broken.
   */
  g_return_val_if_fail (ret, FALSE);

  DEBUG ("Received stream error; consider the remote connection to be closed");
  remote_connection_closed (self, error);
  g_error_free (error);
  return TRUE;
}

static void
wocky_c2s_porter_constructed (GObject *object)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (object);
  WockyC2SPorterPrivate *priv = self->priv;

  if (G_OBJECT_CLASS (wocky_c2s_porter_parent_class)->constructed)
    G_OBJECT_CLASS (wocky_c2s_porter_parent_class)->constructed (object);

  g_assert (priv->connection != NULL);

  /* Register the IQ reply handler */
  wocky_porter_register_handler_from_anyone (WOCKY_PORTER (self),
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_RESULT,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      handle_iq_reply, self, NULL);

  wocky_porter_register_handler_from_anyone (WOCKY_PORTER (self),
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_ERROR,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      handle_iq_reply, self, NULL);

  /* Register the stream error handler. We use _from_anyone() here because we can
   * trust servers not to relay spurious stream errors to us, and this feels
   * safer than risking missing a stream error to bugs in the _from_server()
   * checking code. */
  wocky_porter_register_handler_from_anyone (WOCKY_PORTER (self),
      WOCKY_STANZA_TYPE_STREAM_ERROR, WOCKY_STANZA_SUB_TYPE_NONE,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      handle_stream_error, self, NULL);
}

static void
wocky_c2s_porter_class_init (
    WockyC2SPorterClass *wocky_c2s_porter_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_c2s_porter_class);

  g_type_class_add_private (wocky_c2s_porter_class,
      sizeof (WockyC2SPorterPrivate));

  object_class->constructed = wocky_c2s_porter_constructed;
  object_class->set_property = wocky_c2s_porter_set_property;
  object_class->get_property = wocky_c2s_porter_get_property;
  object_class->dispose = wocky_c2s_porter_dispose;
  object_class->finalize = wocky_c2s_porter_finalize;

  g_object_class_override_property (object_class,
      PROP_CONNECTION, "connection");
  g_object_class_override_property (object_class,
      PROP_FULL_JID, "full-jid");
  g_object_class_override_property (object_class,
      PROP_BARE_JID, "bare-jid");
  g_object_class_override_property (object_class,
      PROP_RESOURCE, "resource");
}

void
wocky_c2s_porter_dispose (GObject *object)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (object);
  WockyC2SPorterPrivate *priv =
      self->priv;

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

  if (priv->close_cancellable != NULL)
    {
      g_object_unref (priv->close_cancellable);
      priv->close_cancellable = NULL;
    }

  if (priv->force_close_result != NULL)
    {
      g_object_unref (priv->force_close_result);
      priv->force_close_result = NULL;
    }

  if (priv->force_close_cancellable != NULL)
    {
      g_object_unref (priv->force_close_cancellable);
      priv->force_close_cancellable = NULL;
    }

  if (G_OBJECT_CLASS (wocky_c2s_porter_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_c2s_porter_parent_class)->dispose (object);
}

void
wocky_c2s_porter_finalize (GObject *object)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (object);
  WockyC2SPorterPrivate *priv =
      self->priv;

  DEBUG ("finalize porter %p", self);

  /* sending_queue_elem keeps a ref on the Porter (through the
   * GSimpleAsyncResult) so it shouldn't be destroyed while there are
   * elements in the queue. */
  g_assert_cmpuint (g_queue_get_length (priv->sending_queue), ==, 0);
  g_queue_free (priv->sending_queue);

  g_hash_table_unref (priv->handlers_by_id);
  g_list_free (priv->handlers);
  g_hash_table_unref (priv->iq_reply_handlers);

  g_queue_free (priv->unimportant_queue);

  g_queue_foreach (&priv->queueable_stanza_patterns, (GFunc) g_object_unref, NULL);
  g_queue_clear (&priv->queueable_stanza_patterns);

  g_free (priv->full_jid);
  g_free (priv->bare_jid);
  g_free (priv->resource);
  g_free (priv->domain);

  G_OBJECT_CLASS (wocky_c2s_porter_parent_class)->finalize (object);
}

/**
 * wocky_c2s_porter_new:
 * @connection: #WockyXmppConnection which will be used to receive and send
 * #WockyStanza
 * @full_jid: the full JID of the user
 *
 * Convenience function to create a new #WockyC2SPorter.
 *
 * Returns: a new #WockyPorter.
 */
WockyPorter *
wocky_c2s_porter_new (WockyXmppConnection *connection,
    const gchar *full_jid)
{
  return g_object_new (WOCKY_TYPE_C2S_PORTER,
    "connection", connection,
    "full-jid", full_jid,
    NULL);
}

static void
send_head_stanza (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;
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
      elem->stanza, elem->cancellable, send_stanza_cb, g_object_ref (self));

  g_signal_emit_by_name (self, "sending", elem->stanza);
}

static void
terminate_sending_operations (WockyC2SPorter *self,
    GError *error)
{
  WockyC2SPorterPrivate *priv = self->priv;
  sending_queue_elem *elem;

  g_return_if_fail (error != NULL);

  while ((elem = g_queue_pop_head (priv->sending_queue)))
    {
      g_simple_async_result_set_from_error (elem->result, error);
      g_simple_async_result_complete (elem->result);
      sending_queue_elem_free (elem);
    }
}

static gboolean
sending_in_progress (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;

  return g_queue_get_length (priv->sending_queue) > 0 ||
    priv->sending_whitespace_ping;
}

static void
close_if_waiting (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;

  if (priv->waiting_to_close && !sending_in_progress (self))
    {
      /* Nothing to send left and we are waiting to close the connection. */
      DEBUG ("Queue has been flushed. Closing the connection.");
      send_close (self);
    }
}

static void
send_stanza_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (user_data);
  WockyC2SPorterPrivate *priv = self->priv;
  GError *error = NULL;

  if (!wocky_xmpp_connection_send_stanza_finish (
        WOCKY_XMPP_CONNECTION (source), res, &error))
    {
      /* Sending failed. Cancel this sending operation and all the others
       * pending ones as we won't be able to send any more stanza. */
      terminate_sending_operations (self, error);
      g_error_free (error);
    }
  else
    {
      sending_queue_elem *elem = g_queue_pop_head (priv->sending_queue);

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

  close_if_waiting (self);

  g_object_unref (self);
}

static void
send_cancelled_cb (GCancellable *cancellable,
    gpointer user_data)
{
  sending_queue_elem *elem = (sending_queue_elem *) user_data;
  WockyC2SPorterPrivate *priv = elem->self->priv;
  GError error = { G_IO_ERROR, G_IO_ERROR_CANCELLED, "Sending was cancelled" };

  g_simple_async_result_set_from_error (elem->result, &error);
  g_simple_async_result_complete_in_idle (elem->result);

  g_queue_remove (priv->sending_queue, elem);
  sending_queue_elem_free (elem);
}

static void
wocky_c2s_porter_send_async (WockyPorter *porter,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;
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

  if (g_queue_get_length (priv->sending_queue) == 1 &&
      !priv->sending_whitespace_ping)
    {
      send_head_stanza (self);
    }
  else if (cancellable != NULL)
    {
      elem->cancelled_sig_id = g_cancellable_connect (cancellable,
          G_CALLBACK (send_cancelled_cb), elem, NULL);
    }
}

static gboolean
wocky_c2s_porter_send_finish (WockyPorter *porter,
    GAsyncResult *result,
    GError **error)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_c2s_porter_send_async), FALSE);

  return TRUE;
}

static void receive_stanza (WockyC2SPorter *self);

static void
complete_close (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;
  GSimpleAsyncResult *tmp;

  if (g_cancellable_is_cancelled (priv->close_cancellable))
    {
      g_simple_async_result_set_error (priv->close_result, G_IO_ERROR,
          G_IO_ERROR_CANCELLED, "closing operation was cancelled");
    }

  if (priv->close_cancellable)
    g_object_unref (priv->close_cancellable);

  priv->close_cancellable = NULL;

 if (priv->force_close_cancellable)
    g_object_unref (priv->force_close_cancellable);

  priv->force_close_cancellable = NULL;

  tmp = priv->close_result;
  priv->close_result = NULL;
  g_simple_async_result_complete (tmp);
  g_object_unref (tmp);
}

static gboolean
stanza_is_from_server (
    WockyC2SPorter *self,
    const gchar *nfrom)
{
  return (nfrom == NULL ||
      !wocky_strdiff (nfrom, self->priv->full_jid) ||
      !wocky_strdiff (nfrom, self->priv->bare_jid) ||
      !wocky_strdiff (nfrom, self->priv->domain));
}

/* Return TRUE if not spoofed. */
static gboolean
check_spoofing (WockyC2SPorter *self,
    WockyStanza *reply,
    const gchar *should_be_from)
{
  const gchar *from;
  gchar *nfrom;
  gboolean ret = TRUE;

  from = wocky_stanza_get_from (reply);

  /* fast path for a byte-for-byte match */
  if (G_LIKELY (!wocky_strdiff (from, should_be_from)))
    return TRUE;

  /* OK, we have to do some work */

  nfrom = wocky_normalise_jid (from);

  /* nearly-as-fast path for a normalized match */
  if (!wocky_strdiff (nfrom, should_be_from))
    goto finally;

  /* if we sent an IQ without a 'to' attribute, it's to our server: allow it
   * to use our full/bare JID or domain to reply */
  if (should_be_from == NULL)
    {
      if (stanza_is_from_server (self, nfrom))
        goto finally;
    }

  /* If we sent an IQ to the server itself, allow it to
   * omit 'from' in its reply, which is normally used
   * for messages from the server on behalf of our own
   * account (as of 2013-09-02, the Facebook beta server
   * does this). See fd.o #68829 */

  if (from == NULL && !wocky_strdiff (should_be_from, self->priv->domain)) {
      goto finally;
  }

  /* if we sent an IQ to our full or bare JID, allow our server to omit 'to'
   * in the reply (Prosody 0.6.1 does this with the resulting error if we
   * send disco#info to our own bare JID), or to use our full JID. */
  if (from == NULL || !wocky_strdiff (nfrom, self->priv->full_jid))
    {
      if (!wocky_strdiff (should_be_from, self->priv->full_jid) ||
          !wocky_strdiff (should_be_from, self->priv->bare_jid))
        goto finally;
    }

  DEBUG ("'%s' (normal: '%s') attempts to spoof an IQ reply from '%s'",
      from == NULL ? "(null)" : from,
      nfrom == NULL ? "(null)" : nfrom,
      should_be_from == NULL ? "(null)" : should_be_from);
  DEBUG ("Our full JID is '%s' and our bare JID is '%s'",
      self->priv->full_jid, self->priv->bare_jid);

  ret = FALSE;

finally:
  g_free (nfrom);
  return ret;
}

static gboolean
handle_iq_reply (WockyPorter *porter,
    WockyStanza *reply,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;
  const gchar *id;
  StanzaIqHandler *handler;
  gboolean ret = FALSE;

  id = wocky_node_get_attribute (wocky_stanza_get_top_node (reply), "id");
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

  if (!check_spoofing (self, reply, handler->recipient))
    return FALSE;

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
handle_stanza (WockyC2SPorter *self,
    WockyStanza *stanza)
{
  WockyC2SPorterPrivate *priv = self->priv;
  GList *l;
  const gchar *from;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  gchar *node = NULL, *domain = NULL, *resource = NULL;
  gboolean is_from_server;
  gboolean handled = FALSE;

  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  /* The from attribute of the stanza need not always be present, for example
   * when receiving roster items, so don't enforce it. */
  from = wocky_stanza_get_from (stanza);

  if (from == NULL)
    {
      is_from_server = TRUE;
    }
  else if (wocky_decode_jid (from, &node, &domain, &resource))
    {
      /* FIXME: the stanza should really ensure that 'from' and 'to' are
       * pre-validated and normalized so we don't have to do this again.
       */
      gchar *nfrom = wocky_compose_jid (node, domain, resource);

      is_from_server = stanza_is_from_server (self, nfrom);
      g_free (nfrom);
    }
  else
    {
      is_from_server = FALSE;
    }

  for (l = priv->handlers; l != NULL && !handled; l = g_list_next (l))
    {
      StanzaHandler *handler = (StanzaHandler *) l->data;

      if (type != handler->type &&
          handler->type != WOCKY_STANZA_TYPE_NONE)
        continue;

      if (sub_type != handler->sub_type &&
          handler->sub_type != WOCKY_STANZA_SUB_TYPE_NONE)
        continue;

      switch (handler->sender_match)
        {
          case MATCH_ANYONE:
            break;

          case MATCH_SERVER:
            if (!is_from_server)
              continue;
            break;

          case MATCH_JID:
            g_assert (handler->jid.domain != NULL);

            if (wocky_strdiff (node, handler->jid.node))
              continue;

            if (wocky_strdiff (domain, handler->jid.domain))
              continue;

            /* If a resource was specified, we need to match against it. */
            if (handler->jid.resource != NULL &&
                wocky_strdiff (resource, handler->jid.resource))
              continue;

            break;
        }

      /* Check if the stanza matches the pattern */
      if (handler->match != NULL &&
          !wocky_node_is_superset (wocky_stanza_get_top_node (stanza),
              wocky_stanza_get_top_node (handler->match)))
        continue;

      handled = handler->callback (WOCKY_PORTER (self), stanza,
          handler->user_data);
    }

  if (!handled)
    {
      DEBUG ("Stanza not handled");

      if (type == WOCKY_STANZA_TYPE_IQ &&
          (sub_type == WOCKY_STANZA_SUB_TYPE_GET ||
           sub_type == WOCKY_STANZA_SUB_TYPE_SET))
        wocky_porter_send_iq_error (WOCKY_PORTER (self), stanza,
            WOCKY_XMPP_ERROR_SERVICE_UNAVAILABLE, NULL);
    }

  g_free (node);
  g_free (domain);
  g_free (resource);
}

/* immediately handle any queued stanzas */
static void
flush_unimportant_queue (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;

  while (!g_queue_is_empty (priv->unimportant_queue))
    {
      WockyStanza *stanza = g_queue_pop_head (priv->unimportant_queue);
      handle_stanza (self, stanza);
      g_object_unref (stanza);
    }
}

/* create a list of patterns of stanzas that can be safely queued */
static void
build_queueable_stanza_patterns (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;
  gchar **node_name = NULL;
  gchar *node_names [] = {
      "http://jabber.org/protocol/geoloc",
      "http://jabber.org/protocol/nick",
      "http://laptop.org/xmpp/buddy-properties",
      "http://laptop.org/xmpp/activities",
      "http://laptop.org/xmpp/current-activity",
      "http://laptop.org/xmpp/activity-properties",
      NULL};

  for (node_name = node_names; *node_name != NULL ; node_name++)
    {
      WockyStanza *pattern = wocky_stanza_build (
          WOCKY_STANZA_TYPE_MESSAGE,
          WOCKY_STANZA_SUB_TYPE_NONE, NULL, NULL,
          '(', "event",
            ':', WOCKY_XMPP_NS_PUBSUB_EVENT,
            '(', "items",
            '@', "node", *node_name,
            ')',
          ')',
          NULL);

      g_queue_push_tail (&priv->queueable_stanza_patterns, pattern);
    }
}

static gboolean
is_stanza_important (WockyC2SPorter *self,
    WockyStanza *stanza)
{
  WockyC2SPorterPrivate *priv = self->priv;
  WockyNode *node = wocky_stanza_get_top_node (stanza);
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  GList *l;

  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  /* <presence/> and <presence type="unavailable"/> are queueable */
  if (type == WOCKY_STANZA_TYPE_PRESENCE &&
      (sub_type == WOCKY_STANZA_SUB_TYPE_NONE ||
       sub_type == WOCKY_STANZA_SUB_TYPE_UNAVAILABLE))
    {
      return FALSE;
    }

  if (priv->queueable_stanza_patterns.length == 0)
    build_queueable_stanza_patterns (self);

  /* check whether stanza matches any of the queueable patterns */
  for (l = priv->queueable_stanza_patterns.head; l != NULL; l = l->next)
    {
      if (wocky_node_is_superset (node, wocky_stanza_get_top_node (
          WOCKY_STANZA (l->data))))
        return FALSE;
    }

  /* everything else is important */
  return TRUE;
}

static void
queue_or_handle_stanza (WockyC2SPorter *self,
    WockyStanza *stanza)
{
  WockyC2SPorterPrivate *priv = self->priv;

  if (priv->power_saving_mode)
    {
      if (is_stanza_important (self, stanza))
        {
          flush_unimportant_queue (self);
          handle_stanza (self, stanza);
        }
      else
        {
          g_queue_push_tail (priv->unimportant_queue, g_object_ref (stanza));
        }
    }
  else
    {
      handle_stanza (self, stanza);
    }
}

static void
abort_pending_iqs (WockyC2SPorter *self,
    GError *error)
{
  WockyC2SPorterPrivate *priv = self->priv;
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
remote_connection_closed (WockyC2SPorter *self,
    GError *error)
{
  WockyC2SPorterPrivate *priv = self->priv;
  gboolean error_occured = TRUE;

  /* Completing a close operation and firing the remote-closed/remote-error
   * signals could make the library user unref the porter. So we take a
   * reference to ourself for the duration of this function.
   */
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
      g_signal_emit_by_name (self, "remote-error", error->domain,
          error->code, error->message);
    }
  else
    {
      g_signal_emit_by_name (self, "remote-closed");
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
  WockyC2SPorter *self = WOCKY_C2S_PORTER (user_data);
  WockyC2SPorterPrivate *priv = self->priv;
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
  WockyC2SPorter *self = WOCKY_C2S_PORTER (user_data);
  WockyC2SPorterPrivate *priv = self->priv;
  WockyStanza *stanza;
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

  /* Calling out to a stanza handler could make the library user unref the
   * porter; hence, we take a reference to ourself for the rest of the
   * function.
   */
  g_object_ref (self);

  queue_or_handle_stanza (self, stanza);
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
receive_stanza (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;

  wocky_xmpp_connection_recv_stanza_async (priv->connection,
      priv->receive_cancellable, stanza_received_cb, self);
}

static void
wocky_c2s_porter_start (WockyPorter *porter)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;

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
  WockyC2SPorter *self = WOCKY_C2S_PORTER (user_data);
  WockyC2SPorterPrivate *priv = self->priv;
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
send_close (WockyC2SPorter *self)
{
  WockyC2SPorterPrivate *priv = self->priv;

  wocky_xmpp_connection_send_close_async (priv->connection,
      NULL, close_sent_cb, self);
  priv->waiting_to_close = FALSE;
}

static void
wocky_c2s_porter_close_async (WockyPorter *porter,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;

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
    callback, user_data, wocky_c2s_porter_close_async);

  g_assert (priv->close_cancellable == NULL);

  if (cancellable != NULL)
    priv->close_cancellable = g_object_ref (cancellable);

  g_signal_emit_by_name (self, "closing");

  if (sending_in_progress (self))
    {
      DEBUG ("Sending queue is not empty. Flushing it before "
          "closing the connection.");
      priv->waiting_to_close = TRUE;
      return;
    }

  send_close (self);
}

static gboolean
wocky_c2s_porter_close_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_c2s_porter_close_async), FALSE);

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

static guint
wocky_c2s_porter_register_handler_internal (WockyC2SPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    SenderMatch sender_match,
    JidTriple *jid,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyC2SPorterPrivate *priv = self->priv;
  StanzaHandler *handler;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);

  handler = stanza_handler_new (type, sub_type, sender_match, jid, priority,
      stanza, callback, user_data);

  g_hash_table_insert (priv->handlers_by_id,
      GUINT_TO_POINTER (priv->next_handler_id), handler);
  priv->handlers = g_list_insert_sorted (priv->handlers, handler,
      (GCompareFunc) compare_handler);

  return priv->next_handler_id++;
}

static guint
wocky_c2s_porter_register_handler_from_by_stanza (WockyPorter *porter,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  JidTriple jid;
  gboolean from_valid;

  g_return_val_if_fail (from != NULL, 0);

  from_valid = wocky_decode_jid (from, &jid.node, &jid.domain, &jid.resource);
  if (!from_valid)
    {
      g_critical ("from='%s' isn't a valid JID", from);
      return 0;
    }

  return wocky_c2s_porter_register_handler_internal (self, type, sub_type,
      MATCH_JID, &jid,
      priority, callback, user_data, stanza);
}

static guint
wocky_c2s_porter_register_handler_from_anyone_by_stanza (
    WockyPorter *porter,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);

  return wocky_c2s_porter_register_handler_internal (self, type, sub_type,
      MATCH_ANYONE, NULL,
      priority, callback, user_data, stanza);
}

/**
 * wocky_c2s_porter_register_handler_from_server_va:
 * @self: A #WockyC2SPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @ap: a wocky_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  passed to this function, as per wocky_node_is_superset().
 *
 * A <type>va_list</type> version of
 * wocky_c2s_porter_register_handler_from_server(); see that function for more
 * details.
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_c2s_porter_register_handler_from_server_va (
    WockyC2SPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    va_list ap)
{
  guint ret;
  WockyStanza *stanza;

  g_return_val_if_fail (WOCKY_IS_C2S_PORTER (self), 0);

  if (type == WOCKY_STANZA_TYPE_NONE)
    {
      stanza = NULL;
      g_return_val_if_fail (
          (va_arg (ap, WockyNodeBuildTag) == 0) &&
          "Pattern-matching is not supported when matching stanzas "
          "of any type", 0);
    }
  else
    {
      stanza = wocky_stanza_build_va (type, WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL, ap);
      g_assert (stanza != NULL);
    }

  ret = wocky_c2s_porter_register_handler_from_server_by_stanza (self, type, sub_type,
      priority, callback, user_data, stanza);

  if (stanza != NULL)
    g_object_unref (stanza);

  return ret;
}

/**
 * wocky_c2s_porter_register_handler_from_server_by_stanza:
 * @self: A #WockyC2SPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @stanza: a #WockyStanza. The handler will match a stanza only if
 *  the stanza received is a superset of the one passed to this
 *  function, as per wocky_node_is_superset().
 *
 * A #WockyStanza version of
 * wocky_c2s_porter_register_handler_from_server(); see that function for more
 * details.
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_c2s_porter_register_handler_from_server_by_stanza (
    WockyC2SPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  g_return_val_if_fail (WOCKY_IS_C2S_PORTER (self), 0);

  if (type == WOCKY_STANZA_TYPE_NONE)
    g_return_val_if_fail (stanza == NULL, 0);
  else
    g_return_val_if_fail (WOCKY_IS_STANZA (stanza), 0);

  return wocky_c2s_porter_register_handler_internal (self, type, sub_type,
      MATCH_SERVER, NULL,
      priority, callback, user_data, stanza);
}

/**
 * wocky_c2s_porter_register_handler_from_server:
 * @self: A #WockyC2SPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @...: a wocky_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  passed to this function, as per wocky_node_is_superset().
 *
 * Registers a handler for incoming stanzas from the local user's server; that
 * is, stanzas with no "from" attribute, or where the sender is the user's own
 * bare or full JID.
 *
 * For example, to register a handler for roster pushes, call:
 *
 * |[
 * id = wocky_c2s_porter_register_handler_from_server (porter,
 *   WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_SET,
 *   WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, roster_push_received_cb, NULL,
 *   '(',
 *     "query", ':', WOCKY_XMPP_NS_ROSTER,
 *   ')', NULL);
 * ]|
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_c2s_porter_register_handler_from_server (
    WockyC2SPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    ...)
{
  va_list ap;
  guint ret;

  g_return_val_if_fail (WOCKY_IS_C2S_PORTER (self), 0);

  va_start (ap, user_data);
  ret = wocky_c2s_porter_register_handler_from_server_va (self, type, sub_type,
      priority, callback, user_data, ap);
  va_end (ap);

  return ret;
}

static void
wocky_c2s_porter_unregister_handler (WockyPorter *porter,
    guint id)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;
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

/**
 * wocky_c2s_porter_enable_power_saving_mode:
 * @porter: a #WockyC2SPorter
 * @enable: A boolean specifying whether power saving mode should be used
 *
 * Enable or disable power saving. In power saving mode, Wocky will
 * attempt to queue "uninteresting" stanza until it is either manually
 * flushed, until important stanza arrives, or until the power saving
 * mode is disabled.
 *
 * Queueable stanzas are:
 *
 * <itemizedlist>
 *  <listitem><code>&lt;presence/&gt;</code> and
 *      <code>&lt;presence type="unavailable"/&gt;</code>;</listitem>
 *  <listitem>PEP updates for a hardcoded list of namespaces.</listitem>
 * </itemizedlist>
 *
 * Whenever stanza is handled, all previously queued stanzas
 * (if any) are handled as well, in the order they arrived. This preserves
 * stanza ordering.
 *
 * Note that exiting the power saving mode will immediately handle any
 * queued stanzas.
 */
void
wocky_c2s_porter_enable_power_saving_mode (WockyC2SPorter *porter,
    gboolean enable)
{
  WockyC2SPorterPrivate *priv = porter->priv;

  if (priv->power_saving_mode && !enable)
    {
      flush_unimportant_queue (porter);
    }

  priv->power_saving_mode = enable;
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
  WockyC2SPorter *self = WOCKY_C2S_PORTER (source);
  StanzaIqHandler *handler = (StanzaIqHandler *) user_data;
  GError *error = NULL;

  handler->sent = TRUE;

  if (wocky_c2s_porter_send_finish (WOCKY_PORTER (self), res, &error))
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

static void
wocky_c2s_porter_send_iq_async (WockyPorter *porter,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;
  StanzaIqHandler *handler;
  const gchar *recipient;
  gchar *id = NULL;
  GSimpleAsyncResult *result;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;

  if (priv->close_result != NULL || priv->force_close_result != NULL)
    {
      gchar *node = NULL;

      g_assert (stanza != NULL && wocky_stanza_get_top_node (stanza) != NULL);

      node = wocky_node_to_string (wocky_stanza_get_top_node (stanza));
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_CLOSING,
          "Porter is closing: iq '%s' aborted", node);
      g_free (node);

      return;
    }

  wocky_stanza_get_type_info (stanza, &type, &sub_type);

  if (type != WOCKY_STANZA_TYPE_IQ)
    goto wrong_stanza;

  if (sub_type != WOCKY_STANZA_SUB_TYPE_GET &&
      sub_type != WOCKY_STANZA_SUB_TYPE_SET)
    goto wrong_stanza;

  recipient = wocky_stanza_get_to (stanza);

  /* Set an unique ID */
  do
    {
      g_free (id);
      id = wocky_xmpp_connection_new_id (priv->connection);
    }
  while (g_hash_table_lookup (priv->iq_reply_handlers, id) != NULL);

  wocky_node_set_attribute (wocky_stanza_get_top_node (stanza), "id", id);

  result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_c2s_porter_send_iq_async);

  handler = stanza_iq_handler_new (self, id, result, cancellable,
      recipient);

  if (cancellable != NULL)
    {
      handler->cancelled_sig_id = g_cancellable_connect (cancellable,
          G_CALLBACK (send_iq_cancelled_cb), handler, NULL);
    }

  g_hash_table_insert (priv->iq_reply_handlers, id, handler);

  wocky_c2s_porter_send_async (WOCKY_PORTER (self), stanza, cancellable,
      iq_sent_cb, handler);
  return;

wrong_stanza:
  g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
      user_data, WOCKY_PORTER_ERROR,
      WOCKY_PORTER_ERROR_NOT_IQ,
      "Stanza is not an IQ query");
}

static WockyStanza *
wocky_c2s_porter_send_iq_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  WockyStanza *reply;

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_c2s_porter_send_iq_async), NULL);

  reply = g_simple_async_result_get_op_res_gpointer (
      G_SIMPLE_ASYNC_RESULT (result));

  return g_object_ref (reply);
}

static void
wocky_c2s_porter_force_close_async (WockyPorter *porter,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyC2SPorter *self = WOCKY_C2S_PORTER (porter);
  WockyC2SPorterPrivate *priv = self->priv;
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
      g_signal_emit_by_name (self, "closing");
    }

  priv->force_close_result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_c2s_porter_force_close_async);

  g_assert (priv->force_close_cancellable == NULL);

  if (cancellable != NULL)
    priv->force_close_cancellable = g_object_ref (cancellable);

  /* force_close_result now keeps a ref on ourself so we can release the ref
   * without risking to destroy the object */
  g_object_unref (self);

  /* Terminate all the pending sending operations */
  terminate_sending_operations (self, &err);

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
          g_object_unref (priv->force_close_result);
          priv->force_close_result = NULL;
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

static gboolean
wocky_c2s_porter_force_close_finish (
    WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_c2s_porter_force_close_async), FALSE);

  return TRUE;
}

static void
send_whitespace_ping_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *res_out = user_data;
  WockyC2SPorter *self = WOCKY_C2S_PORTER (
      g_async_result_get_source_object (G_ASYNC_RESULT (res_out)));
  WockyC2SPorterPrivate *priv = self->priv;
  GError *error = NULL;

  priv->sending_whitespace_ping = FALSE;

  if (!wocky_xmpp_connection_send_whitespace_ping_finish (
        WOCKY_XMPP_CONNECTION (source), res, &error))
    {
      g_simple_async_result_set_from_error (res_out, error);
      g_simple_async_result_complete (res_out);

      /* Sending the ping failed; there is no point in trying to send
       * anything else at this point. */
      terminate_sending_operations (self, error);

      g_error_free (error);
    }
  else
    {
      g_simple_async_result_complete (res_out);

      /* Somebody could have tried sending a stanza while we were sending
       * the ping */
      if (g_queue_get_length (priv->sending_queue) > 0)
        send_head_stanza (self);
    }

  close_if_waiting (self);

  g_object_unref (self);
  g_object_unref (res_out);
}

/**
 * wocky_c2s_porter_send_whitespace_ping_async:
 * @self: a #WockyC2SPorter
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Request asynchronous sending of a whitespace ping. When the operation is
 * finished @callback will be called. You can then call
 * wocky_c2s_porter_send_whitespace_ping_finish() to get the result of the
 * operation.
 * No pings are sent if there are already other stanzas or pings being sent
 * when this function is called; it would be useless.
 */
void
wocky_c2s_porter_send_whitespace_ping_async (WockyC2SPorter *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyC2SPorterPrivate *priv = self->priv;
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_c2s_porter_send_whitespace_ping_async);

  if (priv->close_result != NULL || priv->force_close_result != NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_PORTER_ERROR,
          WOCKY_PORTER_ERROR_CLOSING, "Porter is closing");
      g_simple_async_result_complete_in_idle (result);
    }
  else if (sending_in_progress (self))
    {
      g_simple_async_result_complete_in_idle (result);
    }
  else
    {
      priv->sending_whitespace_ping = TRUE;

      wocky_xmpp_connection_send_whitespace_ping_async (priv->connection,
          cancellable, send_whitespace_ping_cb, g_object_ref (result));

      g_signal_emit_by_name (self, "sending", NULL);
    }

  g_object_unref (result);
}

/**
 * wocky_c2s_porter_send_whitespace_ping_finish:
 * @self: a #WockyC2SPorter
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes sending a whitespace ping.
 *
 * Returns: TRUE if the ping was succesfully sent, FALSE on error.
 */
gboolean
wocky_c2s_porter_send_whitespace_ping_finish (WockyC2SPorter *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self,
      wocky_c2s_porter_send_whitespace_ping_async);
}

static const gchar *
wocky_c2s_porter_get_full_jid (WockyPorter *porter)
{
  WockyC2SPorter *self;

  g_return_val_if_fail (WOCKY_IS_C2S_PORTER (porter), NULL);

  self = (WockyC2SPorter *) porter;

  return self->priv->full_jid;
}

static const gchar *
wocky_c2s_porter_get_bare_jid (WockyPorter *porter)
{
  WockyC2SPorter *self;

  g_return_val_if_fail (WOCKY_IS_C2S_PORTER (porter), NULL);

  self = (WockyC2SPorter *) porter;

  return self->priv->bare_jid;
}

static const gchar *
wocky_c2s_porter_get_resource (WockyPorter *porter)
{
  WockyC2SPorter *self;

  g_return_val_if_fail (WOCKY_IS_C2S_PORTER (porter), NULL);

  self = (WockyC2SPorter *) porter;

  return self->priv->resource;
}

static void
wocky_porter_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  WockyPorterInterface *iface = g_iface;

  iface->get_full_jid = wocky_c2s_porter_get_full_jid;
  iface->get_bare_jid = wocky_c2s_porter_get_bare_jid;
  iface->get_resource = wocky_c2s_porter_get_resource;

  iface->start = wocky_c2s_porter_start;

  iface->send_async = wocky_c2s_porter_send_async;
  iface->send_finish = wocky_c2s_porter_send_finish;

  iface->register_handler_from_by_stanza =
    wocky_c2s_porter_register_handler_from_by_stanza;
  iface->register_handler_from_anyone_by_stanza =
    wocky_c2s_porter_register_handler_from_anyone_by_stanza;

  iface->unregister_handler = wocky_c2s_porter_unregister_handler;

  iface->close_async = wocky_c2s_porter_close_async;
  iface->close_finish = wocky_c2s_porter_close_finish;

  iface->send_iq_async = wocky_c2s_porter_send_iq_async;
  iface->send_iq_finish = wocky_c2s_porter_send_iq_finish;

  iface->force_close_async = wocky_c2s_porter_force_close_async;
  iface->force_close_finish = wocky_c2s_porter_force_close_finish;
}
