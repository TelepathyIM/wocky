/*
 * wocky-pubsub-helpers.c — PubSub helper functions
 * Copyright © 2009–2010 Collabora Ltd.
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

#include "wocky-pubsub-helpers.h"

#include "wocky-namespaces.h"

/**
 * wocky_pubsub_make_publish_stanza:
 * @service: the JID of a PubSub service, or %NULL
 * @node: the name of a node on @service; may not be %NULL
 * @publish_out: address at which to store a pointer to the <publish/> node
 * @item_out: address at which to store a pointer to the <item/> node
 *
 * <!-- -->
 *
 * Returns: a new iq[type='set']/pubsub/publish/item stanza
 */
WockyXmppStanza *
wocky_pubsub_make_publish_stanza (
    const gchar *service,
    const gchar *node,
    WockyXmppNode **publish_out,
    WockyXmppNode **item_out)
{
  WockyXmppNode *publish, *item;
  WockyXmppStanza *stanza;

  g_return_val_if_fail (node != NULL, NULL);

  stanza = wocky_xmpp_stanza_build (
      WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_SET,
      NULL, service,
        WOCKY_NODE, "pubsub",
          WOCKY_NODE_XMLNS, WOCKY_XMPP_NS_PUBSUB,
          WOCKY_NODE, "publish",
            WOCKY_NODE_ASSIGN_TO, &publish,
            WOCKY_NODE_ATTRIBUTE, "node", node,
            WOCKY_NODE, "item",
              WOCKY_NODE_ASSIGN_TO, &item,
            WOCKY_NODE_END,
          WOCKY_NODE_END,
        WOCKY_NODE_END,
      WOCKY_STANZA_END);

  if (publish_out != NULL)
    *publish_out = publish;

  if (item_out != NULL)
    *item_out = item;

  return stanza;
}
