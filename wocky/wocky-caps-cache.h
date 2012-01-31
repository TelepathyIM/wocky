/*
 * wocky-caps-cache.h - Header for WockyCapsCache
 * Copyright (C) 2010 Collabora Ltd.
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

#ifndef __WOCKY_CAPS_CACHE_H__
#define __WOCKY_CAPS_CACHE_H__

#include <glib-object.h>

#include "wocky-node-tree.h"

G_BEGIN_DECLS

/**
 * WockyCapsCache:
 *
 * An object providing a permanent cache for capabilities.
 */
typedef struct _WockyCapsCache WockyCapsCache;

/**
 * WockyCapsCacheClass:
 *
 * The class of a #WockyCapsCache.
 */
typedef struct _WockyCapsCacheClass WockyCapsCacheClass;
typedef struct _WockyCapsCachePrivate WockyCapsCachePrivate;

#define WOCKY_TYPE_CAPS_CACHE wocky_caps_cache_get_type()
#define WOCKY_CAPS_CACHE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_CAPS_CACHE, \
        WockyCapsCache))
#define WOCKY_CAPS_CACHE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_CAPS_CACHE, \
        WockyCapsCacheClass))
#define WOCKY_IS_CAPS_CACHE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_CAPS_CACHE))
#define WOCKY_IS_CAPS_CACHE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_CAPS_CACHE))
#define WOCKY_CAPS_CACHE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_CAPS_CACHE, \
        WockyCapsCacheClass))

struct _WockyCapsCache
{
  /*<private>*/
  GObject parent;
  WockyCapsCachePrivate *priv;
};

struct _WockyCapsCacheClass
{
  /*<private>*/
  GObjectClass parent_class;
};

GType
wocky_caps_cache_get_type (void);

WockyNodeTree *wocky_caps_cache_lookup (WockyCapsCache *self,
    const gchar *node);

void wocky_caps_cache_insert (WockyCapsCache *self,
    const gchar *node,
    WockyNodeTree *query_node);

WockyCapsCache *
wocky_caps_cache_new (const gchar *path);

WockyCapsCache *
wocky_caps_cache_dup_shared (void);

void
wocky_caps_cache_free_shared (void);

G_END_DECLS

#endif /* ifndef __WOCKY_CAPS_CACHE_H__ */
