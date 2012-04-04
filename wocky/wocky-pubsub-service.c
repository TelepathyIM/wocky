/*
 * wocky-pubsub-service.c - WockyPubsubService
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-pubsub-service.h"
#include "wocky-pubsub-service-protected.h"

#include "wocky-porter.h"
#include "wocky-utils.h"
#include "wocky-pubsub-helpers.h"
#include "wocky-pubsub-node.h"
#include "wocky-pubsub-node-protected.h"
#include "wocky-pubsub-node-internal.h"
#include "wocky-namespaces.h"
#include "wocky-signals-marshal.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_PUBSUB
#include "wocky-debug-internal.h"

static gboolean pubsub_service_propagate_event (WockyPorter *porter,
    WockyStanza *event_stanza,
    gpointer user_data);

G_DEFINE_TYPE (WockyPubsubService, wocky_pubsub_service, G_TYPE_OBJECT)

/* signal enum */
enum
{
  SIG_EVENT_RECEIVED,
  SIG_SUB_STATE_CHANGED,
  SIG_NODE_DELETED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

enum
{
  PROP_SESSION = 1,
  PROP_JID,
};

/* private structure */
typedef struct _EventTrampoline EventTrampoline;

struct _EventTrampoline
{
  const WockyPubsubNodeEventMapping *mapping;
  WockyPubsubService *self;
  guint handler_id;
};

struct _WockyPubsubServicePrivate
{
  WockySession *session;
  WockyPorter *porter;

  gchar *jid;
  /* owned (gchar *) => weak reffed (WockyPubsubNode *) */
  GHashTable *nodes;

  /* slice-allocated (EventTrampoline *) s, used for <event> handlers */
  GPtrArray *trampolines;

  gboolean dispose_has_run;
};

GQuark
wocky_pubsub_service_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-pubsub-service-error");

  return quark;
}
static void
wocky_pubsub_service_init (WockyPubsubService *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      WOCKY_TYPE_PUBSUB_SERVICE, WockyPubsubServicePrivate);

  self->priv->nodes = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
}

static void
wocky_pubsub_service_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_SESSION:
        priv->session = g_value_get_object (value);
        break;
      case PROP_JID:
        priv->jid = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_pubsub_service_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_SESSION:
        g_value_set_object (value, priv->session);
        break;
      case PROP_JID:
        g_value_set_string (value, priv->jid);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_pubsub_service_dispose (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->porter != NULL)
    {
      guint i;

      for (i = 0; i < priv->trampolines->len; i++)
        {
          EventTrampoline *t = g_ptr_array_index (priv->trampolines, i);

          wocky_porter_unregister_handler (priv->porter, t->handler_id);
          g_slice_free (EventTrampoline, t);
        }

      g_ptr_array_unref (priv->trampolines);
      priv->trampolines = NULL;

      g_object_unref (priv->porter);
      priv->porter = NULL;
    }

  if (G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->dispose (object);
}

static void
wocky_pubsub_service_finalize (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = self->priv;

  g_free (priv->jid);
  g_hash_table_unref (priv->nodes);

  G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->finalize (object);
}

static void
wocky_pubsub_service_constructed (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = self->priv;
  const WockyPubsubNodeEventMapping *m;
  guint n_mappings;

  g_assert (priv->session != NULL);
  g_assert (priv->jid != NULL);

  priv->porter = wocky_session_get_porter (priv->session);
  g_object_ref (priv->porter);

  m = _wocky_pubsub_node_get_event_mappings (&n_mappings);
  priv->trampolines = g_ptr_array_sized_new (n_mappings);

  for (; m->action != NULL; m++)
    {
      EventTrampoline *t = g_slice_new (EventTrampoline);

      t->mapping = m;
      t->self = self;
      t->handler_id = wocky_porter_register_handler_from (priv->porter,
          WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
          priv->jid,
          WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
          pubsub_service_propagate_event, t,
            '(', "event",
              ':', WOCKY_XMPP_NS_PUBSUB_EVENT,
              '(', m->action, ')',
            ')',
          NULL);

      g_ptr_array_add (priv->trampolines, t);
    }
}

