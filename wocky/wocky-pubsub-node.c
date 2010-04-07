/*
 * wocky-pubsub-node.c - WockyPubsubNode
 * Copyright (C) 2009 Collabora Ltd.
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

#include "wocky-pubsub-node.h"
#include "wocky-pubsub-node-protected.h"
#include "wocky-pubsub-node-enumtypes.h"

#include "wocky-namespaces.h"
#include "wocky-porter.h"
#include "wocky-pubsub-helpers.h"
#include "wocky-pubsub-service-protected.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_PUBSUB
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyPubsubNode, wocky_pubsub_node, G_TYPE_OBJECT)

enum
{
  SIG_EVENT_RECEIVED,
  SIG_SUB_STATE_CHANGED,
  SIG_DELETED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

enum
{
  PROP_SERVICE = 1,
  PROP_NAME,
};

/* private structure */
typedef struct _WockyPubsubNodePrivate WockyPubsubNodePrivate;

struct _WockyPubsubNodePrivate
{
  WockyPubsubService *service;
  WockyPorter *porter;

  gchar *service_jid;
  gchar *name;

  gboolean dispose_has_run;
};

#define WOCKY_PUBSUB_NODE_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_PUBSUB_NODE, \
    WockyPubsubNodePrivate))

static void
wocky_pubsub_node_init (WockyPubsubNode *obj)
{
  /*
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (obj);
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  */
}

static void
wocky_pubsub_node_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (object);
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_SERVICE:
        priv->service = g_value_dup_object (value);
        break;
      case PROP_NAME:
        priv->name = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_pubsub_node_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (object);
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_SERVICE:
        g_value_set_object (value, priv->service);
        break;
      case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
