
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

/** WockySaslChallengeFunc:
 * Called in two cases:
 *
 *  * When authentication begins, in case the mechanism allows a response to
 *    an implicit challenge during SASL initiation (which, in XMPP,
 *    corresponds to sending the <auth/> stanza to the server). In this case,
 *    @stanza is NULL. The function may return NULL to indicate that it has no
 *    initial response to send.
 *  * During authentication, when a <challenge/> stanza is received. The
 *    handler should return a response to the challenge.
 *
 * The handler is responsible for Base64-encoding responses if appropriate. In
 * either case, the handler may return NULL and pass an error via @error to
 * indicate that an error occurred.
 **/
typedef gchar * (*WockySaslChallengeFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

/** WockySaslSuccessFunc:
 * Called when a <success/> stanza is received during authentication. If no
 * error is returned, then authentication is considered finished. (Typically,
 * an error is only raised if the <success/> stanza was received earlier than
 * expected.)
 **/
typedef void (*WockySaslSuccessFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

/** WockySaslSuccessFunc:
 * Called when a <failure/> stanza is received during authentication. The
 * handler may provide a detailed error via @error. If no error is returned,
 * authentication will fail with a general authentication error.
 **/
typedef void (*WockySaslFailureFunc) (
    WockySaslHandler *handler, WockyXmppStanza *stanza, GError **error);

void
wocky_sasl_handler_free (WockySaslHandler *handler);

GType
wocky_sasl_handler_get_type (void);

const gchar *
wocky_sasl_handler_get_mechanism (WockySaslHandler *handler);

gboolean
wocky_sasl_handler_is_plain (WockySaslHandler *handler);

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

/**
 * WockySaslHandlerIface:
 * @parent: The parent interface.
 * @mechanism: The SASL mechanism which this handler responds to challenges
 *    for.
 * @plain: Whether the mechanism this handler handles sends secrets in
 *    plaintext.
 * @challenge_func: Called when a <challenge/> stanza is received.
 * @success_func: Called when a <success/> stanza is received.
 * @failure_func: Called when a <failure/> stanza is received.
 **/
struct _WockySaslHandlerIface
{
    GTypeInterface parent;
    gchar *mechanism;
    gboolean plain;
    WockySaslChallengeFunc challenge_func;
    WockySaslSuccessFunc success_func;
    WockySaslFailureFunc failure_func;
};

G_END_DECLS

#endif /* defined _WOCKY_SASL_HANDLER_H */
