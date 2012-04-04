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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gio/gio.h>

#include "wocky-roster.h"

#include "wocky-bare-contact.h"
#include "wocky-c2s-porter.h"
#include "wocky-namespaces.h"
#include "wocky-stanza.h"
#include "wocky-utils.h"
#include "wocky-signals-marshal.h"
#include "wocky-contact-factory.h"
#include "wocky-porter.h"
#include "wocky-session.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_ROSTER
#include "wocky-debug-internal.h"

#define GOOGLE_ROSTER_VERSION "2"

G_DEFINE_TYPE (WockyRoster, wocky_roster, G_TYPE_OBJECT)

typedef struct
{
  WockyRoster *self;

  /* The result of the already-sent operation */
  /* List of GSimpleAsyncResult to which an IQ has been sent to the server and
   * that will be completed once we receive the reply of this IQ. */
  GSList *flying_operations;

  gchar *jid;

  /* The future name of the contact, or NULL to not change it */
  gchar *new_name;
  /* (const gchar *) => TRUE
   * Each key is a group to add to the contact */
  GHashTable *groups_to_add;
  /* owned (gchar *) => TRUE
   * Each key is a group to remove from the contact */
  GHashTable *groups_to_remove;
  /* TRUE if a 'add' operation is waiting */
  gboolean add_contact;
  /* TRUE if a 'remove' operation is waiting */
  gboolean remove_contact;

  /* List of GSimpleAsyncResult that will be completed once the changes stored
   * in this PendingOperation structure will have be completed */
  GSList *waiting_operations;
} PendingOperation;

static PendingOperation *
pending_operation_new (WockyRoster *self,
    GSimpleAsyncResult *result,
    const gchar *jid)
{
  PendingOperation *pending = g_slice_new0 (PendingOperation);

  g_assert (self != NULL);
  g_assert (result != NULL);
  g_assert (jid != NULL);

  pending->self = g_object_ref (self);
  pending->flying_operations = g_slist_append (pending->flying_operations,
      result);
  pending->jid = g_strdup (jid);

  pending->groups_to_add = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
  pending->groups_to_remove = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  return pending;
}

static void
pending_operation_free (PendingOperation *pending)
{
  g_object_unref (pending->self);
  g_free (pending->new_name);
  g_free (pending->jid);

  g_slist_foreach (pending->flying_operations, (GFunc) g_object_unref, NULL);
  g_slist_free (pending->flying_operations);
  g_slist_foreach (pending->waiting_operations, (GFunc) g_object_unref, NULL);
  g_slist_free (pending->waiting_operations);

  g_hash_table_unref (pending->groups_to_add);
  g_hash_table_unref (pending->groups_to_remove);

  g_slice_free (PendingOperation, pending);
}

static void
pending_operation_set_new_name (PendingOperation *pending,
    const gchar *name)
{
  g_free (pending->new_name);
  pending->new_name = g_strdup (name);
}

static void
pending_operation_set_groups (PendingOperation *pending,
    GStrv groups)
{
  guint i;

  g_hash_table_remove_all (pending->groups_to_add);
  g_hash_table_remove_all (pending->groups_to_remove);

  for (i = 0; groups[i] != NULL; i++)
    g_hash_table_insert (pending->groups_to_add, g_strdup (groups[i]),
          GUINT_TO_POINTER (TRUE));
}

static void
pending_operation_add_group (PendingOperation *pending,
    const gchar *group)
{
  g_hash_table_insert (pending->groups_to_add, g_strdup (group),
      GUINT_TO_POINTER (TRUE));
  g_hash_table_remove (pending->groups_to_remove, group);
}

static void
pending_operation_remove_group (PendingOperation *pending,
    const gchar *group)
{
  g_hash_table_insert (pending->groups_to_remove, g_strdup (group),
      GUINT_TO_POINTER (TRUE));
  g_hash_table_remove (pending->groups_to_add, group);
}

