#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef _WOCKY_SASL_PLAIN_H
#define _WOCKY_SASL_PLAIN_H

#include <glib-object.h>

#include "wocky-auth-handler.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_SASL_PLAIN wocky_sasl_plain_get_type()

#define WOCKY_SASL_PLAIN(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_SASL_PLAIN, WockySaslPlain))

#define WOCKY_SASL_PLAIN_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_SASL_PLAIN, \
        WockySaslPlainClass))

#define WOCKY_IS_SASL_PLAIN(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_SASL_PLAIN))

#define WOCKY_IS_SASL_PLAIN_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_SASL_PLAIN))

#define WOCKY_SASL_PLAIN_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_SASL_PLAIN, \
        WockySaslPlainClass))

typedef struct _WockySaslPlainPrivate WockySaslPlainPrivate;

typedef struct
{
  GObject parent;
  WockySaslPlainPrivate *priv;
} WockySaslPlain;

typedef struct
{
  GObjectClass parent_class;
} WockySaslPlainClass;

GType
wocky_sasl_plain_get_type (void);

WockySaslPlain *wocky_sasl_plain_new (
    const gchar *username, const gchar *password);

G_END_DECLS

#endif /* defined _WOCKY_SASL_PLAIN_H */
