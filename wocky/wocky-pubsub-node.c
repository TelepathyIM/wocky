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

#include "wocky-namespaces.h"
#include "wocky-porter.h"
#include "wocky-pubsub-helpers.h"
#include "wocky-pubsub-service-protected.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_PUBSUB
#include "wocky-debug.h"

static gboolean pubsub_node_handle_event_stanza (WockyPorter *porter,
    WockyXmppStanza *event_stanza,
    gpointer user_data);

G_DEFINE_TYPE (WockyPubsubNode, wocky_pubsub_node, G_TYPE_OBJECT)

enum
{
  SIG_EVENT_RECEIVED,
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
  guint handler_id;

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

  if (priv->porter != NULL)
    {
      wocky_porter_unregister_handler (priv->porter, priv->handler_id);
      g_object_unref (priv->porter);
    }

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

  priv->handler_id = wocky_porter_register_handler (priv->porter,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
      priv->service_jid,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      pubsub_node_handle_event_stanza, self,
        WOCKY_NODE, "event",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_EVENT,
          WOCKY_NODE, "items",
            WOCKY_NODE_ATTRIBUTE, "node", priv->name,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);
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
}

static gboolean
pubsub_node_handle_event_stanza (WockyPorter *porter,
    WockyXmppStanza *event_stanza,
    gpointer user_data)
{
  WockyPubsubNode *self = WOCKY_PUBSUB_NODE (user_data);

  return _wocky_pubsub_node_handle_event_stanza (self, event_stanza);
}

gboolean
_wocky_pubsub_node_handle_event_stanza (WockyPubsubNode *self,
    WockyXmppStanza *event_stanza)
{
  WockyXmppNode *event_node, *items_node, *item_node;
  GQueue items = G_QUEUE_INIT;
  WockyXmppNodeIter iter;

  event_node = wocky_xmpp_node_get_child_ns (event_stanza->node, "event",
      WOCKY_XMPP_NS_PUBSUB_EVENT);
  g_return_val_if_fail (event_node != NULL, FALSE);
  items_node = wocky_xmpp_node_get_child (event_node, "items");
  g_return_val_if_fail (items_node != NULL, FALSE);

  wocky_xmpp_node_iter_init (&iter, items_node, "item", NULL);

  while (wocky_xmpp_node_iter_next (&iter, &item_node))
    g_queue_push_tail (&items, item_node);

  DEBUG_STANZA (event_stanza, "extracted %u items", items.length);
  wocky_pubsub_node_emit_event_received (self, event_stanza, event_node,
      items_node, items.head);

  g_queue_clear (&items);

  return TRUE;
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
    const gchar *action_name,
    const gchar *jid,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **action_node)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  WockyXmppNode *pubsub, *action;

  g_assert (action_name != NULL);

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, priv->service_jid,
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE_ASSIGN_TO, &pubsub,
          WOCKY_NODE, action_name,
            WOCKY_NODE_ASSIGN_TO, &action,
            WOCKY_NODE_ATTRIBUTE, "node", priv->name,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  if (jid != NULL)
    wocky_xmpp_node_set_attribute (action, "jid", jid);

  if (pubsub_node != NULL)
    *pubsub_node = pubsub;

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

  return pubsub_node_make_action_stanza (self, "subscribe", jid, pubsub_node,
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

  stanza = pubsub_node_make_action_stanza (self, "unsubscribe", jid,
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

  if (!wocky_pubsub_distill_iq_reply (source, res, NULL, NULL, NULL, &error))
    {
      g_simple_async_result_set_from_error (simple, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

/**
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

  if (!wocky_pubsub_distill_iq_reply (source, res, NULL, NULL, NULL, &error))
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

void
wocky_pubsub_node_delete_async (WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  GSimpleAsyncResult *result;

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, priv->service_jid,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "delete",
          WOCKY_NODE_ATTRIBUTE, "node", priv->name,
        WOCKY_NODE_END,
      WOCKY_NODE_END, WOCKY_STANZA_END);

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

WockyPorter *
wocky_pubsub_node_get_porter (WockyPubsubNode *self)
{
  WockyPubsubNodePrivate *priv = WOCKY_PUBSUB_NODE_GET_PRIVATE (self);

  return priv->porter;
}
