/*
 * wocky-utils.h - Header for Wocky utility functions
 * Copyright © 2007–2010 Collabora Ltd.
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

#ifndef __WOCKY_UTILS_H__
#define __WOCKY_UTILS_H__

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
gchar *wocky_compose_jid (const gchar *node,
    const gchar *domain,
    const gchar *resource);

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

gchar *wocky_absolutize_path (const gchar *path);

GList *wocky_list_deep_copy (GBoxedCopyFunc copy, GList *items);

GString *wocky_g_string_dup (const GString *str);

void wocky_g_string_free (GString *str);

/* These are macros so that the critical message raised by
 * g_return_val_if_fail() contains the actual function name and tag. They
 * magically returns from the function rather than evaluating to a boolean
 * because doing the latter in macros seems to be a GCC extension. I could
 * probably rewrite them using , and ?: but...
 *
 * They should really be in GLib, but let's experiment here first.
 */
#define wocky_implement_finish_void(source, tag) \
    G_STMT_START { \
      GSimpleAsyncResult *_simple; \
      _simple = (GSimpleAsyncResult *) result; \
      if (g_simple_async_result_propagate_error ( \
          _simple, error)) \
        return FALSE; \
      g_return_val_if_fail (g_simple_async_result_is_valid (result, \
          G_OBJECT (source), (tag)), \
        FALSE); \
      return TRUE; \
    } G_STMT_END

#define wocky_implement_finish_copy_pointer(source, tag, copy_func, \
    out_param) \
    G_STMT_START { \
      GSimpleAsyncResult *_simple; \
      _simple = (GSimpleAsyncResult *) result; \
      if (g_simple_async_result_propagate_error (_simple, error)) \
        return FALSE; \
      g_return_val_if_fail (g_simple_async_result_is_valid (result, \
          G_OBJECT (source), (tag)), \
        FALSE); \
      if ((out_param) != NULL) \
        { \
          gpointer _p = g_simple_async_result_get_op_res_gpointer (_simple); \
          if (_p != NULL) \
            *(out_param) = (copy_func) (_p); \
          else \
            *(out_param) = NULL; \
        } \
      return TRUE; \
    } G_STMT_END

#define wocky_implement_finish_return_copy_pointer(source, tag, \
    copy_func) \
    G_STMT_START { \
      GSimpleAsyncResult *_simple; \
      gpointer _p; \
      _simple = (GSimpleAsyncResult *) result; \
      if (g_simple_async_result_propagate_error (_simple, error)) \
        return NULL; \
      g_return_val_if_fail (g_simple_async_result_is_valid (result, \
          G_OBJECT (source), (tag)), \
        NULL); \
      _p = g_simple_async_result_get_op_res_gpointer (_simple); \
      if (_p != NULL) \
        return (copy_func) (_p); \
      return NULL; \
    } G_STMT_END

#define wocky_implement_finish_return_pointer(source, tag) \
    G_STMT_START { \
      GSimpleAsyncResult *_simple; \
      _simple = (GSimpleAsyncResult *) result; \
      if (g_simple_async_result_propagate_error (_simple, error)) \
        return NULL; \
      g_return_val_if_fail (g_simple_async_result_is_valid (result, \
              G_OBJECT (source), tag), \
          NULL); \
      return g_simple_async_result_get_op_res_gpointer (_simple); \
    } G_STMT_END

G_END_DECLS

#endif /* #ifndef __WOCKY_UTILS_H__ */
