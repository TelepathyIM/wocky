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
  wocky_xmpp_node_set_attribute_n_ns (node, key, value, strlen (value),
      NULL, NULL);
}

void
wocky_xmpp_node_set_attribute_ns (WockyXmppNode *node, const gchar *key,
    const gchar *value, const gchar *prefix, const gchar *ns)
{
  wocky_xmpp_node_set_attribute_n_ns (node,
      key, value, strlen (value), prefix, ns);
}

void
wocky_xmpp_node_set_attribute_n_ns (WockyXmppNode *node, const gchar *key,
    const gchar *value, gsize value_size, const gchar *prefix, const gchar *ns)
{
  Attribute *a = g_slice_new0 (Attribute);
  GSList *link;
  Tuple search;

  a->key = g_strdup (key);
  a->value = g_strndup (value, value_size);
  a->prefix = g_strdup (prefix);
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
  wocky_xmpp_node_set_attribute_n_ns (node, key, value, value_size, NULL, NULL);
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

/**
 * wocky_xmpp_node_unpack_error - given an XMPP Stanza error #WockyXmppNode
 * (see RFC 3920) this function extracts useful error info:
 *
 * @type: gchar ** into which to write the XMPP Stanza error type
 * @text: #WockyXmppNode ** to hold the node containing the error description
 * @orig: #WockyXmppNode ** to hold the original XMPP Stanza that triggered
 *        the error: XMPP does not require this to be provided in the error
 * @extra: #WockyXmppNode ** to hold any extra domain-specific XML tags
 *         for the error received.
 *
 * The above parameters are all optional, pass NULL to ignore them.
 *
 * The above data are all optional in XMPP, except for @type, which
 * the XMPP spec requires in all stanza errors. See RFC 3920 [9.3.2].
 *
 * None of the above parameters need be freed, they are owned by the
 * parent #WockyXmppNode @node.
 *
 * Returns: a const gchar * indicating the error condition
 */

const gchar *
wocky_xmpp_node_unpack_error (WockyXmppNode *node,
    const gchar **type,
    WockyXmppNode **text,
    WockyXmppNode **orig,
    WockyXmppNode **extra)
{
  WockyXmppNode *error = NULL;
  WockyXmppNode *mesg = NULL;
  WockyXmppNode *xtra = NULL;
  const gchar *cond = NULL;
  GSList *child = NULL;
  GQuark stanza = g_quark_from_string (WOCKY_XMPP_NS_STANZAS);

  g_assert (node != NULL);

  error = wocky_xmpp_node_get_child (node, "error");

  /* not an error? weird, in any case */
  if (error == NULL)
    return NULL;

  if (type != NULL)
    *type = wocky_xmpp_node_get_attribute (error, "type");

  for (child = error->children; child != NULL; child = g_slist_next (child))
    {
      WockyXmppNode *c = child->data;
      if (c->ns != stanza)
        xtra = c;
      else if (wocky_strdiff (c->name, "text"))
        {
          cond = c->name;
        }
      else
        mesg = c;
    }

  if (text != NULL)
    *text = mesg;

  if (extra != NULL)
    *extra = xtra;

  if (orig != NULL)
    {
      WockyXmppNode *first = wocky_xmpp_node_get_first_child (node);
      if (first != error)
        *orig = first;
      else
        *orig = NULL;
    }

  return cond;
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

typedef struct
{
  GString *string;
  gchar *indent;
} _NodeToStringData;

#include <stdio.h>
static gboolean
attribute_to_string (const gchar *key, const gchar *value,
    const gchar *prefix, const gchar *ns,
    gpointer user_data)
{
  _NodeToStringData *data = user_data;

  //fprintf (stderr, "\n\n-- %s.%s = '%s' --\n", ns, key, value);

  g_string_append_c (data->string, ' ');
  if (ns != NULL)
    g_string_append_printf (data->string, "xmlns:%s='%s' ", prefix, ns);

  if (prefix != NULL)
    {
      g_string_append (data->string, prefix);
      g_string_append_c (data->string, ':');
    }
  g_string_append_printf (data->string, "%s='%s'", key, value);

  return TRUE;
}

static gboolean
node_to_string (WockyXmppNode *node, gpointer user_data)
{
  _NodeToStringData *data = user_data;
  gchar *old_indent;
  const gchar *ns;

  g_string_append_printf (data->string, "%s<%s", data->indent, node->name);
  ns = wocky_xmpp_node_get_ns (node);

  if (ns != NULL)
    g_string_append_printf (data->string, " xmlns='%s'", ns);

  wocky_xmpp_node_each_attribute (node, attribute_to_string, data);
  g_string_append_printf (data->string, ">\n");

  old_indent = data->indent;
  data->indent = g_strconcat (data->indent, "  ", NULL);

  if (node->content != NULL)
    g_string_append_printf (data->string, "%s%s\n", data->indent,
        node->content);

  wocky_xmpp_node_each_child (node, node_to_string, data);
  g_free (data->indent);
  data->indent = old_indent;

  g_string_append_printf (data->string, "%s</%s>", data->indent, node->name);
  if (data->indent[0] != '\0')
    g_string_append_c (data->string, '\n');

  return TRUE;
}

gchar *
wocky_xmpp_node_to_string (WockyXmppNode *node)
{
  _NodeToStringData data;
  gchar *result;

  data.string = g_string_new ("");
  data.indent = "";
  node_to_string (node, &data);
  g_string_append_c (data.string, '\n');

  result = data.string->str;
  g_string_free (data.string, FALSE);
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