static void
pending_operation_set_add (PendingOperation *pending)
{
  pending->add_contact = TRUE;
  pending->remove_contact = FALSE;
}

static void
pending_operation_set_remove (PendingOperation *pending)
{
  pending->add_contact = FALSE;
  pending->remove_contact = TRUE;
}

static void
pending_operation_add_waiting_operation (PendingOperation *pending,
    GSimpleAsyncResult *result)
{
  pending->waiting_operations = g_slist_append (pending->waiting_operations,
      result);
}

static gboolean
pending_operation_has_changes (PendingOperation *pending)
{
  if (pending->new_name != NULL)
    return TRUE;

  if (g_hash_table_size (pending->groups_to_add) > 0)
    return TRUE;

  if (g_hash_table_size (pending->groups_to_remove) > 0)
    return TRUE;

  if (pending->add_contact)
    return TRUE;

  if (pending->remove_contact)
    return TRUE;

  return FALSE;
}

/* Called when the flying operations have been completed and the IQ to complete
 * the waiting operations has been sent. */
static void
pending_operation_reset (PendingOperation *pending)
{
  /* Free old flying operations */
  g_slist_foreach (pending->flying_operations, (GFunc) g_object_unref, NULL);
  g_slist_free (pending->flying_operations);

  /* Previously waiting operations are now flying */
  pending->flying_operations = pending->waiting_operations;
  pending->waiting_operations = NULL;
}

/* properties */
enum
{
  PROP_SESSION = 1,
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
struct _WockyRosterPrivate
{
  WockySession *session;
  WockyPorter *porter;
  WockyContactFactory *contact_factory;

  /* owned (gchar *) => reffed (WockyBareContact *) */
  GHashTable *items;
  guint iq_cb;

  /* owned (gchar *) => owned (PendingOperation *)
   * When an edit attempt is "in-flight", we store a PendingOperation * in
   * this hash table which will be used to store required edits until the one
   * we already sent is acknowledged. This prevents some race conditions. */
  GHashTable *pending_operations;

  GSimpleAsyncResult *fetch_result;

  gboolean dispose_has_run;
};

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

static void change_roster_iq_cb (GObject *source_object,
    GAsyncResult *send_iq_res,
    gpointer user_data);

static void
wocky_roster_init (WockyRoster *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_ROSTER,
      WockyRosterPrivate);
}

static void
wocky_roster_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_SESSION:
      /* Don't keep a ref on the session at the session keeps a ref on the
       * roster */
      priv->session = g_value_get_object (value);
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
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_SESSION:
      g_value_set_object (value, priv->session);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

const gchar *
wocky_roster_subscription_to_string (WockyRosterSubscriptionFlags subscription)
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
  WockyRosterPrivate *priv = self->priv;
  WockyBareContact *contact;

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
    WockyStanza *stanza,
    gboolean fire_signals,
    GError **error)
{
  WockyRosterPrivate *priv = self->priv;
  WockyNode *query_node;
  GSList *j;

  /* Check stanza contains query node. */
  query_node = wocky_node_get_child_ns (
      wocky_stanza_get_top_node (stanza), "query", WOCKY_XMPP_NS_ROSTER);

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
      WockyNode *n = (WockyNode *) j->data;
      WockyBareContact *contact = NULL;
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

      jid = wocky_node_get_attribute (n, "jid");

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
      subscription = wocky_node_get_attribute (n, "subscription");

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
          WockyNode *node = (WockyNode *) l->data;

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
          wocky_bare_contact_set_name (contact,
              wocky_node_get_attribute (n, "name"));

          wocky_bare_contact_set_subscription (contact, subscription_type);

          wocky_bare_contact_set_groups (contact, groups);
        }
      else
        {
          /* Create a new contact. */
          contact = wocky_contact_factory_ensure_bare_contact (
              priv->contact_factory, jid);

          g_object_set (contact,
              "name", wocky_node_get_attribute (n, "name"),
              "subscription", subscription_type,
              "groups", groups,
              NULL);

          g_hash_table_insert (priv->items, g_strdup (jid), contact);

          DEBUG ("New contact added:");
          wocky_bare_contact_debug_print (contact);

          if (fire_signals)
            g_signal_emit (self, signals[ADDED], 0, contact);
        }

      g_strfreev (groups);
    }

  return TRUE;
}