static void
wocky_pubsub_service_class_init (
    WockyPubsubServiceClass *wocky_pubsub_service_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_pubsub_service_class);
  GType ctype = G_OBJECT_CLASS_TYPE (wocky_pubsub_service_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_pubsub_service_class,
      sizeof (WockyPubsubServicePrivate));

  object_class->set_property = wocky_pubsub_service_set_property;
  object_class->get_property = wocky_pubsub_service_get_property;
  object_class->dispose = wocky_pubsub_service_dispose;
  object_class->finalize = wocky_pubsub_service_finalize;
  object_class->constructed = wocky_pubsub_service_constructed;

  param_spec = g_param_spec_object ("session", "session",
      "the Wocky Session associated with this pubsub service",
      WOCKY_TYPE_SESSION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SESSION, param_spec);

  param_spec = g_param_spec_string ("jid", "jid",
      "The jid of the pubsub service",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_JID, param_spec);

  /**
   * WockyPubsubService::event-received:
   * @service: a pubsub service
   * @node: the node on @service for which an event has been received
   *        wire
   * @event_stanza: the message/event stanza in its entirity
   * @event_node: the event node from the stanza
   * @items_node: the items node from the stanza
   * @items: a list of WockyNode *s for each item child of @items_node
   *
   * Emitted when an event is received for a node.
   */
  signals[SIG_EVENT_RECEIVED] = g_signal_new ("event-received", ctype,
      0, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_OBJECT_POINTER_POINTER_POINTER,
      G_TYPE_NONE, 5,
      WOCKY_TYPE_PUBSUB_NODE, WOCKY_TYPE_STANZA, G_TYPE_POINTER,
      G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * WockyPubsubService::subscription-state-changed:
   * @service: a pubsub service
   * @node: a pubsub node for which the subscription state has changed
   * @stanza: the message/event stanza in its entirety
   * @event_node: the event node from @stanza
   * @subscription_node: the subscription node from @stanza
   * @subscription: subscription information parsed from @subscription_node
   */
  signals[SIG_SUB_STATE_CHANGED] = g_signal_new ("subscription-state-changed",
      ctype, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_OBJECT_POINTER_POINTER_BOXED,
      G_TYPE_NONE, 5,
      WOCKY_TYPE_PUBSUB_NODE, WOCKY_TYPE_STANZA, G_TYPE_POINTER,
      G_TYPE_POINTER, WOCKY_TYPE_PUBSUB_SUBSCRIPTION);

  /**
   * WockyPubsubService::node-deleted
   * @node: a pubsub node
   * @stanza: the message/event stanza in its entirety
   * @event_node: the event node from @stanza
   * @delete_node: the delete node from @stanza
   *
   * Emitted when a notification of a node's deletion is received from the
   * server.
   */
  signals[SIG_NODE_DELETED] = g_signal_new ("node-deleted",
      ctype, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_OBJECT_POINTER_POINTER,
      G_TYPE_NONE, 4, WOCKY_TYPE_PUBSUB_NODE,
      WOCKY_TYPE_STANZA, G_TYPE_POINTER, G_TYPE_POINTER);

  wocky_pubsub_service_class->node_object_type = WOCKY_TYPE_PUBSUB_NODE;
}

WockyPubsubService *
wocky_pubsub_service_new (WockySession *session,
    const gchar *jid)
{
  return g_object_new (WOCKY_TYPE_PUBSUB_SERVICE,
      "session", session,
      "jid", jid,
      NULL);
}

static gboolean
remove_node (gpointer key,
    gpointer value,
    gpointer node)
{
  return value == node;
}

/* Called when a WockyPubsubNode has been disposed so we can remove it from
 * the hash table. */
static void
node_disposed_cb (gpointer user_data,
    GObject *node)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);
  WockyPubsubServicePrivate *priv = self->priv;

  g_hash_table_foreach_remove (priv->nodes, remove_node, node);
}

