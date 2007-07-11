/*
 * wocky-xmpp-connection.h - Header for WockyXmppConnection
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

#ifndef __WOCKY_XMPP_CONNECTION_H__
#define __WOCKY_XMPP_CONNECTION_H__

#include <glib-object.h>

#include "wocky-transport.h"
#include "wocky-xmpp-stanza.h"

G_BEGIN_DECLS

typedef struct _WockyXmppConnection WockyXmppConnection;
typedef struct _WockyXmppConnectionClass WockyXmppConnectionClass;

typedef enum
{
  WOCKY_XMPP_CONNECTION_STREAM_SENT          = 1 << 0,
  WOCKY_XMPP_CONNECTION_STREAM_RECEIVED      = 1 << 1,
  WOCKY_XMPP_CONNECTION_STREAM_FULLY_OPEN    =
    WOCKY_XMPP_CONNECTION_STREAM_SENT|WOCKY_XMPP_CONNECTION_STREAM_RECEIVED,
  WOCKY_XMPP_CONNECTION_CLOSE_SENT           = 1 << 2,
  WOCKY_XMPP_CONNECTION_CLOSE_RECEIVED       = 1 << 3,
  WOCKY_XMPP_CONNECTION_CLOSE_FULLY_CLOSED   =
    WOCKY_XMPP_CONNECTION_STREAM_FULLY_OPEN|
    WOCKY_XMPP_CONNECTION_CLOSE_SENT|WOCKY_XMPP_CONNECTION_CLOSE_RECEIVED,
} WockyXmppConnectionFlags;

struct _WockyXmppConnectionClass {
    GObjectClass parent_class;
};

struct _WockyXmppConnection {
    GObject parent;
    WockyTransport *transport;
    guint8 stream_flags;
};

GType wocky_xmpp_connection_get_type(void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_CONNECTION \
  (wocky_xmpp_connection_get_type())
#define WOCKY_XMPP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_CONNECTION, WockyXmppConnection))
#define WOCKY_XMPP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_CONNECTION, WockyXmppConnectionClass))
#define WOCKY_IS_XMPP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_CONNECTION))
#define WOCKY_IS_XMPP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_CONNECTION))
#define WOCKY_XMPP_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_CONNECTION, WockyXmppConnectionClass))



WockyXmppConnection *wocky_xmpp_connection_new(WockyTransport *transport); 

void wocky_xmpp_connection_open(WockyXmppConnection *connection,
                                const gchar *to, const gchar *from,
                                const gchar *version);

/* Prepare the connection for a reopen from the other side, for example after
 * successfull SASL authentication */
void wocky_xmpp_connection_restart(WockyXmppConnection *connection);

void wocky_xmpp_connection_close(WockyXmppConnection *connection);

void wocky_xmpp_connection_engage(WockyXmppConnection *connection,
                                   WockyTransport *transport);

void wocky_xmpp_connection_disengage(WockyXmppConnection *connection);

gboolean wocky_xmpp_connection_send(WockyXmppConnection *connection, 
                                    WockyXmppStanza *stanza, 
                                    GError **error);

gchar *
wocky_xmpp_connection_new_id (WockyXmppConnection *connection);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_CONNECTION_H__*/