static gboolean
roster_iq_handler_set_cb (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data)
{
  WockyRoster *self = WOCKY_ROSTER (user_data);
  GError *error = NULL;
  WockyStanza *reply;

  if (!roster_update (self, stanza, TRUE, &error))
    {
      DEBUG ("Failed to update roster: %s",
          error ? error->message : "no message");
      g_error_free (error);
      reply = wocky_stanza_build_iq_error (stanza, NULL);
    }
  else
    {
      /* ack */
      reply = wocky_stanza_build_iq_result (stanza, NULL);
    }

  if (reply != NULL)
    {
      wocky_porter_send (porter, reply);
      g_object_unref (reply);
    }

  return TRUE;
}

static void
wocky_roster_constructed (GObject *object)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = self->priv;

  priv->items = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);

  priv->pending_operations = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) pending_operation_free);

  g_assert (priv->session != NULL);

  priv->porter = wocky_session_get_porter (priv->session);
  g_assert (priv->porter != NULL);
  g_object_ref (priv->porter);

  priv->iq_cb = wocky_c2s_porter_register_handler_from_server (
      WOCKY_C2S_PORTER (priv->porter),
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, roster_iq_handler_set_cb, self,
      '(', "query",
        ':', WOCKY_XMPP_NS_ROSTER,
      ')', NULL);

  priv->contact_factory = wocky_session_get_contact_factory (priv->session);
  g_assert (priv->contact_factory != NULL);
  g_object_ref (priv->contact_factory);
}

static void
wocky_roster_dispose (GObject *object)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->iq_cb != 0)
    {
      wocky_porter_unregister_handler (priv->porter, priv->iq_cb);
      priv->iq_cb = 0;
    }

  g_object_unref (priv->porter);
  g_object_unref (priv->contact_factory);

  if (G_OBJECT_CLASS (wocky_roster_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_roster_parent_class)->dispose (object);
}

static void
wocky_roster_finalize (GObject *object)
{
  WockyRoster *self = WOCKY_ROSTER (object);
  WockyRosterPrivate *priv = self->priv;

  g_hash_table_unref (priv->items);
  g_hash_table_unref (priv->pending_operations);

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

  spec = g_param_spec_object ("session", "Wocky session",
    "the wocky session used by this roster",
    WOCKY_TYPE_SESSION,
    G_PARAM_READWRITE |
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SESSION, spec);

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
wocky_roster_new (WockySession *session)
{
  g_return_val_if_fail (WOCKY_IS_SESSION (session), NULL);

  return g_object_new (WOCKY_TYPE_ROSTER,
      "session", session,
      NULL);
}

static void
roster_fetch_roster_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  WockyStanza *iq;
  WockyRoster *self = WOCKY_ROSTER (user_data);
  WockyRosterPrivate *priv = self->priv;

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
  WockyStanza *iq;

  g_return_if_fail (WOCKY_IS_ROSTER (self));

  priv = self->priv;

  if (priv->fetch_result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, G_IO_ERROR, G_IO_ERROR_PENDING,
          "Another fetch operation is pending");
      return;
    }

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET, NULL, NULL,
        '(', "query",
          ':', WOCKY_XMPP_NS_ROSTER,
        ')',
      NULL);

  priv->fetch_result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_fetch_roster_async);

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
          G_OBJECT (self), wocky_roster_fetch_roster_async), FALSE);

  return TRUE;
}

