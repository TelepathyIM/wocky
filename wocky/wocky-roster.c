/*
 * wocky-roster.c - Source for WockyRoster
 * Copyright (C) 2009 Collabora Ltd.
 * @author Jonny Lamb <jonny.lamb@collabora.co.uk>
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
 * SECTION: wocky-roster
 * @title: WockyRoster
 * @short_description: TODO
 *
 * TODO
 */

#include <string.h>

#include <gio/gio.h>

#include "wocky-roster.h"

#include "wocky-contact.h"
#include "wocky-namespaces.h"
#include "wocky-xmpp-stanza.h"
#include "wocky-utils.h"
#include "wocky-signals-marshal.h"

#define DEBUG_FLAG DEBUG_ROSTER
#include "wocky-debug.h"

#define GOOGLE_ROSTER_VERSION "2"

G_DEFINE_TYPE (WockyRoster, wocky_roster, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_PORTER = 1,
};

/* signal enum */
enum
{
  ADDED,
  REMOVED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _WockyRosterPrivate WockyRosterPrivate;

struct _WockyRosterPrivate
{
  WockyPorter *porter;
  /* owned (gchar *) => reffed (WockyContact *) */
  GHashTable *items;
  guint iq_cb;

  GSimpleAsyncResult *fetch_result;

  gboolean dispose_has_run;
};

#define WOCKY_ROSTER_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_ROSTER, \
    WockyRosterPrivate))

/**
 * wocky_roster_error_quark
 *
 * Get the error quark used by the roster.
 *
 * Returns: the quark for roster errors.
 */
GQuark
wocky_roster_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-roster-error");

  return quark;
}

static void
wocky_roster_init (WockyRoster *obj)
{
  /*
  WockyRoster *self = WOCKY_ROSTER (obj);
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  */
}

static void
wocky_roster_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_PORTER:
      priv->porter = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_roster_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_PORTER:
      g_value_set_object (value, priv->porter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static const gchar *
subscription_to_string (WockyRosterSubscriptionFlags subscription)
{
  switch (subscription)
    {
      case WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE:
        return "none";
      case WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO:
        return "to";
      case WOCKY_ROSTER_SUBSCRIPTION_TYPE_FROM:
        return "from";
      case WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH:
        return "both";
      default:
        g_assert_not_reached ();
        return NULL;
    }
}

static void
remove_item (WockyRoster *self,
    const gchar *jid)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  WockyContact *contact;

  contact = g_hash_table_lookup (priv->items, jid);
  if (contact == NULL)
    {
      DEBUG ("%s is not in the roster; can't remove it", jid);
      return;
    }

  /* Removing the contact from the hash table will unref it. Keep it a ref
   * while firing the 'removed' signal. */
  g_object_ref (contact);
  g_hash_table_remove (priv->items, jid);

  g_signal_emit (self, signals[REMOVED], 0, contact);
  g_object_unref (contact);
}

