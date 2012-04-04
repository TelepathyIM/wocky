/*
 * wocky-pep-service.c - WockyPepService
 * Copyright © 2009,2012 Collabora Ltd.
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
 * SECTION: wocky-pep-service
 * @title: WockyPepService
 * @short_description: Object to represent a single PEP service
 * @include: wocky/wocky-pep-service.h
 *
 * Object to aid with looking up PEP nodes and listening for changes.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-pep-service.h"

#include "wocky-pubsub-helpers.h"
#include "wocky-porter.h"
#include "wocky-utils.h"
#include "wocky-namespaces.h"
#include "wocky-signals-marshal.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_PUBSUB
#include "wocky-debug-internal.h"

G_DEFINE_TYPE (WockyPepService, wocky_pep_service, G_TYPE_OBJECT)

/* signal enum */
enum
{
  CHANGED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

enum
{
  PROP_NODE = 1,
  PROP_SUBSCRIBE,
};

/* private structure */
struct _WockyPepServicePrivate
{
  WockySession *session;
  WockyPorter *porter;
  WockyContactFactory *contact_factory;

  gchar *node;
  gboolean subscribe;
  guint handler_id;

  gboolean dispose_has_run;
};

static void
wocky_pep_service_init (WockyPepService *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_PEP_SERVICE,
      WockyPepServicePrivate);
}

static void
wocky_pep_service_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyPepService *self = WOCKY_PEP_SERVICE (object);
  WockyPepServicePrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_NODE:
        priv->node = g_value_dup_string (value);
        break;
      case PROP_SUBSCRIBE:
        priv->subscribe = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_pep_service_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyPepService *self = WOCKY_PEP_SERVICE (object);
  WockyPepServicePrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_NODE:
        g_value_set_string (value, priv->node);
        break;
      case PROP_SUBSCRIBE:
        g_value_set_boolean (value, priv->subscribe);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
wocky_pep_service_dispose (GObject *object)
{
  WockyPepService *self = WOCKY_PEP_SERVICE (object);
  WockyPepServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->porter != NULL)
    {
      g_assert (priv->handler_id != 0);
      wocky_porter_unregister_handler (priv->porter, priv->handler_id);
      g_object_unref (priv->porter);
    }

  if (priv->contact_factory != NULL)
    g_object_unref (priv->contact_factory);

  if (G_OBJECT_CLASS (wocky_pep_service_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_pep_service_parent_class)->dispose (object);
}

static void
wocky_pep_service_finalize (GObject *object)
{
  WockyPepService *self = WOCKY_PEP_SERVICE (object);
  WockyPepServicePrivate *priv = self->priv;

  g_free (priv->node);

  G_OBJECT_CLASS (wocky_pep_service_parent_class)->finalize (object);
}

static void
wocky_pep_service_constructed (GObject *object)
{
  WockyPepService *self = WOCKY_PEP_SERVICE (object);
  WockyPepServicePrivate *priv = self->priv;

  g_assert (priv->node != NULL);
}

static void
wocky_pep_service_class_init (WockyPepServiceClass *wocky_pep_service_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_pep_service_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_pep_service_class,
      sizeof (WockyPepServicePrivate));

  object_class->set_property = wocky_pep_service_set_property;
  object_class->get_property = wocky_pep_service_get_property;
  object_class->dispose = wocky_pep_service_dispose;
  object_class->finalize = wocky_pep_service_finalize;
  object_class->constructed = wocky_pep_service_constructed;

  /**
   * WockyPepService:node:
   *
   * Namespace of the PEP node.
   */
  param_spec = g_param_spec_string ("node", "node",
      "namespace of the pep node",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NODE, param_spec);

  /**
   * WockyPepService:subscribe:
   *
   * %TRUE if Wocky is to subscribe to the notifications of the node.
   */
  param_spec = g_param_spec_boolean ("subscribe", "subscribe",
      "if TRUE, Wocky will subscribe to the notifications of the node",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SUBSCRIBE, param_spec);

  /**
   * WockyPepService::changed:
   * @self: a #WockyPepService object
   * @contact: the #WockyBareContact who changed the node
   * @stanza: the #WockyStanza
   * @item: the first—and typically only—&lt;item&gt; element in @stanza, or
   *  %NULL if there is none.
   *
   * Emitted when the node value changes.
   */
  signals[CHANGED] = g_signal_new ("changed",
      G_OBJECT_CLASS_TYPE (wocky_pep_service_class),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_OBJECT_POINTER,
      G_TYPE_NONE, 3, WOCKY_TYPE_BARE_CONTACT, WOCKY_TYPE_STANZA, G_TYPE_POINTER);
}

/**
 * wocky_pep_service_new:
 * @node: the namespace of the PEP node
 * @subscribe: %TRUE if Wocky is to subscribe to the notifications of
 *   the node, otherwise %FALSE
 *
 * A convenience function to create a new #WockyPepService object.
 *
 * Returns: a new #WockyPepService
 */
WockyPepService *
wocky_pep_service_new (const gchar *node,
    gboolean subscribe)
{
  return g_object_new (WOCKY_TYPE_PEP_SERVICE,
      "node", node,
      "subscribe", subscribe,
      NULL);
}

