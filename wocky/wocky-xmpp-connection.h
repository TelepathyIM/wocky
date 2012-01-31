/*
 * wocky-xmpp-connection.h - Header for WockyXmppConnection
 * Copyright (C) 2006-2009 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#ifndef __WOCKY_XMPP_CONNECTION_H__
#define __WOCKY_XMPP_CONNECTION_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "wocky-stanza.h"

G_BEGIN_DECLS

typedef struct _WockyXmppConnection WockyXmppConnection;

/**
 * WockyXmppConnectionClass:
 *
 * The class of a #WockyXmppConnection.
 */
typedef struct _WockyXmppConnectionClass WockyXmppConnectionClass;
typedef struct _WockyXmppConnectionPrivate WockyXmppConnectionPrivate;

/**
 * WockyXmppConnectionError:
 * @WOCKY_XMPP_CONNECTION_ERROR_EOS : Connection got closed before receiving
 *                                    an XMPP stream close.
 * @WOCKY_XMPP_CONNECTION_ERROR_CLOSED : Other side closed the xmpp stream.
 * @WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN : Trying to send or receive while the
 * connection isn't open.
 * @WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED : Trying to send or receive
 * while the connection is closed.
 * @WOCKY_XMPP_CONNECTION_ERROR_IS_OPEN: Trying to send or receive
 * the connection opening when it's already open
 *
 * The #WockyXmppConnection specific errors that can occur while
 *  reading a stream.
 */
typedef enum {
  WOCKY_XMPP_CONNECTION_ERROR_EOS,
  WOCKY_XMPP_CONNECTION_ERROR_CLOSED,
  WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN,
  WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
  WOCKY_XMPP_CONNECTION_ERROR_IS_OPEN,
} WockyXmppConnectionError;

GQuark wocky_xmpp_connection_error_quark (void);

/**
 * WOCKY_XMPP_CONNECTION_ERROR:
 *
 * Get access to the error quark of the xmpp connection.
 */
#define WOCKY_XMPP_CONNECTION_ERROR (wocky_xmpp_connection_error_quark ())

struct _WockyXmppConnectionClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _WockyXmppConnection {
    /*<private>*/
    GObject parent;

    WockyXmppConnectionPrivate *priv;
};

GType wocky_xmpp_connection_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_CONNECTION \
  (wocky_xmpp_connection_get_type ())
#define WOCKY_XMPP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_CONNECTION, \
   WockyXmppConnection))
#define WOCKY_XMPP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_CONNECTION, \
   WockyXmppConnectionClass))
#define WOCKY_IS_XMPP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_CONNECTION))
#define WOCKY_IS_XMPP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_CONNECTION))
#define WOCKY_XMPP_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_CONNECTION, \
   WockyXmppConnectionClass))

WockyXmppConnection *wocky_xmpp_connection_new (GIOStream *stream);

void wocky_xmpp_connection_send_open_async (WockyXmppConnection *connection,
    const gchar *to,
    const gchar *from,
    const gchar *version,
    const gchar *lang,
    const gchar *id,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_connection_send_open_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_connection_recv_open_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_connection_recv_open_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    gchar **to,
    gchar **from,
    gchar **version,
    gchar **lang,
    gchar **id,
    GError **error);

void wocky_xmpp_connection_send_stanza_async (WockyXmppConnection *connection,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_connection_send_stanza_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_connection_recv_stanza_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyStanza *wocky_xmpp_connection_recv_stanza_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_connection_send_close_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_connection_send_close_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_connection_force_close_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_connection_force_close_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_connection_send_whitespace_ping_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_xmpp_connection_send_whitespace_ping_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error);

void wocky_xmpp_connection_reset (WockyXmppConnection *connection);

gchar * wocky_xmpp_connection_new_id (WockyXmppConnection *self);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_CONNECTION_H__*/
