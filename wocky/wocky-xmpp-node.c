/*
 * wocky-xmpp-node.c - Code for Wocky xmpp nodes
 * Copyright (C) 2006 Collabora Ltd.
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

#include <glib.h>
#include <string.h>

#include "wocky-xmpp-node.h"
#include "wocky-utils.h"
#include "wocky-namespaces.h"

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

WockyXmppNode *
wocky_xmpp_node_new (const char *name)
{
  WockyXmppNode *result = g_slice_new0 (WockyXmppNode);

  result->name = g_strdup (name);

  return result;
}

static void
attribute_free (Attribute *a)
{
  g_free (a->key);
  g_free (a->value);
  g_free (a->prefix);
  g_slice_free (Attribute, a);
}

/* Frees the node and all it's children */
void
wocky_xmpp_node_free (WockyXmppNode *node)
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
      wocky_xmpp_node_free ((WockyXmppNode *) l->data);
    }
  g_slist_free (node->children);

  for (l = node->attributes; l != NULL ; l = l->next)
    {
      Attribute *a = (Attribute *) l->data;
      attribute_free (a);
    }
  g_slist_free (node->attributes);

  g_slice_free (WockyXmppNode, node);
}

void
wocky_xmpp_node_each_attribute (WockyXmppNode *node,
    wocky_xmpp_node_each_attr_func func, gpointer user_data)
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

