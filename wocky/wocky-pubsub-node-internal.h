/*
 * wocky-pubsub-node-internal.h - internal methods on WockyPubsubNode
 *                                used by WockyPubsubService
 * Copyright © 2010 Collabora Ltd.
 * Copyright © 2010 Nokia Corporation
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
#if !defined (WOCKY_COMPILATION)
# error "This is an internal header."
#endif

#ifndef WOCKY_PUBSUB_NODE_INTERNAL_H
#define WOCKY_PUBSUB_NODE_INTERNAL_H

#include "wocky-pubsub-node.h"

typedef void (*WockyPubsubNodeEventHandler) (
    WockyPubsubNode *self,
    WockyStanza *event_stanza,
    WockyNode *event_node,
    WockyNode *action_node);

typedef struct {
    const gchar *action;
    WockyPubsubNodeEventHandler method;
} WockyPubsubNodeEventMapping;

const WockyPubsubNodeEventMapping *_wocky_pubsub_node_get_event_mappings (
    guint *n_mappings);

#endif /* WOCKY_PUBSUB_NODE_INTERNAL_H */
