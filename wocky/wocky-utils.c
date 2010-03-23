/*
 * gibber-util.c - Code for Wocky utility functions
 * Copyright (C) 2007-2010 Collabora Ltd.
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
 *
 * wocky_g_value_slice_* functions have been copied from telepathy-glib's
 * util.c file:
 *  Copyright (C) 2006-2007 Collabora Ltd. <http://www.collabora.co.uk/>
 *  Copyright (C) 2006-2007 Nokia Corporation
 */

#include "wocky-utils.h"

#include <string.h>

#include <gio/gio.h>

/**
 * wocky_strdiff:
 * @left: The first string to compare (may be NULL)
 * @right: The second string to compare (may be NULL)
 *
 * Return %TRUE if the given strings are different. Unlike #strcmp this
 * function will handle null pointers, treating them as distinct from any
 * string.
 *
 * Returns: %FALSE if @left and @right are both %NULL, or if
 *          neither is %NULL and both have the same contents; %TRUE otherwise
 */
gboolean
wocky_strdiff (const gchar *left,
    const gchar *right)
{
  return g_strcmp0 (left, right) != 0;
}

static gboolean
validate_jid_node (const gchar *node)
{
  /* See RFC 3920 §3.3. */
  const gchar *c;

  for (c = node; *c; c++)
    if (strchr ("\"&'/:<>@", *c))
      /* RFC 3920 §A.5 */
      return FALSE;

  return TRUE;
}

static gboolean
validate_jid_domain (const gchar *domain)
{
  /* XXX: This doesn't do proper validation, it just checks the character
   * range. In theory, we check that the domain is a well-formed IDN or
   * an IPv4/IPv6 address literal.
   *
   * See RFC 3920 §3.2.
   */

  const gchar *c;

  for (c = domain; *c; c++)
    if (!g_ascii_isalnum (*c) && !strchr (":-.", *c))
      return FALSE;

  return TRUE;
}

/**
 * wocky_decode_jid:
 * @jid: a JID
 * @node: address to which return the username/room part of the JID
 * @domain: address to which return the server/service part of the JID
 * @resource: address to which return the resource/nick part of the JID
 *
 *  If the JID is valid, returns TRUE and sets the caller's
 * node/domain/resource pointers if they are not NULL. The node and resource
 * pointers will be set to NULL if the respective part is not present in the
 * JID. The node and domain are lower-cased because the Jabber protocol treats
 * them case-insensitively.
 *
 * XXX: Do nodeprep/resourceprep and length checking.
 *
 * See RFC 3920 §3.
 *
 * Returns: %TRUE if the JID is valid
 */
gboolean
wocky_decode_jid (const gchar *jid,
    gchar **node,
    gchar **domain,
    gchar **resource)
{
  char *tmp_jid, *tmp_node, *tmp_domain, *tmp_resource;

  g_assert (jid != NULL);

  if (node != NULL)
    *node = NULL;

  if (domain != NULL)
    *domain = NULL;

  if (resource != NULL)
    *resource = NULL;

  /* Take a local copy so we don't modify the caller's string. */
  tmp_jid = g_strdup (jid);

  /* If there's a slash in tmp_jid, split it in two and take the second part as
   * the resource.
   */
  tmp_resource = strchr (tmp_jid, '/');

  if (tmp_resource)
    {
      *tmp_resource = '\0';
      tmp_resource++;
    }
  else
    {
      tmp_resource = NULL;
    }

  /* If there's an at sign in tmp_jid, split it in two and set tmp_node and
   * tmp_domain appropriately. Otherwise, tmp_node is NULL and the domain is
   * the whole string.
   */
  tmp_domain = strchr (tmp_jid, '@');

  if (tmp_domain)
    {
      *tmp_domain = '\0';
      tmp_domain++;
      tmp_node = tmp_jid;
    }
  else
    {
      tmp_domain = tmp_jid;
      tmp_node = NULL;
    }

  /* Domain must be non-empty and not contain invalid characters. If the node
   * or the resource exist, they must be non-empty and the node must not
   * contain invalid characters.
   */
  if (*tmp_domain == '\0' ||
      !validate_jid_domain (tmp_domain) ||
      (tmp_node != NULL &&
         (*tmp_node == '\0' || !validate_jid_node (tmp_node))) ||
      (tmp_resource != NULL && *tmp_resource == '\0'))
    {
      g_free (tmp_jid);
      return FALSE;
    }

  /* the server must be stored after we find the resource, in case we
   * truncated a resource from it */
  if (domain != NULL)
    *domain = g_utf8_strdown (tmp_domain, -1);

  /* store the username if the user provided a pointer */
  if (tmp_node != NULL && node != NULL)
    *node = g_utf8_strdown (tmp_node, -1);

  /* store the resource if the user provided a pointer */
  if (tmp_resource != NULL && resource != NULL)
    *resource = g_strdup (tmp_resource);

  /* free our working copy */
  g_free (tmp_jid);
  return TRUE;
}

