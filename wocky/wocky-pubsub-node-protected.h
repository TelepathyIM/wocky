/*
 * wocky-pubsub-node-protected.h - protected methods on WockyPubsubNode
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef WOCKY_PUBSUB_NODE_PROTECTED_H
#define WOCKY_PUBSUB_NODE_PROTECTED_H

#include "wocky-pubsub-node.h"

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
    GList *affiliates,
    WockyNode **pubsub_node,
    WockyNode **affiliations_node);

WockyStanza *wocky_pubsub_node_make_get_configuration_stanza (
    WockyPubsubNode *self,
    WockyNode **pubsub_node,
    WockyNode **configure_node);

#endif /* WOCKY_PUBSUB_NODE_PROTECTED_H */
