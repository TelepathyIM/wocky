/*
 * wocky-pubsub-service-protected.h - protected methods on WockyPubsubService
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

#ifndef WOCKY_PUBSUB_SERVICE_PROTECTED_H
#define WOCKY_PUBSUB_SERVICE_PROTECTED_H

#include "wocky-pubsub-service.h"

WockyStanza *wocky_pubsub_service_create_retrieve_subscriptions_stanza (
    WockyPubsubService *self,
    WockyPubsubNode *node,
    WockyNode **pubsub_node,
    WockyNode **subscriptions_node);

WockyPubsubSubscription *
wocky_pubsub_service_parse_subscription (WockyPubsubService *self,
    WockyNode *subscription_node,
    const gchar *parent_node_attr,
    GError **error);

GList * wocky_pubsub_service_parse_subscriptions (WockyPubsubService *self,
    WockyNode *subscriptions_node,
    GList **subscription_nodes);

WockyStanza *wocky_pubsub_service_create_create_node_stanza (
    WockyPubsubService *self,
    const gchar *name,
    WockyDataForm *config,
    WockyNode **pubsub_node,
    WockyNode **create_node);

WockyPubsubNode *wocky_pubsub_service_handle_create_node_reply (
    WockyPubsubService *self,
    WockyNodeTree *create_tree,
    const gchar *requested_name,
    GError **error);

WockyPorter *wocky_pubsub_service_get_porter (WockyPubsubService *self);

#endif /* WOCKY_PUBSUB_SERVICE_PROTECTED_H */
