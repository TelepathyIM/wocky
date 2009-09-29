/*
 * wocky-pubsub-node.h - Header of WockyPubsubNode
 * Copyright (C) 2009 Collabora Ltd.
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

#ifndef __WOCKY_PUBSUB_NODE_H__
#define __WOCKY_PUBSUB_NODE_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "wocky-types.h"
#include "wocky-session.h"
#include "wocky-pubsub-service.h"

G_BEGIN_DECLS

typedef struct _WockyPubsubNodeClass WockyPubsubNodeClass;

struct _WockyPubsubNodeClass {
  GObjectClass parent_class;
};

struct _WockyPubsubNode {
  GObject parent;
};

GType wocky_pubsub_node_get_type (void);

#define WOCKY_TYPE_PUBSUB_NODE \
  (wocky_pubsub_node_get_type ())
#define WOCKY_PUBSUB_NODE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PUBSUB_NODE, \
   WockyPubsubNode))
#define WOCKY_PUBSUB_NODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PUBSUB_NODE, \
   WockyPubsubNodeClass))
#define WOCKY_IS_PUBSUB_NODE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PUBSUB_NODE))
#define WOCKY_IS_PUBSUB_NODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PUBSUB_NODE))
#define WOCKY_PUBSUB_NODE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PUBSUB_NODE, \
   WockyPubsubNodeClass))

WockyPubsubNode * wocky_pubsub_node_new (WockyPubsubService *service,
    const gchar *name);

const gchar * wocky_pubsub_node_get_name (WockyPubsubNode *node);

G_END_DECLS

#endif /* __WOCKY_PUBSUB_NODE_H__ */
