#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef _WOCKY_JABBER_AUTH_PASSWORD_H
#define _WOCKY_JABBER_AUTH_PASSWORD_H

#include <glib-object.h>

#include "wocky-auth-handler.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_JABBER_AUTH_PASSWORD wocky_jabber_auth_password_get_type()

#define WOCKY_JABBER_AUTH_PASSWORD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_JABBER_AUTH_PASSWORD, \
        WockyJabberAuthPassword))

#define WOCKY_JABBER_AUTH_PASSWORD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_JABBER_AUTH_PASSWORD, \
        WockyJabberAuthPasswordClass))

#define WOCKY_IS_JABBER_AUTH_PASSWORD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_JABBER_AUTH_PASSWORD))

#define WOCKY_IS_JABBER_AUTH_PASSWORD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_JABBER_AUTH_PASSWORD))

#define WOCKY_JABBER_AUTH_PASSWORD_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_JABBER_AUTH_PASSWORD, \
        WockyJabberAuthPasswordClass))

typedef struct _WockyJabberAuthPasswordPrivate WockyJabberAuthPasswordPrivate;

typedef struct
{
  GObject parent;
  WockyJabberAuthPasswordPrivate *priv;
} WockyJabberAuthPassword;

typedef struct
{
  GObjectClass parent_class;
} WockyJabberAuthPasswordClass;

GType
wocky_jabber_auth_password_get_type (void);

WockyJabberAuthPassword *wocky_jabber_auth_password_new (
    const gchar *password);

G_END_DECLS

#endif /* defined _WOCKY_JABBER_AUTH_PASSWORD_H */