WockyBareContact *
wocky_roster_get_contact (WockyRoster *self,
    const gchar *jid)
{
  WockyRosterPrivate *priv = self->priv;

  return g_hash_table_lookup (priv->items, jid);
}

GSList *
wocky_roster_get_all_contacts (WockyRoster *self)
{
  WockyRosterPrivate *priv = self->priv;
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

/* Build an IQ set stanza containing the current state of the contact.
 * If not NULL, item_node will contain a pointer on the "item" node */
static WockyStanza *
build_iq_for_contact (WockyBareContact *contact,
    WockyNode **item_node)
{
  WockyStanza *iq;
  WockyNode *item = NULL;
  const gchar *jid, *name;
  const gchar * const *groups;
  guint i;
  WockyRosterSubscriptionFlags subscription;

  jid = wocky_bare_contact_get_jid (contact);
  g_return_val_if_fail (jid != NULL, NULL);

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
        '(', "query",
          ':', WOCKY_XMPP_NS_ROSTER,
          '(', "item",
            '*', &item,
            '@', "jid", jid,
          ')',
        ')',
      NULL);

  g_assert (item != NULL);

  name = wocky_bare_contact_get_name (contact);
  if (name != NULL)
    {
      wocky_node_set_attribute (item, "name", name);
    }

  subscription = wocky_bare_contact_get_subscription (contact);
  if (subscription != WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE)
    {
      wocky_node_set_attribute (item, "subscription",
          wocky_roster_subscription_to_string (subscription));
    }

  groups = wocky_bare_contact_get_groups (contact);
  for (i = 0; groups != NULL && groups[i] != NULL; i++)
    {
      WockyNode *group;

      group = wocky_node_add_child (item, "group");
      wocky_node_set_content (group, groups[i]);
    }

  if (item_node != NULL)
    *item_node = item;

  return iq;
}

static WockyStanza *
build_remove_contact_iq (WockyBareContact *contact)
{
  return wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
        '(', "query",
          ':', WOCKY_XMPP_NS_ROSTER,
          '(', "item",
            '@', "jid", wocky_bare_contact_get_jid (contact),
            '@', "subscription", "remove",
          ')',
        ')',
      NULL);
}

static WockyStanza *
build_iq_for_pending (WockyRoster *self,
    PendingOperation *pending)
{
  WockyRosterPrivate *priv = self->priv;
  WockyBareContact *contact, *tmp;
  WockyStanza *iq;
  GHashTableIter iter;
  gpointer group;

  contact = g_hash_table_lookup (priv->items, pending->jid);

  if (!pending_operation_has_changes (pending))
    {
      /* Nothing to change */
      return NULL;
    }

  /* There is no sense to try to add and remove a contact at the same time */
  g_assert (!pending->add_contact || !pending->remove_contact);

  if (contact == NULL)
    {
      if (pending->remove_contact)
        {
          DEBUG ("Contact %s was already removed", pending->jid);
          return NULL;
        }
      else if (!pending->add_contact)
        {
          GSList *l;

          DEBUG ("contact is not in the roster any more");

          for (l = pending->waiting_operations; l != NULL; l = g_slist_next (l))
            {
              GSimpleAsyncResult *result = (GSimpleAsyncResult *) l->data;

              g_simple_async_result_set_error (result, WOCKY_ROSTER_ERROR,
                  WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
                  "Contact %s is not in the roster any more", pending->jid);
            }

          return NULL;
        }

      tmp = g_object_new (WOCKY_TYPE_BARE_CONTACT,
          "jid", pending->jid,
          NULL);
    }
  else
    {
      if (pending->remove_contact)
        {
          DEBUG ("Remove contact %s", pending->jid);
          return build_remove_contact_iq (contact);
        }

      tmp = wocky_bare_contact_copy (contact);
    }

  if (pending->new_name != NULL)
    wocky_bare_contact_set_name (tmp, pending->new_name);

  g_hash_table_iter_init (&iter, pending->groups_to_add);
  while (g_hash_table_iter_next (&iter, &group, NULL))
    {
      wocky_bare_contact_add_group (tmp, (const gchar *) group);
    }

  g_hash_table_iter_init (&iter, pending->groups_to_remove);
  while (g_hash_table_iter_next (&iter, &group, NULL))
    {
      wocky_bare_contact_remove_group (tmp, (const gchar *) group);
    }

  if (wocky_bare_contact_equal (contact, tmp))
    {
      DEBUG ("No change needed");
      g_object_unref (tmp);
      return NULL;
    }

  iq = build_iq_for_contact (tmp, NULL);
  g_object_unref (tmp);

  return iq;
}