wocky_pubsub_node_dispose (GObject *object)
{
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (object);
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_object_unref (priv->service);
  g_object_unref (priv->porter);

  if (G_OBJECT_CLASS (wocky_pubsub_node_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_pubsub_node_parent_class)->dispose (object);
}

static void
wocky_pubsub_node_finalize (GObject *object)
{
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (object);
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  g_free (priv->name);
  g_free (priv->service_jid);

  G_OBJECT_CLASS (wocky_pubsub_node_parent_class)->finalize (object);
}

static void
wocky_pubsub_node_constructed (GObject *object)
{
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (object);
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockySession *session;

  g_assert (priv->service != NULL);
  g_assert (priv->name != NULL);

  g_object_get (priv->service,
      "jid", &(priv->service_jid),
      "session", &session,
      NULL);
  g_assert (priv->service_jid != NULL);

  g_assert (session != NULL);
  priv->porter = wocky_session_get_porter (session);
  g_object_ref (priv->porter);
  g_object_unref (session);
}

static void
wocky_pubsub_node_emit_event_received (
    WockyPubsubNode *self,
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *items_node,
    GList *items)
{
  g_signal_emit (self, signals[SIG_EVENT_RECEIVED], 0, event_stanza,
      event_node, items_node, items);
}

static void
wocky_pubsub_node_emit_subscription_state_changed (
    WockyPubsubNode *self,
    WockyXmppStanza *stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *subscription_node,
    WockyPubsubSubscription *subscription)
{
  g_signal_emit (self, signals[SIG_SUB_STATE_CHANGED], 0, stanza,
      event_node, subscription_node, subscription);
}

static void
wocky_pubsub_node_emit_deleted (
    WockyPubsubNode *self,
    WockyXmppStanza *stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *delete_node)
{
  g_signal_emit (self, signals[SIG_DELETED], 0, stanza,
      event_node, delete_node);
}

static void
wocky_pubsub_node_class_init (
    WockyPubsubNodeClass *wocky_pubsub_node_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_pubsub_node_class);
  GType ctype = G_OBJECT_CLASS_TYPE (wocky_pubsub_node_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_pubsub_node_class,
      sizeof (WockyPubsubNodePrivate));

  object_class->set_property = wocky_pubsub_node_set_property;
  object_class->get_property = wocky_pubsub_node_get_property;
  object_class->dispose = wocky_pubsub_node_dispose;
  object_class->finalize = wocky_pubsub_node_finalize;
  object_class->constructed = wocky_pubsub_node_constructed;

  param_spec = g_param_spec_object ("service", "service",
      "the Wocky Pubsub service associated with this pubsub node",
      WOCKY_TYPE_PUBSUB_SERVICE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SERVICE, param_spec);

  param_spec = g_param_spec_string ("name", "name",
      "The name of the pubsub node",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, param_spec);

  /**
   * WockyPubsubNode::event-received:
   * @node: a pubsub node
   * @event_stanza: the message/event stanza in its entirity
   * @event_node: the event node from the stanza
   * @items_node: the items node from the stanza
   * @items: a list of WockyXmppNode *s for each item child of @items_node
   */
  signals[SIG_EVENT_RECEIVED] = g_signal_new ("event-received", ctype,
      0, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_POINTER_POINTER_POINTER,
      G_TYPE_NONE, 4,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * WockyPubsubNode::subscription-state-changed:
   * @node: a pubsub node
   * @stanza: the message/event stanza in its entirety
   * @event_node: the event node from @stanza
   * @subscription_node: the subscription node from @stanza
   * @subscription: subscription information parsed from @subscription_node
   */
  signals[SIG_SUB_STATE_CHANGED] = g_signal_new ("subscription-state-changed",
      ctype, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_POINTER_POINTER_BOXED,
      G_TYPE_NONE, 4,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_POINTER, G_TYPE_POINTER,
      WOCKY_TYPE_PUBSUB_SUBSCRIPTION);

  /**
   * WockyPubsubNode::deleted
   * @node: a pubsub node
   * @stanza: the message/event stanza in its entirety
   * @event_node: the event node from @stanza
   * @delete_node: the delete node from @stanza
   *
   * Emitted when a notification of this node's deletion is received from the
   * server.
   */
  signals[SIG_DELETED] = g_signal_new ("deleted",
      ctype, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_POINTER_POINTER,
      G_TYPE_NONE, 3,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
pubsub_node_handle_items_event (WockyPubsubNode *self,
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *items_node)
{
  WockyXmppNode *item_node;
  GQueue items = G_QUEUE_INIT;
  WockyXmppNodeIter iter;

  wocky_xmpp_node_iter_init (&iter, items_node, "item", NULL);

  while (wocky_xmpp_node_iter_next (&iter, &item_node))
    g_queue_push_tail (&items, item_node);

  DEBUG_STANZA (event_stanza, "extracted %u items", items.length);
  wocky_pubsub_node_emit_event_received (self, event_stanza, event_node,
      items_node, items.head);

  g_queue_clear (&items);
}

static void
pubsub_node_handle_subscription_event (WockyPubsubNode *self,
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *subscription_node)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyPubsubSubscription *sub;
  GError *error = NULL;

  sub = wocky_pubsub_service_parse_subscription (priv->service,
      subscription_node, NULL, &error);

  if (sub == NULL)
    {
      DEBUG ("received unparseable subscription state change notification: %s",
          error->message);
      g_clear_error (&error);
    }
  else
    {
      wocky_pubsub_node_emit_subscription_state_changed (self, event_stanza,
          event_node, subscription_node, sub);
      wocky_pubsub_subscription_free (sub);
    }
}

static const WockyPubsubNodeEventMapping mappings[] = {
    { "items", pubsub_node_handle_items_event, },
    { "subscription", pubsub_node_handle_subscription_event, },
    { "delete", wocky_pubsub_node_emit_deleted, },
    { NULL, }
};

const WockyPubsubNodeEventMapping *
_wocky_pubsub_node_get_event_mappings (guint *n_mappings)
{
  if (n_mappings != NULL)
    *n_mappings = G_N_ELEMENTS (mappings) - 1;

  return mappings;
}

const gchar *
wocky_pubsub_node_get_name (WockyPubsubNode *self)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  return priv->name;
}

WockyXmppStanza *
wocky_pubsub_node_make_publish_stanza (WockyPubsubNode *self,
    WockyXmppNode **pubsub_out,
    WockyXmppNode **publish_out,
    WockyXmppNode **item_out)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  return wocky_pubsub_make_publish_stanza (priv->service_jid, priv->name,
      pubsub_out, publish_out, item_out);
}

