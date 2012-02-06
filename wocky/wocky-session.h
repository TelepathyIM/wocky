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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_SESSION_H__
#define __WOCKY_SESSION_H__

#include <glib-object.h>

#include "wocky-types.h"
#include "wocky-xmpp-connection.h"
#include "wocky-porter.h"
#include "wocky-contact-factory.h"

G_BEGIN_DECLS

/**
 * WockySessionClass:
 *
 * The class of a #WockySession.
 */
typedef struct _WockySessionClass WockySessionClass;
typedef struct _WockySessionPrivate WockySessionPrivate;


struct _WockySessionClass {
  /*<private>*/
  GObjectClass parent_class;
};

struct _WockySession {
  /*<private>*/
  GObject parent;

  WockySessionPrivate *priv;
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

WockySession * wocky_session_new_ll (const gchar *full_jid);

WockySession * wocky_session_new_with_connection (WockyXmppConnection *conn,
    const gchar *full_jid);

void wocky_session_start (WockySession *session);

WockyPorter * wocky_session_get_porter (WockySession *session);

WockyContactFactory * wocky_session_get_contact_factory (WockySession *session);

void wocky_session_set_jid (WockySession *session, const gchar *jid);

const gchar *wocky_session_get_jid (WockySession *session);

G_END_DECLS

#endif /* #ifndef __WOCKY_SESSION_H__*/
