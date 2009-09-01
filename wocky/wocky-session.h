/*
 * wocky-session.h - Header for WockySession
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

#ifndef __WOCKY_SESSION_H__
#define __WOCKY_SESSION_H__

#include <glib-object.h>

#include "wocky-types.h"
#include "wocky-xmpp-connection.h"
#include "wocky-porter.h"

G_BEGIN_DECLS

typedef struct _WockySessionClass WockySessionClass;

struct _WockySessionClass {
  GObjectClass parent_class;
};

struct _WockySession {
  GObject parent;
};

GType wocky_session_get_type (void);

#define WOCKY_TYPE_SESSION \
  (wocky_session_get_type ())
#define WOCKY_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_SESSION, \
   WockySession))
#define WOCKY_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_SESSION, \
   WockySessionClass))
#define WOCKY_IS_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_SESSION))
#define WOCKY_IS_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_SESSION))
#define WOCKY_SESSION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_SESSION, \
   WockySessionClass))

WockySession * wocky_session_new (WockyXmppConnection *conn);

WockyPorter * wocky_session_get_porter (WockySession *session);

G_END_DECLS

#endif /* #ifndef __WOCKY_SESSION_H__*/
