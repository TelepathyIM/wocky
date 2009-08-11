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
  LAST_SIGNAL,
};

/*
static guint signals[LAST_SIGNAL] = {0};
*/

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

static gboolean
roster_update (WockyRoster *self,
    WockyXmppStanza *stanza,
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
          WOCKY_ROSTER_INVALID_STANZA,
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
      else
        subscription_type = WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE;

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
              wocky_xmpp_node_get_attribute (n, "name"), NULL);

          wocky_contact_set_subscription (contact,
              subscription_type, NULL);

          wocky_contact_set_groups (contact, groups, NULL);
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

  if (!roster_update (self, stanza, &error))
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

  if (!roster_update (self, iq, &error))
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
      result = g_slist_prepend (result, value);
    }

  return result;
}
