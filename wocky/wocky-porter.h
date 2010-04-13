/*
 * wocky-porter.h - Header for WockyPorter
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

#ifndef __WOCKY_PORTER_H__
#define __WOCKY_PORTER_H__

#include <glib-object.h>

#include "wocky-xmpp-connection.h"
#include "wocky-stanza.h"

G_BEGIN_DECLS

/**
 * WockyPorter:
 *
 * An object providing a convenient wrapper around a #WockyXmppConnection to
 * send and receive stanzas.
 */
typedef struct _WockyPorter WockyPorter;
typedef struct _WockyPorterClass WockyPorterClass;
typedef struct _WockyPorterPrivate WockyPorterPrivate;

/**
 * WockyPorterError:
 * @WOCKY_PORTER_ERROR_NOT_STARTED : The #WockyPorter has not been started yet
 * @WOCKY_PORTER_ERROR_CLOSING : The #WockyPorter is closing
 * @WOCKY_PORTER_ERROR_CLOSED : The #WockyPorter is closed
 * @WOCKY_PORTER_ERROR_NOT_IQ : The #WockyStanza is not an IQ
 * @WOCKY_PORTER_ERROR_FORCIBLY_CLOSED : The #WockyPorter has been forced to
 * close
 *
 * The #WockyPorter specific errors.
 */
typedef enum {
  WOCKY_PORTER_ERROR_NOT_STARTED,
  WOCKY_PORTER_ERROR_CLOSING,
  WOCKY_PORTER_ERROR_CLOSED,
  WOCKY_PORTER_ERROR_NOT_IQ,
  WOCKY_PORTER_ERROR_FORCIBLY_CLOSED,
} WockyPorterError;

GQuark wocky_porter_error_quark (void);

#define WOCKY_PORTER_HANDLER_PRIORITY_MIN 0
#define WOCKY_PORTER_HANDLER_PRIORITY_NORMAL (guint) (G_MAXUINT / 2)
#define WOCKY_PORTER_HANDLER_PRIORITY_MAX G_MAXUINT

/**
 * WOCKY_PORTER_ERROR:
 *
 * Get access to the error quark of the xmpp porter.
 */
#define WOCKY_PORTER_ERROR (wocky_porter_error_quark ())


struct _WockyPorterClass {
    GObjectClass parent_class;
};

struct _WockyPorter {
    GObject parent;
    WockyPorterPrivate *priv;
};

GType wocky_porter_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_PORTER \
  (wocky_porter_get_type ())
#define WOCKY_PORTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PORTER, \
   WockyPorter))
#define WOCKY_PORTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PORTER, \
   WockyPorterClass))
#define WOCKY_IS_PORTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PORTER))
#define WOCKY_IS_PORTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PORTER))
#define WOCKY_PORTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PORTER, \
   WockyPorterClass))

WockyPorter * wocky_porter_new (WockyXmppConnection *connection);

void wocky_porter_send_async (WockyPorter *porter,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_porter_send_finish (
    WockyPorter *porter,
    GAsyncResult *result,
    GError **error);

void wocky_porter_send (WockyPorter *porter,
    WockyStanza *stanza);

void wocky_porter_start (WockyPorter *porter);

/**
 * WockyPorterHandlerFunc:
 * @porter: the #WockyPorter dispatching the #WockyStanza
 * @stanza: the #WockyStanza being dispatched
 * @user_data: the data passed when the handler has been registered
 *
 * Handler called when a matching stanza has been received by the
 * #WockyPorter.
 *
 * If a handler returns %TRUE, this means that it has taken responsibility
 * for handling the stanza and (if applicable) sending a reply.
 *
 * If a handler returns %FALSE, this indicates that it has declined to process
 * the stanza. The next handler (if any) is invoked.
 *
 * A handler must not assume that @stanza will continue to exist after the
 * handler has returned, unless it has taken a reference to @stanza using
 * g_object_ref().
 *
 * Returns: %TRUE if the stanza has been handled, %FALSE if not
 */
typedef gboolean (* WockyPorterHandlerFunc) (
    WockyPorter *porter,
    WockyStanza *stanza,
    gpointer user_data);

guint wocky_porter_register_handler (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    ...) G_GNUC_NULL_TERMINATED;

void wocky_porter_unregister_handler (WockyPorter *porter,
    guint id);

void wocky_porter_close_async (WockyPorter *porter,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_porter_close_finish (
    WockyPorter *porter,
    GAsyncResult *result,
    GError **error);

void wocky_porter_send_iq_async (WockyPorter *porter,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyStanza * wocky_porter_send_iq_finish (
    WockyPorter *porter,
    GAsyncResult *result,
    GError **error);

void wocky_porter_force_close_async (WockyPorter *porter,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_porter_force_close_finish (
    WockyPorter *porter,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif /* #ifndef __WOCKY_PORTER_H__*/
