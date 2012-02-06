/*
 * wocky-bare-contact.c - Source for WockyBareContact
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
 * SECTION: wocky-bare-contact
 * @title: WockyBareContact
 * @short_description: Wrapper around a roster item.
 * @include: wocky/wocky-bare-contact.h
 *
 * Stores information regarding a roster item and provides a higher level API
 * for altering its details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-bare-contact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <gio/gio.h>

#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_ROSTER
#include "wocky-debug-internal.h"

G_DEFINE_TYPE (WockyBareContact, wocky_bare_contact, WOCKY_TYPE_CONTACT)

/* properties */
enum
{
  PROP_JID = 1,
  PROP_NAME,
  PROP_SUBSCRIPTION,
  PROP_GROUPS,
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
struct _WockyBareContactPrivate
{
  gboolean dispose_has_run;

  gchar *jid;
  gchar *name;
  WockyRosterSubscriptionFlags subscription;
  gchar **groups;

  /* list of weak reffed (WockyResourceContact *)
   * Each WockyResourceContact has a ref on its WockyBareContact so we don't
   * have to keep a ref on it. */
  GSList *resources;
};

static void
wocky_bare_contact_init (WockyBareContact *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      WOCKY_TYPE_BARE_CONTACT, WockyBareContactPrivate);

  self->priv->resources = NULL;
}

static void
wocky_bare_contact_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyBareContact *self = WOCKY_BARE_CONTACT (object);
  WockyBareContactPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_JID:
      priv->jid = g_value_dup_string (value);
      break;
    case PROP_NAME:
      wocky_bare_contact_set_name (WOCKY_BARE_CONTACT (object),
          g_value_get_string (value));
      break;
    case PROP_SUBSCRIPTION:
      priv->subscription = g_value_get_uint (value);
      break;
    case PROP_GROUPS:
      priv->groups = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_bare_contact_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyBareContact *self = WOCKY_BARE_CONTACT (object);
  WockyBareContactPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_JID:
      g_value_set_string (value, priv->jid);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_SUBSCRIPTION:
      g_value_set_uint (value, priv->subscription);
      break;
    case PROP_GROUPS:
      g_value_set_boxed (value, priv->groups);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_bare_contact_constructed (GObject *object)
{
  WockyBareContact *self = WOCKY_BARE_CONTACT (object);
  WockyBareContactPrivate *priv = self->priv;

  g_assert (priv->jid != NULL);
}

static void
resource_disposed_cb (gpointer user_data,
    GObject *resource)
{
  WockyBareContact *self = WOCKY_BARE_CONTACT (user_data);
  WockyBareContactPrivate *priv = self->priv;

  priv->resources = g_slist_remove (priv->resources, resource);
}

static void
wocky_bare_contact_dispose (GObject *object)
{
  WockyBareContact *self = WOCKY_BARE_CONTACT (object);
  WockyBareContactPrivate *priv = self->priv;
  GSList *l;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  for (l = priv->resources; l != NULL; l = g_slist_next (l))
    {
      g_object_weak_unref (G_OBJECT (l->data), resource_disposed_cb, self);
    }

  if (G_OBJECT_CLASS (wocky_bare_contact_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_bare_contact_parent_class)->dispose (object);
}

static void
wocky_bare_contact_finalize (GObject *object)
{
  WockyBareContact *self = WOCKY_BARE_CONTACT (object);
  WockyBareContactPrivate *priv = self->priv;

  if (priv->jid != NULL)
    g_free (priv->jid);

  if (priv->name != NULL)
    g_free (priv->name);

  if (priv->groups != NULL)
    g_strfreev (priv->groups);

  g_slist_free (priv->resources);

  G_OBJECT_CLASS (wocky_bare_contact_parent_class)->finalize (object);
}

static gchar *
bare_contact_dup_jid (WockyContact *contact)
{
  return g_strdup (wocky_bare_contact_get_jid (WOCKY_BARE_CONTACT (contact)));
}

static void
wocky_bare_contact_class_init (WockyBareContactClass *wocky_bare_contact_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_bare_contact_class);
  WockyContactClass *contact_class = WOCKY_CONTACT_CLASS (wocky_bare_contact_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_bare_contact_class,
      sizeof (WockyBareContactPrivate));

  object_class->constructed = wocky_bare_contact_constructed;
  object_class->set_property = wocky_bare_contact_set_property;
  object_class->get_property = wocky_bare_contact_get_property;
  object_class->dispose = wocky_bare_contact_dispose;
  object_class->finalize = wocky_bare_contact_finalize;

  contact_class->dup_jid = bare_contact_dup_jid;

  /**
   * WockyBareContact:jid:
   *
   * The contact's bare JID, according to the roster.
   */
  spec = g_param_spec_string ("jid", "Contact JID",
      "Contact JID",
      "",
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_JID, spec);

  /**
   * WockyBareContact:name:
   *
   * The contact's name, according to the roster.
   */
  spec = g_param_spec_string ("name", "Contact Name",
      "Contact Name",
      "",
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, spec);

  /**
   * WockyBareContact:subscription:
   *
   * The subscription type of the contact, according to the roster.
   */
  spec = g_param_spec_uint ("subscription", "Contact Subscription",
      "Contact Subscription",
      0,
      LAST_WOCKY_ROSTER_SUBSCRIPTION_TYPE,
      WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SUBSCRIPTION, spec);

  /**
   * WockyBareContact:groups:
   *
   * A list of the contact's groups, according to the roster.
   */
  spec = g_param_spec_boxed ("groups", "Contact Groups",
      "Contact Groups",
      G_TYPE_STRV,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_GROUPS, spec);
}

