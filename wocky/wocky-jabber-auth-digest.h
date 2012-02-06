#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef _WOCKY_JABBER_AUTH_DIGEST_H
#define _WOCKY_JABBER_AUTH_DIGEST_H

#include <glib-object.h>

#include "wocky-auth-handler.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_JABBER_AUTH_DIGEST wocky_jabber_auth_digest_get_type()

#define WOCKY_JABBER_AUTH_DIGEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_JABBER_AUTH_DIGEST, \
        WockyJabberAuthDigest))

#define WOCKY_JABBER_AUTH_DIGEST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_JABBER_AUTH_DIGEST, \
        WockyJabberAuthDigestClass))

#define WOCKY_IS_JABBER_AUTH_DIGEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_JABBER_AUTH_DIGEST))

#define WOCKY_IS_JABBER_AUTH_DIGEST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_JABBER_AUTH_DIGEST))

#define WOCKY_JABBER_AUTH_DIGEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_JABBER_AUTH_DIGEST, \
        WockyJabberAuthDigestClass))

typedef struct _WockyJabberAuthDigestPrivate WockyJabberAuthDigestPrivate;

typedef struct
{
  /*<private>*/
  GObject parent;
  WockyJabberAuthDigestPrivate *priv;
} WockyJabberAuthDigest;

typedef struct
{
  /*<private>*/
  GObjectClass parent_class;
} WockyJabberAuthDigestClass;

GType
wocky_jabber_auth_digest_get_type (void);

WockyJabberAuthDigest *wocky_jabber_auth_digest_new (
    const gchar *server, const gchar *password);

G_END_DECLS

#endif /* defined _WOCKY_JABBER_AUTH_DIGEST_H */
