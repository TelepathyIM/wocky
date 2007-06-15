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

typedef struct {
  gchar *key;
  gchar *value;
  GQuark ns;
} Attribute;

typedef struct {
  const gchar *key;
  GQuark ns;
} Tuple;

WockyXmppNode *
wocky_xmpp_node_new(const char *name) {
  WockyXmppNode *result = g_slice_new0(WockyXmppNode);

  result->name = g_strdup(name);

  return result;
}

/* Frees the node and all it's children */
void 
wocky_xmpp_node_free(WockyXmppNode *node) {
  GSList *l;

  if (node == NULL)  {
    return ;
  }

  g_free(node->name);
  g_free(node->content);
  g_free(node->language);

  for (l = node->children; l != NULL ; l = l->next) {
    wocky_xmpp_node_free((WockyXmppNode *)l->data);
  }
  g_slist_free(node->children);

  for (l = node->attributes; l != NULL ; l = l->next) {
    Attribute *a = (Attribute *)l->data;
    g_free(a->key);
    g_free(a->value);
    g_slice_free(Attribute, a);
  }
  g_slist_free(node->attributes);

  g_slice_free(WockyXmppNode, node);
}

void 
wocky_xmpp_node_each_attribute(WockyXmppNode *node,
                               wocky_xmpp_node_each_attr_func func,
                               gpointer user_data) {
  GSList *l;
  for (l = node->attributes; l != NULL ; l = l->next) {
    Attribute *a = (Attribute *)l->data;
    if (!func(a->key, a->value, g_quark_to_string(a->ns), user_data)) {
      return;
    }
  }
}

void 
wocky_xmpp_node_each_child(WockyXmppNode *node,
                           wocky_xmpp_node_each_child_func func,
                           gpointer user_data) {
  GSList *l;
  for (l = node->children; l != NULL ; l = l->next) {
    WockyXmppNode *n = (WockyXmppNode *)l->data;
    if (!func(n, user_data)) {
      return;
    }
  }
}

static gint 
attribute_compare(gconstpointer a, gconstpointer b) {
  const Attribute *attr = (const Attribute *)a;
  const Tuple *target = (const Tuple *)b;

  if (target->ns != 0 && attr->ns != target->ns) {
    return 1;
  }

  return strcmp(attr->key, target->key);
}


const gchar *
wocky_xmpp_node_get_attribute_ns(WockyXmppNode *node, 
                                 const gchar *key,
                                 const gchar *ns) {
  GSList *link;
  Tuple search;

  search.key = (gchar *)key;
  search.ns = (ns != NULL ? g_quark_from_string(ns) : 0);

  link = g_slist_find_custom(node->attributes, &search, attribute_compare); 

  return (link == NULL) ? NULL : ((Attribute *)(link->data))->value;
}

const gchar *
wocky_xmpp_node_get_attribute(WockyXmppNode *node, const gchar *key) {
  return wocky_xmpp_node_get_attribute_ns(node, key, NULL);
}

void  
wocky_xmpp_node_set_attribute(WockyXmppNode *node, 
                              const gchar *key, const gchar *value) {
  g_assert(value != NULL);
  wocky_xmpp_node_set_attribute_n_ns(node, key, value, strlen(value), NULL);
}

void  
wocky_xmpp_node_set_attribute_ns(WockyXmppNode *node, 
                                 const gchar *key, 
                                 const gchar *value,
                                 const gchar *ns) {
  wocky_xmpp_node_set_attribute_n_ns(node, key, value, strlen(value), ns);
}

void  
wocky_xmpp_node_set_attribute_n_ns(WockyXmppNode *node, 
                                   const gchar *key, 
                                   const gchar *value,
                                   gsize value_size,
                                   const gchar *ns) {
  Attribute *a = g_slice_new0(Attribute);
  a->key = g_strdup(key);
  a->value = g_strndup(value, value_size);
  a->ns = (ns != NULL) ? g_quark_from_string(ns) : 0;

  node->attributes = g_slist_append(node->attributes, a);
}

