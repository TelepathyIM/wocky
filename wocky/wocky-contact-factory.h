/*
 * wocky-resource-contact.h - Header for WockyContactFactory
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

#ifndef __WOCKY_CONTACT_FACTORY_H__
#define __WOCKY_CONTACT_FACTORY_H__

#include <glib-object.h>

#include "wocky-bare-contact.h"

G_BEGIN_DECLS

typedef struct _WockyContactFactory WockyContactFactory;
typedef struct _WockyContactFactoryClass WockyContactFactoryClass;

struct _WockyContactFactoryClass {
  GObjectClass parent_class;
};

struct _WockyContactFactory {
  GObject parent;
};

GType wocky_contact_factory_get_type (void);

#define WOCKY_TYPE_CONTACT_FACTORY \
  (wocky_contact_factory_get_type ())
#define WOCKY_CONTACT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_CONTACT_FACTORY, \
   WockyContactFactory))
#define WOCKY_CONTACT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_CONTACT_FACTORY, \
   WockyContactFactoryClass))
#define WOCKY_IS_CONTACT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_CONTACT_FACTORY))
#define WOCKY_IS_CONTACT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_CONTACT_FACTORY))
#define WOCKY_CONTACT_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_CONTACT_FACTORY, \
   WockyContactFactoryClass))

WockyContactFactory * wocky_contact_factory_new (void);

WockyBareContact * wocky_contact_factory_ensure_bare_contact (
    WockyContactFactory *factory,
    const gchar *bare_jid);

G_END_DECLS

#endif /* #ifndef __WOCKY_CONTACT_FACTORY_H__*/
