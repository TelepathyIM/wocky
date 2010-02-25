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
#include "wocky-xmpp-stanza.h"

G_BEGIN_DECLS

typedef struct _WockyPorter WockyPorter;
typedef struct _WockyPorterClass WockyPorterClass;

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
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_porter_send_finish (
    WockyPorter *porter,
    GAsyncResult *result,
    GError **error);

void wocky_porter_send (WockyPorter *porter,
    WockyXmppStanza *stanza);

void wocky_porter_start (WockyPorter *porter);

typedef gboolean (* WockyPorterHandlerFunc) (
    WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer user_data);

guint wocky_porter_register_handler (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyBuildTag spec,
    ...);

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
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyXmppStanza * wocky_porter_send_iq_finish (
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
