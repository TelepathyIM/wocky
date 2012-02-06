/*
 * wocky-loopback-stream.h - Header for WockyLoopbackStream
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_LOOPBACK_STREAM_H__
#define __WOCKY_LOOPBACK_STREAM_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _WockyLoopbackStream WockyLoopbackStream;
typedef struct _WockyLoopbackStreamClass WockyLoopbackStreamClass;
typedef struct _WockyLoopbackStreamPrivate WockyLoopbackStreamPrivate;

struct _WockyLoopbackStreamClass
{
  GIOStreamClass parent_class;
};

struct _WockyLoopbackStream
{
  GIOStream parent;

  WockyLoopbackStreamPrivate *priv;
};

GType wocky_loopback_stream_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_LOOPBACK_STREAM \
  (wocky_loopback_stream_get_type ())
#define WOCKY_LOOPBACK_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_LOOPBACK_STREAM, \
      WockyLoopbackStream))
#define WOCKY_LOOPBACK_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_LOOPBACK_STREAM, \
      WockyLoopbackStreamClass))
#define WOCKY_IS_LOOPBACK_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_LOOPBACK_STREAM))
#define WOCKY_IS_LOOPBACK_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_LOOPBACK_STREAM))
#define WOCKY_LOOPBACK_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_LOOPBACK_STREAM, \
      WockyLoopbackStreamClass))

GIOStream * wocky_loopback_stream_new (void);

G_END_DECLS

#endif /* #ifndef __WOCKY_LOOPBACK_STREAM_H__*/