static void
waiting_operations_completed (WockyRoster *self,
    PendingOperation *pending)
{
  GSList *l;

  for (l = pending->waiting_operations; l != NULL; l = g_slist_next (l))
    {
      GSimpleAsyncResult *result = (GSimpleAsyncResult *) l->data;

      g_simple_async_result_complete (result);
    }
}

static void
flying_operation_completed (PendingOperation *pending,
    GError *error)
{
  WockyRoster *self = pending->self;
  WockyRosterPrivate *priv = self->priv;
  WockyStanza *iq;
  GSList *l;

  /* Flying operations are completed */
  for (l = pending->flying_operations; l != NULL; l = g_slist_next (l))
    {
      GSimpleAsyncResult *result = (GSimpleAsyncResult *) l->data;

      if (error != NULL)
        g_simple_async_result_set_from_error (result, error);

      g_simple_async_result_complete (result);
    }

  if (g_slist_length (pending->waiting_operations) == 0)
    {
      /* No waiting operation, we are done */
      DEBUG ("No waiting operations");
      g_hash_table_remove (priv->pending_operations, pending->jid);
      return;
    }

  iq = build_iq_for_pending (self, pending);
  if (iq == NULL)
    {
      /* No need to send an IQ; complete waiting operations right now */
      DEBUG ("No need to send an IQ; complete waiting operations");
      waiting_operations_completed (self, pending);
      g_hash_table_remove (priv->pending_operations, pending->jid);
      return;
    }

  pending_operation_reset (pending);

  wocky_porter_send_iq_async (priv->porter,
      iq, NULL, change_roster_iq_cb, pending);

  g_object_unref (iq);
}

static void
change_roster_iq_cb (GObject *source_object,
    GAsyncResult *send_iq_res,
    gpointer user_data)
{
  PendingOperation *pending = (PendingOperation *) user_data;
  WockyStanza *reply;
  GError *error = NULL;

  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source_object),
      send_iq_res, &error);
  if (reply == NULL)
    goto out;

  wocky_stanza_extract_errors (reply, NULL, &error, NULL, NULL);

  /* According to the XMPP RFC, the server has to send a roster upgrade to
   * each client (including the one which requested the change) before
   * replying to the 'set' stanza. We upgraded our list of contacts when this
   * notification has been received.
   * We can't really check that this upgrade has been actually receive as we
   * could receive other notifications from the server due to other clients
   * actions. So we just have to trust the server on this. */

out:
  if (reply != NULL)
    g_object_unref (reply);

  flying_operation_completed (pending, error);

  if (error != NULL)
    g_error_free (error);
}

static PendingOperation *
get_pending_operation (WockyRoster *self,
    const gchar *jid)
{
  WockyRosterPrivate *priv = self->priv;

  DEBUG ("Look for pending operation with contact %s", jid);
  return g_hash_table_lookup (priv->pending_operations, jid);
}

/* Creates a new PendingOperation structure and associates it with the given
 * jid. This function is used when starting a edit operation while there is no
 * flying operation with this contact atm. The PendingOperation won't contain
 * any information as this operation is going to be sent right away and so
 * doesn't have to be queued. */
