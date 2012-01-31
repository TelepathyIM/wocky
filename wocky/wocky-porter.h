/*
 * wocky-porter.h - Header for WockyPorter
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

#ifndef __WOCKY_PORTER_H__
#define __WOCKY_PORTER_H__

#include <glib-object.h>

#include <gio/gio.h>

#include "wocky-stanza.h"

G_BEGIN_DECLS

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

/**
 * WOCKY_PORTER_ERROR:
 *
 * Get access to the error quark of the xmpp porter.
 */
#define WOCKY_PORTER_ERROR (wocky_porter_error_quark ())

GType wocky_porter_get_type (void);

/* type macros */
#define WOCKY_TYPE_PORTER \
  (wocky_porter_get_type ())
#define WOCKY_PORTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_PORTER, \
      WockyPorter))
#define WOCKY_IS_PORTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_PORTER))
#define WOCKY_PORTER_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), WOCKY_TYPE_PORTER, \
      WockyPorterInterface))

typedef struct _WockyPorter WockyPorter;
typedef struct _WockyPorterInterface WockyPorterInterface;

#define WOCKY_PORTER_HANDLER_PRIORITY_MIN 0
#define WOCKY_PORTER_HANDLER_PRIORITY_NORMAL (guint) (G_MAXUINT / 2)
#define WOCKY_PORTER_HANDLER_PRIORITY_MAX G_MAXUINT

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

/**
 * WockyPorterInterface:
 * @parent_iface: Fields shared with #GTypeInterface.
 * @get_full_jid: Return the full JID of the user according to the
 *   porter; see wocky_porter_get_full_jid() for more details.
 * @get_bare_jid: Return the bare JID of the user according to the
 *   porter; see wocky_porter_get_full_jid() for more details.
 * @get_resource: Return the resource of the user according to the
 *   porter; see wocky_porter_get_full_jid() for more details.
 * @start: Start the porter; see wocky_porter_start() for more
 *   details.
 * @send_async: Start an asynchronous stanza send operation; see
 *   wocky_porter_send_async() for more details.
 * @send_finish: Finish an asynchronous stanza send operation; see
 *   wocky_porter_send_finish() for more details.
 * @register_handler_from_by_stanza: Register a stanza handler from a
 *   specific contact; see
 *   wocky_porter_register_handler_from_by_stanza() for more details.
 * @register_handler_from_anyone_by_stanza: Register a stanza hander
 *   from any contact; see
 *   wocky_porter_register_handler_from_anyone_by_stanza() for more
 *   details.
 * @unregister_handler: Unregister a stanza handler; see
 *   wocky_porter_unregister_handler() for more details.
 * @close_async: Start an asynchronous porter close operation; see
 *   wocky_porter_close_async() for more details.
 * @close_finish: Finish an asynchronous porter close operation; see
 *   wocky_porter_close_finish() for more details.
 * @send_iq_async: Start an asynchronous IQ stanza send operation; see
 *   wocky_porter_send_iq_async() for more details.
 * @send_iq_finish: Finish an asynchronous IQ stanza send operation;
 *   see wocky_porter_send_iq_finish() for more details.
 * @force_close_async: Start an asynchronous porter force close
 *   operation; see wocky_porter_force_close_async() for more details.
 * @force_close_finish: Finish an asynchronous porter force close
 *   operation; see wocky_porter_force_close_finish() for more details.
 *
 * The vtable for a porter implementation.
 */
struct _WockyPorterInterface
{
  GTypeInterface parent_iface;

  const gchar * (*get_full_jid) (WockyPorter *self);
  const gchar * (*get_bare_jid) (WockyPorter *self);
  const gchar * (*get_resource) (WockyPorter *self);

  void (*start) (WockyPorter *porter);

  void (*send_async) (WockyPorter *porter,
      WockyStanza *stanza,
      GCancellable *cancellable,
      GAsyncReadyCallback callback,
      gpointer user_data);

  gboolean (*send_finish) (WockyPorter *porter,
      GAsyncResult *result,
      GError **error);

  guint (*register_handler_from_by_stanza) (
      WockyPorter *self,
      WockyStanzaType type,
      WockyStanzaSubType sub_type,
      const gchar *from,
      guint priority,
      WockyPorterHandlerFunc callback,
      gpointer user_data,
      WockyStanza *stanza);

  guint (*register_handler_from_anyone_by_stanza) (
      WockyPorter *self,
      WockyStanzaType type,
      WockyStanzaSubType sub_type,
      guint priority,
      WockyPorterHandlerFunc callback,
      gpointer user_data,
      WockyStanza *stanza);

  void (*unregister_handler) (WockyPorter *self,
      guint id);

  void (*close_async) (WockyPorter *self,
      GCancellable *cancellable,
      GAsyncReadyCallback callback,
      gpointer user_data);

  gboolean (*close_finish) (WockyPorter *self,
      GAsyncResult *result,
      GError **error);

  void (*send_iq_async) (WockyPorter *porter,
      WockyStanza *stanza,
      GCancellable *cancellable,
      GAsyncReadyCallback callback,
      gpointer user_data);

  WockyStanza * (*send_iq_finish) (WockyPorter *porter,
      GAsyncResult *result,
      GError **error);

  void (*force_close_async) (WockyPorter *porter,
      GCancellable *cancellable,
      GAsyncReadyCallback callback,
      gpointer user_data);

  gboolean (*force_close_finish) (WockyPorter *porter,
      GAsyncResult *result,
      GError **error);
};

void wocky_porter_start (WockyPorter *porter);

const gchar * wocky_porter_get_full_jid (WockyPorter *self);
const gchar * wocky_porter_get_bare_jid (WockyPorter *self);
const gchar * wocky_porter_get_resource (WockyPorter *self);

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

guint wocky_porter_register_handler_from_va (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    va_list ap);

guint wocky_porter_register_handler_from_by_stanza (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza);

guint wocky_porter_register_handler_from (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    ...) G_GNUC_NULL_TERMINATED;

guint wocky_porter_register_handler_from_anyone_va (
    WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    va_list ap);

guint wocky_porter_register_handler_from_anyone_by_stanza (
    WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza);

guint wocky_porter_register_handler_from_anyone (
    WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
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

gboolean wocky_porter_close_finish (WockyPorter *porter,
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
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

void wocky_porter_acknowledge_iq (
    WockyPorter *porter,
    WockyStanza *stanza,
    ...) G_GNUC_NULL_TERMINATED;

void wocky_porter_send_iq_error (
    WockyPorter *porter,
    WockyStanza *stanza,
    WockyXmppError error_code,
    const gchar *message);

void wocky_porter_send_iq_gerror (
    WockyPorter *porter,
    WockyStanza *stanza,
    const GError *error);

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
