/*
 * wocky-contact-factory.c - Source for WockyContactFactory
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
 * @title: WockyContactFactory
 * @short_description:
 * @include: wocky/wocky-resource-contact.h
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "wocky-contact-factory.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_ROSTER
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyContactFactory, wocky_contact_factory, G_TYPE_OBJECT)

#if 0
/* properties */
enum
{
};
#endif

/* signal enum */
enum
{
  LAST_SIGNAL,
};

/*
static guint signals[LAST_SIGNAL] = {0};
*/

/* private structure */
typedef struct _WockyContactFactoryPrivate WockyContactFactoryPrivate;

struct _WockyContactFactoryPrivate
{
  /* bare JID (gchar *) => weak reffed (WockyBareContact *) */
  GHashTable *bare_contacts;
  /* full JID (gchar *) => weak reffed (WockyResourceContact *) */
  GHashTable *resource_contacts;

  gboolean dispose_has_run;
};

#define WOCKY_CONTACT_FACTORY_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_CONTACT_FACTORY, \
    WockyContactFactoryPrivate))

static void
wocky_contact_factory_init (WockyContactFactory *obj)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (obj);
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);

  priv->bare_contacts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      NULL);
  priv->resource_contacts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
}

static void
wocky_contact_factory_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_contact_factory_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
wocky_contact_factory_constructed (GObject *object)
{
}

static gboolean
remove_contact (gpointer key,
    gpointer value,
    gpointer contact)
{
  return value == contact;
}

static void
bare_contact_disposed_cb (gpointer user_data,
    GObject *contact)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (user_data);
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);

  g_hash_table_foreach_remove (priv->bare_contacts, remove_contact, contact);
}

static void
resource_contact_disposed_cb (gpointer user_data,
    GObject *contact)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (user_data);
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);

  g_hash_table_foreach_remove (priv->resource_contacts, remove_contact,
      contact);
}

static void
wocky_contact_factory_dispose (GObject *object)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (object);
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);
  GHashTableIter iter;
  gpointer contact;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_hash_table_iter_init (&iter, priv->bare_contacts);
  while (g_hash_table_iter_next (&iter, NULL, &contact))
    {
      g_object_weak_unref (G_OBJECT (contact), bare_contact_disposed_cb, self);
    }

  g_hash_table_iter_init (&iter, priv->resource_contacts);
  while (g_hash_table_iter_next (&iter, NULL, &contact))
    {
      g_object_weak_unref (G_OBJECT (contact), resource_contact_disposed_cb,
          self);
    }

  if (G_OBJECT_CLASS (wocky_contact_factory_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_contact_factory_parent_class)->dispose (object);
}

static void
wocky_contact_factory_finalize (GObject *object)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (object);
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);

  g_hash_table_destroy (priv->bare_contacts);
  g_hash_table_destroy (priv->resource_contacts);

  G_OBJECT_CLASS (wocky_contact_factory_parent_class)->finalize (object);
}

static void
wocky_contact_factory_class_init (
    WockyContactFactoryClass *wocky_contact_factory_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_contact_factory_class);

  g_type_class_add_private (wocky_contact_factory_class,
      sizeof (WockyContactFactoryPrivate));

  object_class->constructed = wocky_contact_factory_constructed;
  object_class->set_property = wocky_contact_factory_set_property;
  object_class->get_property = wocky_contact_factory_get_property;
  object_class->dispose = wocky_contact_factory_dispose;
  object_class->finalize = wocky_contact_factory_finalize;
}

WockyContactFactory *
wocky_contact_factory_new (void)
{
  return g_object_new (WOCKY_TYPE_CONTACT_FACTORY,
      NULL);
}

WockyBareContact *
wocky_contact_factory_ensure_bare_contact (WockyContactFactory *self,
    const gchar *bare_jid)
{
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);
  WockyBareContact *contact;

  contact = g_hash_table_lookup (priv->bare_contacts, bare_jid);
  if (contact != NULL)
    return g_object_ref (contact);

  contact = wocky_bare_contact_new (bare_jid);

  g_object_weak_ref (G_OBJECT (contact), bare_contact_disposed_cb, self);
  g_hash_table_insert (priv->bare_contacts, g_strdup (bare_jid), contact);

  return contact;
}

WockyBareContact *
wocky_contact_factory_lookup_bare_contact (WockyContactFactory *self,
    const gchar *bare_jid)
{
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);

  return g_hash_table_lookup (priv->bare_contacts, bare_jid);
}

WockyResourceContact *
wocky_contact_factory_ensure_resource_contact (WockyContactFactory *self,
    const gchar *full_jid)
{
  WockyContactFactoryPrivate *priv = WOCKY_CONTACT_FACTORY_GET_PRIVATE (self);
  WockyBareContact *bare;
  WockyResourceContact *contact;
  gchar *node, *domain, *resource, *bare_jid;

  contact = g_hash_table_lookup (priv->resource_contacts, full_jid);
  if (contact != NULL)
    return g_object_ref (contact);

  wocky_decode_jid (full_jid, &node, &domain, &resource);
  bare_jid = g_strdup_printf ("%s@%s", node, domain);

  bare = wocky_contact_factory_ensure_bare_contact (self, bare_jid);

  contact = wocky_resource_contact_new (bare, resource);

  g_object_weak_ref (G_OBJECT (contact), resource_contact_disposed_cb, self);
  g_hash_table_insert (priv->resource_contacts, g_strdup (full_jid), contact);

  g_free (node);
  g_free (domain);
  g_free (resource);
  g_free (bare_jid);
  g_object_unref (bare);

  return contact;
}