static void
pubsub_service_node_event_received_cb (
    WockyPubsubNode *node,
    WockyStanza *event_stanza,
    WockyNode *event_node,
    WockyNode *items_node,
    GList *items,
    gpointer user_data)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);

  g_signal_emit (self, signals[SIG_EVENT_RECEIVED], 0, node, event_stanza,
      event_node, items_node, items);
}

static void
pubsub_service_node_subscription_state_changed_cb (
    WockyPubsubNode *node,
    WockyStanza *stanza,
    WockyNode *event_node,
    WockyNode *subscription_node,
    WockyPubsubSubscription *subscription,
    gpointer user_data)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);

  g_signal_emit (self, signals[SIG_SUB_STATE_CHANGED], 0, node, stanza,
      event_node, subscription_node, subscription);
}

static void
pubsub_service_node_deleted_cb (
    WockyPubsubNode *node,
    WockyStanza *stanza,
    WockyNode *event_node,
    WockyNode *delete_node,
    gpointer user_data)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);

  g_signal_emit (self, signals[SIG_NODE_DELETED], 0, node, stanza,
      event_node, delete_node);
}

static WockyPubsubNode *
pubsub_service_create_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = self->priv;
  WockyPubsubServiceClass *class = WOCKY_PUBSUB_SERVICE_GET_CLASS (self);
  WockyPubsubNode *node;

  g_return_val_if_fail (
      g_type_is_a (class->node_object_type, WOCKY_TYPE_PUBSUB_NODE),
      NULL);

  node = g_object_new (class->node_object_type,
      "service", self,
      "name", name,
      NULL);

  g_object_weak_ref (G_OBJECT (node), node_disposed_cb, self);
  g_hash_table_insert (priv->nodes, g_strdup (name), node);

  /* It's safe to never explicitly disconnect these handlers: the node holds a
   * reference to the service, so the service will always outlive the node.
   */
  g_signal_connect (node, "event-received",
      (GCallback) pubsub_service_node_event_received_cb, self);
  g_signal_connect (node, "subscription-state-changed",
      (GCallback) pubsub_service_node_subscription_state_changed_cb, self);
  g_signal_connect (node, "deleted",
      (GCallback) pubsub_service_node_deleted_cb, self);

  return node;
}

/**
 * wocky_pubsub_service_ensure_node:
 * @self: a pubsub service
 * @name: the name of a node on @self
 *
 * Fetches or creates an object representing a node on the pubsub service. Note
 * that this does not ensure that a node exists on the server; it merely
 * ensures a local representation.
 *
 * Returns: a new reference to an object representing a node named @name on
 *          @self
 */
WockyPubsubNode *
wocky_pubsub_service_ensure_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = self->priv;
  WockyPubsubNode *node;

  node = g_hash_table_lookup (priv->nodes, name);

  if (node != NULL)
    return g_object_ref (node);
  else
    return pubsub_service_create_node (self, name);
}

/**
 * wocky_pubsub_service_lookup_node:
 * @self: a pubsub service
 * @name: the name of a node on @self
 *
 * Fetches an object representing a node on a pubsub service, if the object
 * already exists; if not, returns %NULL. Note that this does not check whether
 * a node exists on the server; it only checks for a local representation.
 *
 * Returns: a borrowed reference to a node, or %NULL
 */
WockyPubsubNode *
wocky_pubsub_service_lookup_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = self->priv;

  return g_hash_table_lookup (priv->nodes, name);
}

static gboolean
pubsub_service_propagate_event (WockyPorter *porter,
    WockyStanza *event_stanza,
    gpointer user_data)
{
  EventTrampoline *trampoline = user_data;
  WockyPubsubService *self = trampoline->self;
  WockyNode *event_node, *action_node;
  const gchar *node_name;
  WockyPubsubNode *node;

  g_assert (WOCKY_IS_PUBSUB_SERVICE (self));

  event_node = wocky_node_get_child_ns (
      wocky_stanza_get_top_node (event_stanza), "event",
        WOCKY_XMPP_NS_PUBSUB_EVENT);
  g_return_val_if_fail (event_node != NULL, FALSE);
  action_node = wocky_node_get_child (event_node,
      trampoline->mapping->action);
  g_return_val_if_fail (action_node != NULL, FALSE);

  node_name = wocky_node_get_attribute (action_node, "node");

  if (node_name == NULL)
    {
      DEBUG_STANZA (event_stanza, "no node='' attribute on <%s/>",
          trampoline->mapping->action);
      return FALSE;
    }

  node = wocky_pubsub_service_ensure_node (self, node_name);
  trampoline->mapping->method (node, event_stanza, event_node, action_node);
  g_object_unref (node);

  return TRUE;
}

