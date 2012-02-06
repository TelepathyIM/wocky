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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_SASL_AUTH_H__
#define __WOCKY_SASL_AUTH_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "wocky-stanza.h"
#include "wocky-xmpp-connection.h"
#include "wocky-auth-registry.h"

G_BEGIN_DECLS

typedef struct _WockySaslAuth WockySaslAuth;

/**
 * WockySaslAuthClass:
 *
 * The class of a #WockySaslAuth.
 */
typedef struct _WockySaslAuthClass WockySaslAuthClass;
typedef struct _WockySaslAuthPrivate WockySaslAuthPrivate;


struct _WockySaslAuthClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _WockySaslAuth {
    /*<private>*/
    GObject parent;

    WockySaslAuthPrivate *priv;
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
    WockyXmppConnection *connection,
    WockyAuthRegistry *auth_registry);

void wocky_sasl_auth_add_handler (WockySaslAuth *sasl,
    WockyAuthHandler *handler);

void wocky_sasl_auth_authenticate_async (WockySaslAuth *sasl,
    WockyStanza *features,
    gboolean allow_plain,
    gboolean is_secure,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_sasl_auth_authenticate_finish (WockySaslAuth *sasl,
  GAsyncResult *result,
  GError **error);

void
wocky_sasl_auth_add_handler (WockySaslAuth *auth, WockyAuthHandler *handler);

G_END_DECLS

#endif /* #ifndef __WOCKY_SASL_AUTH_H__*/
