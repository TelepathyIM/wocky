
#ifndef _WOCKY_SASL_HANDLER_H
#define _WOCKY_SASL_HANDLER_H

#include <glib.h>

#include "wocky-xmpp-stanza.h"

G_BEGIN_DECLS

typedef struct _WockySaslHandler WockySaslHandler;

typedef gchar * (*WockySaslChallengeFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

typedef void (*WockySaslSuccessFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

typedef void (*WockySaslFailureFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

struct _WockySaslHandler {
    gchar *mechanism;
    WockySaslChallengeFunc challenge_func;
    WockySaslSuccessFunc success_func;
    WockySaslFailureFunc failure_func;
    gpointer context;
};

WockySaslHandler *
wocky_sasl_handler_new (
    const gchar *mechanism,
    WockySaslChallengeFunc challenge_func,
    WockySaslSuccessFunc success_func,
    WockySaslFailureFunc failure_func,
    gpointer context);

void
wocky_sasl_handler_free (WockySaslHandler *handler);

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

G_END_DECLS

#endif /* defined _WOCKY_SASL_HANDLER_H */
