/*
 * wocky-node.c - Code for Wocky xmpp nodes
 * Copyright (C) 2006-2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd@luon.net>
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

#include <glib.h>
#include <string.h>

#include "wocky-node.h"
#include "wocky-node-private.h"
#include "wocky-node-tree.h"
#include "wocky-utils.h"
#include "wocky-namespaces.h"

/**
 * SECTION: wocky-node
 * @title: WockyNode
 * @short_description: representation of a XMPP node
 * @include: wocky/wocky-node.h
 *
 * Low level representation of a XMPP node. Provides ways to set various
 * parameters on the node, such as content, language, namespaces and prefixes.
 * It also offers methods to lookup children of a node.
 */

typedef struct {
  gchar *key;
  gchar *value;
  gchar *prefix;
  GQuark ns;
} Attribute;

typedef struct {
  const gchar *key;
  GQuark ns;
} Tuple;

typedef struct {
  const gchar *ns_urn;
  gchar *prefix;
  GQuark ns;
} NSPrefix;

static NSPrefix default_attr_ns_prefixes[] =
  { { WOCKY_GOOGLE_NS_AUTH, "ga" },
    { NULL, NULL } };

static GHashTable *user_ns_prefixes = NULL;
static GHashTable *default_ns_prefixes = NULL;

/* Do a strndup operation, but at the same time replace all characters that
 * aren't valid according to g_utf8_validate by � */

static gchar *
strndup_make_valid (const gchar *str, gssize len)
{
  const gchar *remainder = str;
  GString *result;
  const gchar *endp;
  gssize left = len;

  /* Simplify things by always keeping track of the string lenght */
  if (left < 0)
    left = strlen (remainder);

  result = g_string_sized_new (len);

  while (!g_utf8_validate (remainder, left, &endp))
    {
      g_string_append_len (result, remainder, endp - remainder);
      /* append U+FFFD REPLACEMENT CHARACTER */
      g_string_append (result, "\357\277\275");

      /* left, minus the valid part of the string */
      left -= (endp - remainder);

      remainder = g_utf8_find_next_char (endp, endp + left);
      /* left, minus the skipped part, if there is no next utf8 character,
       * nothing is left */
      if (remainder == NULL)
        left = 0;
      else if (left > 0)
        left -= (remainder - endp);
    }
  g_string_append_len (result, remainder, left);

  return g_string_free (result, FALSE);
}

static gchar *
strndup_validated (const gchar *str, gssize len)
{
  if (str == NULL)
    return NULL;

  /* Fast path, string happily validates, simple copy */
  if (G_LIKELY (g_utf8_validate (str, len, NULL)))
    {
      if (len < 0)
        return g_strdup (str);
      else
        return g_strndup (str, len);
    }

  /* slow path, string doesn't validate.. */
  return strndup_make_valid (str, len);
}

static gchar *
concat_validated (const gchar *s1, const gchar *s2, gssize s2_size)
{
  gchar *result;
  gssize s1_size;
  /* data to be freed after use if needed */
  const gchar *to_free = NULL;

  /* concatting to nothing, iotw, strndup :) */
  if (s1 == NULL)
    return strndup_validated (s2, s2_size);

  s1_size = strlen (s1);
  if (s2_size < 0)
    s2_size = strlen (s2);

  if (G_UNLIKELY (!g_utf8_validate (s2, s2_size, NULL)))
    {
      /* Make a validated copy we will free later on. Making a copy to just
       * concat and then free isn't the most efficient way, but at this point
       * we're out of the fast-path anyway */
      to_free = s2 = strndup_make_valid (s2, s2_size);
      s2_size = strlen (s2);
    }

  result = g_malloc0 (s1_size + s2_size + 1);
  memcpy (result, s1, s1_size);
  memcpy (result + s1_size, s2, s2_size);
  g_free ((gchar *) to_free);

  return result;
}


static WockyNode *
new_node (const char *name, GQuark ns)
{
  WockyNode *result;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (ns != 0, NULL);

  result = g_slice_new0 (WockyNode);

  result->name = strndup_validated (name, -1);
  result->ns = ns;

  return result;
}

/**
 * wocky_node_new:
 * @name: the node's name (may not be NULL)
 * @ns: the nodes namespace (may not be NULL)
 *
 * Convenience function which creates a #WockyNode and sets its
 * name to @name.
 *
 * Returns: a newly allocated #WockyNode.
 */
WockyNode *
wocky_node_new (const char *name, const gchar *ns)
{
  g_return_val_if_fail (ns != NULL, NULL);

  return new_node (name, g_quark_from_string (ns));
}

static void
attribute_free (Attribute *a)
{
  g_free (a->key);
  g_free (a->value);
  g_free (a->prefix);
  g_slice_free (Attribute, a);
}

/**
 * wocky_node_free:
 * @node: a #WockyNode.
 *
 * Convenience function that frees the passed in #WockyNode and
 * all of its children.
 */
void
wocky_node_free (WockyNode *node)
{
  GSList *l;

  if (node == NULL)
    {
      return ;
    }

  g_free (node->name);
  g_free (node->content);
  g_free (node->language);

  for (l = node->children; l != NULL ; l = l->next)
    {
      wocky_node_free ((WockyNode *) l->data);
    }
  g_slist_free (node->children);

  for (l = node->attributes; l != NULL ; l = l->next)
    {
      Attribute *a = (Attribute *) l->data;
      attribute_free (a);
    }
  g_slist_free (node->attributes);

  g_slice_free (WockyNode, node);
}