static gboolean
roster_update (WockyRoster *self,
    WockyXmppStanza *stanza,
    gboolean fire_signals,
    GError **error)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  gboolean google_roster = FALSE;
  WockyXmppNode *query_node;
  GSList *j;

  /* Check for google roster support */
  if (FALSE /* FIXME: can support google */)
    {
      const gchar *gr_ext;

      /* FIXME: this is wrong, we should use _get_attribute_ns instead of
       * assuming the prefix */
      gr_ext = wocky_xmpp_node_get_attribute (stanza->node, "gr:ext");

      if (!wocky_strdiff (gr_ext, GOOGLE_ROSTER_VERSION))
        google_roster = TRUE;
    }

  /* Check stanza contains query node. */
  query_node = wocky_xmpp_node_get_child_ns (stanza->node, "query",
      WOCKY_XMPP_NS_ROSTER);

  if (query_node == NULL)
    {
      g_set_error_literal (error, WOCKY_ROSTER_ERROR,
          WOCKY_ROSTER_ERROR_INVALID_STANZA,
          "IQ does not have query node");

      return FALSE;
    }

  /* Iterate through item nodes. */
  for (j = query_node->children; j; j = j->next)
    {
      const gchar *jid;
      WockyXmppNode *n = (WockyXmppNode *) j->data;
      WockyContact *contact = NULL;
      const gchar *subscription;
      WockyRosterSubscriptionFlags subscription_type;
      GPtrArray *groups_arr;
      GStrv groups = { NULL };
      GSList *l;

      if (wocky_strdiff (n->name, "item"))
        {
          DEBUG ("Node %s is not item, skipping", n->name);
          continue;
        }

      jid = wocky_xmpp_node_get_attribute (n, "jid");

      if (jid == NULL)
        {
          DEBUG ("Node %s has no jid attribute, skipping", n->name);
          continue;
        }

      if (strchr (jid, '/') != NULL)
        {
          DEBUG ("Item node has resource in jid, skipping");
          continue;
        }

      /* Parse item. */
      subscription = wocky_xmpp_node_get_attribute (n, "subscription");

      if (!wocky_strdiff (subscription, "to"))
        subscription_type = WOCKY_ROSTER_SUBSCRIPTION_TYPE_TO;
      else if (!wocky_strdiff (subscription, "from"))
        subscription_type = WOCKY_ROSTER_SUBSCRIPTION_TYPE_FROM;
      else if (!wocky_strdiff (subscription, "both"))
        subscription_type = WOCKY_ROSTER_SUBSCRIPTION_TYPE_BOTH;
      else if (!wocky_strdiff (subscription, "none"))
        subscription_type = WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE;
      else if (!wocky_strdiff (subscription, "remove"))
        {
          remove_item (self, jid);
          continue;
        }
      else
        {
          DEBUG ("Unknown subscription: %s; ignoring", subscription);
          continue;
        }

      groups_arr = g_ptr_array_new ();

      /* Look for "group" nodes */
      for (l = n->children; l != NULL; l = g_slist_next (l))
        {
          WockyXmppNode *node = (WockyXmppNode *) l->data;

          if (wocky_strdiff (node->name, "group"))
            continue;

          g_ptr_array_add (groups_arr, g_strdup (node->content));
        }

      /* Add trailing NULL */
      g_ptr_array_add (groups_arr, NULL);
      groups = (GStrv) g_ptr_array_free (groups_arr, FALSE);

      contact = g_hash_table_lookup (priv->items, jid);
      if (contact != NULL)
        {
          /* Contact already exists; update. */
          wocky_contact_set_name (contact,
              wocky_xmpp_node_get_attribute (n, "name"));

          wocky_contact_set_subscription (contact, subscription_type);

          wocky_contact_set_groups (contact, groups);
        }
      else
        {
          /* Create a new contact. */
          contact = g_object_new (WOCKY_TYPE_CONTACT,
              "jid", jid,
              "name", wocky_xmpp_node_get_attribute (n, "name"),
              "subscription", subscription_type,
              "groups", groups,
              NULL);

          g_hash_table_insert (priv->items, g_strdup (jid), contact);

          if (fire_signals)
            g_signal_emit (self, signals[ADDED], 0, contact);
        }

      g_strfreev (groups);
    }

  return TRUE;
}

static gboolean
roster_iq_handler_set_cb (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data)
{
  WockyRoster *self = WOCKY_ROSTER (user_data);
  const gchar *from;
  GError *error = NULL;
  WockyXmppStanza *reply;

  from = wocky_xmpp_node_get_attribute (stanza->node, "from");

  if (from != NULL)
    {
      /* TODO: discard roster IQs which are not from ourselves or the
       * server. */
      return TRUE;
    }

  if (!roster_update (self, stanza, TRUE, &error))
    {
      DEBUG ("Failed to update roster: %s",
          error ? error->message : "no message");
      g_error_free (error);
      reply = wocky_xmpp_stanza_build_iq_error (stanza, WOCKY_STANZA_END);
    }
  else
    {
      /* ack */
      reply = wocky_xmpp_stanza_build_iq_result (stanza, WOCKY_STANZA_END);
    }

  wocky_porter_send (porter, reply);
  g_object_unref (reply);

  return TRUE;
}

static void
wocky_roster_constructed (GObject *object)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);

  priv->items = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);

  priv->iq_cb = wocky_porter_register_handler (priv->porter,
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET, NULL,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, roster_iq_handler_set_cb, self,
      WOCKY_NODE, "query",
        WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
      WOCKY_NODE_END, WOCKY_STANZA_END);
}

static void
wocky_roster_dispose (GObject *object)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->iq_cb != 0)
    {
      wocky_porter_unregister_handler (priv->porter, priv->iq_cb);
      priv->iq_cb = 0;
    }

  if (priv->porter != NULL)
    g_object_unref (priv->porter);

  if (G_OBJECT_CLASS (wocky_roster_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_roster_parent_class)->dispose (object);
}

static void
wocky_roster_finalize (GObject *object)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);

  g_hash_table_destroy (priv->items);

  G_OBJECT_CLASS (wocky_roster_parent_class)->finalize (object);
}

