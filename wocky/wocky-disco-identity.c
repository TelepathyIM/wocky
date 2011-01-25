/*
 * wocky-disco-identity.c - Source for WockyDiscoIdentity
 * Copyright (C) 2010 Collabora Ltd.
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
 * SECTION: wocky-disco-identity
 * @title: WockyDiscoIdentity
 * @short_description: Structure holding XMPP disco identity information.
 *
 * Contains information regarding the identity information in disco
 * replies, as described in XEP-0030.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-disco-identity.h"

#define DEBUG_FLAG DEBUG_PRESENCE
#include "wocky-debug.h"

G_DEFINE_BOXED_TYPE (WockyDiscoIdentity, wocky_disco_identity,
    wocky_disco_identity_copy, wocky_disco_identity_free)

/**
 * wocky_disco_identity_new:
 * @category: disco category
 * @type: disco type
 * @lang: disco language
 * @name: disco name
 *
 * <!-- -->
 *
 * Returns: a new #WockyDiscoIdentity which should be freed using
 *          wocky_disco_identity_free().
 */
WockyDiscoIdentity *
wocky_disco_identity_new (const gchar *category,
    const gchar *type,
    const gchar *lang,
    const gchar *name)
{
  WockyDiscoIdentity *ret;

  g_return_val_if_fail (category != NULL, NULL);
  g_return_val_if_fail (type != NULL, NULL);

  ret = g_slice_new (WockyDiscoIdentity);
  ret->category = g_strdup (category);
  ret->type = g_strdup (type);
  ret->lang = g_strdup (lang);
  ret->name = g_strdup (name);
  return ret;
}

/**
 * wocky_disco_identity_copy:
 * @source: the #WockyDiscoIdentity to copy
 *
 * Creates a new #WockyDiscoIdentity structure with the data given by
 * @source. The copy also copies the internal data so @source can be
 * freed after this function is called.
 *
 * Returns: a new #WockyDiscoIdentity which is a deep copy of @source
 */
WockyDiscoIdentity *
wocky_disco_identity_copy (const WockyDiscoIdentity *source)
{
  g_return_val_if_fail (source != NULL, NULL);

  return wocky_disco_identity_new (source->category, source->type,
      source->lang, source->name);
}

/**
 * wocky_disco_identity_get_category:
 * @identity: a #WockyDiscoIdentity
 *
 * <!-- -->
 *
 * Returns: @identity's category
 */
const gchar *
wocky_disco_identity_get_category (WockyDiscoIdentity *identity)
{
  return identity->category;
}

/**
 * wocky_disco_identity_get_identity_type:
 * @identity: a #WockyDiscoIdentity
 *
 * <!-- -->
 *
 * Returns: @identity's type
 */
const gchar *
wocky_disco_identity_get_identity_type (WockyDiscoIdentity *identity)
{
  return identity->type;
}

/**
 * wocky_disco_identity_get_lang:
 * @identity: a #WockyDiscoIdentity
 *
 * <!-- -->
 *
 * Returns: @identity's language
 */
const gchar *
wocky_disco_identity_get_lang (WockyDiscoIdentity *identity)
{
  return identity->lang;
}

/**
 * wocky_disco_identity_get_name:
 * @identity: a #WockyDiscoIdentity
 *
 * <!-- -->
 *
 * Returns: @identity's name
 */
const gchar *
wocky_disco_identity_get_name (WockyDiscoIdentity *identity)
{
  return identity->name;
}

/**
 * wocky_disco_identity_free:
 * @identity: a #WockyDiscoIdentity
 *
 * Frees the memory used by @identity.
 */
void
wocky_disco_identity_free (WockyDiscoIdentity *identity)
{
  if (identity == NULL)
    return;

  g_free (identity->category);
  g_free (identity->type);
  g_free (identity->lang);
  g_free (identity->name);
  g_slice_free (WockyDiscoIdentity, identity);
}

/**
 * wocky_disco_identity_array_new:
 *
 * Creates a new array of #WockyDiscoIdentity structures.
 *
 * Returns: A newly instantiated
 *          array. wocky_disco_identity_array_free() should beq used
 *          to free the memory allocated by this array.
 * See: wocky_disco_identity_array_free()
 */
GPtrArray *
wocky_disco_identity_array_new (void)
{
  return g_ptr_array_new_with_free_func (
      (GDestroyNotify) wocky_disco_identity_free);
}

/**
 * wocky_disco_identity_array_copy:
 * @source: The source array to be copied.
 *
 * Copies an array of #WockyDiscoIdentity objects. The returned array contains
 * new copies of the contents of the source array.
 *
 * Returns: A newly instantiated array with new copies of the contents of the
 *          source array.
 * See: wocky_disco_identity_array_new()
 */
GPtrArray *
wocky_disco_identity_array_copy (const GPtrArray *source)
{
  GPtrArray *ret;
  guint i;

  if (!source)
    return NULL;

  ret = g_ptr_array_sized_new (source->len);
  g_ptr_array_set_free_func (ret, (GDestroyNotify) wocky_disco_identity_free);
  for (i = 0; i < source->len; ++i)
    {
      g_ptr_array_add (ret,
         wocky_disco_identity_copy (g_ptr_array_index (source, i)));
    }
  return ret;
}

/**
 * wocky_disco_identity_array_free:
 * @arr: Array to be freed.
 *
 * Frees an array of #WockyDiscoIdentity objects created with
 * wocky_disco_identity_array_new() or returned by
 * wocky_disco_identity_array_copy().
 *
 * Note that if this method is called with an array created with
 * g_ptr_array_new(), the caller should also free the array contents.
 *
 * See: wocky_disco_identity_array_new(), wocky_disco_identity_array_copy()
 */
void
wocky_disco_identity_array_free (GPtrArray *arr)
{
  if (!arr)
    return;

  g_ptr_array_free (arr, TRUE);
}