/**
 * wocky_node_each_attribute:
 * @node: a #WockyNode
 * @func: the function to be called on each node's attribute
 * @user_data: user data to pass to the function
 *
 * Calls a function for each attribute of a #WockyNode.
 */
void
wocky_node_each_attribute (WockyNode *node,
    wocky_node_each_attr_func func, gpointer user_data)
{
  GSList *l;

  for (l = node->attributes; l != NULL ; l = l->next)
    {
      Attribute *a = (Attribute *) l->data;
      const gchar *ns = g_quark_to_string (a->ns);
      if (!func (a->key, a->value, a->prefix, ns, user_data))
        {
          return;
        }
    }
}

/**
 * wocky_node_each_child:
 * @node: a #WockyNode
 * @func: the function to be called on each node's child
 * @user_data: user data to pass to the function
 *
 * Calls a function for each child of a #WockyNode.
 */
void
wocky_node_each_child (WockyNode *node,
    wocky_node_each_child_func func, gpointer user_data)
{
  GSList *l;

  for (l = node->children; l != NULL ; l = l->next)
    {
      WockyNode *n = (WockyNode *) l->data;
      if (!func (n, user_data))
        {
          return;
        }
    }
}

static gint
attribute_compare (gconstpointer a, gconstpointer b)
{
  const Attribute *attr = (const Attribute *)a;
  const Tuple *target = (const Tuple *)b;

  if (target->ns != 0 && attr->ns != target->ns)
    {
      return 1;
    }

  return strcmp (attr->key, target->key);
}

/**
 * wocky_node_get_attribute_ns:
 * @node: a #WockyNode
 * @key: the attribute name
 * @ns: the namespace to search within, or %NULL
 *
 * Returns the value of an attribute in a #WockyNode, limiting the search
 * within a specific namespace. If the namespace is %NULL, this is equivalent
 * to wocky_node_get_attribute().
 *
 * Returns: the value of the attribute @key, or %NULL if @node doesn't
 * have such attribute in @ns.
 */
const gchar *
wocky_node_get_attribute_ns (WockyNode *node,
    const gchar *key, const gchar *ns)
{
  GSList *link;
  Tuple search;

  search.key = (gchar *) key;
  search.ns = (ns != NULL ? g_quark_from_string (ns) : 0);

  link = g_slist_find_custom (node->attributes, &search, attribute_compare);

  return (link == NULL) ? NULL : ((Attribute *) (link->data))->value;
}

/**
 * wocky_node_get_attribute:
 * @node: a #WockyNode
 * @key: the attribute name
 *
 * Returns the value of an attribute in a #WockyNode.
 *
 * Returns: the value of the attribute @key, or %NULL if @node doesn't
 * have such attribute.
 */
const gchar *
wocky_node_get_attribute (WockyNode *node, const gchar *key)
{
  return wocky_node_get_attribute_ns (node, key, NULL);
}

/**
 * wocky_node_set_attribute:
 * @node: a #WockyNode
 * @key: the attribute name to set
 * @value: the value to set
 *
 * Sets an attribute in a #WockyNode to a specific value.
 */
void
wocky_node_set_attribute (WockyNode *node,
    const gchar *key, const gchar *value)
{
  g_assert (value != NULL);
  wocky_node_set_attribute_n_ns (node, key, value, strlen (value), NULL);
}

/**
 * wocky_node_set_attributes:
 * @node: a #WockyNode
 * @key: the attribute name to set
 * @...: pairs of keys and values, terminated by %NULL
 *
 * Sets attributes in a #WockyNode to specific values.
 */
void
wocky_node_set_attributes (WockyNode *node,
    const gchar *key,
    ...)
{
  va_list args;

  g_return_if_fail (key != NULL);

  va_start (args, key);
  while (key != NULL)
    {
      const gchar *value;
      value = (const gchar *) va_arg (args, gchar *);

      wocky_node_set_attribute (node, key, value);

      key = (const gchar *) va_arg (args, gchar *);
    }
  va_end (args);
}

/**
 * wocky_node_set_attribute_ns:
 * @node: a #WockyNode
 * @key: the attribute name to set
 * @value: the value to set
 * @ns: a namespace, or %NULL
 *
 * Sets an attribute in a #WockyNode, within a specific namespace.
 * If the namespace is %NULL, this is equivalent to
 * wocky_node_set_attribute().
 */
void
wocky_node_set_attribute_ns (WockyNode *node, const gchar *key,
    const gchar *value, const gchar *ns)
{
  wocky_node_set_attribute_n_ns (node,
      key, value, strlen (value), ns);
}

static NSPrefix *
ns_prefix_new (const gchar *urn,
    GQuark ns,
    const gchar *prefix)
{
  NSPrefix *nsp = g_slice_new0 (NSPrefix);
  nsp->ns_urn = urn;
  nsp->prefix = strndup_validated (prefix, -1);
  nsp->ns = ns;

  return nsp;
}

static void
ns_prefix_free (NSPrefix *nsp)
{
  g_free (nsp->prefix);
  g_slice_free (NSPrefix, nsp);
}