/**
 * wocky_normalise_jid:
 * @jid: a JID
 *
 * Returns: a normalised JID, using the same rules as wocky_decode_jid(),
 * or %NULL if the JID could not be sensibly decoded.
 * This value should be freed when you are done with it.
 */
gchar *
wocky_normalise_jid (const gchar *jid)
{
  gchar *node = NULL;
  gchar *domain = NULL;
  gchar *resource = NULL;
  GString *normal = NULL;
  gchar *rval = NULL;

  if (jid == NULL)
    return NULL;

  if (!wocky_decode_jid (jid, &node, &domain, &resource))
    return NULL;

  normal = g_string_sized_new (strlen (jid));

  if (node != NULL && *node != '\0')
    g_string_printf (normal, "%s@%s", node, domain);
  else
    g_string_printf (normal, "%s", domain);

  if (resource != NULL && *resource != '\0')
    {
      if (normal->len > 0)
        {
          g_string_append_printf (normal, "/%s", resource);
          rval = g_string_free (normal, FALSE);
        }
    }
  else
    {
      rval = g_string_free (normal, FALSE);
    }

  g_free (node);
  g_free (domain);
  g_free (resource);

  return rval;
}

/**
 * wocky_g_value_slice_new:
 * @type: The type desired for the new GValue
 *
 * Slice-allocate an empty #GValue. wocky_g_value_slice_new_boolean() and similar
 * functions are likely to be more convenient to use for the types supported.
 *
 * Returns: a newly allocated, newly initialized #GValue, to be freed with
 * wocky_g_value_slice_free() or g_slice_free().
 * Since: 0.5.14
 */
GValue *
wocky_g_value_slice_new (GType type)
{
  GValue *ret = g_slice_new0 (GValue);

  g_value_init (ret, type);
  return ret;
}

/**
 * wocky_g_value_slice_new_boolean:
 * @b: a boolean value
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_BOOLEAN with value @b, to be freed with
 * wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_boolean (gboolean b)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_BOOLEAN);

  g_value_set_boolean (v, b);
  return v;
}

/**
 * wocky_g_value_slice_new_int:
 * @n: an integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_INT with value @n, to be freed with
 * wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_int (gint n)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_INT);

  g_value_set_int (v, n);
  return v;
}

/**
 * wocky_g_value_slice_new_int64:
 * @n: a 64-bit integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_INT64 with value @n, to be freed with
 * wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_int64 (gint64 n)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_INT64);

  g_value_set_int64 (v, n);
  return v;
}

/**
 * wocky_g_value_slice_new_uint:
 * @n: an unsigned integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_UINT with value @n, to be freed with
 * wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_uint (guint n)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_UINT);

  g_value_set_uint (v, n);
  return v;
}

/**
 * wocky_g_value_slice_new_uint64:
 * @n: a 64-bit unsigned integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_UINT64 with value @n, to be freed with
 * wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_uint64 (guint64 n)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_UINT64);

  g_value_set_uint64 (v, n);
  return v;
}

/**
 * wocky_g_value_slice_new_double:
 * @d: a number
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_DOUBLE with value @n, to be freed with
 * wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_double (double n)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_DOUBLE);

  g_value_set_double (v, n);
  return v;
}

/**
 * wocky_g_value_slice_new_string:
 * @string: a string to be copied into the value
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_STRING whose value is a copy of @string,
 * to be freed with wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_string (const gchar *string)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_STRING);

  g_value_set_string (v, string);
  return v;
}

/**
 * wocky_g_value_slice_new_static_string:
 * @string: a static string which must remain valid forever, to be pointed to
 *  by the value
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_STRING whose value is @string,
 * to be freed with wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_static_string (const gchar *string)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_STRING);

  g_value_set_static_string (v, string);
  return v;
}

/**
 * wocky_g_value_slice_new_take_string:
 * @string: a string which will be freed with g_free() by the returned #GValue
 *  (the caller must own it before calling this function, but no longer owns
 *  it after this function returns)
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_STRING whose value is @string,
 * to be freed with wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_take_string (gchar *string)
{
  GValue *v = wocky_g_value_slice_new (G_TYPE_STRING);

  g_value_take_string (v, string);
  return v;
}

/**
 * wocky_g_value_slice_new_boxed:
 * @type: a boxed type
 * @p: a pointer of type @type, which will be copied
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type @type whose value is a copy of @p,
 * to be freed with wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_boxed (GType type,
                            gconstpointer p)
{
  GValue *v;

  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);
  v = wocky_g_value_slice_new (type);
  g_value_set_boxed (v, p);
  return v;
}

/**
 * wocky_g_value_slice_new_static_boxed:
 * @type: a boxed type
 * @p: a pointer of type @type, which must remain valid forever
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type @type whose value is @p,
 * to be freed with wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_static_boxed (GType type,
                                   gconstpointer p)
{
  GValue *v;

  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);
  v = wocky_g_value_slice_new (type);
  g_value_set_static_boxed (v, p);
  return v;
}

/**
 * wocky_g_value_slice_new_take_boxed:
 * @type: a boxed type
 * @p: a pointer of type @type which will be freed with g_boxed_free() by the
 *  returned #GValue (the caller must own it before calling this function, but
 *  no longer owns it after this function returns)
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type @type whose value is @p,
 * to be freed with wocky_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
wocky_g_value_slice_new_take_boxed (GType type,
                                 gpointer p)
{
  GValue *v;

  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);
  v = wocky_g_value_slice_new (type);
  g_value_take_boxed (v, p);
  return v;
}

/**
 * wocky_g_value_slice_free:
 * @value: A GValue which was allocated with the g_slice API
 *
 * Unset and free a slice-allocated GValue.
 *
 * <literal>(GDestroyNotify) wocky_g_value_slice_free</literal> can be used
 * as a destructor for values in a #GHashTable, for example.
 */