/**
 * wocky_bare_contact_new:
 * @jid: the JID of the contact to create
 *
 * Creates a new #WockyBareContact for a given JID.
 *
 * Returns: a newly constructed #WockyBareContact
 */

WockyBareContact *
wocky_bare_contact_new (const gchar *jid)
{
  return g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", jid,
      NULL);
}

/**
 * wocky_bare_contact_get_jid:
 * @contact: a #WockyBareContact instance
 *
 * Returns the JID of the contact wrapped by @contact.
 *
 * Returns: @contact's JID.
 */
const gchar *
wocky_bare_contact_get_jid (WockyBareContact *contact)
{
  WockyBareContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_BARE_CONTACT (contact), NULL);

  priv = contact->priv;

  return priv->jid;
}

/**
 * wocky_bare_contact_get_name:
 * @contact: #WockyBareContact instance
 *
 * Returns the name of the contact wrapped by @contact.
 *
 * Returns: @contact's name
 */
const gchar *
wocky_bare_contact_get_name (WockyBareContact *contact)
{
  WockyBareContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_BARE_CONTACT (contact), NULL);

  priv = contact->priv;

  return priv->name;
}

/* FIXME: document that wocky_bare_contact_set_* shouldn't be used by users */

/**
 * wocky_bare_contact_set_name:
 * @contact: a #WockyBareContact instance
 * @name: the name to set @contact
 *
 * Sets @contact's name to @name.
 *
 */
void
wocky_bare_contact_set_name (WockyBareContact *contact,
    const gchar *name)
{
  WockyBareContactPrivate *priv;

  g_return_if_fail (WOCKY_IS_BARE_CONTACT (contact));

  priv = contact->priv;

  if (!wocky_strdiff (priv->name, name))
    return;

  g_free (priv->name);
  priv->name = g_strdup (name);
  g_object_notify (G_OBJECT (contact), "name");
}

/**
 * wocky_bare_contact_get_subscription:
 * @contact: a #WockyBareContact instance
 *
 * Gets the subscription type @contact has.
 *
 * Returns: @contact's subscription.
 */
WockyRosterSubscriptionFlags
wocky_bare_contact_get_subscription (WockyBareContact *contact)
{
  WockyBareContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_BARE_CONTACT (contact),
      WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE);
  priv = contact->priv;

  return priv->subscription;
}

/**
 * wocky_bare_contact_set_subscription:
 * @contact: a #WockyBareContact instance
 * @subscription: the new subscription type
 *
 * Sets the subscription of @contact.
 *
 */
void
wocky_bare_contact_set_subscription (WockyBareContact *contact,
    WockyRosterSubscriptionFlags subscription)
{
  WockyBareContactPrivate *priv;

  g_return_if_fail (WOCKY_IS_BARE_CONTACT (contact));

  priv = contact->priv;

  if (priv->subscription == subscription)
    return;

  priv->subscription = subscription;
  g_object_notify (G_OBJECT (contact), "subscription");
}

/**
 * wocky_bare_contact_get_groups:
 * @contact: a #WockyBareContact instance
 *
 * Returns the list of the groups of @contact.
 *
 * Returns: a list of @contact's groups
 */