static void
default_configuration_iq_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;
  WockyNodeTree *default_tree;
  WockyDataForm *form;

  if (wocky_pubsub_distill_iq_reply (source, res,
        WOCKY_XMPP_NS_PUBSUB_OWNER, "default", &default_tree, &error))
    {
      form = wocky_data_form_new_from_form (
          wocky_node_tree_get_top_node (default_tree), &error);

      if (form != NULL)
        g_simple_async_result_set_op_res_gpointer (result, form, NULL);

      g_object_unref (default_tree);
    }

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

void
wocky_pubsub_service_get_default_node_configuration_async (
    WockyPubsubService *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubServicePrivate *priv = self->priv;
  WockyStanza *stanza;
  GSimpleAsyncResult *result;

  stanza = wocky_pubsub_make_stanza (priv->jid, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_XMPP_NS_PUBSUB_OWNER, "default", NULL, NULL);

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
    wocky_pubsub_service_get_default_node_configuration_async);

  wocky_porter_send_iq_async (priv->porter, stanza, NULL,
      default_configuration_iq_cb, result);

  g_object_unref (stanza);
}

WockyDataForm *
wocky_pubsub_service_get_default_node_configuration_finish (
    WockyPubsubService *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self),
    wocky_pubsub_service_get_default_node_configuration_async), NULL);

  return g_simple_async_result_get_op_res_gpointer (
      G_SIMPLE_ASYNC_RESULT (result));
}

