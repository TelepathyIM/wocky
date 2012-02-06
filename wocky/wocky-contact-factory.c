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
 * SECTION: wocky-contact-factory
 * @title: WockyContactFactory
 * @short_description: creates and looks up #WockyContact objects
 * @include: wocky/wocky-contact-factory.h
 *
 * Provides a way to create #WockyContact objects. The objects created
 * this way are cached by the factory and you can eventually look them up
 * without creating them again.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-contact-factory.h"

#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>

#include <gio/gio.h>

#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_ROSTER
#include "wocky-debug-internal.h"

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
  BARE_CONTACT_ADDED,
  RESOURCE_CONTACT_ADDED,
  LL_CONTACT_ADDED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
struct _WockyContactFactoryPrivate
{
  /* bare JID (gchar *) => weak reffed (WockyBareContact *) */
  GHashTable *bare_contacts;
  /* full JID (gchar *) => weak reffed (WockyResourceContact *) */
  GHashTable *resource_contacts;
  /* JID (gchar *) => weak reffed (WockyLLContact *) */
  GHashTable *ll_contacts;

  gboolean dispose_has_run;
};

static void
wocky_contact_factory_init (WockyContactFactory *self)
{
  WockyContactFactoryPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_CONTACT_FACTORY,
      WockyContactFactoryPrivate);
  priv = self->priv;

  priv->bare_contacts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      NULL);
  priv->resource_contacts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
  priv->ll_contacts = g_hash_table_new_full (g_str_hash, g_str_equal,
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

/* Called when a WockyBareContact, WockyResourceContact or
 * WockyLLContact has been disposed so we can remove it from his hash
 * table. */
static void
contact_disposed_cb (gpointer user_data,
    GObject *contact)
{
  GHashTable *table = (GHashTable *) user_data;

  g_hash_table_foreach_remove (table, remove_contact, contact);
}

static void
wocky_contact_factory_dispose (GObject *object)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (object);
  WockyContactFactoryPrivate *priv = self->priv;
  GHashTableIter iter;
  gpointer contact;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_hash_table_iter_init (&iter, priv->bare_contacts);
  while (g_hash_table_iter_next (&iter, NULL, &contact))
    {
      g_object_weak_unref (G_OBJECT (contact), contact_disposed_cb,
          priv->bare_contacts);
    }

  g_hash_table_iter_init (&iter, priv->resource_contacts);
  while (g_hash_table_iter_next (&iter, NULL, &contact))
    {
      g_object_weak_unref (G_OBJECT (contact), contact_disposed_cb,
          priv->resource_contacts);
    }

  g_hash_table_iter_init (&iter, priv->ll_contacts);
  while (g_hash_table_iter_next (&iter, NULL, &contact))
    {
      g_object_weak_unref (G_OBJECT (contact), contact_disposed_cb,
          priv->ll_contacts);
    }

  if (G_OBJECT_CLASS (wocky_contact_factory_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_contact_factory_parent_class)->dispose (object);
}

static void
wocky_contact_factory_finalize (GObject *object)
{
  WockyContactFactory *self = WOCKY_CONTACT_FACTORY (object);
  WockyContactFactoryPrivate *priv = self->priv;

  g_hash_table_unref (priv->bare_contacts);
  g_hash_table_unref (priv->resource_contacts);
  g_hash_table_unref (priv->ll_contacts);

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

  signals[BARE_CONTACT_ADDED] = g_signal_new ("bare-contact-added",
      G_OBJECT_CLASS_TYPE (wocky_contact_factory_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[RESOURCE_CONTACT_ADDED] = g_signal_new ("resource-contact-added",
      G_OBJECT_CLASS_TYPE (wocky_contact_factory_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[LL_CONTACT_ADDED] = g_signal_new ("ll-contact-added",
      G_OBJECT_CLASS_TYPE (wocky_contact_factory_class),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

/**
 * wocky_contact_factory_new:
 *
 * Convenience function to create a new #WockyContactFactory object.
 *
 * Returns: a newly created instance of #WockyContactFactory
 */
WockyContactFactory *
wocky_contact_factory_new (void)
{
  return g_object_new (WOCKY_TYPE_CONTACT_FACTORY,
      NULL);
}

/**
 * wocky_contact_factory_ensure_bare_contact:
 * @factory: a #WockyContactFactory instance
 * @bare_jid: the JID of a bare contact
 *
 * Returns an instance of #WockyBareContact for @bare_jid. The factory cache
 * is used, but if the contact is not found in the cache, a new
 * #WockyBareContact is created and cached for future use.
 *
 * Returns: a new reference to a #WockyBareContact instance, which the caller
 *  is expected to release with g_object_unref() after use.
 */
WockyBareContact *
wocky_contact_factory_ensure_bare_contact (WockyContactFactory *self,
    const gchar *bare_jid)
{
  WockyContactFactoryPrivate *priv = self->priv;
  WockyBareContact *contact;

  contact = g_hash_table_lookup (priv->bare_contacts, bare_jid);
  if (contact != NULL)
    return g_object_ref (contact);

  contact = wocky_bare_contact_new (bare_jid);

  g_object_weak_ref (G_OBJECT (contact), contact_disposed_cb,
      priv->bare_contacts);
  g_hash_table_insert (priv->bare_contacts, g_strdup (bare_jid), contact);

  g_signal_emit (self, signals[BARE_CONTACT_ADDED], 0, contact);

  return contact;
}

/**
 * wocky_contact_factory_lookup_bare_contact:
 * @factory: a #WockyContactFactory instance
 * @bare_jid: the JID of a bare contact
 *
 * Looks up if there's a #WockyBareContact for @bare_jid in the cache, and
 * returns it if it's found.
 *
 * Returns: a borrowed #WockyBareContact instance (which the caller should
 *  reference with g_object_ref() if it will be kept), or %NULL if the
 *  contact is not found.
 */
WockyBareContact *
wocky_contact_factory_lookup_bare_contact (WockyContactFactory *self,
    const gchar *bare_jid)
{
  WockyContactFactoryPrivate *priv = self->priv;

  return g_hash_table_lookup (priv->bare_contacts, bare_jid);
}

/**
 * wocky_contact_factory_ensure_resource_contact:
 * @factory: a #WockyContactFactory instance
 * @full_jid: the full JID of a resource
 *
 * Returns an instance of #WockyResourceContact for @full_jid.
 * The factory cache is used, but if the resource is not found in the cache,
 * a new #WockyResourceContact is created and cached for future use.
 *
 * Returns: a new reference to a #WockyResourceContact instance, which the
 *  caller is expected to release with g_object_unref() after use.
 */
WockyResourceContact *
wocky_contact_factory_ensure_resource_contact (WockyContactFactory *self,
    const gchar *full_jid)
{
  WockyContactFactoryPrivate *priv = self->priv;
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

  g_object_weak_ref (G_OBJECT (contact), contact_disposed_cb,
      priv->resource_contacts);
  g_hash_table_insert (priv->resource_contacts, g_strdup (full_jid), contact);

  wocky_bare_contact_add_resource (bare, contact);

  g_free (node);
  g_free (domain);
  g_free (resource);
  g_free (bare_jid);
  g_object_unref (bare);

  g_signal_emit (self, signals[RESOURCE_CONTACT_ADDED], 0, contact);

  return contact;
}

/**
 * wocky_contact_factory_lookup_resource_contact:
 * @factory: a #WockyContactFactory instance
 * @full_jid: the full JID of a resource
 *
 * Looks up if there's a #WockyResourceContact for @full_jid in the cache, and
 * returns it if it's found.
 *
 * Returns: a borrowed #WockyResourceContact instance (which the caller should
 *  reference with g_object_ref() if it will be kept), or %NULL if the
 *  contact is not found.
 */
WockyResourceContact *
wocky_contact_factory_lookup_resource_contact (WockyContactFactory *self,
    const gchar *full_jid)
{
  WockyContactFactoryPrivate *priv = self->priv;

  return g_hash_table_lookup (priv->resource_contacts, full_jid);
}

/**
 * wocky_contact_factory_ensure_ll_contact:
 * @factory: a #WockyContactFactory instance
 * @jid: the JID of a contact
 *
 * Returns an instance of #WockyLLContact for @jid.
 * The factory cache is used, but if the contact is not found in the cache,
 * a new #WockyLLContact is created and cached for future use.
 *
 * Returns: a new reference to a #WockyLLContact instance, which the
 *  caller is expected to release with g_object_unref() after use.
 */
WockyLLContact *
wocky_contact_factory_ensure_ll_contact (WockyContactFactory *self,
    const gchar *jid)
{
  WockyContactFactoryPrivate *priv = self->priv;
  WockyLLContact *contact;

  g_return_val_if_fail (jid != NULL, NULL);

  contact = g_hash_table_lookup (priv->ll_contacts, jid);
  if (contact != NULL)
    return g_object_ref (contact);

  contact = wocky_ll_contact_new (jid);

  g_object_weak_ref (G_OBJECT (contact), contact_disposed_cb,
      priv->ll_contacts);
  g_hash_table_insert (priv->ll_contacts, g_strdup (jid), contact);

  g_signal_emit (self, signals[LL_CONTACT_ADDED], 0, contact);

  return contact;
}

/**
 * wocky_contact_factory_lookup_ll_contact:
 * @factory: a #WockyContactFactory instance
 * @jid: the JID of a contact
 *
 * Looks up if there's a #WockyLLContact for @jid in the cache, and
 * returns it if it's found.
 *
 * Returns: a borrowed #WockyLLContact instance (which the caller should
 *  reference with g_object_ref() if it will be kept), or %NULL if the
 *  contact is not found.
 */
WockyLLContact *
wocky_contact_factory_lookup_ll_contact (WockyContactFactory *self,
    const gchar *jid)
{
  WockyContactFactoryPrivate *priv = self->priv;

  return g_hash_table_lookup (priv->ll_contacts, jid);
}

/**
 * wocky_contact_factory_add_ll_contact:
 * @factory: a #WockyContactFactory instance
 * @contact: a #WockyLLContact
 *
 * Adds @contact to the contact factory.
 */
void
wocky_contact_factory_add_ll_contact (WockyContactFactory *self,
    WockyLLContact *contact)
{
  WockyContactFactoryPrivate *priv = self->priv;
  gchar *jid = wocky_contact_dup_jid (WOCKY_CONTACT (contact));
  WockyLLContact *old_contact = g_hash_table_lookup (priv->ll_contacts, jid);

  if (old_contact == contact)
    {
      g_free (jid);
      return;
    }

  if (old_contact != NULL)
    {
      g_object_weak_unref (G_OBJECT (old_contact), contact_disposed_cb,
          priv->ll_contacts);
    }

  g_object_weak_ref (G_OBJECT (contact), contact_disposed_cb,
      priv->ll_contacts);
  g_hash_table_insert (priv->ll_contacts, jid, contact);

  g_signal_emit (self, signals[LL_CONTACT_ADDED], 0, contact);
}

/**
 * wocky_contact_factory_get_ll_contacts:
 * @factory: a #WockyContactFactory instance
 *
 * <!-- -->
 *
 * Returns: a newly allocated #GList of #WockyLLContact<!-- -->s which
 *   should be freed using g_list_free().
 */
GList *
wocky_contact_factory_get_ll_contacts (WockyContactFactory *self)
{
  return g_hash_table_get_values (self->priv->ll_contacts);
}
