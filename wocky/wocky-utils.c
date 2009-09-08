/*
 * gibber-util.c - Code for Wocky utility functions
 * Copyright (C) 2007,2009 Collabora Ltd.
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

#include <string.h>

#include "wocky-utils.h"

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