WockyPubsubSubscription *
wocky_pubsub_service_parse_subscription (WockyPubsubService *self,
    WockyNode *subscription_node,
    const gchar *parent_node_attr,
    GError **error)
{
  const gchar *node;
  const gchar *jid = wocky_node_get_attribute (subscription_node, "jid");
  const gchar *subscription = wocky_node_get_attribute (subscription_node,
      "subscription");
  const gchar *subid = wocky_node_get_attribute (subscription_node,
      "subid");
  WockyPubsubNode *node_obj;
  gint state;
  WockyPubsubSubscription *sub;

  if (parent_node_attr != NULL)
    node = parent_node_attr;
  else
    node = wocky_node_get_attribute (subscription_node, "node");

#define FAIL_IF_NULL(attr) \
  if (attr == NULL) \
    { \
      g_set_error (error, WOCKY_PUBSUB_SERVICE_ERROR, \
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY, \
          "<subscription> missing " #attr "='' attribute"); \
      return NULL; \
    }

  FAIL_IF_NULL (node);
  FAIL_IF_NULL (jid);
  FAIL_IF_NULL (subscription);
  /* subid is technically a MUST if the service supports it, but... */

#undef FAIL_IF_NULL

  if (!wocky_enum_from_nick (WOCKY_TYPE_PUBSUB_SUBSCRIPTION_STATE,
          subscription, &state))
    {
      g_set_error (error, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "subscription='%s' is not a valid state", subscription);
      return NULL;
    }

  node_obj = wocky_pubsub_service_ensure_node (self, node);
  sub = wocky_pubsub_subscription_new (node_obj, jid, state, subid);
  g_object_unref (node_obj);

  return sub;
}

GList *
wocky_pubsub_service_parse_subscriptions (WockyPubsubService *self,
    WockyNode *subscriptions_node,
    GList **subscription_nodes)
{
  const gchar *parent_node_attr = wocky_node_get_attribute (
      subscriptions_node, "node");
  GQueue subs = G_QUEUE_INIT;
  GQueue sub_nodes = G_QUEUE_INIT;
  WockyNode *n;
  WockyNodeIter i;

  wocky_node_iter_init (&i, subscriptions_node, "subscription", NULL);

  while (wocky_node_iter_next (&i, &n))
    {
      GError *error = NULL;
      WockyPubsubSubscription *sub = wocky_pubsub_service_parse_subscription (
          self, n, parent_node_attr, &error);

      if (sub != NULL)
        {
          g_queue_push_tail (&subs, sub);
          g_queue_push_tail (&sub_nodes, n);
        }
      else
        {
          DEBUG ("%s", error->message);
          g_clear_error (&error);
        }
    }

  if (subscription_nodes == NULL)
    g_queue_clear (&sub_nodes);
  else
    *subscription_nodes = sub_nodes.head;

  return subs.head;
}

static void
receive_subscriptions_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (
      g_async_result_get_source_object (user_data));
  WockyNodeTree *subs_tree;
  GError *error = NULL;

  if (wocky_pubsub_distill_iq_reply (source, res, WOCKY_XMPP_NS_PUBSUB,
          "subscriptions", &subs_tree, &error))
    {
      GList *subs = wocky_pubsub_service_parse_subscriptions (self,
          wocky_node_tree_get_top_node (subs_tree), NULL);

      g_simple_async_result_set_op_res_gpointer (simple, subs,
          (GDestroyNotify) wocky_pubsub_subscription_list_free);
      g_object_unref (subs_tree);
    }
  else
    {
      g_simple_async_result_set_from_error (simple, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
  g_object_unref (self);
}

WockyStanza *
wocky_pubsub_service_create_retrieve_subscriptions_stanza (
    WockyPubsubService *self,
    WockyPubsubNode *node,
    WockyNode **pubsub_node,
    WockyNode **subscriptions_node)
{
  WockyPubsubServicePrivate *priv = self->priv;
  WockyStanza *stanza;
  WockyNode *subscriptions;

  stanza = wocky_pubsub_make_stanza (priv->jid, WOCKY_STANZA_SUB_TYPE_GET,
      WOCKY_XMPP_NS_PUBSUB, "subscriptions", pubsub_node, &subscriptions);

  if (node != NULL)
    wocky_node_set_attribute (subscriptions, "node",
        wocky_pubsub_node_get_name (node));

  if (subscriptions_node != NULL)
    *subscriptions_node = subscriptions;

  return stanza;
}

void
wocky_pubsub_service_retrieve_subscriptions_async (
    WockyPubsubService *self,
    WockyPubsubNode *node,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubServicePrivate *priv = self->priv;
  GSimpleAsyncResult *simple = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_pubsub_service_retrieve_subscriptions_async);
  WockyStanza *stanza;

  stanza = wocky_pubsub_service_create_retrieve_subscriptions_stanza (self,
      node, NULL, NULL);

  wocky_porter_send_iq_async (priv->porter, stanza, cancellable,
      receive_subscriptions_cb, simple);

  g_object_unref (stanza);
}

gboolean
wocky_pubsub_service_retrieve_subscriptions_finish (
    WockyPubsubService *self,
    GAsyncResult *result,
    GList **subscriptions,
    GError **error)
{
  wocky_implement_finish_copy_pointer (self,
      wocky_pubsub_service_retrieve_subscriptions_async,
      wocky_pubsub_subscription_list_copy, subscriptions);
}

/**
 * wocky_pubsub_service_handle_create_node_reply:
 * @self: a pubsub service
 * @create_tree: the &lt;create/&gt; tree from the reply to an attempt to
 *               create a node, or %NULL if none was present in the reply.
 * @requested_name: the name we asked the server to use for the node, or %NULL
 *                  if we requested an instant node
 * @error: location at which to store an error
 *
 * Handles the body of a reply to a create node request. This is
 * ever-so-slightly involved, because the server is allowed to omit the body of
 * the reply if you specified a node name and it created a node with that name,
 * but it may also tell you "hey, you asked for 'ringo', but I gave you
 * 'george'". Good times.
 *
 * Returns: a pubsub node if the reply made sense, or %NULL with @error set if
 *          not.
 */
WockyPubsubNode *
wocky_pubsub_service_handle_create_node_reply (
    WockyPubsubService *self,
    WockyNodeTree *create_tree,
    const gchar *requested_name,
    GError **error)
{
  WockyPubsubNode *node = NULL;
  const gchar *name = NULL;

  if (create_tree != NULL)
    {
      /* If the reply contained <pubsub><create>, it'd better contain the
       * nodeID.
       */
      name = wocky_node_get_attribute (
          wocky_node_tree_get_top_node (create_tree), "node");

      if (name == NULL)
        g_set_error (error, WOCKY_PUBSUB_SERVICE_ERROR,
            WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
            "reply doesn't contain node='' attribute");
    }
  else if (requested_name == NULL)
    {
      g_set_error (error, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "requested an instant node, but the server did not report the "
          "newly-created node's name");
    }
  else
    {
      name = requested_name;
    }

  if (name != NULL)
    {
      node = wocky_pubsub_service_ensure_node (self, name);
      DEBUG ("node %s created\n", name);
    }

  return node;
}

static void
create_node_iq_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyPubsubService *self;
  WockyPubsubNode *node = NULL;
  const gchar *requested_name;
  WockyNodeTree *create_tree;
  GError *error = NULL;

  self = WOCKY_PUBSUB_SERVICE (g_async_result_get_source_object (user_data));
  requested_name = g_object_get_data ((GObject *) result, "requested-name");

  if (wocky_pubsub_distill_ambivalent_iq_reply (source, res,
        WOCKY_XMPP_NS_PUBSUB, "create", &create_tree, &error))
    {
      node = wocky_pubsub_service_handle_create_node_reply (self,
          create_tree, requested_name, &error);

      if (create_tree != NULL)
        g_object_unref (create_tree);
    }

  if (node != NULL)
    {
      /* 'result' steals our reference to 'node' */
      g_simple_async_result_set_op_res_gpointer (result, node, g_object_unref);
    }
  else
    {
      g_assert (error != NULL);
      g_simple_async_result_set_from_error (result, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
  g_object_unref (self);
}

WockyStanza *
wocky_pubsub_service_create_create_node_stanza (
    WockyPubsubService *self,
    const gchar *name,
    WockyDataForm *config,
    WockyNode **pubsub_node,
    WockyNode **create_node)
{
  WockyPubsubServicePrivate *priv = self->priv;
  WockyStanza *stanza;
  WockyNode *pubsub, *create;

  stanza = wocky_pubsub_make_stanza (priv->jid, WOCKY_STANZA_SUB_TYPE_SET,
        WOCKY_XMPP_NS_PUBSUB, "create", &pubsub, &create);

  if (name != NULL)
    wocky_node_set_attribute (create, "node", name);

  if (config != NULL)
    wocky_data_form_submit (config, wocky_node_add_child (pubsub,
        "configure"));

  if (pubsub_node != NULL)
    *pubsub_node = pubsub;

  if (create_node != NULL)
    *create_node = create;

  return stanza;
}

void
wocky_pubsub_service_create_node_async (WockyPubsubService *self,
    const gchar *name,
    WockyDataForm *config,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubServicePrivate *priv = self->priv;
  WockyStanza *stanza = wocky_pubsub_service_create_create_node_stanza (
      self, name, config, NULL, NULL);
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_pubsub_service_create_node_async);

  g_object_set_data_full ((GObject *) result, "requested-name",
      g_strdup (name), g_free);

  wocky_porter_send_iq_async (priv->porter, stanza, NULL,
      create_node_iq_cb, result);
  g_object_unref (stanza);
}

WockyPubsubNode *
wocky_pubsub_service_create_node_finish (WockyPubsubService *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;
  WockyPubsubNode *node;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (self), wocky_pubsub_service_create_node_async), NULL);

  simple = (GSimpleAsyncResult *) result;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  node = WOCKY_PUBSUB_NODE (g_simple_async_result_get_op_res_gpointer (simple));

  return g_object_ref (node);
}