static GHashTable *
_init_prefix_table (void)
{
  /* do NOT use the astonishingly poorly named g_int_hash here */
  /* it most emphatically does NOT do what it says on the tin  */
  return g_hash_table_new_full (g_direct_hash, /* quarks are uint32s */
      g_direct_equal,                          /* ibid               */
      NULL,                                    /* cannot free quarks */
      (GDestroyNotify)ns_prefix_free);
}

/* convert the NS URN Quark to a base-26 number represented as a *
 * lowercase a-z digit string (aa, ab, ac, ad ... etc)           *
 * then prepend a string ("wocky-") to make the attr ns prefix   */
static gchar *
_generate_ns_prefix (GQuark ns)
{
  GString *prefix = g_string_new ("wocky-");
  int p = ns;

  /* actually, I think we might end up with the digits in le order *
   * having re-read the code, but that doesn't actually matter.    */
  while (p > 0)
    {
      guchar x = (p % 26);
      p -= x;
      p /= 26;
      x += 'a';
      g_string_append_c (prefix, x);
    }

  return g_string_free (prefix, FALSE);
}

static void
_init_user_prefix_table (void)
{
  if (user_ns_prefixes == NULL)
    user_ns_prefixes = _init_prefix_table ();
}

static const NSPrefix *
_add_prefix_to_table (GHashTable *table,
    GQuark ns,
    const gchar *urn,
    const gchar *prefix)
{
  NSPrefix *nsp = ns_prefix_new (urn, ns, prefix);
  g_hash_table_insert (table, GINT_TO_POINTER (ns), nsp);
  return nsp;
}

static void
_init_default_prefix_table (void)
{
  int i;

  if (default_ns_prefixes != NULL)
    return;

  default_ns_prefixes = _init_prefix_table ();

  for (i = 0; default_attr_ns_prefixes[i].ns_urn != NULL; i++)
    {
      const gchar *urn = default_attr_ns_prefixes[i].ns_urn;
      GQuark ns = g_quark_from_string (urn);
      gchar *prefix = _generate_ns_prefix (ns);
      _add_prefix_to_table (default_ns_prefixes, ns, urn, prefix);
      g_free (prefix);
    }
}

static const gchar *
_attribute_ns_get_prefix (GQuark ns,
    const gchar *urn)
{
  const NSPrefix *nsp = NULL;
  gchar *prefix;

  /* check user-registered explicit prefixes for this namespace */
  nsp = g_hash_table_lookup (user_ns_prefixes, GINT_TO_POINTER (ns));
  if (nsp != NULL)
    return nsp->prefix;

  /* check any built-in explicit prefixes for this namespace */
  nsp = g_hash_table_lookup (default_ns_prefixes, GINT_TO_POINTER (ns));
  if (nsp != NULL)
    return nsp->prefix;

  /* ok, there was no registered prefix - generate and register a prefix */
  /* initialise the user prefix table here if we need to                 */
  prefix = _generate_ns_prefix (ns);
  nsp = _add_prefix_to_table (user_ns_prefixes, ns, urn, prefix);
  g_free (prefix);
  return nsp->prefix;
}

/**
 * wocky_node_attribute_ns_get_prefix_from_quark:
 * @ns: a quark corresponding to an XML namespace URN
 *
 * Gets the prefix of the namespace identified by the quark.
 *
 * Returns: a string containing the prefix of the namespace @ns.
 */
const gchar *
wocky_node_attribute_ns_get_prefix_from_quark (GQuark ns)
{
  const gchar *urn;

  if (ns == 0)
    return NULL;

  urn = g_quark_to_string (ns);

  /* fetch an existing prefix, a default prefix, or a newly allocated one *
   * in that order of preference                                          */
  return _attribute_ns_get_prefix (ns, urn);
}

/**
 * wocky_node_attribute_ns_get_prefix_from_urn:
 * @urn: a string containing an URN
 *
 * Gets the prefix of the namespace identified by the URN.
 *
 * Returns: a string containing the prefix of the namespace @urn.
 */
const gchar *
wocky_node_attribute_ns_get_prefix_from_urn (const gchar *urn)
{
  GQuark ns;

  if ((urn == NULL) || (*urn == '\0'))
    return NULL;

  ns = g_quark_from_string (urn);

  /* fetch an existing prefix, a default prefix, or a newly allocated one *
   * in that order of preference                                          */
  return _attribute_ns_get_prefix (ns, urn);
}

/**
 * wocky_node_attribute_ns_set_prefix:
 * @ns: a #GQuark
 * @prefix: a string containing the desired prefix
 *
 * Sets a desired prefix for a namespace.
 */
void
wocky_node_attribute_ns_set_prefix (GQuark ns, const gchar *prefix)
{
  const gchar *urn = g_quark_to_string (ns);

  /* add/replace user prefix table entry */
  _add_prefix_to_table (user_ns_prefixes, ns, urn, prefix);
}

/**
 * wocky_node_set_attribute_n_ns:
 * @node: a #WockyNode
 * @key: the attribute to set
 * @value: the value to set
 * @value_size: the number of bytes of @value to set as a value
 * @ns: a namespace, or %NULL
 *
 * Sets a new attribute to a #WockyNode, with the supplied values.
 * If the namespace is %NULL, this is equivalent to
 * wocky_node_set_attribute_n().
 */