void
wocky_g_value_slice_free (GValue *value)
{
  g_value_unset (value);
  g_slice_free (GValue, value);
}


/**
 * wocky_g_value_slice_dup:
 * @value: A GValue
 *
 * <!-- 'Returns' says it all -->
 *
 * Returns: a newly allocated copy of @value, to be freed with
 * wocky_g_value_slice_free() or g_slice_free().
 * Since: 0.5.14
 */
GValue *
wocky_g_value_slice_dup (const GValue *value)
{
  GValue *ret = wocky_g_value_slice_new (G_VALUE_TYPE (value));

  g_value_copy (value, ret);
  return ret;
}

/**
 * wocky_enum_from_nick:
 * @enum_type: the GType of a subtype of GEnum
 * @nick: a non-%NULL string purporting to be the nickname of a value of
 *        @enum_type
 * @value: the address at which to store the value of @enum_type corresponding
 *         to @nick if this functions returns %TRUE; if this function returns
 *         %FALSE, this variable will be left untouched.
 *
 * <!-- -->
 *
 * Returns: %TRUE if @nick is a member of @enum_type, or %FALSE otherwise
 */
gboolean
wocky_enum_from_nick (
    GType enum_type,
    const gchar *nick,
    gint *value)
{
  GEnumClass *klass = g_type_class_ref (enum_type);
  GEnumValue *enum_value;

  g_return_val_if_fail (klass != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  enum_value = g_enum_get_value_by_nick (klass, nick);
  g_type_class_unref (klass);

  if (enum_value != NULL)
    {
      *value = enum_value->value;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/**
 * wocky_enum_to_nick:
 * @enum_type: the GType of a subtype of GEnum
 * @value: a value of @enum_type
 *
 * <!-- -->
 *
 * Returns: the nickname of @value, or %NULL if it is not, in fact, a value of
 * @enum_type
 */
const gchar *
wocky_enum_to_nick (
    GType enum_type,
    gint value)
{
  GEnumClass *klass = g_type_class_ref (enum_type);
  GEnumValue *enum_value;

  g_return_val_if_fail (klass != NULL, NULL);

  enum_value = g_enum_get_value (klass, value);
  g_type_class_unref (klass);

  if (enum_value != NULL)
    return enum_value->value_nick;
  else
    return NULL;
}

/**
 * wocky_absolutize_path:
 * @path: an absolute or relative path
 *
 * Return an absolute form of @path. This cleans up duplicate slashes, "." or
 * ".." path segments, etc., and prepends g_get_current_dir() if necessary, but
 * does not necessarily resolve symlinks.
 *
 * Returns: an absolute path which must be freed with g_free()
 */
gchar *
wocky_absolutize_path (const gchar *path)
{
  GFile *cwd, *absolute;
  gchar *ret;

  cwd = g_file_new_for_path (g_get_current_dir ());
  absolute = g_file_resolve_relative_path (cwd, path);
  ret = g_file_get_path (absolute);
  g_object_unref (cwd);
  g_object_unref (absolute);
  return ret;
}