static WockyXmppStanza *
pubsub_node_make_action_stanza (WockyPubsubNode *self,
    WockyStanzaSubType sub_type,
    const gchar *pubsub_ns,
    const gchar *action_name,
    const gchar *jid,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **action_node)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  WockyXmppNode *action;

  g_assert (pubsub_ns != NULL);
  g_assert (action_name != NULL);

  stanza = wocky_pubsub_make_stanza (priv->service_jid, sub_type, pubsub_ns,
      action_name, pubsub_node, &action);
  wocky_xmpp_node_set_attribute (action, "node", priv->name);

  if (jid != NULL)
    wocky_xmpp_node_set_attribute (action, "jid", jid);

  if (action_node != NULL)
    *action_node = action;

  return stanza;
}

WockyXmppStanza *
wocky_pubsub_node_make_subscribe_stanza (WockyPubsubNode *self,
    const gchar *jid,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **subscribe_node)
{
  /* TODO: when the connection/porter/session/something knows our own JID, we
   * should provide an easy way to say “my bare JID” or “my full JID”. Could be
   * really evil and use 0x1 and 0x3 or something on the assumption that those
   * will never be strings....
   */
  g_return_val_if_fail (jid != NULL, NULL);

  return pubsub_node_make_action_stanza (self, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_XMPP_NS_PUBSUB, "subscribe", jid, pubsub_node,
      subscribe_node);
}

static void
subscribe_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (
      g_async_result_get_source_object (user_data));
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyXmppNode *subscription_node;
  WockyPubsubSubscription *sub = NULL;
  GError *error = NULL;

  if (wocky_pubsub_distill_iq_reply (source, res, WOCKY_XMPP_NS_PUBSUB,
          "subscription", &subscription_node, &error))
    sub = wocky_pubsub_service_parse_subscription (priv->service,
        subscription_node, NULL, &error);

  if (sub != NULL)
    {
      g_simple_async_result_set_op_res_gpointer (simple,
          sub,
          (GDestroyNotify) wocky_pubsub_subscription_free);
    }
  else
    {
      g_simple_async_result_set_from_error (simple, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

/**
 * wocky_pubsub_node_subscribe_async:
 * @self: a pubsub node
 * @jid: the JID to use as the subscribed JID (usually the connection's bare or
 *       full JID); may not be %NULL
 * @cancellable: optional GCancellable object, %NULL to ignore
 * @callback: a callback to call when the request is completed
 * @user_data: data to pass to @callback
 *
 * Attempts to subscribe to @self.
 */
void
wocky_pubsub_node_subscribe_async (WockyPubsubNode *self,
    const gchar *jid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_pubsub_node_subscribe_async);
  WockyXmppStanza *stanza;

  g_return_if_fail (jid != NULL);

  stanza = wocky_pubsub_node_make_subscribe_stanza (self, jid, NULL, NULL);

  wocky_porter_send_iq_async (priv->porter, stanza, cancellable,
      subscribe_cb, simple);

  g_object_unref (stanza);
}

WockyPubsubSubscription *
wocky_pubsub_node_subscribe_finish (WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (self), wocky_pubsub_node_subscribe_async), NULL);

  simple = (GSimpleAsyncResult *) result;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;
  else
    return wocky_pubsub_subscription_copy (
        g_simple_async_result_get_op_res_gpointer (simple));
}

