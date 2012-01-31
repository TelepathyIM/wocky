#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef _WOCKY_SASL_DIGEST_MD5_H
#define _WOCKY_SASL_DIGEST_MD5_H

#include <glib-object.h>

#include "wocky-auth-handler.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_SASL_DIGEST_MD5 \
    wocky_sasl_digest_md5_get_type ()

#define WOCKY_SASL_DIGEST_MD5(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_SASL_DIGEST_MD5, \
        WockySaslDigestMd5))

#define WOCKY_SASL_DIGEST_MD5_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_SASL_DIGEST_MD5, \
        WockySaslDigestMd5Class))

#define WOCKY_IS_SASL_DIGEST_MD5(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_SASL_DIGEST_MD5))

#define WOCKY_IS_SASL_DIGEST_MD5_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_SASL_DIGEST_MD5))

#define WOCKY_SASL_DIGEST_MD5_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_SASL_DIGEST_MD5, \
        WockySaslDigestMd5Class))

typedef struct _WockySaslDigestMd5Private WockySaslDigestMd5Private;

typedef struct
{
  GObject parent;
  WockySaslDigestMd5Private *priv;
} WockySaslDigestMd5;

typedef struct
{
  GObjectClass parent_class;
} WockySaslDigestMd5Class;

GType
wocky_sasl_digest_md5_get_type (void);

WockySaslDigestMd5 *
wocky_sasl_digest_md5_new (
    const gchar *server, const gchar *username, const gchar *password);

G_END_DECLS

#endif /* _WOCKY_SASL_DIGEST_MD5_H */