static void
wocky_roster_class_init (WockyRosterClass *wocky_roster_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_roster_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_roster_class,
      sizeof (WockyRosterPrivate));

  object_class->constructed = wocky_roster_constructed;
  object_class->set_property = wocky_roster_set_property;
  object_class->get_property = wocky_roster_get_property;
  object_class->dispose = wocky_roster_dispose;
  object_class->finalize = wocky_roster_finalize;

  spec = g_param_spec_object ("porter", "Wocky porter",
    "the wocky porter used by this roster",
    WOCKY_TYPE_PORTER,
    G_PARAM_READWRITE |
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PORTER, spec);

  signals[ADDED] = g_signal_new ("added",
      G_OBJECT_CLASS_TYPE (wocky_roster_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[REMOVED] = g_signal_new ("removed",
      G_OBJECT_CLASS_TYPE (wocky_roster_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

WockyRoster *
wocky_roster_new (WockyPorter *porter)
{
  g_return_val_if_fail (WOCKY_IS_PORTER (porter), NULL);

  return g_object_new (WOCKY_TYPE_ROSTER,
      "porter", porter,
      NULL);
}

static void
roster_fetch_roster_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  WockyXmppStanza *iq;
  WockyRoster *self = WOCKY_ROSTER (user_data);
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);

  iq = wocky_porter_send_iq_finish (WOCKY_PORTER (source_object), res, &error);

  if (iq == NULL)
    goto out;

  if (!roster_update (self, iq, FALSE, &error))
    goto out;

out:
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (priv->fetch_result, error);
      g_error_free (error);
    }

  if (iq != NULL)
    g_object_unref (iq);

  g_simple_async_result_complete (priv->fetch_result);
  g_object_unref (priv->fetch_result);
  priv->fetch_result = NULL;
}

void
wocky_roster_fetch_roster_async (WockyRoster *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv;
  WockyXmppStanza *iq;

  g_return_if_fail (WOCKY_IS_ROSTER (self));

  priv = WOCKY_ROSTER_GET_PRIVATE (self);

  if (priv->fetch_result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, G_IO_ERROR, G_IO_ERROR_PENDING,
          "Another fetch operation is pending");
      return;
    }

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, NULL, NULL,
        WOCKY_NODE, "query",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  priv->fetch_result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_fetch_roster_finish);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, roster_fetch_roster_cb, self);
  g_object_unref (iq);
}

gboolean
wocky_roster_fetch_roster_finish (WockyRoster *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), wocky_roster_fetch_roster_finish), FALSE);

  return TRUE;
}

WockyContact *
wocky_roster_get_contact (WockyRoster *self,
    const gchar *jid)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);

  return g_hash_table_lookup (priv->items, jid);
}

GSList *
wocky_roster_get_all_contacts (WockyRoster *self)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  GSList *result = NULL;
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, priv->items);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      result = g_slist_prepend (result, g_object_ref (value));
    }

  return result;
}

static void
change_roster_iq_cb (GObject *source_object,
    GAsyncResult *send_iq_res,
    gpointer user_data)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
  WockyXmppStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source_object),
      send_iq_res, &error);
  if (reply == NULL)
    goto out;

  error = wocky_xmpp_stanza_to_gerror (reply);

  /* According to the XMPP RFC, the server has to send a roster upgrade to
   * each client (including the one which requested the change) before
   * replying to the 'set' stanza. We upgraded our list of contacts when this
   * notification has been received.
   * FIXME: Should we check if this upgrade has actually be received and raise
   * en error if it has not? */

out:
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }

  if (reply != NULL)
    g_object_unref (reply);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/* Build an IQ set stanza containing the current state of the contact.
 * If not NULL, item_node will contain a pointer on the "item" node */
static WockyXmppStanza *
build_iq_for_contact (WockyContact *contact,
    WockyXmppNode **item_node)
{
  WockyXmppStanza *iq;
  WockyXmppNode *item = NULL;
  const gchar *jid, *name;
  const gchar * const *groups;
  guint i;
  WockyRosterSubscriptionFlags subscription;

  jid = wocky_contact_get_jid (contact);
  g_return_val_if_fail (jid != NULL, NULL);

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
        WOCKY_NODE, "query",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
          WOCKY_NODE, "item",
            WOCKY_NODE_ASSIGN_TO, &item,
            WOCKY_NODE_ATTRIBUTE, "jid", jid,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  g_assert (item != NULL);

  name = wocky_contact_get_name (contact);
  if (name != NULL)
    {
      wocky_xmpp_node_set_attribute (item, "name", name);
    }

  subscription = wocky_contact_get_subscription (contact);
  if (subscription != WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE)
    {
      wocky_xmpp_node_set_attribute (item, "subscription",
          subscription_to_string (subscription));
    }

  groups = wocky_contact_get_groups (contact);
  for (i = 0; groups[i] != NULL; i++)
    {
      WockyXmppNode *group;

      group = wocky_xmpp_node_add_child (item, "group");
      wocky_xmpp_node_set_content (group, groups[i]);
    }

  if (item_node != NULL)
    *item_node = item;

  return iq;
}