void
wocky_node_set_attribute_n_ns (WockyNode *node, const gchar *key,
    const gchar *value, gsize value_size, const gchar *ns)
{
  Attribute *a = g_slice_new0 (Attribute);
  GSList *link;
  Tuple search;

  a->key = strndup_validated (key, -1);
  a->value = strndup_validated (value, value_size);
  a->prefix = g_strdup (wocky_node_attribute_ns_get_prefix_from_urn (ns));
  a->ns = (ns != NULL) ? g_quark_from_string (ns) : 0;

  /* Remove the old attribute if needed */
  search.key = a->key;
  search.ns = a->ns;
  link = g_slist_find_custom (node->attributes, &search, attribute_compare);
  if (link != NULL)
    {
      Attribute *old = (Attribute *) link->data;
      attribute_free (old);
      node->attributes = g_slist_delete_link (node->attributes, link);
    }

  node->attributes = g_slist_append (node->attributes, a);
}

/**
 * wocky_node_set_attribute_n:
 * @node: a #WockyNode
 * @key: the attribute to set
 * @value: the value to set
 * @value_size: the number of bytes of @value to set as a value
 *
 * Sets a new attribute to a #WockyNode, with the supplied values.
 */
void
wocky_node_set_attribute_n (WockyNode *node, const gchar *key,
    const gchar *value, gsize value_size)
{
  wocky_node_set_attribute_n_ns (node, key, value, value_size, NULL);
}

static gint
node_compare_child (gconstpointer a, gconstpointer b)
{
  const WockyNode *node = (const WockyNode *)a;
  Tuple *target = (Tuple *) b;

  if (target->ns != 0 && target->ns != node->ns)
    return 1;

  if (target->key == NULL)
    return 0;

  return strcmp (node->name, target->key);
}

/**
 * wocky_node_get_child_ns:
 * @node: a #WockyNode
 * @name: the name of the child to get
 * @ns: the namespace of the child to get, or %NULL
 *
 * Gets the child of a node, searching by name and limiting the search
 * to the specified namespace.
 * If the namespace is %NULL, this is equivalent to wocky_node_get_child()
 *
 * Returns: a #WockyNode.
 */
WockyNode *
wocky_node_get_child_ns (WockyNode *node, const gchar *name,
     const gchar *ns)
{
  GSList *link;
  Tuple t;

  /* This secretly works just fine if @name is %NULL, but don't tell anyone!
   * wocky_node_get_first_child_ns() is what people should be using.
   * */
  t.key = name;
  t.ns = (ns != NULL ?  g_quark_from_string (ns) : 0);

  link = g_slist_find_custom (node->children, &t, node_compare_child);

  return (link == NULL) ? NULL : (WockyNode *) (link->data);
}

/**
 * wocky_node_get_child:
 * @node: a #WockyNode
 * @name: the name of the child to get
 *
 * Gets a child of a node, searching by name.
 *
 * Returns: a #WockyNode.
 */
WockyNode *
wocky_node_get_child (WockyNode *node, const gchar *name)
{
  return wocky_node_get_child_ns (node, name, NULL);
}

/**
 * wocky_node_get_first_child:
 * @node: a #WockyNode
 *
 * Convenience function to return the first child of a #WockyNode.
 *
 * Returns: a #WockyNode, or %NULL if @node has no children.
 */
WockyNode *
wocky_node_get_first_child (WockyNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  if (node->children == NULL)
    return NULL;

  return (WockyNode *) node->children->data;
}

/**
 * wocky_node_get_first_child_ns:
 * @node: a #WockyNode
 * @ns: the namespace of the child node you seek.
 *
 * Returns the first child of @node whose namespace is @ns, saving you the
 * bother of faffing around with a #WockyNodeIter.
 *
 * Returns: the first child of @node whose namespace is @ns, or %NULL if none
 *          is found.
 */
WockyNode *
wocky_node_get_first_child_ns (WockyNode *node,
    const gchar *ns)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (ns != NULL, NULL);

  return wocky_node_get_child_ns (node, NULL, ns);
}

/**
 * wocky_node_get_content_from_child:
 * @node: a #WockyNode
 * @name: the name of the child whose content to retrieve
 *
 * Retrieves the content from a child of a node, if it exists.
 *
 * Returns: the content of the child of @node named @name, or %NULL if @node
 *          has no such child.
 */
const gchar *
wocky_node_get_content_from_child (WockyNode *node,
    const gchar *name)
{
  return wocky_node_get_content_from_child_ns (node, name, NULL);
}

/**
 * wocky_node_get_content_from_child_ns:
 * @node: a #WockyNode
 * @name: the name of the child whose content to retrieve
 * @ns: the namespace of the child whose content to retrieve
 *
 * Retrieves the content from a child of a node, if it exists.
 *
 * Returns: the content of the child of @node named @name in @ns, or %NULL if
 *          @node has no such child.
 */
const gchar *wocky_node_get_content_from_child_ns (WockyNode *node,
    const gchar *name,
    const gchar *ns)
{
  WockyNode *child = wocky_node_get_child_ns (node, name, ns);

  if (child == NULL)
    return NULL;
  else
    return child->content;
}

/**
 * wocky_node_add_child:
 * @node: a #WockyNode
 * @name: the name of the child to add
 *
 * Adds a #WockyNode with the specified name to an already existing node.
 *
 * Returns: the newly added #WockyNode.
 */
WockyNode *
wocky_node_add_child (WockyNode *node, const gchar *name)
{
  return wocky_node_add_child_with_content_ns_q (node, name, NULL, 0);
}

