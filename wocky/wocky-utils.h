/*
 * gibber-util.h - Header for Wocky utility functions
 * Copyright (C) 2007-2009 Collabora Ltd.
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

#ifndef __WOCKY_UTIL_H__
#define __WOCKY_UTIL_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

gboolean wocky_strdiff (const gchar *left,
    const gchar *right);

gchar * wocky_normalise_jid (const gchar *jid);

gboolean wocky_decode_jid (const gchar *jid,
    gchar **node,
    gchar **domain,
    gchar **resource);

GValue *wocky_g_value_slice_new (GType type);

GValue *wocky_g_value_slice_new_boolean (gboolean b);
GValue *wocky_g_value_slice_new_int (gint n);
GValue *wocky_g_value_slice_new_int64 (gint64 n);
GValue *wocky_g_value_slice_new_uint (guint n);
GValue *wocky_g_value_slice_new_uint64 (guint64 n);
GValue *wocky_g_value_slice_new_double (double d);

GValue *wocky_g_value_slice_new_string (const gchar *string);
GValue *wocky_g_value_slice_new_static_string (const gchar *string);
GValue *wocky_g_value_slice_new_take_string (gchar *string);

GValue *wocky_g_value_slice_new_boxed (GType type, gconstpointer p);
GValue *wocky_g_value_slice_new_static_boxed (GType type, gconstpointer p);
GValue *wocky_g_value_slice_new_take_boxed (GType type, gpointer p);

void wocky_g_value_slice_free (GValue *value);

GValue *wocky_g_value_slice_dup (const GValue *value);

gboolean wocky_enum_from_nick (GType enum_type, const gchar *nick, gint *value);
const gchar *wocky_enum_to_nick (GType enum_type, gint value);

G_END_DECLS

#endif /* #ifndef __WOCKY_UTIL_H__ */
