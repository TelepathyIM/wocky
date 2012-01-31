/*
 * wocky-contact.h - Header for WockyContact
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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

#ifndef __WOCKY_CONTACT_H__
#define __WOCKY_CONTACT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _WockyContact WockyContact;

/**
 * WockyContactClass:
 *
 * The class of a #WockyContact.
 */
typedef struct _WockyContactClass WockyContactClass;
typedef struct _WockyContactPrivate WockyContactPrivate;

typedef gchar * (*WockyContactDupJidImpl) (WockyContact *self);

struct _WockyContactClass {
  /*<private>*/
  GObjectClass parent_class;

  WockyContactDupJidImpl dup_jid;
};

struct _WockyContact {
  /*<private>*/
  GObject parent;

  WockyContactPrivate *priv;
};

GType wocky_contact_get_type (void);

#define WOCKY_TYPE_CONTACT \
  (wocky_contact_get_type ())
#define WOCKY_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_CONTACT, \
   WockyContact))
#define WOCKY_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_CONTACT, \
   WockyContactClass))
#define WOCKY_IS_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_CONTACT))
#define WOCKY_IS_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_CONTACT))
#define WOCKY_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_CONTACT, \
   WockyContactClass))

gchar * wocky_contact_dup_jid (WockyContact *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* #ifndef __WOCKY_CONTACT_H__*/