/**
 * wocky_node_add_child_ns:
 * @node: a #WockyNode
 * @name: the name of the child to add
 * @ns: a namespace
 *
 * Adds a #WockyNode with the specified name to an already existing node,
 * under the specified namespace. If the namespace is %NULL, this is equivalent
 * to wocky_node_add_child().
 *
 * Returns: the newly added #WockyNode.
 */
WockyNode *
wocky_node_add_child_ns (WockyNode *node, const gchar *name,
    const gchar *ns)
{
  return wocky_node_add_child_with_content_ns (node, name, NULL, ns);
}
/**
 * wocky_node_add_child_ns_q:
 * @node: a #WockyNode
 * @name: the name of the child to add
 * @ns: a namespace
 *
 * Adds a #WockyNode with the specified name to an already existing node,
 * under the specified namespace. If the namespace is 0, this is equivalent
 * to wocky_node_add_child().
 *
 * Returns: the newly added #WockyNode.
 */
WockyNode *
wocky_node_add_child_ns_q (WockyNode *node,
    const gchar *name,
    GQuark ns)
{
  return wocky_node_add_child_with_content_ns_q (node, name, NULL, ns);
}

/**
 * wocky_node_add_child_with_content:
 * @node: a #WockyNode
 * @name: the name of the child to add
 * @content: the content of the child to add
 *
 * Adds a #WockyNode with the specified name and containing the
 * specified content to an already existing node.
 *
 * Returns: the newly added #WockyNode.
 */
WockyNode *
wocky_node_add_child_with_content (WockyNode *node,
     const gchar *name, const char *content)
{
  return wocky_node_add_child_with_content_ns_q (node, name,
      content, 0);
}

/**
 * wocky_node_add_child_with_content_ns:
 * @node: a #WockyNode
 * @name: the name of the child to add
 * @content: the content of the child to add
 * @ns: a namespace
 *
 * Adds a #WockyNode with the specified name and the specified content
 * to an already existing node, under the specified namespace.
 * If the namespace is %NULL, this is equivalent to
 * wocky_node_add_child_with_content().
 *
 * Returns: the newly added #WockyNode.
 */
WockyNode *
wocky_node_add_child_with_content_ns (WockyNode *node,
    const gchar *name, const gchar *content, const gchar *ns)
{
  return wocky_node_add_child_with_content_ns_q (node, name, content,
    ns != NULL ? g_quark_from_string (ns) : 0);
}

/**
 * wocky_node_add_child_with_content_ns_q:
 * @node: a #WockyNode
 * @name: the name of the child to add
 * @content: the content of the child to add
 * @ns: a namespace
 *
 * Adds a #WockyNode with the specified name and the specified content
 * to an already existing node, under the specified namespace.
 * If the namespace is 0, this is equivalent to
 * wocky_node_add_child_with_content().
 *
 * Returns: the newly added #WockyNode.
 */
WockyNode *
wocky_node_add_child_with_content_ns_q (WockyNode *node,
    const gchar *name, const gchar *content, GQuark ns)
{
  WockyNode *result = new_node (name, ns != 0 ? ns : node->ns);

  wocky_node_set_content (result, content);

  node->children = g_slist_append (node->children, result);
  return result;
}

/**
 * wocky_node_get_ns:
 * @node: a #WockyNode
 *
 * Gets the namespace of a #WockyNode
 *
 * Returns: a string containing the namespace of the node.
 */
const gchar *
wocky_node_get_ns (WockyNode *node)
{
  return g_quark_to_string (node->ns);
}

gboolean
wocky_node_has_ns (WockyNode *node, const gchar *ns)
{
  return wocky_node_has_ns_q (node, g_quark_try_string (ns));
}

gboolean
wocky_node_has_ns_q (WockyNode *node, GQuark ns)
{
  return node->ns == ns;
}

/**
 * wocky_node_matches_q:
 * @node: a #WockyNode
 * @name: the expected element name, which may not be %NULL
 * @ns: the expected element namespace, which may not be 0
 *
 * Checks whether a node has a particular name and namespace.
 *
 * Returns: %TRUE if @node is named @name, in namespace @ns.
 */
gboolean
wocky_node_matches_q (
    WockyNode *node,
    const gchar *name,
    GQuark ns)
{
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (ns != 0, FALSE);

  if (wocky_strdiff (node->name, name))
    return FALSE;

  return wocky_node_has_ns_q (node, ns);
}

/**
 * wocky_node_matches:
 * @node: a #WockyNode
 * @name: the expected element name, which may not be %NULL
 * @ns: the expected element namespace, which may not be %NULL
 *
 * Checks whether a node has a particular name and namespace.
 *
 * Returns: %TRUE if @node is named @name, in namespace @ns.
 */
gboolean
wocky_node_matches (
    WockyNode *node,
    const gchar *name,
    const gchar *ns)
{
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (ns != NULL, FALSE);

  return wocky_node_matches_q (node, name, g_quark_try_string (ns));
}

/**
 * wocky_node_get_language:
 * @node: a #WockyNode
 *
 * Gets the language of a #WockyNode
 *
 * Returns: a string containing the language of the node.
 */
const gchar *
wocky_node_get_language (WockyNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  return node->language;
}

/**
 * wocky_node_set_language_n:
 * @node: a #WockyNode
 * @lang: a language
 * @lang_size: the length of @lang, in bytes.
 *
 * Sets the language of a #WockyNode.
 */