WockyPorter *
wocky_pubsub_service_get_porter (WockyPubsubService *self)
{
  WockyPubsubServicePrivate *priv = self->priv;

  return priv->porter;
}

WockyPubsubSubscription *
wocky_pubsub_subscription_new (
    WockyPubsubNode *node,
    const gchar *jid,
    WockyPubsubSubscriptionState state,
    const gchar *subid)
{
  WockyPubsubSubscription *sub = g_slice_new (WockyPubsubSubscription);

  sub->node = g_object_ref (node);
  sub->jid = g_strdup (jid);
  sub->state = state;
  sub->subid = g_strdup (subid);

  return sub;
}

WockyPubsubSubscription *
wocky_pubsub_subscription_copy (WockyPubsubSubscription *sub)
{
  g_return_val_if_fail (sub != NULL, NULL);

  return wocky_pubsub_subscription_new (sub->node, sub->jid, sub->state,
      sub->subid);
}

void
wocky_pubsub_subscription_free (WockyPubsubSubscription *sub)
{
  g_return_if_fail (sub != NULL);

  g_object_unref (sub->node);
  g_free (sub->jid);
  g_free (sub->subid);
  g_slice_free (WockyPubsubSubscription, sub);
}

GList *
wocky_pubsub_subscription_list_copy (GList *subs)
{
  return wocky_list_deep_copy ((GBoxedCopyFunc) wocky_pubsub_subscription_copy,
      subs);
}

