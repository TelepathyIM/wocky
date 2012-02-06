/*
 * wocky-ll-contact.h - Header for WockyLLContact
 * Copyright (C) 2011 Collabora Ltd.
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

#ifndef __WOCKY_LL_CONTACT_H__
#define __WOCKY_LL_CONTACT_H__

#include <glib-object.h>

#include <gio/gio.h>

#include "wocky-types.h"
#include "wocky-contact.h"

G_BEGIN_DECLS

typedef struct _WockyLLContactClass WockyLLContactClass;
typedef struct _WockyLLContactPrivate WockyLLContactPrivate;

typedef GList * (*WockyLLContactGetAddressesImpl) (WockyLLContact *);

struct _WockyLLContactClass {
  WockyContactClass parent_class;

  WockyLLContactGetAddressesImpl get_addresses;
};

struct _WockyLLContact {
  WockyContact parent;

  WockyLLContactPrivate *priv;
};

GType wocky_ll_contact_get_type (void);

#define WOCKY_TYPE_LL_CONTACT \
  (wocky_ll_contact_get_type ())
#define WOCKY_LL_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_LL_CONTACT, \
   WockyLLContact))
#define WOCKY_LL_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_LL_CONTACT, \
   WockyLLContactClass))
#define WOCKY_IS_LL_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_LL_CONTACT))
#define WOCKY_IS_LL_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_LL_CONTACT))
#define WOCKY_LL_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_LL_CONTACT, \
   WockyLLContactClass))

WockyLLContact * wocky_ll_contact_new (const gchar *jid);

const gchar *wocky_ll_contact_get_jid (WockyLLContact *contact);

gboolean wocky_ll_contact_equal (WockyLLContact *a,
    WockyLLContact *b);

GList * wocky_ll_contact_get_addresses (WockyLLContact *self);

gboolean wocky_ll_contact_has_address (WockyLLContact *self,
    GInetAddress *address);

G_END_DECLS

#endif /* #ifndef __WOCKY_LL_CONTACT_H__*/