void
wocky_node_set_language_n (WockyNode *node, const gchar *lang,
    gsize lang_size)
{
  g_free (node->language);
  node->language = strndup_validated (lang, lang_size);
}

/**
 * wocky_node_set_language:
 * @node: a #WockyNode
 * @lang: a %NULL-terminated string containing the language
 *
 * Sets the language of a #WockyNode.
 */
void
wocky_node_set_language (WockyNode *node, const gchar *lang)
{
  gsize lang_size = 0;
  if (lang != NULL) {
    lang_size = strlen (lang);
  }
  wocky_node_set_language_n (node, lang, lang_size);
}

/**
 * wocky_node_set_content:
 * @node: a #WockyNode
 * @content: the content to set to the node
 *
 * Sets the content of a #WockyNode.
 */
void
wocky_node_set_content (WockyNode *node, const gchar *content)
{
  g_free (node->content);
  node->content = strndup_validated (content, -1);
}

/**
 * wocky_node_append_content:
 * @node: a #WockyNode
 * @content: the content to append to the node
 *
 * Appends some content to the content of a #WockyNode.
 */
void
wocky_node_append_content (WockyNode *node,
    const gchar *content)
{
  gchar *t = node->content;
  node->content = concat_validated (t, content, -1);
  g_free (t);
}

/**
 * wocky_node_append_content_n:
 * @node: a #WockyNode
 * @content: the content to append to the node
 * @size: the size of the content to append
 *
 * Appends a specified number of content bytes to the content of a
 * #WockyNode.
 */
void
wocky_node_append_content_n (WockyNode *node, const gchar *content,
    gsize size)
{
  gchar *t = node->content;
  node->content = concat_validated (t, content, size);
  g_free (t);
}

static gboolean
attribute_to_string (const gchar *key, const gchar *value,
    const gchar *prefix, const gchar *ns,
    gpointer user_data)
{
  GString *str = user_data;

  g_string_append_c (str, ' ');
  if (ns != NULL)
    g_string_append_printf (str, "xmlns:%s='%s' ", prefix, ns);

  if (prefix != NULL)
    {
      g_string_append (str, prefix);
      g_string_append_c (str, ':');
    }
  g_string_append_printf (str, "%s='%s'", key, value);

  return TRUE;
}

static gboolean
node_to_string (WockyNode *node,
    GQuark parent_ns,
    const gchar *prefix,
    GString *str)
{
  GSList *l;
  gchar *nprefix;

  g_string_append_printf (str, "%s* %s", prefix, node->name);

  if (parent_ns != node->ns)
    {
      const gchar *ns = wocky_node_get_ns (node);
      g_string_append_printf (str, " xmlns='%s'", ns);
    }

  wocky_node_each_attribute (node, attribute_to_string, str);
  g_string_append_c (str, '\n');

  nprefix = g_strdup_printf ("%s    ", prefix);
  if (node->content != NULL && *node->content != '\0')
    g_string_append_printf (str, "%s\"%s\"\n", nprefix, node->content);

  for (l = node->children ; l != NULL; l = g_slist_next (l))
    node_to_string (l->data, node->ns, nprefix, str);

  g_free (nprefix);

  return TRUE;
}

/**
 * wocky_node_to_string:
 * @node: a #WockyNode
 *
 * Obtains a string representation of a #WockyNode.
 *
 * Returns: a newly allocated string containing a serialization of
 * the node.
 */
gchar *
wocky_node_to_string (WockyNode *node)
{
  GString *str;
  gchar *result;

  str = g_string_new ("");
  node_to_string (node, 0, "", str);

  g_string_truncate (str, str->len - 1);
  result = str->str;
  g_string_free (str, FALSE);
  return result;
}

/**
 * wocky_node_equal:
 * @node0: a #WockyNode
 * @node1: a #WockyNode to compare to @node0
 *
 * Compares two #WockyNode<!-- -->s for equality.
 *
 * Returns: %TRUE if the two nodes are equal.
 */
gboolean
wocky_node_equal (WockyNode *node0,
    WockyNode *node1)
{
  GSList *l0, *l1;

  if (wocky_strdiff (node0->name, node1->name))
    return FALSE;

  if (wocky_strdiff (node0->content, node1->content))
    return FALSE;

  if (wocky_strdiff (node0->language, node1->language))
    return FALSE;

  if (node0->ns != node1->ns)
    return FALSE;

  if (g_slist_length (node0->attributes) != g_slist_length (node1->attributes))
    return FALSE;

  /* Compare attributes */
  for (l0 = node0->attributes ; l0 != NULL;  l0 = g_slist_next (l0))
    {
      Attribute *a = (Attribute *) l0->data;
      const gchar *c;

      c = wocky_node_get_attribute_ns (node1, a->key,
        a->ns == 0 ? NULL : g_quark_to_string (a->ns));

      if (wocky_strdiff (a->value, c))
        return FALSE;
    }

  /* Recursively compare children, order matters */
  for (l0 = node0->children, l1 = node1->children ;
      l0 != NULL && l1 != NULL;
      l0 = g_slist_next (l0), l1 = g_slist_next (l1))
    {
      WockyNode *c0 = (WockyNode *) l0->data;
      WockyNode *c1 = (WockyNode *) l1->data;

      if (!wocky_node_equal (c0, c1))
        return FALSE;
    }

  if (l0 != NULL || l1 != NULL)
    return FALSE;

  return TRUE;
}

