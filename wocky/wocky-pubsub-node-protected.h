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
    WockyXmppStanza *event_stanza,
    WockyXmppNode *event_node,
    WockyXmppNode *action_node);

typedef struct {
    const gchar *action;
    WockyPubsubNodeEventHandler method;
} WockyPubsubNodeEventMapping;

const WockyPubsubNodeEventMapping *_wocky_pubsub_node_get_event_mappings (
    guint *n_mappings);

/* for use by subclasses */

WockyPorter *wocky_pubsub_node_get_porter (WockyPubsubNode *self);

WockyXmppStanza *wocky_pubsub_node_make_subscribe_stanza (WockyPubsubNode *self,
    const gchar *jid,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **subscribe_node);

WockyXmppStanza *wocky_pubsub_node_make_unsubscribe_stanza (
    WockyPubsubNode *self,
    const gchar *jid,
    const gchar *subid,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **unsubscribe_node);

WockyXmppStanza *wocky_pubsub_node_make_delete_stanza (
    WockyPubsubNode *self,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **delete_node);

WockyXmppStanza *wocky_pubsub_node_make_list_subscribers_stanza (
    WockyPubsubNode *self,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **subscriptions_node);

WockyXmppStanza *wocky_pubsub_node_make_list_affiliates_stanza (
    WockyPubsubNode *self,
    WockyXmppNode **pubsub_node,
    WockyXmppNode **affiliations_node);

GList *wocky_pubsub_node_parse_affiliations (
    WockyPubsubNode *self,
    WockyXmppNode *affiliations_node);

#endif /* WOCKY_PUBSUB_NODE_PROTECTED_H */