static gboolean
msg_event_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  WockyPepService *self = WOCKY_PEP_SERVICE (user_data);
  WockyPepServicePrivate *priv = self->priv;
  const gchar *from;
  WockyBareContact *contact;
  WockyStanzaSubType sub_type;
  WockyNode *event, *items, *item;

  from = wocky_stanza_get_from (stanza);
  if (from == NULL)
    {
      DEBUG ("No 'from' attribute; ignoring event");
      return FALSE;
    }

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);

  /* type of the message is supposed to be 'headline' but old ejabberd
   * omits it */
  if (sub_type != WOCKY_STANZA_SUB_TYPE_NONE &&
      sub_type != WOCKY_STANZA_SUB_TYPE_HEADLINE)
    {
      return FALSE;
    }

  event = wocky_node_get_child_ns (wocky_stanza_get_top_node (stanza),
      "event", WOCKY_XMPP_NS_PUBSUB_EVENT);
  g_return_val_if_fail (event != NULL, FALSE);
  items = wocky_node_get_child (event, "items");
  g_return_val_if_fail (items != NULL, FALSE);
  item = wocky_node_get_child (items, "item");

  contact = wocky_contact_factory_ensure_bare_contact (
      priv->contact_factory, from);

  g_signal_emit (G_OBJECT (self), signals[CHANGED], 0, contact, stanza, item);

  g_object_unref (contact);
  return TRUE;
}

/**
 * wocky_pep_service_start:
 * @self: a #WockyPepService object
 * @session: a #WockySession object
 *
 * Start listening to the PEP node @node and signal changes by using
 * #WockyPepService::changed.
 */
void
wocky_pep_service_start (WockyPepService *self,
    WockySession *session)
{
  WockyPepServicePrivate *priv = self->priv;

  g_assert (priv->session == NULL);
  priv->session = session;

  priv->porter = wocky_session_get_porter (priv->session);
  g_object_ref (priv->porter);

  priv->contact_factory = wocky_session_get_contact_factory (priv->session);
  g_object_ref (priv->contact_factory);

  /* Register event handler */
  priv->handler_id = wocky_porter_register_handler_from_anyone (priv->porter,
      WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
      WOCKY_PORTER_HANDLER_PRIORITY_MAX,
      msg_event_cb, self,
      '(', "event",
        ':', WOCKY_XMPP_NS_PUBSUB_EVENT,
        '(', "items",
          '@', "node", priv->node,
        ')',
      ')',
      NULL);

  /* TODO: subscribe to node if needed */
}

static void
send_query_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;
  WockyStanza *reply;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, &error);
  if (reply == NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (result, reply, g_object_unref);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * wocky_pep_service_get_async:
 * @self: a #WockyPepService object
 * @contact: a #WockyBareContact object
 * @cancellable: an optional #GCancellable object, or %NULL
 * @callback: a function to call when the node is retrieved
 * @user_data: user data for @callback
 *
 * Starts an asynchronous operation to get the PEP node,
 * #WockyPepService:node.
 *
 * When the operation is complete, @callback will be called and the
 * function should call wocky_pep_service_get_finish().
 */
void
wocky_pep_service_get_async (WockyPepService *self,
    WockyBareContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPepServicePrivate *priv = self->priv;
  WockyStanza *msg;
  GSimpleAsyncResult *result;
  const gchar *jid;

  if (priv->porter == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_PORTER_ERROR, WOCKY_PORTER_ERROR_NOT_STARTED,
          "Service has not been started");
      return;
    }

  jid = wocky_bare_contact_get_jid (contact);

  msg = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      NULL, jid,
      '(', "pubsub",
        ':', WOCKY_XMPP_NS_PUBSUB,
        '(', "items",
          '@', "node", priv->node,
        ')',
      ')', NULL);

  result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, wocky_pep_service_get_async);

  wocky_porter_send_iq_async (priv->porter, msg, cancellable, send_query_cb,
      result);

  g_object_unref (msg);
}

/**
 * wocky_pep_service_get_finish:
 * @self: a #WockyPepService object
 * @result: a #GAsyncResult
 * @item: (out) (allow-none): on success, the first &lt;item&gt; element
 *  in the result, or %NULL if @self has no published items.
 * @error: a location to store a #GError if an error occurs
 *
 * Finishes an asynchronous operation to get the PEP node,
 * #WockyPepService:node. For more details, see
 * wocky_pep_service_get_async().
 *
 * Returns: the #WockyStanza retrieved from getting the PEP node.
 */
WockyStanza *
wocky_pep_service_get_finish (WockyPepService *self,
    GAsyncResult *result,
    WockyNode **item,
    GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  WockyStanza *reply;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_pep_service_get_async), NULL);
  reply = WOCKY_STANZA (g_simple_async_result_get_op_res_gpointer (simple));

  if (item != NULL)
    {
      WockyNode *pubsub_node = wocky_node_get_child_ns (
          wocky_stanza_get_top_node (reply), "pubsub", WOCKY_XMPP_NS_PUBSUB);
      WockyNode *items_node = NULL;

      if (pubsub_node != NULL)
        items_node = wocky_node_get_child (pubsub_node, "items");

      if (items_node != NULL)
        *item = wocky_node_get_child (items_node, "item");
      else
        *item = NULL;
    }

  return g_object_ref (reply);
}

/**
 * wocky_pep_service_make_publish_stanza:
 * @self: a #WockyPepService
 * @item: a location to store the item #WockyNode, or %NULL
 *
 * Generates a new IQ type='set' PEP publish stanza.
 *
 * Returns: a new #WockyStanza PEP publish stanza; free with g_object_unref()
 */
WockyStanza *
wocky_pep_service_make_publish_stanza (WockyPepService *self,
    WockyNode **item)
{
  WockyPepServicePrivate *priv = self->priv;

  return wocky_pubsub_make_publish_stanza (NULL, priv->node, NULL, NULL, item);
}
