/*
 * wocky-connection-factory.h - Header for WockyConnectionFactory
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

#ifndef __WOCKY_CONNECTION_FACTORY_H__
#define __WOCKY_CONNECTION_FACTORY_H__

#include <glib-object.h>

#include <gio/gio.h>

#include "wocky-xmpp-connection.h"
#include "wocky-ll-contact.h"

G_BEGIN_DECLS

typedef struct _WockyConnectionFactory WockyConnectionFactory;
typedef struct _WockyConnectionFactoryClass WockyConnectionFactoryClass;
typedef struct _WockyConnectionFactoryPrivate WockyConnectionFactoryPrivate;

typedef enum
{
  WOCKY_CONNECTION_FACTORY_ERROR_NO_CONTACT_ADDRESSES,
  WOCKY_CONNECTION_FACTORY_ERROR_NO_CONTACT_ADDRESS_CAN_BE_CONNECTED_TO, /* omg so long! */
} WockyConnectionFactoryError;

GQuark wocky_connection_factory_error_quark (void);

#define WOCKY_CONNECTION_FACTORY_ERROR (wocky_connection_factory_error_quark ())

struct _WockyConnectionFactoryClass {
  GObjectClass parent_class;
};

struct _WockyConnectionFactory {
  GObject parent;
  WockyConnectionFactoryPrivate *priv;
};

GType wocky_connection_factory_get_type (void);

#define WOCKY_TYPE_CONNECTION_FACTORY \
  (wocky_connection_factory_get_type ())
#define WOCKY_CONNECTION_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_CONNECTION_FACTORY, \
   WockyConnectionFactory))
#define WOCKY_CONNECTION_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_CONNECTION_FACTORY, \
   WockyConnectionFactoryClass))
#define WOCKY_IS_CONNECTION_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_CONNECTION_FACTORY))
#define WOCKY_IS_CONNECTION_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_CONNECTION_FACTORY))
#define WOCKY_CONNECTION_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_CONNECTION_FACTORY, \
   WockyConnectionFactoryClass))

WockyConnectionFactory * wocky_connection_factory_new (void);

void wocky_connection_factory_make_connection_async (
    WockyConnectionFactory *factory,
    WockyLLContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyXmppConnection * wocky_connection_factory_make_connection_finish (
    WockyConnectionFactory *factory,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif /* #ifndef __WOCKY_CONNECTION_FACTORY_H__*/
