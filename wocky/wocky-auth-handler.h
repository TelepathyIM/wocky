
#ifndef _WOCKY_AUTH_HANDLER_H
#define _WOCKY_AUTH_HANDLER_H

#include <glib.h>

#include "wocky-stanza.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_AUTH_HANDLER (wocky_auth_handler_get_type ())
#define WOCKY_AUTH_HANDLER(obj) (G_TYPE_CHECK_INSTANCE_CAST( \
    (obj), WOCKY_TYPE_AUTH_HANDLER, WockyAuthHandler))
#define WOCKY_IS_AUTH_HANDLER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_AUTH_HANDLER))
#define WOCKY_AUTH_HANDLER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ( \
    (obj), WOCKY_TYPE_AUTH_HANDLER, WockyAuthHandlerIface))

typedef struct _WockyAuthHandler WockyAuthHandler;

/**
 * WockyAuthInitialResponseFunc:
 *
 * Called when authentication begins, if the mechanism allows a response to
 * an implicit challenge during AUTH initiation (which, in XMPP,
 * corresponds to sending the <auth/> stanza to the server).
 *
 * The function should return TRUE on success and optionally set the
 * initial_data to a string (allocated using g_malloc) if there is initial data
 * to send. On error it should return FALSE and set the error
 **/
typedef gboolean (*WockyAuthInitialResponseFunc) (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error);

/**
 * WockyAuthChallengeFunc:
 *
 * Called During authentication, when a <challenge/> stanza or a <success />
 * with data is received. The handler should put response data in response
 * (allocate using g_malloc) if appropriate. The handler is responsible for
 * Base64-encoding responses if appropriate.
 *
 * On success the handler should return TRUE and on failure it should return
 * FALSE and must set the error passed via @error.
 **/
typedef gboolean (*WockyAuthAuthDataFunc) (
    WockyAuthHandler *handler,
    const GString *data,
    GString **response,
    GError **error);

/**
 * WockyAuthSuccessFunc:
 *
 * Called when a <success/> stanza is received during authentication. If no
 * error is returned, then authentication is considered finished. (Typically,
 * an error is only raised if the <success/> stanza was received earlier than
 * expected)
 **/
typedef gboolean (*WockyAuthSuccessFunc) (
    WockyAuthHandler *handler,
    GError **error);

void
wocky_auth_handler_free (WockyAuthHandler *handler);

GType
wocky_auth_handler_get_type (void);

const gchar *
wocky_auth_handler_get_mechanism (WockyAuthHandler *handler);

gboolean
wocky_auth_handler_is_plain (WockyAuthHandler *handler);

gboolean
wocky_auth_handler_get_initial_response (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error);

gboolean
wocky_auth_handler_handle_auth_data (
    WockyAuthHandler *handler,
    const GString *data,
    GString **response,
    GError **error);

gboolean
wocky_auth_handler_handle_success (
    WockyAuthHandler *handler,
    GError **error);

typedef struct _WockyAuthHandlerIface WockyAuthHandlerIface;

/**
 * WockyAuthHandlerIface:
 * @parent: The parent interface.
 * @mechanism: The AUTH mechanism which this handler responds to challenges
 *    for.
 * @plain: Whether the mechanism this handler handles sends secrets in
 *    plaintext.
 * @initial_response_func: Called when the initial <auth/> stanza is generated
 * @auth_data_func: Called when any authentication data from the server
 *                  is received
 * @success_func: Called when a <success/> stanza is received.
 **/
struct _WockyAuthHandlerIface
{
    GTypeInterface parent;
    gchar *mechanism;
    gboolean plain;
    WockyAuthInitialResponseFunc initial_response_func;
    WockyAuthAuthDataFunc auth_data_func;
    WockyAuthSuccessFunc success_func;
};

G_END_DECLS

#endif /* defined _WOCKY_AUTH_HANDLER_H */
