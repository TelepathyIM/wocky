/*
 * wocky-resource-contact.c - Source for WockyResourceContact
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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
 * SECTION: wocky-resource-contact
 * @title: WockyResourceContact
 * @short_description:
 * @include: wocky/wocky-resource-contact.h
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-resource-contact.h"

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

G_DEFINE_TYPE (WockyResourceContact, wocky_resource_contact, WOCKY_TYPE_CONTACT)

/* properties */
enum
{
  PROP_RESOURCE = 1,
  PROP_BARE_CONTACT,
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
struct _WockyResourceContactPrivate
{
  gboolean dispose_has_run;

  gchar *resource;
  WockyBareContact *bare_contact;
};

static void
wocky_resource_contact_init (WockyResourceContact *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_RESOURCE_CONTACT,
      WockyResourceContactPrivate);
}

static void
wocky_resource_contact_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyResourceContact *self = WOCKY_RESOURCE_CONTACT (object);
  WockyResourceContactPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_RESOURCE:
      priv->resource = g_value_dup_string (value);
      break;
    case PROP_BARE_CONTACT:
      priv->bare_contact = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_resource_contact_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyResourceContact *self = WOCKY_RESOURCE_CONTACT (object);
  WockyResourceContactPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_RESOURCE:
      g_value_set_string (value, priv->resource);
      break;
    case PROP_BARE_CONTACT:
      g_value_set_object (value, priv->bare_contact);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_resource_contact_constructed (GObject *object)
{
  WockyResourceContact *self = WOCKY_RESOURCE_CONTACT (object);
  WockyResourceContactPrivate *priv = self->priv;

  g_assert (priv->resource != NULL);
  g_assert (priv->bare_contact != NULL);
}

static void
wocky_resource_contact_dispose (GObject *object)
{
  WockyResourceContact *self = WOCKY_RESOURCE_CONTACT (object);
  WockyResourceContactPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_object_unref (priv->bare_contact);

  if (G_OBJECT_CLASS (wocky_resource_contact_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_resource_contact_parent_class)->dispose (object);
}

static void
wocky_resource_contact_finalize (GObject *object)
{
  WockyResourceContact *self = WOCKY_RESOURCE_CONTACT (object);
  WockyResourceContactPrivate *priv = self->priv;

  g_free (priv->resource);

  G_OBJECT_CLASS (wocky_resource_contact_parent_class)->finalize (object);
}

static gchar *
wocky_resource_contact_dup_jid (WockyContact *contact)
{
  WockyResourceContact *self = WOCKY_RESOURCE_CONTACT (contact);
  const gchar *bare = wocky_bare_contact_get_jid (self->priv->bare_contact);

  return g_strdup_printf ("%s/%s", bare, self->priv->resource);
}

static void
wocky_resource_contact_class_init (
    WockyResourceContactClass *wocky_resource_contact_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_resource_contact_class);
  WockyContactClass *contact_class = WOCKY_CONTACT_CLASS (wocky_resource_contact_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_resource_contact_class,
      sizeof (WockyResourceContactPrivate));

  object_class->constructed = wocky_resource_contact_constructed;
  object_class->set_property = wocky_resource_contact_set_property;
  object_class->get_property = wocky_resource_contact_get_property;
  object_class->dispose = wocky_resource_contact_dispose;
  object_class->finalize = wocky_resource_contact_finalize;

  contact_class->dup_jid = wocky_resource_contact_dup_jid;

  /**
   * WockyResourceContact:resource:
   *
   * The resource of the contact.
   */
  spec = g_param_spec_string ("resource", "Contact resource",
      "Contact resource",
      NULL,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RESOURCE, spec);

  /**
   * WockyResourceContact:bare-contact:
   *
   * The #WockyBareContact associated with this #WockyResourceContact
   */
  spec = g_param_spec_object ("bare-contact", "Bare contact",
      "the WockyBareContact associated with this WockyResourceContact",
      WOCKY_TYPE_BARE_CONTACT,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_BARE_CONTACT, spec);
}

WockyResourceContact *
wocky_resource_contact_new (WockyBareContact *bare,
    const gchar *resource)
{
  return g_object_new (WOCKY_TYPE_RESOURCE_CONTACT,
      "bare-contact", bare,
      "resource", resource,
      NULL);
}

const gchar *
wocky_resource_contact_get_resource (WockyResourceContact *self)
{
  WockyResourceContactPrivate *priv = self->priv;

  return priv->resource;
}

WockyBareContact *
wocky_resource_contact_get_bare_contact (WockyResourceContact *self)
{
  WockyResourceContactPrivate *priv = self->priv;

  return priv->bare_contact;
}

gboolean
wocky_resource_contact_equal (WockyResourceContact *a,
    WockyResourceContact *b)
{
  if (a == NULL || b == NULL)
    return FALSE;

  if (wocky_strdiff (wocky_resource_contact_get_resource (a),
        wocky_resource_contact_get_resource (b)))
    return FALSE;

  return wocky_bare_contact_equal (wocky_resource_contact_get_bare_contact (a),
      wocky_resource_contact_get_bare_contact (b));
}
