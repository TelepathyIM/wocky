/*
 * wocky-pubsub-internal.h — header for PubSub helper functions
 * Copyright © 2009–2012 Collabora Ltd.
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

#ifndef WOCKY_PUBSUB_HELPERS_H
#define WOCKY_PUBSUB_HELPERS_H

#include <gio/gio.h>

#include "wocky-stanza.h"

WockyStanza * wocky_pubsub_make_event_stanza (
    const gchar *node,
    const gchar *from,
    WockyNode **item_out);

WockyStanza *wocky_pubsub_make_stanza (
    const gchar *service,
    WockyStanzaSubType sub_type,
    const gchar *pubsub_ns,
    const gchar *action_name,
    WockyNode **pubsub_node,
    WockyNode **action_node);

WockyStanza *wocky_pubsub_make_publish_stanza (
    const gchar *service,
    const gchar *node,
    WockyNode **pubsub_out,
    WockyNode **publish_out,
    WockyNode **item_out);

void wocky_send_ll_pep_event (WockySession *session,
    WockyStanza *stanza);

gboolean wocky_pubsub_distill_iq_reply (GObject *source,
    GAsyncResult *res,
    const gchar *pubsub_ns,
    const gchar *child_name,
    WockyNodeTree **child_out,
    GError **error);

gboolean wocky_pubsub_distill_ambivalent_iq_reply (GObject *source,
    GAsyncResult *res,
    const gchar *pubsub_ns,
    const gchar *child_name,
    WockyNodeTree **child_out,
    GError **error);

gboolean wocky_pubsub_distill_void_iq_reply (GObject *source,
    GAsyncResult *res,
    GError **error);

gboolean wocky_pubsub_distill_stanza (WockyStanza *result,
    const gchar *pubsub_ns,
    const gchar *child_name,
    gboolean body_optional,
    WockyNodeTree **child_out,
    GError **error);

#endif /* WOCKY_PUBSUB_HELPERS_H */