static PendingOperation *
add_pending_operation (WockyRoster *self,
    const gchar *jid,
    GSimpleAsyncResult *result)
{
  WockyRosterPrivate *priv = self->priv;
  PendingOperation *pending = pending_operation_new (self, result, jid);

  DEBUG ("Add pending operation for %s", jid);
  g_hash_table_insert (priv->pending_operations, g_strdup (jid), pending);
  return pending;
}

void
wocky_roster_add_contact_async (WockyRoster *self,
    const gchar *jid,
    const gchar *name,
    const gchar * const * groups,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = self->priv;
  WockyStanza *iq;
  GSimpleAsyncResult *result;
  WockyBareContact *contact, *existing_contact;
  PendingOperation *pending;

  g_return_if_fail (jid != NULL);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_add_contact_async);

  pending = get_pending_operation (self, jid);
  if (pending != NULL)
    {
      DEBUG ("Another operation is pending for contact %s; queuing this one",
          jid);
      pending_operation_set_new_name (pending, name);
      pending_operation_set_groups (pending, (GStrv) groups);
      pending_operation_add_waiting_operation (pending, result);
      pending_operation_set_add (pending);
      return;
    }

  contact = g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", jid,
      NULL);

  if (name != NULL)
    wocky_bare_contact_set_name (contact, name);

  if (groups != NULL)
    wocky_bare_contact_set_groups (contact, (gchar **) groups);

  existing_contact = g_hash_table_lookup (priv->items, jid);
  if (existing_contact != NULL)
    {
      /* contact is already in the roster. Check if we need to change him. */
      if (wocky_bare_contact_equal (contact, existing_contact))
        {
          DEBUG ("Contact %s is already present in the roster; "
              "no need to change him", jid);
          g_simple_async_result_complete_in_idle (result);
          g_object_unref (contact);
          g_object_unref (result);
          return;
        }
    }

  iq = build_iq_for_contact (contact, NULL);

  pending = add_pending_operation (self, jid, result);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, pending);

  /* A new contact object will be created and added when we'll receive the
   * server push notification. */
  g_object_unref (contact);
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
          G_OBJECT (self), wocky_roster_add_contact_async), FALSE);

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
    WockyBareContact *contact)
{
  WockyRosterPrivate *priv = self->priv;

  return g_hash_table_find (priv->items, is_contact, contact) != NULL;
}

void
wocky_roster_remove_contact_async (WockyRoster *self,
    WockyBareContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = self->priv;
  WockyStanza *iq;
  GSimpleAsyncResult *result;
  PendingOperation *pending;
  const gchar *jid;

  g_return_if_fail (contact != NULL);
  jid = wocky_bare_contact_get_jid (contact);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_remove_contact_async);

  pending = get_pending_operation (self, jid);
  if (pending != NULL)
    {
      DEBUG ("Another operation is pending for contact %s; queuing this one",
          jid);
      pending_operation_set_remove (pending);
      pending_operation_add_waiting_operation (pending, result);
      return;
    }

  if (!contact_in_roster (self, contact))
    {
      DEBUG ("Contact %s is not in the roster", wocky_bare_contact_get_jid (
            contact));
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  pending = add_pending_operation (self, jid, result);

  iq = build_remove_contact_iq (contact);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, pending);

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
          G_OBJECT (self), wocky_roster_remove_contact_async), FALSE);

  return TRUE;
}

void
wocky_roster_change_contact_name_async (WockyRoster *self,
    WockyBareContact *contact,
    const gchar *name,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = self->priv;
  WockyStanza *iq;
  WockyNode *item;
  GSimpleAsyncResult *result;
  PendingOperation *pending;
  const gchar *jid;

  g_return_if_fail (contact != NULL);
  jid = wocky_bare_contact_get_jid (contact);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_change_contact_name_async);

  pending = get_pending_operation (self, jid);
  if (pending != NULL)
    {
      DEBUG ("Another operation is pending for contact %s; queuing this one",
          jid);
      pending_operation_set_new_name (pending, name);
      pending_operation_add_waiting_operation (pending, result);
      return;
    }

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", wocky_bare_contact_get_jid (contact));
      g_object_unref (result);
      return;
    }

  if (!wocky_strdiff (wocky_bare_contact_get_name (contact), name))
    {
      DEBUG ("No need to change name; complete immediately");
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  pending = add_pending_operation (self, jid, result);

  iq = build_iq_for_contact (contact, &item);

  /* set new name */
  wocky_node_set_attribute (item, "name", name);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, pending);

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
        G_OBJECT (self), wocky_roster_change_contact_name_async),
      FALSE);

  return TRUE;
}