WockyXmppStanza *
wocky_pubsub_node_make_unsubscribe_stanza (WockyPubsubNode *self,
    const gchar *jid,
    const gchar *subid,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **unsubscribe_node)
{
  WockyXmppStanza *stanza;
  WockyXmppNode *unsubscribe;

  /* TODO: when the connection/porter/session/something knows our own JID, we
   * should provide an easy way to say “my bare JID” or “my full JID”. Could be
   * really evil and use 0x1 and 0x3 or something on the assumption that those
   * will never be strings....
   */
  g_return_val_if_fail (jid != NULL, NULL);

  stanza = pubsub_node_make_action_stanza (self, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_XMPP_NS_PUBSUB, "unsubscribe", jid,
      pubsub_node, &unsubscribe);

  if (subid != NULL)
    wocky_xmpp_node_set_attribute (unsubscribe, "subid", subid);

  if (unsubscribe_node != NULL)
    *unsubscribe_node = unsubscribe;

  return stanza;
}

static void
unsubscribe_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;

  if (!wocky_pubsub_distill_void_iq_reply (source, res, &error))
    {
      g_simple_async_result_set_from_error (simple, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

/**
 * wocky_pubsub_node_unsubscribe_async:
 * @self: a pubsub node
 * @jid: the JID subscribed to @self (usually the connection's bare or
 *       full JID); may not be %NULL
 * @subid: the identifier associated with the subscription
 * @cancellable: optional GCancellable object, %NULL to ignore
 * @callback: a callback to call when the request is completed
 * @user_data: data to pass to @callback
 *
 * Attempts to unsubscribe from @self.
 */
void
wocky_pubsub_node_unsubscribe_async (WockyPubsubNode *self,
    const gchar *jid,
    const gchar *subid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_pubsub_node_unsubscribe_async);
  WockyXmppStanza *stanza;

  g_return_if_fail (jid != NULL);

  stanza = wocky_pubsub_node_make_unsubscribe_stanza (self, jid, subid, NULL,
      NULL);

  wocky_porter_send_iq_async (priv->porter, stanza, cancellable,
      unsubscribe_cb, simple);

  g_object_unref (stanza);
}

gboolean
wocky_pubsub_node_unsubscribe_finish (WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (self), wocky_pubsub_node_unsubscribe_async), FALSE);

  simple = (GSimpleAsyncResult *) result;

  return !g_simple_async_result_propagate_error (simple, error);
}

static void
delete_node_iq_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;

  if (!wocky_pubsub_distill_void_iq_reply (source, res, &error))
    {
      g_simple_async_result_set_from_error (result, error);
      g_clear_error (&error);
    }
  else
    {
      DEBUG ("node deleted");
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

WockyXmppStanza *
wocky_pubsub_node_make_delete_stanza (
    WockyPubsubNode *self,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **delete_node)
{
  return pubsub_node_make_action_stanza (self, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_XMPP_NS_PUBSUB_OWNER, "delete", NULL, pubsub_node, delete_node);
}

void
wocky_pubsub_node_delete_async (WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  GSimpleAsyncResult *result;

  stanza = wocky_pubsub_node_make_delete_stanza (self, NULL, NULL);
  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
    wocky_pubsub_node_delete_async);
  wocky_porter_send_iq_async (priv->porter, stanza, NULL, delete_node_iq_cb,
      result);
  g_object_unref (stanza);
}

gboolean
wocky_pubsub_node_delete_finish (WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (self), wocky_pubsub_node_delete_async), FALSE);

  simple = (GSimpleAsyncResult *) result;

  return !g_simple_async_result_propagate_error (simple, error);
}

WockyXmppStanza *
wocky_pubsub_node_make_list_subscribers_stanza (
    WockyPubsubNode *self,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **subscriptions_node)
{
  return pubsub_node_make_action_stanza (self, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_XMPP_NS_PUBSUB_OWNER, "subscriptions", NULL,
      pubsub_node, subscriptions_node);
}

