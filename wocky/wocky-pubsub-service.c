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

#include "wocky-pubsub-service.h"

#include "wocky-porter.h"
#include "wocky-utils.h"
#include "wocky-pubsub-node.h"
#include "wocky-pubsub-node-protected.h"
#include "wocky-namespaces.h"
#include "wocky-signals-marshal.h"

#define DEBUG_FLAG DEBUG_PUBSUB
#include "wocky-debug.h"

static gboolean pubsub_service_handle_event_stanza (WockyPorter *porter,
    WockyXmppStanza *event_stanza,
    gpointer user_data);

G_DEFINE_TYPE (WockyPubsubService, wocky_pubsub_service, G_TYPE_OBJECT)

/* signal enum */
enum
{
  SIG_EVENT_RECEIVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

enum
{
  PROP_SESSION = 1,
  PROP_JID,
};

/* private structure */
typedef struct _WockyPubsubServicePrivate WockyPubsubServicePrivate;

struct _WockyPubsubServicePrivate
{
  WockySession *session;
  WockyPorter *porter;

  gchar *jid;
  /* owned (gchar *) => weak reffed (WockyPubsubNode *) */
  GHashTable *nodes;

  guint handler_id;

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

#define WOCKY_PUBSUB_SERVICE_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_PUBSUB_SERVICE, \
    WockyPubsubServicePrivate))

static void
wocky_pubsub_service_init (WockyPubsubService *obj)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (obj);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  priv->nodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
wocky_pubsub_service_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

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
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

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
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->porter != NULL)
    {
      wocky_porter_unregister_handler (priv->porter, priv->handler_id);
      priv->handler_id = 0;

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
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  g_free (priv->jid);
  g_hash_table_unref (priv->nodes);

  G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->finalize (object);
}

static void
wocky_pubsub_service_constructed (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  g_assert (priv->session != NULL);
  g_assert (priv->jid != NULL);

  priv->porter = wocky_session_get_porter (priv->session);
  g_object_ref (priv->porter);

  priv->handler_id = wocky_porter_register_handler (priv->porter,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
      priv->jid,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
      pubsub_service_handle_event_stanza, self,
        WOCKY_NODE, "event",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_EVENT,
          WOCKY_NODE, "items", WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);
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
   * @items: a list of WockyXmppNode *s for each item child of @items_node
   *
   * Emitted when an event is received for a node.
   */
  signals[SIG_EVENT_RECEIVED] = g_signal_new ("event-received", ctype,
      0, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_OBJECT_POINTER_POINTER_POINTER,
      G_TYPE_NONE, 5,
      WOCKY_TYPE_PUBSUB_NODE, WOCKY_TYPE_XMPP_STANZA, G_TYPE_POINTER,
      G_TYPE_POINTER, G_TYPE_POINTER);
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
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  g_hash_table_foreach_remove (priv->nodes, remove_node, node);
}

static void
pubsub_service_node_event_received_cb (
    WockyPubsubNode *node,
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *items_node,
    GList *items,
    gpointer user_data)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);

  g_signal_emit (self, signals[SIG_EVENT_RECEIVED], 0, node, event_stanza,
      event_node, items_node, items);
}

static WockyPubsubNode *
create_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);
  WockyPubsubNode *node;

  node = wocky_pubsub_node_new (self, name);

  g_object_weak_ref (G_OBJECT (node), node_disposed_cb, self);
  g_hash_table_insert (priv->nodes, g_strdup (name), node);

  /* It's safe to never explicitly disconnect this handler: the node holds a
   * reference to the service, so the service will always outlive the node.
   */
  g_signal_connect (node, "event-received",
      (GCallback) pubsub_service_node_event_received_cb, self);

  return node;
}

/**
 * wocky_pubsub_service_ensure_node:
 * @self: a pubsub service
 * @name: the name of a node on @self
 *
 * Fetches or creates an object representing a node on the pubsub service. Note
 * that this does not ensure that @node exists on the server; it merely ensures
 * a local representation.
 *
 * Returns: a new reference to an object representing @node on @self
 */
WockyPubsubNode *
wocky_pubsub_service_ensure_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);
  WockyPubsubNode *node;

  node = g_hash_table_lookup (priv->nodes, name);

  if (node != NULL)
    return g_object_ref (node);
  else
    return create_node (self, name);
}

/**
 * wocky_pubsub_service_lookup_node:
 * @self: a pubsub service
 * @name: the name of a node on @self
 *
 * Fetches an object representing a node on a pubsub service, if the object
 * already exists; if not, returns %NULL. Note that this does not check whether
 * @node exists on the server; it only chceks for a local representation.
 *
 * Returns: a borrowed reference to a node, or %NULL
 */
WockyPubsubNode *
wocky_pubsub_service_lookup_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  return g_hash_table_lookup (priv->nodes, name);
}