void
wocky_roster_add_contact_async (WockyRoster *self,
    WockyContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  const gchar *jid;
  WockyXmppStanza *iq;
  GSimpleAsyncResult *result;

  g_return_if_fail (contact != NULL);

  jid = wocky_contact_get_jid (contact);
  g_assert (jid != NULL);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_add_contact_finish);

  if (g_hash_table_lookup (priv->items, jid) != NULL)
    {

      DEBUG ("Contact %s is already present in the roster", jid);
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  iq = build_iq_for_contact (contact, NULL);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, result);

  g_object_unref (iq);
}

gboolean
wocky_roster_add_contact_finish (WockyRoster *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), wocky_roster_add_contact_finish), FALSE);

  return TRUE;
}

static gboolean
is_contact (gpointer key,
    gpointer value,
    gpointer contact)
{
  return value == contact;
}

static gboolean
contact_in_roster (WockyRoster *self,
    WockyContact *contact)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);

  return g_hash_table_find (priv->items, is_contact, contact) != NULL;
}

void
wocky_roster_remove_contact_async (WockyRoster *self,
    WockyContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  WockyXmppStanza *iq;
  GSimpleAsyncResult *result;

  g_return_if_fail (contact != NULL);

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", wocky_contact_get_jid (contact));
      return;
    }

  iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
        WOCKY_NODE, "query",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_ROSTER,
          WOCKY_NODE, "item",
            WOCKY_NODE_ATTRIBUTE, "jid", wocky_contact_get_jid (contact),
            WOCKY_NODE_ATTRIBUTE, "subscription", "remove",
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_remove_contact_finish);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, result);

  g_object_unref (iq);
}

gboolean
wocky_roster_remove_contact_finish (WockyRoster *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), wocky_roster_remove_contact_finish), FALSE);

  return TRUE;
}

void
wocky_roster_change_contact_name_async (WockyRoster *self,
    WockyContact *contact,
    const gchar *name,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  WockyXmppStanza *iq;
  WockyXmppNode *item;
  GSimpleAsyncResult *result;

  g_return_if_fail (contact != NULL);

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", wocky_contact_get_jid (contact));
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_change_contact_name_finish);

  if (!wocky_strdiff (wocky_contact_get_name (contact), name))
    {
      DEBUG ("No need to change name; complete immediately");
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  iq = build_iq_for_contact (contact, &item);

  /* set new name */
  wocky_xmpp_node_set_attribute (item, "name", name);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, result);

  g_object_unref (iq);
}

gboolean
wocky_roster_change_contact_name_finish (WockyRoster *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), wocky_roster_change_contact_name_finish),
      FALSE);

  return TRUE;
}

void
wocky_roster_contact_add_group_async (WockyRoster *self,
    WockyContact *contact,
    const gchar *group,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  WockyXmppStanza *iq;
  WockyXmppNode *item, *group_node;
  GSimpleAsyncResult *result;

  g_return_if_fail (contact != NULL);

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", wocky_contact_get_jid (contact));
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_contact_add_group_finish);

  if (wocky_contact_in_group (contact, group))
    {
      DEBUG ("Contact %s in already in group %s; complete immediately",
          wocky_contact_get_jid (contact), group);
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  iq = build_iq_for_contact (contact, &item);

  /* add new group */
  group_node = wocky_xmpp_node_add_child (item, "group");
  wocky_xmpp_node_set_content (group_node, group);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, result);

  g_object_unref (iq);
}

gboolean
wocky_roster_contact_add_group_finish (WockyRoster *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), wocky_roster_contact_add_group_finish),
      FALSE);

  return TRUE;
}

void
wocky_roster_contact_remove_group_async (WockyRoster *self,
    WockyContact *contact,
    const gchar *group,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = WOCKY_ROSTER_GET_PRIVATE (self);
  WockyXmppStanza *iq;
  WockyXmppNode *item;
  GSimpleAsyncResult *result;
  GSList *l;

  g_return_if_fail (contact != NULL);

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", wocky_contact_get_jid (contact));
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_contact_remove_group_finish);

  if (!wocky_contact_in_group (contact, group))
    {
      DEBUG ("Contact %s is not in group %s; complete immediately",
          wocky_contact_get_jid (contact), group);
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  iq = build_iq_for_contact (contact, &item);

  /* remove the group */
  /* FIXME: should we add a wocky_xmpp_node_remove_child () ? */
  for (l = item->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *group_node = (WockyXmppNode *) l->data;

      if (wocky_strdiff (group_node->name, "group"))
        continue;

      if (!wocky_strdiff (group_node->content, group))
        {
          wocky_xmpp_node_free (group_node);
          item->children = g_slist_delete_link (item->children, l);
          break;
        }
    }

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, result);

  g_object_unref (iq);
}

gboolean
wocky_roster_contact_remove_group_finish (WockyRoster *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), wocky_roster_contact_remove_group_finish),
      FALSE);

  return TRUE;
}