static void
list_subscribers_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (
      g_async_result_get_source_object (user_data));
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyXmppNode *subscriptions_node;
  GError *error = NULL;

  if (wocky_pubsub_distill_iq_reply (source, res, WOCKY_XMPP_NS_PUBSUB_OWNER,
          "subscriptions", &subscriptions_node, &error))
    {
      g_simple_async_result_set_op_res_gpointer (simple,
          wocky_pubsub_service_parse_subscriptions (priv->service,
              subscriptions_node, NULL),
          (GDestroyNotify) wocky_pubsub_subscription_list_free);
    }
  else
    {
      g_simple_async_result_set_from_error (simple, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

/**
 * wocky_pubsub_node_list_subscribers_async:
 * @self: a pubsub node
 * @cancellable: optional #GCancellable object
 * @callback: function to call when the subscribers have been retrieved or an
 *            error has occured
 * @user_data: data to pass to @callback.
 *
 * Retrieves the list of subscriptions to a node you own. @callback may
 * complete the call using wocky_pubsub_node_list_subscribers_finish().
 *
 * (A note on naming: this is §8.8.1 — Retrieve Subscriptions List — in
 * XEP-0060, not to be confused with §5.6 — Retrieve Subscriptions. The
 * different terminology in Wocky is intended to help disambiguate!)
 */
void
wocky_pubsub_node_list_subscribers_async (
    WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_pubsub_node_list_subscribers_async);
  WockyXmppStanza *stanza;

  stanza = wocky_pubsub_node_make_list_subscribers_stanza (self, NULL,
      NULL);
  wocky_porter_send_iq_async (priv->porter, stanza, cancellable,
      list_subscribers_cb, simple);
  g_object_unref (stanza);
}

/**
 * wocky_pubsub_node_list_subscribers_finish:
 * @self: a pubsub node
 * @result: the result passed to a callback
 * @subscribers: location at which to store a list of #WockyPubsubSubscription
 *               pointers, or %NULL
 * @error: location at which to store an error, or %NULL
 *
 * Completes a call to wocky_pubsub_node_list_subscribers_async(). The list
 * returned in @subscribers should be freed with
 * wocky_pubsub_subscription_list_free() when it is no longer needed.
 *
 * Returns: %TRUE if the list of subscribers was successfully retrieved; %FALSE
 *          and sets @error if an error occured.
 */
gboolean
wocky_pubsub_node_list_subscribers_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GList **subscribers,
    GError **error)
{
  wocky_implement_finish_copy_pointer (self,
      wocky_pubsub_node_list_subscribers_async,
      wocky_pubsub_subscription_list_copy, subscribers)
}

WockyXmppStanza *
wocky_pubsub_node_make_list_affiliates_stanza (
    WockyPubsubNode *self,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **affiliations_node)
{
  return pubsub_node_make_action_stanza (self, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_XMPP_NS_PUBSUB_OWNER, "affiliations", NULL,
      pubsub_node, affiliations_node);
}

GList *
wocky_pubsub_node_parse_affiliations (
    WockyPubsubNode *self,
    WockyXmppNode *affiliations_node)
{
  GQueue affs = G_QUEUE_INIT;
  WockyXmppNodeIter i;
  WockyXmppNode *n;

  wocky_xmpp_node_iter_init (&i, affiliations_node, "affiliation", NULL);

  while (wocky_xmpp_node_iter_next (&i, &n))
    {
      const gchar *jid = wocky_xmpp_node_get_attribute (n, "jid");
      const gchar *affiliation = wocky_xmpp_node_get_attribute (n,
          "affiliation");
      gint state;

      if (jid == NULL)
        {
          DEBUG ("<affiliation> missing jid=''; skipping");
          continue;
        }

      if (affiliation == NULL)
        {
          DEBUG ("<affiliation> missing affiliation=''; skipping");
          continue;
        }

      if (!wocky_enum_from_nick (WOCKY_TYPE_PUBSUB_AFFILIATION_STATE,
              affiliation, &state))
        {
          DEBUG ("unknown affiliation '%s'; skipping", affiliation);
          continue;
        }

      g_queue_push_tail (&affs,
          wocky_pubsub_affiliation_new (self, jid, state));
    }

  return affs.head;
}