static gboolean
pubsub_service_handle_event_stanza (WockyPorter *porter,
    WockyXmppStanza *event_stanza,
    gpointer user_data)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);
  WockyXmppNode *event_node, *items_node;
  const gchar *node_name;
  WockyPubsubNode *node;

  event_node = wocky_xmpp_node_get_child_ns (event_stanza->node, "event",
      WOCKY_XMPP_NS_PUBSUB_EVENT);
  g_return_val_if_fail (event_node != NULL, FALSE);
  items_node = wocky_xmpp_node_get_child (event_node, "items");
  g_return_val_if_fail (items_node != NULL, FALSE);

  node_name = wocky_xmpp_node_get_attribute (items_node, "node");

  if (node_name == NULL)
    {
      DEBUG_STANZA (event_stanza, "no node='' attribute on <items/>");
      return FALSE;
    }

  node = wocky_pubsub_service_ensure_node (self, node_name);
  _wocky_pubsub_node_handle_event_stanza (node, event_stanza);
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
  WockyXmppStanza *reply;
  WockyXmppNode *node;
  WockyDataForms *forms;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, &error);
  if (reply == NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
      goto out;
    }

  if (wocky_xmpp_stanza_extract_errors (reply, NULL, &error, NULL, NULL))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
      goto out;
    }

  node = wocky_xmpp_node_get_child_ns (reply->node, "pubsub",
      WOCKY_XMPP_NS_PUBSUB_OWNER);
  if (node == NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "Reply doesn't contain 'pubsub' node");
      goto out;
    }

  node = wocky_xmpp_node_get_child (node, "default");
  if (node == NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "Reply doesn't contain 'default' node");
      goto out;
    }

  forms = wocky_data_forms_new_from_form (node, &error);
  if (forms == NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
      goto out;
    }

  g_simple_async_result_set_op_res_gpointer (result, forms, NULL);

out:
  g_simple_async_result_complete (result);
  g_object_unref (result);
  if (reply != NULL)
    g_object_unref (reply);
}

void
wocky_pubsub_service_get_default_node_configuration_async (
    WockyPubsubService *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  GSimpleAsyncResult *result;

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      NULL, priv->jid,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB_OWNER,
        WOCKY_NODE, "default",
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
    wocky_pubsub_service_get_default_node_configuration_finish);

  wocky_porter_send_iq_async (priv->porter, stanza, NULL,
      default_configuration_iq_cb, result);

  g_object_unref (stanza);
}

WockyDataForms *
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
    wocky_pubsub_service_get_default_node_configuration_finish), NULL);

  return g_simple_async_result_get_op_res_gpointer (
      G_SIMPLE_ASYNC_RESULT (result));
}

static void
create_node_iq_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyPubsubService *self;
  GError *error = NULL;
  WockyXmppStanza *reply;
  WockyPubsubNode *node;
  const gchar *name;
  WockyXmppNode *n;

  self = WOCKY_PUBSUB_SERVICE (g_async_result_get_source_object (user_data));

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, &error);
  if (reply == NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
      goto out;
    }

  if (wocky_xmpp_stanza_extract_errors (reply, NULL, &error, NULL, NULL))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
      goto out;
    }

  n = wocky_xmpp_node_get_child_ns (reply->node, "pubsub",
      WOCKY_XMPP_NS_PUBSUB);
  if (n == NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "reply doesn't contain pubsub node");
      goto out;
    }

  n = wocky_xmpp_node_get_child (n, "create");
  if (n == NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "reply doesn't contain create node");
      goto out;
    }

  name = wocky_xmpp_node_get_attribute (n, "node");
  if (name == NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "reply doesn't contain node attribute");
      goto out;
    }

  node = wocky_pubsub_service_ensure_node (self, name);
  DEBUG ("node %s created\n", name);

  /* 'result' steals our reference to 'node' */
  g_simple_async_result_set_op_res_gpointer (result, node, g_object_unref);

out:
  g_simple_async_result_complete (result);
  g_object_unref (result);
  if (reply != NULL)
    g_object_unref (reply);
  /* g_async_result_get_source_object refs the object */
  g_object_unref (self);
}

void
wocky_pubsub_service_create_node_async (WockyPubsubService *self,
    const gchar *name,
    WockyDataForms *config,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);
  WockyXmppStanza *stanza;
  GSimpleAsyncResult *result;
  WockyXmppNode *node, *configure;

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, priv->jid,
      WOCKY_NODE, "pubsub",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
        WOCKY_NODE, "create",
          WOCKY_NODE_ASSIGN_TO, &node,
        WOCKY_NODE_END,
        WOCKY_NODE, "configure",
          WOCKY_NODE_ASSIGN_TO, &configure,
        WOCKY_NODE_END,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  if (name != NULL)
    wocky_xmpp_node_set_attribute (node, "node", name);

  if (config != NULL)
    wocky_data_forms_submit (config, configure);

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
    wocky_pubsub_service_create_node_async);

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