/**
 * wocky_node_is_superset:
 * @node: the #WockyNode to test
 * @subset: the supposed subset
 *
 * Returns: %TRUE if @node is a superset of @subset.
 */
gboolean
wocky_node_is_superset (WockyNode *node,
    WockyNode *subset)
{
  GSList *l;

  if (subset == NULL)
    /* We are always a superset of nothing */
    return TRUE;

  if (node == NULL)
    /* subset is not NULL so we are not a superset */
    return FALSE;

  if (wocky_strdiff (node->name, subset->name))
    /* Node name doesn't match */
    return FALSE;

  if (subset->ns != 0 &&
      node->ns != subset->ns)
    /* Namespace doesn't match */
    return FALSE;

  if (subset->content != NULL &&
      wocky_strdiff (node->content, subset->content))
    /* Content doesn't match */
    return FALSE;

  /* Check attributes */
  for (l = subset->attributes; l != NULL;  l = g_slist_next (l))
    {
      Attribute *a = (Attribute *) l->data;
      const gchar *c;

      c = wocky_node_get_attribute_ns (node, a->key,
        a->ns == 0 ? NULL : g_quark_to_string (a->ns));

      if (wocky_strdiff (a->value, c))
        return FALSE;
    }

  /* Recursively check children; order doesn't matter */
  for (l = subset->children; l != NULL; l = g_slist_next (l))
    {
      WockyNode *pattern_child = (WockyNode *) l->data;
      WockyNode *node_child;

      node_child = wocky_node_get_child_ns (node, pattern_child->name,
          wocky_node_get_ns (pattern_child));

      if (!wocky_node_is_superset (node_child, pattern_child))
        return FALSE;
    }

  return TRUE;
}

/**
 * wocky_node_iter_init:
 * @iter: unitialized iterator
 * @node: Node whose children to iterate over
 * @name: Name to filter on or %NULL
 * @ns: namespace to filter on or %NULL
 *
 * Initializes an iterator that can be used to iterate over the children of
 * @node, filtered by @name and @ns
 * |[
 * WockyNodeIter iter;
 * WockyNode *child;
 *
 * wocky_node_iter_init (&iter, wocky_stanza_get_top_node (stanza),
 *    "payload-type",
 *    WOCKY_XMPP_NS_JINGLE_RTP);
 * while (wocky_node_iter_next (iter, &child))
 *   {
 *     /<!-- -->* do something with the child *<!-- -->/
 *   }
 * ]|
 */
void
wocky_node_iter_init (WockyNodeIter *iter,
    WockyNode *node,
    const gchar *name,
    const gchar *ns)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (node != NULL);

  iter->node = node;
  iter->pending = node->children;
  iter->current = NULL;
  iter->name = name;
  iter->ns = g_quark_from_string (ns);
}

/**
 * wocky_node_iter_next:
 * @iter: an initialized WockyNodeIter
 * @next: a location to store the next child
 *
 * Advances iter to the next child that matches its filter. if %FALSE is
 * returned next is not set and the iterator becomes invalid
 *
 * Returns: %FALSE if the last child has been reached
 *
 */
gboolean
wocky_node_iter_next (WockyNodeIter *iter,
    WockyNode **next)
{
  while (iter->pending != NULL)
    {
      WockyNode *ln = (WockyNode *) iter->pending->data;

      iter->current = iter->pending;
      iter->pending = g_slist_next (iter->pending);

      if (iter->name != NULL && wocky_strdiff (ln->name, iter->name))
        continue;

      if (iter->ns != 0 && iter->ns != ln->ns)
        continue;

      if (next != NULL)
        *next = ln;

      return TRUE;
    }

  iter->current = NULL;
  return FALSE;
}

/**
 * wocky_node_iter_remove:
 * @iter: an initialized #WockyNodeIter
 *
 * Removes and frees the node returned by the last call to
 * wocky_node_iter_next() from its parent. Can only be called after
 * wocky_node_iter_next() returned %TRUE, and cannot be called more than once
 * per successful call to wocky_node_iter_next().
 */
void
wocky_node_iter_remove (WockyNodeIter *iter)
{
  g_return_if_fail (iter->node != NULL);
  g_return_if_fail (iter->current != NULL);

  g_assert (iter->current->data != NULL);
  wocky_node_free (iter->current->data);

  iter->node->children = g_slist_delete_link (iter->node->children,
      iter->current);

  iter->current = NULL;
}

/**
 * wocky_node_add_build:
 * @node: The node under which to add a new subtree
 * @...: the description of the stanza to build,
 *  terminated with %NULL
 *
 * Add a node subtree to an existing parent node.
 * <example><programlisting>
 * wocky_node_add_build (node,
 *    '(', "body",
 *        '$', "Telepathy rocks!",
 *    ')',
 *   NULL);
 * </programlisting></example>
 *
 * The above examples adds the following subtree under the given node:
 * <programlisting language="xml">
 * &lt;body&gt;
 *    Telepathy rocks!
 * &lt;/body&gt;
 * </programlisting>
 *
 */
void
wocky_node_add_build (WockyNode *node,
    ...)
{
  va_list ap;

  va_start (ap, node);
  wocky_node_add_build_va (node, ap);
  va_end (ap);
}

