
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

/** WockySaslInitialResponseFunc:
 * When authentication begins, in case the mechanism allows a response to
 * an implicit challenge during SASL initiation (which, in XMPP,
 * corresponds to sending the <auth/> stanza to the server).
 *
 * The function should return TRUE on success and optionally set the
 * initial_data to a string (allocated using g_malloc) if there is initial data
 * to send. On error it should return FALSE and set the error
 **/
typedef gboolean (*WockySaslInitialResponseFunc) (WockySaslHandler *handler,
    gchar **initial_data,
    GError **error);

/** WockySaslChallengeFunc:
 * Called During authentication, when a <challenge/> stanza or a <success />
 * with data is received. The handler should put response data in response
 * (allocate using g_malloc) if appropriate. The handler is responsible for
 * Base64-encoding responses if appropriate.
 *
 * On success the handler should return TRUE and on failure it should return
 * FALSE and must set the error passed via @error.
 **/
typedef gboolean (*WockySaslAuthDataFunc) (
    WockySaslHandler *handler,
    const gchar *data,
    gchar **response,
    GError **error);

/** WockySaslSuccessFunc:
 * Called when a <success/> stanza is received during authentication. If no
 * error is returned, then authentication is considered finished. (Typically,
 * an error is only raised if the <success/> stanza was received earlier than
 * expected)
 **/
typedef gboolean (*WockySaslSuccessFunc) (
    WockySaslHandler *handler,
    GError **error);

void
wocky_sasl_handler_free (WockySaslHandler *handler);

GType
wocky_sasl_handler_get_type (void);

const gchar *
wocky_sasl_handler_get_mechanism (WockySaslHandler *handler);

gboolean
wocky_sasl_handler_is_plain (WockySaslHandler *handler);

gboolean
wocky_sasl_handler_get_initial_response(WockySaslHandler *handler,
    gchar **initial_data,
    GError **error);

gboolean
wocky_sasl_handler_handle_auth_data (
    WockySaslHandler *handler,
    const gchar *data,
    gchar **response,
    GError **error);

gboolean
wocky_sasl_handler_handle_success (
    WockySaslHandler *handler,
    GError **error);

typedef struct _WockySaslHandlerIface WockySaslHandlerIface;

/**
 * WockySaslHandlerIface:
 * @parent: The parent interface.
 * @mechanism: The SASL mechanism which this handler responds to challenges
 *    for.
 * @plain: Whether the mechanism this handler handles sends secrets in
 *    plaintext.
 * @initial_response_func: Called when the initial <auth/> stanza is generated
 * @auth_data_func: Called when any authentication data from the server
 *                  is received
 * @success_func: Called when a <success/> stanza is received.
 **/
struct _WockySaslHandlerIface
{
    GTypeInterface parent;
    gchar *mechanism;
    gboolean plain;
    WockySaslInitialResponseFunc initial_response_func;
    WockySaslAuthDataFunc auth_data_func;
    WockySaslSuccessFunc success_func;
};

G_END_DECLS

#endif /* defined _WOCKY_SASL_HANDLER_H */