const gchar * const *
wocky_bare_contact_get_groups (WockyBareContact *contact)
{
  WockyBareContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_BARE_CONTACT (contact), NULL);
  priv = contact->priv;

  return (const gchar * const *) priv->groups;
}

static gint
cmp_str (gchar **a,
    gchar **b)
{
  return strcmp (*a, *b);
}

static GPtrArray *
sort_groups (GStrv groups)
{
  GPtrArray *arr;
  guint i;

  arr = g_ptr_array_sized_new (g_strv_length (groups));
  for (i = 0; groups[i] != NULL; i++)
    {
      g_ptr_array_add (arr, groups[i]);
    }

  g_ptr_array_sort (arr, (GCompareFunc) cmp_str);

  return arr;
}

static gboolean
groups_equal (const gchar * const * groups_a,
  const gchar * const * groups_b)
{
  GPtrArray *arr_a, *arr_b;
  guint i;
  gboolean result = TRUE;

  if (groups_a == NULL && groups_b == NULL)
    return TRUE;

  if (groups_a == NULL || groups_b == NULL)
    return FALSE;

  if (g_strv_length ((GStrv) groups_a) != g_strv_length ((GStrv) groups_b))
    return FALSE;

  /* Sort both groups array and then compare elements one by one */
  arr_a = sort_groups ((GStrv) groups_a);
  arr_b = sort_groups ((GStrv) groups_b);

  for (i = 0; i != arr_a->len && result; i++)
    {
      const gchar *a = g_ptr_array_index (arr_a, i);
      const gchar *b = g_ptr_array_index (arr_b, i);

      if (wocky_strdiff (a, b))
        result = FALSE;
    }

  g_ptr_array_unref (arr_a);
  g_ptr_array_unref (arr_b);
  return result;
}

/**
 * wocky_bare_contact_set_groups:
 * @contact: a #WockyBareContact instance
 * @groups: a list of groups
 *
 * Sets @contact's groups.
 *
 */
void
wocky_bare_contact_set_groups (WockyBareContact *contact,
    gchar **groups)
{
  WockyBareContactPrivate *priv;

  g_return_if_fail (WOCKY_IS_BARE_CONTACT (contact));

  priv = contact->priv;

  if (groups_equal ((const gchar * const *) groups,
        (const gchar * const *) priv->groups))
    return;

  if (priv->groups != NULL)
    g_strfreev (priv->groups);

  priv->groups = g_strdupv (groups);
  g_object_notify (G_OBJECT (contact), "groups");
}

/**
 * wocky_bare_contact_equal:
 * @a: a #WockyBareContact instance
 * @b: a #WockyBareContact instance to compare with @a
 *
 * Compares whether two #WockyBareContact instances refer to the same
 * roster item.
 *
 * Returns: #TRUE if the two contacts match.
 */
gboolean
wocky_bare_contact_equal (WockyBareContact *a,
    WockyBareContact *b)
{
  const gchar * const * groups_a;
  const gchar * const * groups_b;

  if (a == NULL || b == NULL)
    return FALSE;

  if (wocky_strdiff (wocky_bare_contact_get_jid (a),
        wocky_bare_contact_get_jid (b)))
    return FALSE;

  if (wocky_strdiff (wocky_bare_contact_get_name (a),
        wocky_bare_contact_get_name (b)))
    return FALSE;

  if (wocky_bare_contact_get_subscription (a) !=
      wocky_bare_contact_get_subscription (b))
    return FALSE;

  groups_a = wocky_bare_contact_get_groups (a);
  groups_b = wocky_bare_contact_get_groups (b);

  return groups_equal (groups_a, groups_b);
}

/**
 * wocky_bare_contact_add_group:
 * @contact: a #WockyBareContact instance
 * @group: a group
 *
 * Adds @group to contact's groups.
 */
void
wocky_bare_contact_add_group (WockyBareContact *self,
    const gchar *group)
{
  WockyBareContactPrivate *priv = self->priv;
  GPtrArray *arr;
  gboolean group_already_present = FALSE;

  if (priv->groups != NULL)
    {
      guint len, i;

      len = g_strv_length (priv->groups);
      arr = g_ptr_array_sized_new (len + 2);

      for (i = 0; priv->groups[i] != NULL; i++)
        {
          g_ptr_array_add (arr, g_strdup (priv->groups[i]));

          if (!wocky_strdiff (priv->groups[i], group))
            /* Don't add the group twice */
            group_already_present = TRUE;
        }

      g_strfreev (priv->groups);
    }
  else
    {
      arr = g_ptr_array_sized_new (2);
    }

  if (!group_already_present)
    g_ptr_array_add (arr, g_strdup (group));

  /* Add trailing NULL */
  g_ptr_array_add (arr, NULL);

  priv->groups = (GStrv) g_ptr_array_free (arr, FALSE);
}