void
wocky_xmpp_node_each_child (WockyXmppNode *node,
    wocky_xmpp_node_each_child_func func, gpointer user_data)
{
  GSList *l;

  for (l = node->children; l != NULL ; l = l->next)
    {
      WockyXmppNode *n = (WockyXmppNode *) l->data;
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

const gchar *
wocky_xmpp_node_get_attribute_ns (WockyXmppNode *node,
    const gchar *key, const gchar *ns)
{
  GSList *link;
  Tuple search;

  search.key = (gchar *) key;
  search.ns = (ns != NULL ? g_quark_from_string (ns) : 0);

  link = g_slist_find_custom (node->attributes, &search, attribute_compare);

  return (link == NULL) ? NULL : ((Attribute *) (link->data))->value;
}

const gchar *
wocky_xmpp_node_get_attribute (WockyXmppNode *node, const gchar *key)
{
  return wocky_xmpp_node_get_attribute_ns (node, key, NULL);
}

void
wocky_xmpp_node_set_attribute (WockyXmppNode *node,
    const gchar *key, const gchar *value)
{
  g_assert (value != NULL);
  wocky_xmpp_node_set_attribute_n_ns (node, key, value, strlen (value), NULL);
}

void
wocky_xmpp_node_set_attribute_ns (WockyXmppNode *node, const gchar *key,
    const gchar *value, const gchar *ns)
{
  wocky_xmpp_node_set_attribute_n_ns (node,
      key, value, strlen (value), ns);
}

static NSPrefix *
ns_prefix_new (const gchar *urn,
    GQuark ns,
    const gchar *prefix)
{
  NSPrefix *nsp = g_slice_new0 (NSPrefix);
  nsp->ns_urn = urn;
  nsp->prefix = g_strdup (prefix);
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

const gchar *
wocky_xmpp_node_attribute_ns_get_prefix_from_quark (GQuark ns)
{
  const gchar *urn;

  if (ns == 0)
    return NULL;

  urn = g_quark_to_string (ns);

  /* fetch an existing prefix, a default prefix, or a newly allocated one *
   * in that order of preference                                          */
  return _attribute_ns_get_prefix (ns, urn);
}

const gchar *
wocky_xmpp_node_attribute_ns_get_prefix_from_urn (const gchar *urn)
{
  GQuark ns;

  if ((urn == NULL) || (*urn == '\0'))
    return NULL;

  ns = g_quark_from_string (urn);

  /* fetch an existing prefix, a default prefix, or a newly allocated one *
   * in that order of preference                                          */
  return _attribute_ns_get_prefix (ns, urn);
}

void
wocky_xmpp_node_attribute_ns_set_prefix (GQuark ns, const gchar *prefix)
{
  const gchar *urn = g_quark_to_string (ns);

  /* add/replace user prefix table entry */
  _add_prefix_to_table (user_ns_prefixes, ns, urn, prefix);
}

void
wocky_xmpp_node_set_attribute_n_ns (WockyXmppNode *node, const gchar *key,
    const gchar *value, gsize value_size, const gchar *ns)
{
  Attribute *a = g_slice_new0 (Attribute);
  GSList *link;
  Tuple search;

  a->key = g_strdup (key);
  a->value = g_strndup (value, value_size);
  a->prefix = g_strdup (wocky_xmpp_node_attribute_ns_get_prefix_from_urn (ns));
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

void
wocky_xmpp_node_set_attribute_n (WockyXmppNode *node, const gchar *key,
    const gchar *value, gsize value_size)
{
  wocky_xmpp_node_set_attribute_n_ns (node, key, value, value_size, NULL);
}

static gint
node_compare_child (gconstpointer a, gconstpointer b)
{
  const WockyXmppNode *node = (const WockyXmppNode *)a;
  Tuple *target = (Tuple *) b;

  if (target->ns != 0 && target->ns != node->ns)
    {
      return 1;
    }

  return strcmp (node->name, target->key);
}

WockyXmppNode *
wocky_xmpp_node_get_child_ns (WockyXmppNode *node, const gchar *name,
     const gchar *ns)
{
  GSList *link;
  Tuple t;

  t.key = name;
  t.ns = (ns != NULL ?  g_quark_from_string (ns) : 0);

  link = g_slist_find_custom (node->children, &t, node_compare_child);

  return (link == NULL) ? NULL : (WockyXmppNode *) (link->data);
}

WockyXmppNode *
wocky_xmpp_node_get_child (WockyXmppNode *node, const gchar *name)
{
  return wocky_xmpp_node_get_child_ns (node, name, NULL);
}

WockyXmppNode *
wocky_xmpp_node_get_first_child (WockyXmppNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (node->children != NULL, NULL);
  return (WockyXmppNode *) node->children->data;
}

WockyXmppNode *
wocky_xmpp_node_add_child (WockyXmppNode *node, const gchar *name)
{
  return wocky_xmpp_node_add_child_with_content_ns (node, name, NULL, NULL);
}

WockyXmppNode *
wocky_xmpp_node_add_child_ns (WockyXmppNode *node, const gchar *name,
    const gchar *ns)
{
  return wocky_xmpp_node_add_child_with_content_ns (node, name, NULL, ns);
}

WockyXmppNode *
wocky_xmpp_node_add_child_with_content (WockyXmppNode *node,
     const gchar *name, const char *content)
{
  return wocky_xmpp_node_add_child_with_content_ns (node, name,
      content, NULL);
}

WockyXmppNode *
wocky_xmpp_node_add_child_with_content_ns (WockyXmppNode *node,
    const gchar *name, const gchar *content, const gchar *ns)
{
  WockyXmppNode *result = wocky_xmpp_node_new (name);

  wocky_xmpp_node_set_content (result, content);
  if (ns != NULL)
    wocky_xmpp_node_set_ns (result, ns);
  else
    result->ns = node->ns;

  node->children = g_slist_append (node->children, result);
  return result;
}

void
wocky_xmpp_node_set_ns (WockyXmppNode *node, const gchar *ns)
{
  node->ns = (ns != NULL) ? g_quark_from_string (ns) : 0;
}

const gchar *
wocky_xmpp_node_get_ns (WockyXmppNode *node)
{
  return g_quark_to_string (node->ns);
}

const gchar *
wocky_xmpp_node_get_language (WockyXmppNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  return node->language;
}

void
wocky_xmpp_node_set_language_n (WockyXmppNode *node, const gchar *lang,
    gsize lang_size)
{
  g_free (node->language);
  node->language = g_strndup (lang, lang_size);
}

void
wocky_xmpp_node_set_language (WockyXmppNode *node, const gchar *lang)
{
  gsize lang_size = 0;
  if (lang != NULL) {
    lang_size = strlen (lang);
  }
  wocky_xmpp_node_set_language_n (node, lang, lang_size);
}


void
wocky_xmpp_node_set_content (WockyXmppNode *node, const gchar *content)
{
  g_free (node->content);
  node->content = g_strdup (content);
}

void wocky_xmpp_node_append_content (WockyXmppNode *node,
    const gchar *content)
{
  gchar *t = node->content;
  node->content = g_strconcat (t, content, NULL);
  g_free (t);
}

void
wocky_xmpp_node_append_content_n (WockyXmppNode *node, const gchar *content,
    gsize size)
{
  gsize csize = node->content != NULL ? strlen (node->content) : 0;
  node->content = g_realloc (node->content, csize + size + 1);
  g_strlcpy (node->content + csize, content, size + 1);
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
node_to_string (WockyXmppNode *node,
    GQuark parent_ns,
    const gchar *prefix,
    GString *str)
{
  GSList *l;
  gchar *nprefix;

  g_string_append_printf (str, "%s* %s", prefix, node->name);

  if (parent_ns != node->ns)
    {
      const gchar *ns = wocky_xmpp_node_get_ns (node);
      g_string_append_printf (str, " xmlns='%s'", ns);
    }

  wocky_xmpp_node_each_attribute (node, attribute_to_string, str);
  g_string_append_c (str, '\n');

  nprefix = g_strdup_printf ("%s    ", prefix);
  if (node->content != NULL && *node->content != '\0')
    g_string_append_printf (str, "%s\"%s\"\n", nprefix, node->content);

  for (l = node->children ; l != NULL; l = g_slist_next (l))
    node_to_string (l->data, node->ns, nprefix, str);

  g_free (nprefix);

  return TRUE;
}

gchar *
wocky_xmpp_node_to_string (WockyXmppNode *node)
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

gboolean
wocky_xmpp_node_equal (WockyXmppNode *node0,
    WockyXmppNode *node1)
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

      c = wocky_xmpp_node_get_attribute_ns (node1, a->key,
        a->ns == 0 ? NULL : g_quark_to_string (a->ns));

      if (wocky_strdiff (a->value, c))
        return FALSE;
    }

  /* Recursively compare children, order matters */
  for (l0 = node0->children, l1 = node1->children ;
      l0 != NULL && l1 != NULL;
      l0 = g_slist_next (l0), l1 = g_slist_next (l1))
    {
      WockyXmppNode *c0 = (WockyXmppNode *) l0->data;
      WockyXmppNode *c1 = (WockyXmppNode *) l1->data;

      if (!wocky_xmpp_node_equal (c0, c1))
        return FALSE;
    }

  if (l0 != NULL || l1 != NULL)
    return FALSE;

  return TRUE;
}

/**
 * wocky_xmpp_node_is_superset:
 * @node: the #WockyXmppNode to test
 * @subset: the supposed subset
 *
 * Returns: %TRUE if @node is a superset of @subset.
 */
gboolean
wocky_xmpp_node_is_superset (WockyXmppNode *node,
    WockyXmppNode *subset)
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

      c = wocky_xmpp_node_get_attribute_ns (node, a->key,
        a->ns == 0 ? NULL : g_quark_to_string (a->ns));

      if (wocky_strdiff (a->value, c))
        return FALSE;
    }

  /* Recursively check children; order doesn't matter */
  for (l = subset->children; l != NULL; l = g_slist_next (l))
    {
      WockyXmppNode *pattern_child = (WockyXmppNode *) l->data;
      WockyXmppNode *node_child;

      node_child = wocky_xmpp_node_get_child_ns (node, pattern_child->name,
          wocky_xmpp_node_get_ns (pattern_child));

      if (!wocky_xmpp_node_is_superset (node_child, pattern_child))
        return FALSE;
    }

  return TRUE;
}

void
wocky_xmpp_node_init ()
{
  _init_user_prefix_table ();
  _init_default_prefix_table ();
}

void
wocky_xmpp_node_deinit ()
{
  g_hash_table_unref (user_ns_prefixes);
  g_hash_table_unref (default_ns_prefixes);
}
