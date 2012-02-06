/*
 * wocky-ll-connection-factory.h - Header for WockyLLConnectionFactory
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

#ifndef __WOCKY_LL_CONNECTION_FACTORY_H__
#define __WOCKY_LL_CONNECTION_FACTORY_H__

#include <glib-object.h>

#include <gio/gio.h>

#include "wocky-xmpp-connection.h"
#include "wocky-ll-contact.h"

G_BEGIN_DECLS

typedef struct _WockyLLConnectionFactory WockyLLConnectionFactory;
typedef struct _WockyLLConnectionFactoryClass WockyLLConnectionFactoryClass;
typedef struct _WockyLLConnectionFactoryPrivate WockyLLConnectionFactoryPrivate;

typedef enum
{
  WOCKY_LL_CONNECTION_FACTORY_ERROR_NO_CONTACT_ADDRESSES,
  WOCKY_LL_CONNECTION_FACTORY_ERROR_NO_CONTACT_ADDRESS_CAN_BE_CONNECTED_TO, /* omg so long! */
} WockyLLConnectionFactoryError;

GQuark wocky_ll_connection_factory_error_quark (void);

#define WOCKY_LL_CONNECTION_FACTORY_ERROR (wocky_ll_connection_factory_error_quark ())

struct _WockyLLConnectionFactoryClass {
  GObjectClass parent_class;
};

struct _WockyLLConnectionFactory {
  GObject parent;
  WockyLLConnectionFactoryPrivate *priv;
};

GType wocky_ll_connection_factory_get_type (void);

#define WOCKY_TYPE_LL_CONNECTION_FACTORY \
  (wocky_ll_connection_factory_get_type ())
#define WOCKY_LL_CONNECTION_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_LL_CONNECTION_FACTORY, \
   WockyLLConnectionFactory))
#define WOCKY_LL_CONNECTION_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_LL_CONNECTION_FACTORY, \
   WockyLLConnectionFactoryClass))
#define WOCKY_IS_LL_CONNECTION_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_LL_CONNECTION_FACTORY))
#define WOCKY_IS_LL_CONNECTION_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_LL_CONNECTION_FACTORY))
#define WOCKY_LL_CONNECTION_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_LL_CONNECTION_FACTORY, \
   WockyLLConnectionFactoryClass))

WockyLLConnectionFactory * wocky_ll_connection_factory_new (void);

void wocky_ll_connection_factory_make_connection_async (
    WockyLLConnectionFactory *factory,
    WockyLLContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyXmppConnection * wocky_ll_connection_factory_make_connection_finish (
    WockyLLConnectionFactory *factory,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif /* #ifndef __WOCKY_LL_CONNECTION_FACTORY_H__*/
