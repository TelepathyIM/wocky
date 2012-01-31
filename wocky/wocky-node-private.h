/*
 * wocky-node-private.h - Private header for dealing with Wocky xmpp nodes
 * Copyright (C) 2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#ifndef __WOCKY__NODE_PRIVATE_H__
#define __WOCKY__NODE_PRIVATE_H__

#include <glib.h>
#include <wocky-node.h>

G_BEGIN_DECLS

WockyNode *_wocky_node_copy (WockyNode *node);

G_END_DECLS

#endif /* #ifndef __WOCKY_NODE__PRIVATE_H__*/
