/*
 * wocky-disco-identity.c - Source for Wocky utility functions
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-disco-identity.h"

#define DEBUG_FLAG DEBUG_PRESENCE
#include "wocky-debug.h"

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

WockyDiscoIdentity *
wocky_disco_identity_copy (const WockyDiscoIdentity *source)
{
  g_return_val_if_fail (source != NULL, NULL);

  return wocky_disco_identity_new (source->category, source->type,
      source->lang, source->name);
}

const gchar *
wocky_disco_identity_get_category (WockyDiscoIdentity *identity)
{
  return identity->category;
}

const gchar *
wocky_disco_identity_get_type (WockyDiscoIdentity *identity)
{
  return identity->type;
}

const gchar *
wocky_disco_identity_get_lang (WockyDiscoIdentity *identity)
{
  return identity->lang;
}

const gchar *
wocky_disco_identity_get_name (WockyDiscoIdentity *identity)
{
  return identity->name;
}

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
 * Creates a new array of WockyDiscoIdentity objects.
 *
 * Returns: A newly instantiated array.
 * See: wocky_disco_identity_array_free()
 */
GPtrArray *
wocky_disco_identity_array_new (void)
{
  return g_ptr_array_new_with_free_func (
      (GDestroyNotify) wocky_disco_identity_free);
}

/**
 * wocky_disco_identity_array_copy():
 * @source: The source array to be copied.
 *
 * Copies an array of WockyDiscoIdentity objects. The returned array contains
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
 * wocky_disco_identity_array_free():
 * @arr: Array to be freed.
 *
 * Frees an array of WockyDiscoIdentity objects created with
 * wocky_disco_identity_array_new() or returned by
 * wocky_disco_identity_array_copy().
 *
 * Note that if this method is called with an array created with
 * g_ptr_array_new, the caller should also free the array contents.
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

