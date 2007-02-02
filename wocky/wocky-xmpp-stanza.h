/*
 * wocky-xmpp-stanza.h - Header for WockyXmppStanza
 * Copyright (C) 2006 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd@luon.net>
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

#ifndef __WOCKY_XMPP_STANZA_H__
#define __WOCKY_XMPP_STANZA_H__

#include <glib-object.h>
#include "wocky-xmpp-node.h"

G_BEGIN_DECLS

typedef struct _WockyXmppStanza WockyXmppStanza;
typedef struct _WockyXmppStanzaClass WockyXmppStanzaClass;

struct _WockyXmppStanzaClass {
    GObjectClass parent_class;
};

struct _WockyXmppStanza {
    GObject parent;
    WockyXmppNode *node;
};

GType wocky_xmpp_stanza_get_type(void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_STANZA \
  (wocky_xmpp_stanza_get_type())
#define WOCKY_XMPP_STANZA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanza))
#define WOCKY_XMPP_STANZA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanzaClass))
#define WOCKY_IS_XMPP_STANZA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_STANZA))
#define WOCKY_IS_XMPP_STANZA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_STANZA))
#define WOCKY_XMPP_STANZA_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanzaClass))

WockyXmppStanza *
wocky_xmpp_stanza_new(gchar *name);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_STANZA_H__*/
