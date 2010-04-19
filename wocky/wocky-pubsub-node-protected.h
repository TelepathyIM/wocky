/*
 * wocky-pubsub-node-internal.h - protected methods on WockyPubsubNode
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

#ifndef WOCKY_PUBSUB_NODE_PROTECTED_H
#define WOCKY_PUBSUB_NODE_PROTECTED_H

#include "wocky-pubsub-node.h"

/* for use by WockyPubsubService */

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

/* for use by subclasses */

WockyPorter *wocky_pubsub_node_get_porter (WockyPubsubNode *self);

WockyStanza *wocky_pubsub_node_make_subscribe_stanza (WockyPubsubNode *self,
    const gchar *jid,
    WockyNode **pubsub_node,
    WockyNode **subscribe_node);

WockyStanza *wocky_pubsub_node_make_unsubscribe_stanza (
    WockyPubsubNode *self,
    const gchar *jid,
    const gchar *subid,
    WockyNode **pubsub_node,
    WockyNode **unsubscribe_node);

WockyStanza *wocky_pubsub_node_make_delete_stanza (
    WockyPubsubNode *self,
    WockyNode **pubsub_node,
    WockyNode **delete_node);

WockyStanza *wocky_pubsub_node_make_list_subscribers_stanza (
    WockyPubsubNode *self,
    WockyNode **pubsub_node,
    WockyNode **subscriptions_node);

WockyStanza *wocky_pubsub_node_make_list_affiliates_stanza (
    WockyPubsubNode *self,
    WockyNode **pubsub_node,
    WockyNode **affiliations_node);

GList *wocky_pubsub_node_parse_affiliations (
    WockyPubsubNode *self,
    WockyNode *affiliations_node);

WockyStanza *wocky_pubsub_node_make_modify_affiliates_stanza (
    WockyPubsubNode *self,
    const GList *affiliates,
    WockyNode **pubsub_node,
    WockyNode **affiliations_node);

WockyStanza *wocky_pubsub_node_make_get_configuration_stanza (
    WockyPubsubNode *self,
    WockyNode **pubsub_node,
    WockyNode **configure_node);

#endif /* WOCKY_PUBSUB_NODE_PROTECTED_H */