void  
wocky_xmpp_node_set_attribute_n(WockyXmppNode *node, 
                                const gchar *key, 
                                const gchar *value,
                                gsize value_size) {
  wocky_xmpp_node_set_attribute_n_ns(node, key, value, value_size, NULL);
}

static gint 
node_compare_child(gconstpointer a, gconstpointer b) {
  const WockyXmppNode *node = (const WockyXmppNode *)a;
  Tuple *target = (Tuple *)b;

  if (target->ns != 0 && target->ns != node->ns) {
    return 1;
  }

  return strcmp(node->name, target->key);
}

WockyXmppNode *
wocky_xmpp_node_get_child_ns(WockyXmppNode *node, 
                             const gchar *name,
                             const gchar *ns) {
  GSList *link;
  Tuple t;

  t.key = name;
  t.ns = (ns != NULL ?  g_quark_from_string(ns) : 0);

  link = g_slist_find_custom(node->children, &t, node_compare_child); 

  return (link == NULL) ? NULL : (WockyXmppNode *)(link->data);
}

WockyXmppNode *
wocky_xmpp_node_get_child(WockyXmppNode *node, const gchar *name) {
  return wocky_xmpp_node_get_child_ns(node, name, NULL);
}


WockyXmppNode *
wocky_xmpp_node_add_child(WockyXmppNode *node, 
                          const gchar *name) {
  return wocky_xmpp_node_add_child_with_content_ns(node, name, NULL, NULL);
}

WockyXmppNode *
wocky_xmpp_node_add_child_ns(WockyXmppNode *node, 
                             const gchar *name,
                             const gchar *ns) {
  return wocky_xmpp_node_add_child_with_content_ns(node, name, NULL, ns);
}

WockyXmppNode *
wocky_xmpp_node_add_child_with_content(WockyXmppNode *node, 
                                       const gchar *name,
                                       const char *content) {
  return wocky_xmpp_node_add_child_with_content_ns(node, name, content, NULL);
}

WockyXmppNode *
wocky_xmpp_node_add_child_with_content_ns(WockyXmppNode *node, 
                                          const gchar *name,
                                          const gchar *content,
                                          const gchar *ns) {
  WockyXmppNode *result = wocky_xmpp_node_new(name);

  wocky_xmpp_node_set_content(result, content);
  wocky_xmpp_node_set_ns(result, ns);

  node->children = g_slist_append(node->children, result);
  return result;
}

void 
wocky_xmpp_node_set_ns(WockyXmppNode *node, const gchar *ns) {
  node->ns = (ns != NULL) ? g_quark_from_string(ns) : 0;
}

const gchar *
wocky_xmpp_node_get_ns(WockyXmppNode *node) {
  return g_quark_to_string(node->ns);
}

const gchar *
wocky_xmpp_node_get_language(WockyXmppNode *node) {
  g_return_val_if_fail(node != NULL, NULL);
  return node->language;
}

void
wocky_xmpp_node_set_language_n(WockyXmppNode *node, 
                               const gchar *lang, gsize lang_size) {
  g_free(node->language);
  node->language = g_strndup(lang, lang_size);
}

void
wocky_xmpp_node_set_language(WockyXmppNode *node, const gchar *lang) {
  gsize lang_size = 0;
  if (lang != NULL) {
    lang_size = strlen (lang);
  }
  wocky_xmpp_node_set_language_n(node, lang, lang_size);
}


void 
wocky_xmpp_node_set_content(WockyXmppNode *node, 
                            const gchar *content) {
  g_free(node->content);
  node->content = g_strdup(content); 
}

void wocky_xmpp_node_append_content(WockyXmppNode *node, 
                                    const gchar *content) {
  gchar *t = node->content;
  node->content = g_strconcat(t, content, NULL);
  g_free(t);
}

void wocky_xmpp_node_append_content_n(WockyXmppNode *node, 
                                      const gchar *content,
                                       gsize size) {
  gsize csize = node->content != NULL ? strlen(node->content) : 0; 
  node->content = g_realloc(node->content, csize + size + 1);
  g_strlcpy(node->content + csize, content, size + 1);
}
