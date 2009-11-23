/*
 * wocky-sasl-auth.h - Header for WockySaslAuth
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

#ifndef __WOCKY_SASL_AUTH_H__
#define __WOCKY_SASL_AUTH_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "wocky-sasl-handler.h"
#include "wocky-xmpp-stanza.h"
#include "wocky-xmpp-connection.h"

G_BEGIN_DECLS

GQuark wocky_sasl_auth_error_quark (void);
#define WOCKY_SASL_AUTH_ERROR \
  wocky_sasl_auth_error_quark ()

typedef enum
{
  /* Failed to initialize our sasl support */
  WOCKY_SASL_AUTH_ERROR_INIT_FAILED,
  /* Server doesn't support sasl (no mechanisms) */
  WOCKY_SASL_AUTH_ERROR_SASL_NOT_SUPPORTED,
  /* Server doesn't support any mechanisms that we support */
  WOCKY_SASL_AUTH_ERROR_NO_SUPPORTED_MECHANISMS,
  /* Couldn't send our stanzas to the server */
  WOCKY_SASL_AUTH_ERROR_NETWORK,
  /* Server sent an invalid reply */
  WOCKY_SASL_AUTH_ERROR_INVALID_REPLY,
  /* Failure to provide user credentials */
  WOCKY_SASL_AUTH_ERROR_NO_CREDENTIALS,
  /* Server sent a failure */
  WOCKY_SASL_AUTH_ERROR_FAILURE,
  /* disconnected */
  WOCKY_SASL_AUTH_ERROR_CONNRESET,
  /* XMPP stream error while authing */
  WOCKY_SASL_AUTH_ERROR_STREAM,
} WockySaslAuthError;

typedef struct _WockySaslAuth WockySaslAuth;
typedef struct _WockySaslAuthClass WockySaslAuthClass;

struct _WockySaslAuthClass {
    GObjectClass parent_class;
};

struct _WockySaslAuth {
    GObject parent;
};

GType wocky_sasl_auth_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_SASL_AUTH \
  (wocky_sasl_auth_get_type ())
#define WOCKY_SASL_AUTH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_SASL_AUTH, WockySaslAuth))
#define WOCKY_SASL_AUTH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_SASL_AUTH, WockySaslAuthClass))
#define WOCKY_IS_SASL_AUTH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_SASL_AUTH))
#define WOCKY_IS_SASL_AUTH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_SASL_AUTH))
#define WOCKY_SASL_AUTH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_SASL_AUTH, WockySaslAuthClass))

WockySaslAuth *wocky_sasl_auth_new (const gchar *server,
    const gchar *username,
    const gchar *password,
    WockyXmppConnection *connection);

void wocky_sasl_auth_add_handler (WockySaslAuth *sasl,
    WockySaslHandler *handler);

void wocky_sasl_auth_authenticate_async (WockySaslAuth *sasl,
    WockyXmppStanza *features,
    gboolean allow_plain,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_sasl_auth_authenticate_finish (WockySaslAuth *sasl,
  GAsyncResult *result,
  GError **error);

void
wocky_sasl_auth_add_handler (WockySaslAuth *auth, WockySaslHandler *handler);

G_END_DECLS

#endif /* #ifndef __WOCKY_SASL_AUTH_H__*/
