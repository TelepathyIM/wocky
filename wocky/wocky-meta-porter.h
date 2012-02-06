/*
 * wocky-meta-porter.h - Header for WockyMetaPorter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the tubesplied warranty of
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

#ifndef __WOCKY_META_PORTER_H__
#define __WOCKY_META_PORTER_H__

#include <glib-object.h>

#include <gio/gio.h>

#include "wocky-contact-factory.h"
#include "wocky-porter.h"

G_BEGIN_DECLS

typedef struct _WockyMetaPorter WockyMetaPorter;
typedef struct _WockyMetaPorterClass WockyMetaPorterClass;
typedef struct _WockyMetaPorterPrivate WockyMetaPorterPrivate;

typedef enum
{
  WOCKY_META_PORTER_ERROR_NO_CONTACT_ADDRESS,
  WOCKY_META_PORTER_ERROR_FAILED_TO_CLOSE,
} WockyMetaPorterError;

GQuark wocky_meta_porter_error_quark (void);

#define WOCKY_META_PORTER_ERROR (wocky_meta_porter_error_quark ())

struct _WockyMetaPorterClass
{
  GObjectClass parent_class;
};

struct _WockyMetaPorter
{
  GObject parent;

  WockyMetaPorterPrivate *priv;
};

GType wocky_meta_porter_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_META_PORTER \
  (wocky_meta_porter_get_type ())
#define WOCKY_META_PORTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_META_PORTER, \
      WockyMetaPorter))
#define WOCKY_META_PORTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_META_PORTER, \
      WockyMetaPorterClass))
#define WOCKY_IS_META_PORTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_META_PORTER))
#define WOCKY_IS_META_PORTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_META_PORTER))
#define WOCKY_META_PORTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_META_PORTER, \
      WockyMetaPorterClass))

WockyPorter * wocky_meta_porter_new (const gchar *jid,
    WockyContactFactory *contact_factory);

guint16 wocky_meta_porter_get_port (WockyMetaPorter *porter);

void wocky_meta_porter_hold (WockyMetaPorter *porter, WockyContact *contact);
void wocky_meta_porter_unhold (WockyMetaPorter *porter, WockyContact *contact);

void wocky_meta_porter_set_jid (WockyMetaPorter *porter, const gchar *jid);

void wocky_meta_porter_open_async (WockyMetaPorter *porter,
    WockyLLContact *contact, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data);

gboolean wocky_meta_porter_open_finish (WockyMetaPorter *porter,
    GAsyncResult *result, GError **error);

GSocketConnection * wocky_meta_porter_borrow_connection (WockyMetaPorter *porter,
    WockyLLContact *contact);

#endif /* #ifndef __WOCKY_META_PORTER_H__*/
