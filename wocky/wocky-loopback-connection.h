/*
 * wocky-loopback-connection.h - Header for WockyLoopbackConnection
 * Copyright (C) 2009-2011 Collabora Ltd.
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

#ifndef __WOCKY_LOOPBACK_CONNECTION_H__
#define __WOCKY_LOOPBACK_CONNECTION_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _WockyLoopbackConnection WockyLoopbackConnection;
typedef struct _WockyLoopbackConnectionClass WockyLoopbackConnectionClass;
typedef struct _WockyLoopbackConnectionPrivate WockyLoopbackConnectionPrivate;

struct _WockyLoopbackConnectionClass
{
  GIOStreamClass parent_class;
};

struct _WockyLoopbackConnection
{
  GIOStream parent;

  WockyLoopbackConnectionPrivate *priv;
};

GType wocky_loopback_connection_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_LOOPBACK_CONNECTION \
  (wocky_loopback_connection_get_type ())
#define WOCKY_LOOPBACK_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_LOOPBACK_CONNECTION, \
      WockyLoopbackConnection))
#define WOCKY_LOOPBACK_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_LOOPBACK_CONNECTION, \
      WockyLoopbackConnectionClass))
#define WOCKY_IS_LOOPBACK_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_LOOPBACK_CONNECTION))
#define WOCKY_IS_LOOPBACK_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_LOOPBACK_CONNECTION))
#define WOCKY_LOOPBACK_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_LOOPBACK_CONNECTION, \
      WockyLoopbackConnectionClass))

GIOStream * wocky_loopback_connection_new (void);

G_END_DECLS

#endif /* #ifndef __WOCKY_LOOPBACK_CONNECTION_H__*/