void
wocky_roster_contact_add_group_async (WockyRoster *self,
    WockyBareContact *contact,
    const gchar *group,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = self->priv;
  WockyStanza *iq;
  WockyNode *item, *group_node;
  GSimpleAsyncResult *result;
  PendingOperation *pending;
  const gchar *jid;

  g_return_if_fail (contact != NULL);
  jid = wocky_bare_contact_get_jid (contact);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_contact_add_group_async);

  pending = get_pending_operation (self, jid);
  if (pending != NULL)
    {
      DEBUG ("Another operation is pending for contact %s; queuing this one",
          jid);
      pending_operation_add_group (pending, group);
      pending_operation_add_waiting_operation (pending, result);
      return;
    }

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", jid);
      g_object_unref (result);
      return;
    }

  if (wocky_bare_contact_in_group (contact, group))
    {
      DEBUG ("Contact %s in already in group %s; complete immediately",
          wocky_bare_contact_get_jid (contact), group);
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  pending = add_pending_operation (self, jid, result);

  iq = build_iq_for_contact (contact, &item);

  /* add new group */
  group_node = wocky_node_add_child (item, "group");
  wocky_node_set_content (group_node, group);

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, pending);

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
        G_OBJECT (self), wocky_roster_contact_add_group_async),
      FALSE);

  return TRUE;
}

void
wocky_roster_contact_remove_group_async (WockyRoster *self,
    WockyBareContact *contact,
    const gchar *group,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyRosterPrivate *priv = self->priv;
  WockyStanza *iq;
  WockyNode *item;
  GSimpleAsyncResult *result;
  GSList *l;
  PendingOperation *pending;
  const gchar *jid;

  g_return_if_fail (contact != NULL);
  jid = wocky_bare_contact_get_jid (contact);

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_roster_contact_remove_group_async);

  pending = get_pending_operation (self, jid);
  if (pending != NULL)
    {
      DEBUG ("Another operation is pending for contact %s; queuing this one",
          jid);
      pending_operation_remove_group (pending, group);
      pending_operation_add_waiting_operation (pending, result);
      return;
    }

  if (!contact_in_roster (self, contact))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, WOCKY_ROSTER_ERROR, WOCKY_ROSTER_ERROR_NOT_IN_ROSTER,
          "Contact %s is not in the roster", jid);
      g_object_unref (result);
      return;
    }

  if (!wocky_bare_contact_in_group (contact, group))
    {
      DEBUG ("Contact %s is not in group %s; complete immediately", jid, group);
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      return;
    }

  pending = add_pending_operation (self, jid, result);

  iq = build_iq_for_contact (contact, &item);

  /* remove the group */
  /* FIXME: should we add a wocky_node_remove_child () ? */
  for (l = item->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *group_node = (WockyNode *) l->data;

      if (wocky_strdiff (group_node->name, "group"))
        continue;

      if (!wocky_strdiff (group_node->content, group))
        {
          wocky_node_free (group_node);
          item->children = g_slist_delete_link (item->children, l);
          break;
        }
    }

  wocky_porter_send_iq_async (priv->porter,
      iq, cancellable, change_roster_iq_cb, pending);

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
        G_OBJECT (self), wocky_roster_contact_remove_group_async),
      FALSE);

  return TRUE;
}
