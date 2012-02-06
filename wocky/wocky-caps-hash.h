/*
 * wocky-caps-hash.h - Headers for computing verification string hash (XEP-0115 v1.5)
 * Copyright (C) 2008-2011 Collabora Ltd.
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

#ifndef __WOCKY_CAPS_HASH_H__
#define __WOCKY_CAPS_HASH_H__

#include "wocky-node.h"

gchar * wocky_caps_hash_compute_from_node (
    WockyNode *node) G_GNUC_WARN_UNUSED_RESULT;

gchar * wocky_caps_hash_compute_from_lists (
    GPtrArray *features, GPtrArray *identities,
    GPtrArray *dataforms) G_GNUC_WARN_UNUSED_RESULT;

#endif /* #ifndef __WOCKY_CAPS_HASH_H__ */
