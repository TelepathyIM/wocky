/*
 * wocky-heartbeat-source.h: header for a GSource wrapping libiphb.
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk>
 * Copyright © 2010 Nokia Corporation
 * @author Will Thompson <will.thompson@collabora.co.uk>
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

#ifndef WOCKY_HEARTBEAT_SOURCE_H
#define WOCKY_HEARTBEAT_SOURCE_H

#include <glib.h>

G_BEGIN_DECLS

typedef void (*WockyHeartbeatCallback) (
    gpointer user_data);

GSource *wocky_heartbeat_source_new (
    guint max_interval);

void wocky_heartbeat_source_update_interval (
    GSource *source,
    guint max_interval);

G_END_DECLS

#endif /* WOCKY_HEARTBEAT_SOURCE_H */
