/*
 * wocky-ping.h - Header for WockyPing
 * Copyright (C) 2010 Collabora Ltd.
 * @author Senko Rasic <senko.rasic@collabora.co.uk>
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

#ifndef __WOCKY_PING_H__
#define __WOCKY_PING_H__

#include <glib-object.h>

#include "wocky-types.h"
#include "wocky-c2s-porter.h"

G_BEGIN_DECLS

typedef struct _WockyPing WockyPing;

/**
 * WockyPingClass:
 *
 * The class of a #WockyPing.
 */
typedef struct _WockyPingClass WockyPingClass;
typedef struct _WockyPingPrivate WockyPingPrivate;

GQuark wocky_ping_error_quark (void);

struct _WockyPingClass {
  /*<private>*/
  GObjectClass parent_class;
};

struct _WockyPing {
  /*<private>*/
  GObject parent;

  WockyPingPrivate *priv;
};

GType wocky_ping_get_type (void);

#define WOCKY_TYPE_PING \
  (wocky_ping_get_type ())
#define WOCKY_PING(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PING, \
   WockyPing))
#define WOCKY_PING_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PING, \
   WockyPingClass))
#define WOCKY_IS_PING(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PING))
#define WOCKY_IS_PING_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PING))
#define WOCKY_PING_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PING, \
   WockyPingClass))

WockyPing * wocky_ping_new (WockyC2SPorter *porter, guint interval);

G_END_DECLS

#endif /* #ifndef __WOCKY_PING_H__ */
