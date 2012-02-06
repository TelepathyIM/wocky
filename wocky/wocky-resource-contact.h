/*
 * wocky-resource-contact.h - Header for WockyResourceContact
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

#ifndef __WOCKY_RESOURCE_CONTACT_H__
#define __WOCKY_RESOURCE_CONTACT_H__

#include <glib-object.h>

#include "wocky-types.h"
#include "wocky-contact.h"
#include "wocky-bare-contact.h"
#include "wocky-roster.h"

G_BEGIN_DECLS

/**
 * WockyResourceContactClass:
 *
 * The class of a #WockyResourceContact.
 */
typedef struct _WockyResourceContactClass WockyResourceContactClass;
typedef struct _WockyResourceContactPrivate WockyResourceContactPrivate;


struct _WockyResourceContactClass {
  /*<private>*/
  WockyContactClass parent_class;
};

struct _WockyResourceContact {
  /*<private>*/
  WockyContact parent;

  WockyResourceContactPrivate *priv;
};

GType wocky_resource_contact_get_type (void);

#define WOCKY_TYPE_RESOURCE_CONTACT \
  (wocky_resource_contact_get_type ())
#define WOCKY_RESOURCE_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_RESOURCE_CONTACT, \
   WockyResourceContact))
#define WOCKY_RESOURCE_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_RESOURCE_CONTACT, \
   WockyResourceContactClass))
#define WOCKY_IS_RESOURCE_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_RESOURCE_CONTACT))
#define WOCKY_IS_RESOURCE_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_RESOURCE_CONTACT))
#define WOCKY_RESOURCE_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_RESOURCE_CONTACT, \
   WockyResourceContactClass))

WockyResourceContact * wocky_resource_contact_new (WockyBareContact *bare,
    const gchar *resource);

const gchar * wocky_resource_contact_get_resource (
    WockyResourceContact *contact);

WockyBareContact * wocky_resource_contact_get_bare_contact (
    WockyResourceContact *contact);

gboolean wocky_resource_contact_equal (WockyResourceContact *a,
    WockyResourceContact *b);

G_END_DECLS

#endif /* #ifndef __WOCKY_RESOURCE_CONTACT_H__*/
