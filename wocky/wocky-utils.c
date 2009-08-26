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

/**
 * wocky_decode_jid:
 * @jid: a JID
 * @node: address to which return the username/room part of the JID
 * @domain: address to which return the server/service part of the JID
 * @resource: address to which return the resource/nick part of the JID
 *
 * Parses a JID which may be one of the following forms:
 *  server
 *  server/resource
 *  username@server
 *  username@server/resource
 *  room@service/nick
 * and sets the caller's node, domain and resource
 * pointers to the username/room, server/service and resource/nick parts
 * respectively, if available in the provided JID. The caller may set any of
 * the pointers to NULL if they are not interested in a certain component.
 *
 * The returned values may be NULL or zero-length if a component was either
 * not present or zero-length respectively in the given JID. The username/room
 * and server/service are lower-cased because the Jabber protocol treats them
 * case-insensitively.
 *
 */
void
wocky_decode_jid (const gchar *jid,
    gchar **node,
    gchar **domain,
    gchar **resource)
{
  char *tmp_jid, *tmp_username, *tmp_server, *tmp_resource;

  g_assert (jid != NULL);
  g_assert (node != NULL || domain != NULL || resource != NULL);

  if (node != NULL)
    *node = NULL;

  if (domain != NULL)
    *domain = NULL;

  if (resource != NULL)
    *resource = NULL;

  /* take a local copy so we don't modify the caller's string */
  tmp_jid = g_strdup (jid);

  /* find an @ in username, truncate username to that length, and point
   * 'server' to the byte afterwards */
  tmp_server = strchr (tmp_jid, '@');
  if (tmp_server)
    {
      tmp_username = tmp_jid;

      *tmp_server = '\0';
      tmp_server++;

      /* store the username if the user provided a pointer */
      if (node != NULL)
        *node = g_utf8_strdown (tmp_username, -1);
    }
  else
    {
      tmp_username = NULL;
      tmp_server = tmp_jid;
    }

  /* if we have a server, find a / in it, truncate it to that length, and point
   * 'resource' to the byte afterwards. otherwise, do the same to username to
   * find any resource there. */
  tmp_resource = strchr (tmp_server, '/');
  if (tmp_resource)
    {
      *tmp_resource = '\0';
      tmp_resource++;

      /* store the resource if the user provided a pointer */
      if (resource != NULL)
        *resource = g_strdup (tmp_resource);
    }

  /* the server must be stored after the resource, in case we truncated a
   * resource from it */
  if (domain != NULL)
    *domain = g_utf8_strdown (tmp_server, -1);

  /* free our working copy */
  g_free (tmp_jid);
}