void
wocky_node_add_build_va (WockyNode *node, va_list ap)
{
  GSList *stack = NULL;
  WockyNodeBuildTag arg;

  stack = g_slist_prepend (stack, node);

  while ((arg = va_arg (ap, WockyNodeBuildTag)) != 0)
    {
      switch (arg)
        {
        case WOCKY_NODE_ATTRIBUTE:
          {
            gchar *key = va_arg (ap, gchar *);
            gchar *value = va_arg (ap, gchar *);

            g_assert (key != NULL);
            g_assert (value != NULL);
            g_assert (stack != NULL);
            wocky_node_set_attribute (stack->data, key, value);
          }
          break;

        case WOCKY_NODE_START:
          {
            gchar *name = va_arg (ap, gchar *);
            WockyNode *child;

            g_assert (name != NULL);
            g_assert (stack != NULL);
            child = wocky_node_add_child (stack->data, name);
            stack = g_slist_prepend (stack, child);
          }
          break;

        case WOCKY_NODE_TEXT:
          {
            gchar *txt = va_arg (ap, gchar *);

            g_assert (stack != NULL);
            wocky_node_set_content (stack->data, txt);
          }
          break;

        case WOCKY_NODE_XMLNS:
          {
            gchar *ns = va_arg (ap, gchar *);

            g_assert (ns != NULL);
            g_assert (stack != NULL);
            ((WockyNode *) stack->data)->ns = g_quark_from_string (ns);
          }
          break;

        case WOCKY_NODE_LANGUAGE:
          {
            gchar *lang = va_arg (ap, gchar *);

            g_assert (lang != NULL);
            wocky_node_set_language ((WockyNode *) stack->data, lang);
          }
          break;

        case WOCKY_NODE_END:
          {
            /* delete the top of the stack */
            stack = g_slist_delete_link (stack, stack);
            /* If you put too many ')'s at the end of your build spec, we just
             * warn; if you actually try to do anything else having fallen off
             * the end, we'll assert in the relevant branch. */
            g_warn_if_fail (stack != NULL);
          }
          break;

        case WOCKY_NODE_ASSIGN_TO:
          {
            WockyNode **dest = va_arg (ap, WockyNode **);

            g_assert (dest != NULL);
            g_assert (stack != NULL);
            *dest = stack->data;
          }
          break;

        default:
          g_critical ("unknown build tag %c", arg);
          g_assert_not_reached ();
        }
    }

  if (G_UNLIKELY (stack != NULL && stack->data != node))
    {
      GString *still_open = g_string_new ("");

      while (stack != NULL && stack->data != node)
        {
          WockyNode *unclosed = stack->data;

          g_string_append_printf (still_open, "</%s> ", unclosed->name);
          stack = stack->next;
        }

      g_warning ("improperly nested build spec! unclosed: %s", still_open->str);
      g_string_free (still_open, TRUE);
    }

  g_slist_free (stack);
}

WockyNode *
_wocky_node_copy (WockyNode *node)
{
  WockyNode *result = new_node (node->name, node->ns);
  GSList *l;

  result->content = g_strdup (node->content);
  result->language = g_strdup (node->language);

  for (l = node->attributes ; l != NULL; l = g_slist_next (l))
    {
      Attribute *a = l->data;
      Attribute *b = g_slice_new0 (Attribute);

      b->key = g_strdup (a->key);
      b->value = g_strdup (a->value);
      b->prefix = g_strdup (a->prefix);
      b->ns = a->ns;

      result->attributes = g_slist_append (result->attributes, b);
    }

  for (l = node->children ; l != NULL; l = g_slist_next (l))
    result->children = g_slist_append (result->children,
      _wocky_node_copy ((WockyNode *) l->data));

  return result;
}

/**
 * wocky_node_add_node_tree:
 * @node: A node
 * @tree: The node tree to add
 *
 * Copies the nodes from @tree, and appends them to @node's children.
 *
 * Returns: the root of the copy of @tree added to @node.
 */
WockyNode *
wocky_node_add_node_tree (WockyNode *node, WockyNodeTree *tree)
{
  WockyNode *copy;

  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (tree != NULL, NULL);

  copy = _wocky_node_copy (wocky_node_tree_get_top_node (tree));
  node->children = g_slist_append (node->children, copy);

  return copy;
}

/**
 * wocky_node_prepend_node_tree:
 * @node: a node
 * @tree: the node tree to prepend to @node's children
 *
 * Copies the nodes from @tree, and inserts them as the first child of @node,
 * before any existing children.
 *
 * Returns: the root of the copy of @tree added to @node.
 */
WockyNode *
wocky_node_prepend_node_tree (
    WockyNode *node,
    WockyNodeTree *tree)
{
  WockyNode *copy;

  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (tree != NULL, NULL);

  copy = _wocky_node_copy (wocky_node_tree_get_top_node (tree));
  node->children = g_slist_prepend (node->children, copy);

  return copy;
}

/**
 * wocky_node_init:
 *
 * Initializes the caches used by #WockyNode.
 * This should be always called before using #WockyNode structs.
 */
void
wocky_node_init ()
{
  _init_user_prefix_table ();
  _init_default_prefix_table ();
}

/**
 * wocky_node_deinit:
 *
 * Releases all the resources used by the #WockyNode caches.
 */
void
wocky_node_deinit ()
{
  g_hash_table_unref (user_ns_prefixes);
  g_hash_table_unref (default_ns_prefixes);
}
