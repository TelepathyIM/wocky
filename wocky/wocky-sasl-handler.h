
#ifndef _WOCKY_SASL_HANDLER_H
#define _WOCKY_SASL_HANDLER_H

#include <glib.h>

#include "wocky-xmpp-stanza.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_SASL_HANDLER (wocky_sasl_handler_get_type ())
#define WOCKY_SASL_HANDLER(obj) (G_TYPE_CHECK_INSTANCE_CAST( \
    (obj), WOCKY_TYPE_SASL_HANDLER, WockySaslHandler))
#define WOCKY_IS_SASL_HANDLER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_SASL_HANDLER))
#define WOCKY_SASL_HANDLER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ( \
    (obj), WOCKY_TYPE_SASL_HANDLER, WockySaslHandlerIface))

typedef struct _WockySaslHandler WockySaslHandler;

typedef gchar * (*WockySaslChallengeFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

typedef void (*WockySaslSuccessFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

typedef void (*WockySaslFailureFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

void
wocky_sasl_handler_free (WockySaslHandler *handler);

GType
wocky_sasl_handler_get_type (void);

const gchar *
wocky_sasl_handler_get_mechanism (WockySaslHandler *handler);

gchar *
wocky_sasl_handler_handle_challenge (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error);

void
wocky_sasl_handler_handle_success (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error);

void
wocky_sasl_handler_handle_failure (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error);

typedef struct _WockySaslHandlerIface WockySaslHandlerIface;

struct _WockySaslHandlerIface
{
    GTypeInterface parent;
    gchar *mechanism;
    WockySaslChallengeFunc challenge_func;
    WockySaslSuccessFunc success_func;
    WockySaslFailureFunc failure_func;
};

G_END_DECLS

#endif /* defined _WOCKY_SASL_HANDLER_H */
