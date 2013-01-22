/*
 * wocky-tls-handler.h - Header for WockyTLSHandler
 * Copyright (C) 2010 Collabora Ltd.
 * @author Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
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

#ifndef __WOCKY_TLS_HANDLER_H__
#define __WOCKY_TLS_HANDLER_H__

#include <glib-object.h>

#include "wocky-tls.h"

G_BEGIN_DECLS

typedef struct _WockyTLSHandler WockyTLSHandler;

/**
 * WockyTLSHandlerClass:
 * @verify_async_func: a function to call to start an asychronous
 *   verify operation; see wocky_tls_handler_verify_async() for more
 *   details
 * @verify_finish_func: a function to call to finish an asychronous
 *   verify operation; see wocky_tls_handler_verify_finish() for more
 *   details
 *
 * The class of a #WockyTLSHandler.
 */
typedef struct _WockyTLSHandlerClass WockyTLSHandlerClass;
typedef struct _WockyTLSHandlerPrivate WockyTLSHandlerPrivate;

typedef void (*WockyTLSHandlerVerifyAsyncFunc) (WockyTLSHandler *self,
    WockyTLSSession *tls_session,
    const gchar *peername,
    GStrv extra_identities,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef gboolean (*WockyTLSHandlerVerifyFinishFunc) (WockyTLSHandler *self,
    GAsyncResult *res,
    GError **error);

struct _WockyTLSHandlerClass {
  /*<private>*/
  GObjectClass parent_class;

  /*<public>*/
  WockyTLSHandlerVerifyAsyncFunc verify_async_func;
  WockyTLSHandlerVerifyFinishFunc verify_finish_func;
};

struct _WockyTLSHandler {
  /*<private>*/
  GObject parent;

  WockyTLSHandlerPrivate *priv;
};

GType wocky_tls_handler_get_type (void);

#define WOCKY_TYPE_TLS_HANDLER \
  (wocky_tls_handler_get_type ())
#define WOCKY_TLS_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_TLS_HANDLER, WockyTLSHandler))
#define WOCKY_TLS_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_TLS_HANDLER, \
      WockyTLSHandlerClass))
#define WOCKY_IS_TLS_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_TLS_HANDLER))
#define WOCKY_IS_TLS_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_TLS_HANDLER))
#define WOCKY_TLS_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_TLS_HANDLER, \
      WockyTLSHandlerClass))

WockyTLSHandler * wocky_tls_handler_new (gboolean ignore_ssl_errors);

void wocky_tls_handler_verify_async (WockyTLSHandler *self,
    WockyTLSSession *tls_session,
    const gchar *peername,
    GStrv extra_identities,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean wocky_tls_handler_verify_finish (WockyTLSHandler *self,
    GAsyncResult *result,
    GError **error);

gboolean wocky_tls_handler_add_ca (WockyTLSHandler *self,
    const gchar *path);
void wocky_tls_handler_forget_cas (WockyTLSHandler *self);

gboolean wocky_tls_handler_add_crl (WockyTLSHandler *self, const gchar *path);

GSList *wocky_tls_handler_get_cas (WockyTLSHandler *self);
GSList *wocky_tls_handler_get_crl (WockyTLSHandler *self);

G_END_DECLS

#endif /* #ifndef __WOCKY_TLS_HANDLER_H__*/