/**
 * wocky_bare_contact_in_group:
 * @contact: a #WockyBareContact instance
 * @group: a group
 *
 * Determines whether the given contact is in @group.
 *
 * Returns: #TRUE if the contact is in the given group.
 */
gboolean
wocky_bare_contact_in_group (WockyBareContact *self,
    const gchar *group)
{
  WockyBareContactPrivate *priv = self->priv;
  guint i;

  if (priv->groups == NULL)
    return FALSE;

  for (i = 0; priv->groups[i] != NULL; i++)
    {
      if (!wocky_strdiff (priv->groups[i], group))
        return TRUE;
    }

  return FALSE;
}

/**
 * wocky_bare_contact_remove_group:
 * @contact: a #WockyBareContact instance
 * @group: a group
 *
 * Removes @group from the contact's groups.
 */
void
wocky_bare_contact_remove_group (WockyBareContact *self,
    const gchar *group)
{
  WockyBareContactPrivate *priv = self->priv;
  GPtrArray *arr;
  guint len, i;

  if (priv->groups == NULL)
    return;

  len = g_strv_length (priv->groups);
  arr = g_ptr_array_sized_new (len);

  for (i = 0; priv->groups[i] != NULL; i++)
    {
      if (!wocky_strdiff (priv->groups[i], group))
        continue;

      g_ptr_array_add (arr, g_strdup (priv->groups[i]));
    }

  g_strfreev (priv->groups);

  /* Add trailing NULL */
  g_ptr_array_add (arr, NULL);

  priv->groups = (GStrv) g_ptr_array_free (arr, FALSE);
}

/**
 * wocky_bare_contact_copy:
 * @contact: a #WockyBareContact instance
 *
 * Convenience function to obtain a copy of the given #WockyBareContact.
 *
 * Returns: a newly created #WockyBareContact which is a copy of the given
 * one.
 */
WockyBareContact *
wocky_bare_contact_copy (WockyBareContact *contact)
{
  return g_object_new (WOCKY_TYPE_BARE_CONTACT,
      "jid", wocky_bare_contact_get_jid (contact),
      "name", wocky_bare_contact_get_name (contact),
      "subscription", wocky_bare_contact_get_subscription (contact),
      "groups", wocky_bare_contact_get_groups (contact),
      NULL);
}

/**
 * wocky_bare_contact_debug_print:
 * @contact: a #WockyBareContact instance
 *
 * Prints debug information for the given #WockyBareContact.
 */
void
wocky_bare_contact_debug_print (WockyBareContact *self)
{
  WockyBareContactPrivate *priv = self->priv;
  guint i;

  DEBUG ("Contact: %s  Name: %s  Subscription: %s  Groups:",
      priv->jid, priv->name,
      wocky_roster_subscription_to_string (priv->subscription));

  for (i = 0; priv->groups[i] != NULL; i++)
    DEBUG ("  - %s", priv->groups[i]);
}

/**
 * wocky_bare_contact_add_resource:
 * @contact: a #WockyBareContact instance
 * @resource: a #WockyResourceContact instance
 *
 * Adds @resource to the contact's resources.
 * The #WockyBareContact instance doesn't assume a reference to @resource.
 */
void
wocky_bare_contact_add_resource (WockyBareContact *self,
    WockyResourceContact *resource)
{
  WockyBareContactPrivate *priv = self->priv;

  g_object_weak_ref (G_OBJECT (resource), resource_disposed_cb, self);
  priv->resources = g_slist_append (priv->resources, resource);
}

/**
 * wocky_bare_contact_get_resources:
 * @contact: a #WockyBareContact instance
 *
 * Gets a #GSList of all the contact's resources.
 * You should call #g_slist_free on the list when done with it.
 *
 * Returns: a #GSList of #WockyResourceContact objects.
 */
GSList *
wocky_bare_contact_get_resources (WockyBareContact *self)
{
  WockyBareContactPrivate *priv = self->priv;

  return g_slist_copy (priv->resources);
}
