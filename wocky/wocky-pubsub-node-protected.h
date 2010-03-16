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

WockyPorter *wocky_pubsub_node_get_porter (WockyPubsubNode *self);

gboolean _wocky_pubsub_node_handle_event_stanza (WockyPubsubNode *self,
    WockyXmppStanza *event_stanza);

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

#endif /* WOCKY_PUBSUB_NODE_PROTECTED_H */