void
wocky_pubsub_subscription_list_free (GList *subs)
{
  g_list_foreach (subs, (GFunc) wocky_pubsub_subscription_free, NULL);
  g_list_free (subs);
}

GType
wocky_pubsub_subscription_get_type (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = g_boxed_type_register_static ("WockyPubsubSubscription",
        (GBoxedCopyFunc) wocky_pubsub_subscription_copy,
        (GBoxedFreeFunc) wocky_pubsub_subscription_free);

  return t;
}

/**
 * WockyPubsubServiceClass:
 * @parent_class: parent
 * @node_object_type: the subtype of #WOCKY_TYPE_PUBSUB_NODE to be created by
 *                    wocky_pubsub_service_ensure_node()
 *
 * The class structure for the #WockyPubsubService type.
 */

/**
 * WockyPubsubSubscription:
 * @node: a PubSub node
 * @jid: the JID which is subscribed to @node. This may be a bare JID, or a
 *       full JID with a resource, depending on which was specified when
 *       subscribing to @node. See XEP-0060 §6.1 Subscribe to a Node
 * @state: the state of this subscription
 * @subid: a unique identifier for this subscription, if a JID is subscribed to
 *         a node multiple times, or %NULL if there is no such identifier. See
 *         XEP-0060 §6.1.6 “Multiple Subscriptions”
 *
 * Represents a subscription to a node on a pubsub service, as seen when
 * listing your own subscriptions on a service with
 * wocky_pubsub_service_retrieve_subscriptions_async() or subscribing to a node
 * with wocky_pubsub_node_subscribe_async().
 */

/**
 * WockyPubsubSubscriptionState:
 * @WOCKY_PUBSUB_SUBSCRIPTION_NONE: The node MUST NOT send event notifications
 *  or payloads to the Entity.
 * @WOCKY_PUBSUB_SUBSCRIPTION_PENDING: An entity has requested to subscribe to
 *  a node and the request has not yet been approved by a node owner. The node
 *  MUST NOT send event notifications or payloads to the entity while it is in
 *  this state.
 * @WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED: An entity has subscribed but its
 *  subscription options have not yet been configured. The node MAY send event
 *  notifications or payloads to the entity while it is in this state. The
 *  service MAY timeout unconfigured subscriptions.
 * @WOCKY_PUBSUB_SUBSCRIPTION_UNCONFIGURED: An entity is subscribed to a node.
 *  The node MUST send all event notifications (and, if configured, payloads)
 *  to the entity while it is in this state (subject to subscriber
 *  configuration and content filtering).
 *
 * Describes the state of a subscription to a node. Definitions are taken from
 * <ulink url="xmpp.org/extensions/xep-0060.html#substates">XEP-0060
 * §4.2</ulink>.
 */
