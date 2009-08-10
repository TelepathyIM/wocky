/*
 * wocky-contact.c - Source for WockyContact
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
 * SECTION: wocky-contact
 * @title: WockyContact
 * @short_description: Wrapper around a roster item.
 * @include: wocky/wocky-contact.h
 *
 * Stores information regarding a roster item and provides a higher level API
 * for altering its details.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "wocky-contact.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_ROSTER
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyContact, wocky_contact, G_TYPE_OBJECT)

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
typedef struct _WockyContactPrivate WockyContactPrivate;

struct _WockyContactPrivate
{
  gboolean dispose_has_run;

  gchar *jid;
  gchar *name;
  WockyRosterSubscriptionFlags subscription;
  gchar **groups;
};

#define WOCKY_CONTACT_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_CONTACT, \
    WockyContactPrivate))

static void
wocky_contact_init (WockyContact *obj)
{
  /*
  WockyContact *self = WOCKY_CONTACT (obj);
  WockyContactPrivate *priv = WOCKY_CONTACT_GET_PRIVATE (self);
  */
}

static void
wocky_contact_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyContactPrivate *priv =
      WOCKY_CONTACT_GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_JID:
      priv->jid = g_value_dup_string (value);
      break;
    case PROP_NAME:
      wocky_contact_set_name (WOCKY_CONTACT (object),
          g_value_get_string (value), NULL);
      break;
    case PROP_SUBSCRIPTION:
      priv->subscription = g_value_get_uint (value);
      break;
    case PROP_GROUPS:
      priv->groups = g_value_get_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_contact_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyContactPrivate *priv =
      WOCKY_CONTACT_GET_PRIVATE (object);

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
wocky_contact_constructed (GObject *object)
{
  /*
  WockyContact *self = WOCKY_CONTACT (object);
  WockyContactPrivate *priv = WOCKY_CONTACT_GET_PRIVATE (self);
  */
}

static void
wocky_contact_dispose (GObject *object)
{
  WockyContact *self = WOCKY_CONTACT (object);
  WockyContactPrivate *priv = WOCKY_CONTACT_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (wocky_contact_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_contact_parent_class)->dispose (object);
}

static void
wocky_contact_finalize (GObject *object)
{
  WockyContact *self = WOCKY_CONTACT (object);
  WockyContactPrivate *priv = WOCKY_CONTACT_GET_PRIVATE (self);

  if (priv->jid != NULL)
    g_free (priv->jid);

  if (priv->name != NULL)
    g_free (priv->name);

  if (priv->groups != NULL)
    g_strfreev (priv->groups);

  G_OBJECT_CLASS (wocky_contact_parent_class)->finalize (object);
}

static void
wocky_contact_class_init (WockyContactClass *wocky_contact_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_contact_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_contact_class,
      sizeof (WockyContactPrivate));

  object_class->constructed = wocky_contact_constructed;
  object_class->set_property = wocky_contact_set_property;
  object_class->get_property = wocky_contact_get_property;
  object_class->dispose = wocky_contact_dispose;
  object_class->finalize = wocky_contact_finalize;

  /**
   * WockyContact:jid:
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
   * WockyContact:name:
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
   * WockyContact:subscription:
   *
   * The subscription type of the contact, according to the roster.
   */
  spec = g_param_spec_uint ("subscription", "Contact Subscription",
      "Contact Subscription",
      0,
      LAST_WOCKY_ROSTER_SUBSCRIPTION_TYPE,
      WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SUBSCRIPTION, spec);

  /**
   * WockyContact:groups:
   *
   * A list of the contact's groups, according to the roster.
   */
  spec = g_param_spec_boxed ("groups", "Contact Groups",
      "Contact Groups",
      G_TYPE_STRV,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_GROUPS, spec);
}

/**
 * wocky_contact_get_jid:
 * @contact: a #WockyContact instance
 *
 * <!-- -->
 *
 * Returns: @contact's JID.
 */
const gchar *
wocky_contact_get_jid (WockyContact *contact)
{
  WockyContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_CONTACT (contact), NULL);

  priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  return priv->jid;
}

/**
 * wocky_contact_get_name:
 * @contact: #WockyContact instance
 *
 * <!-- -->
 *
 * Returns: @contact's name
 */
const gchar *
wocky_contact_get_name (WockyContact *contact)
{
  WockyContactPrivate *priv;

  g_return_val_if_fail (WOCKY_IS_CONTACT (contact), NULL);

  priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  return priv->name;
}

/**
 * wocky_contact_set_name:
 * @contact: a #WockyContact instance
 * @name: the name to set @contact
 * @error: a #GError to fill on failure
 *
 * Sets @contact's name to @name.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 */
gboolean
wocky_contact_set_name (WockyContact *contact,
    const gchar *name,
    GError **error)
{
  WockyContactPrivate *priv;

  /* TODO */
  g_return_val_if_fail (WOCKY_IS_CONTACT (contact), FALSE);

  priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  /* TODO */
  if (priv->name != NULL)
    g_free (priv->name);

  priv->name = g_strdup (name);

  return TRUE;
}

/**
 * wocky_contact_get_subscription:
 * @contact: a #WockyContact instance
 *
 * <!-- -->
 *
 * Returns: @contact's subscription.
 */
WockyRosterSubscriptionFlags
wocky_contact_get_subscription (WockyContact *contact)
{
  WockyContactPrivate *priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  g_return_val_if_fail (WOCKY_IS_CONTACT (contact),
      WOCKY_ROSTER_SUBSCRIPTION_TYPE_NONE);

  return priv->subscription;
}

/**
 * wocky_contact_set_subscription:
 * @contact: a #WockyContact instance
 * @subscription: the new subscription type
 * @error: a #GError to fill on failure
 *
 * Sets the subscription of @contact.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 */
gboolean
wocky_contact_set_subscription (WockyContact *contact,
    WockyRosterSubscriptionFlags subscription,
    GError **error)
{
  WockyContactPrivate *priv;

  /* TODO */
  g_return_val_if_fail (WOCKY_IS_CONTACT (contact), FALSE);

  priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  /* TODO */
  priv->subscription = subscription;

  return TRUE;
}

/**
 * wocky_contact_get_groups:
 * @contact: a #WockyContact instance
 *
 * <!-- -->
 *
 * Returns: a list of @contact's groups
 */
const gchar * const *
wocky_contact_get_groups (WockyContact *contact)
{
  WockyContactPrivate *priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  g_return_val_if_fail (WOCKY_IS_CONTACT (contact), NULL);

  return (const gchar * const *) priv->groups;
}

/**
 * wocky_contact_set_groups:
 * @contact: a #WockyContact instance
 * @groups: a list of groups
 * @error: a #GError to fill on failure
 *
 * Sets @contact's groups.
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
wocky_contact_set_groups (WockyContact *contact,
    gchar **groups,
    GError **error)
{
  WockyContactPrivate *priv;

  /* TODO */
  g_return_val_if_fail (WOCKY_IS_CONTACT (contact), FALSE);

  priv = WOCKY_CONTACT_GET_PRIVATE (contact);

  if (priv->groups != NULL)
    g_strfreev (priv->groups);

  /* TODO */
  priv->groups = g_strdupv (groups);

  return TRUE;
}