static void
list_affiliates_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (
      g_async_result_get_source_object (user_data));
  WockyXmppNode *affiliations_node;
  GError *error = NULL;

  if (wocky_pubsub_distill_iq_reply (source, res, WOCKY_XMPP_NS_PUBSUB_OWNER,
          "affiliations", &affiliations_node, &error))
    {
      g_simple_async_result_set_op_res_gpointer (simple,
          wocky_pubsub_node_parse_affiliations (self, affiliations_node),
          (GDestroyNotify) wocky_pubsub_affiliation_list_free);
    }
  else
    {
      g_simple_async_result_set_from_error (simple, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

/**
 * wocky_pubsub_node_list_affiliates_async:
 * @self: a pubsub node
 * @cancellable: optional #GCancellable object
 * @callback: function to call when the affiliates have been retrieved or an
 *            error has occured
 * @user_data: data to pass to @callback.
 *
 * Retrieves the list of entities affilied to a node you own. @callback may
 * complete the call using wocky_pubsub_node_list_affiliates_finish().
 *
 * (A note on naming: this is §8.9.1 — Retrieve Affiliations List — in
 * XEP-0060, not to be confused with §5.7 — Retrieve Affiliations. The
 * slightly different terminology in Wocky is intended to help disambiguate!)
 */
void
wocky_pubsub_node_list_affiliates_async (
    WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_pubsub_node_list_affiliates_async);
  WockyXmppStanza *stanza;

  stanza = wocky_pubsub_node_make_list_affiliates_stanza (self, NULL,
      NULL);
  wocky_porter_send_iq_async (priv->porter, stanza, cancellable,
      list_affiliates_cb, simple);
  g_object_unref (stanza);
}

/**
 * wocky_pubsub_node_list_affiliates_finish:
 * @self: a pubsub node
 * @result: the result passed to a callback
 * @affiliates: location at which to store a list of #WockyPubsubAffiliation
 *              pointers, or %NULL
 * @error: location at which to store an error, or %NULL
 *
 * Completes a call to wocky_pubsub_node_list_affiliates_async(). The list
 * returned in @affiliates should be freed with
 * wocky_pubsub_affiliation_list_free() when it is no longer needed.
 *
 * Returns: %TRUE if the list of subscribers was successfully retrieved; %FALSE
 *          and sets @error if an error occured.
 */
gboolean
wocky_pubsub_node_list_affiliates_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GList **affiliates,
    GError **error)
{
  wocky_implement_finish_copy_pointer (self,
      wocky_pubsub_node_list_affiliates_async,
      wocky_pubsub_affiliation_list_copy, affiliates)
}

WockyPorter *
wocky_pubsub_node_get_porter (WockyPubsubNode *self)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  return priv->porter;
}


/* WockyPubsubAffiliation boilerplate */

GType
wocky_pubsub_affiliation_get_type (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = g_boxed_type_register_static ("WockyPubsubAffiliation",
        (GBoxedCopyFunc) wocky_pubsub_affiliation_copy,
        (GBoxedFreeFunc) wocky_pubsub_affiliation_free);

  return t;
}

WockyPubsubAffiliation *
wocky_pubsub_affiliation_new (
    WockyPubsubNode *node,
    const gchar *jid,
    WockyPubsubAffiliationState state)
{
  WockyPubsubAffiliation aff = { g_object_ref (node), g_strdup (jid), state };

  return g_slice_dup (WockyPubsubAffiliation, &aff);
}

WockyPubsubAffiliation *
wocky_pubsub_affiliation_copy (
    WockyPubsubAffiliation *aff)
{
  g_return_val_if_fail (aff != NULL, NULL);

  return wocky_pubsub_affiliation_new (aff->node, aff->jid, aff->state);
}

void
wocky_pubsub_affiliation_free (WockyPubsubAffiliation *aff)
{
  g_return_if_fail (aff != NULL);

  g_object_unref (aff->node);
  g_free (aff->jid);
  g_slice_free (WockyPubsubAffiliation, aff);
}

GList *
wocky_pubsub_affiliation_list_copy (GList *affs)
{
  return wocky_list_deep_copy (
      (GBoxedCopyFunc) wocky_pubsub_affiliation_copy, affs);
}

void
wocky_pubsub_affiliation_list_free (GList *affs)
{
  g_list_foreach (affs, (GFunc) wocky_pubsub_affiliation_free, NULL);
  g_list_free (affs);
}
