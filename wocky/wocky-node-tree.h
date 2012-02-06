/*
 * wocky-node-tree.h - Header for WockyNodeTree
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_NODE_TREE_H__
#define __WOCKY_NODE_TREE_H__

#include <stdarg.h>

#include <glib-object.h>
#include "wocky-node.h"
#include "wocky-types.h"
#include "wocky-xmpp-error.h"

G_BEGIN_DECLS

/**
 * WockyNodeTreeClass:
 *
 * The class of a #WockyNodeTree.
 */
typedef struct _WockyNodeTreeClass WockyNodeTreeClass;
typedef struct _WockyNodeTreePrivate WockyNodeTreePrivate;

struct _WockyNodeTreeClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _WockyNodeTree {
    /*<private>*/
    GObject parent;

    WockyNodeTreePrivate *priv;
};

GType wocky_node_tree_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_NODE_TREE \
  (wocky_node_tree_get_type ())
#define WOCKY_NODE_TREE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_NODE_TREE, \
   WockyNodeTree))
#define WOCKY_NODE_TREE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_NODE_TREE, \
    WockyNodeTreeClass))
#define WOCKY_IS_NODE_TREE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_NODE_TREE))
#define WOCKY_IS_NODE_TREE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_NODE_TREE))
#define WOCKY_NODE_TREE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_NODE_TREE, \
    WockyNodeTreeClass))

WockyNodeTree *wocky_node_tree_new (const gchar *name,
  const gchar *ns,
  ...) G_GNUC_NULL_TERMINATED;

WockyNodeTree *wocky_node_tree_new_va (const gchar *name,
    const char *ns,
    va_list va);

WockyNodeTree *wocky_node_tree_new_from_node (WockyNode *node);

WockyNode *wocky_node_tree_get_top_node (WockyNodeTree *self);

G_END_DECLS

#endif /* #ifndef __WOCKY_NODE_TREE_H__*/
