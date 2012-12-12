#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

/* wocky-auth-registry.h */
#ifndef _WOCKY_AUTH_REGISTRY_H
#define _WOCKY_AUTH_REGISTRY_H

#include <glib-object.h>
#include <gio/gio.h>
#include "wocky-auth-handler.h"
#include "wocky-enumtypes.h"

G_BEGIN_DECLS

GQuark wocky_auth_error_quark (void);
#define WOCKY_AUTH_ERROR \
  wocky_auth_error_quark ()

/**
 * WockyAuthError:
 * @WOCKY_AUTH_ERROR_INIT_FAILED: Failed to initialize our auth
 *   support
 * @WOCKY_AUTH_ERROR_NOT_SUPPORTED: Server doesn't support this
 *   authentication method
 * @WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS: Server doesn't support
 *   any mechanisms that we support
 * @WOCKY_AUTH_ERROR_NETWORK: Couldn't send our stanzas to the server
 * @WOCKY_AUTH_ERROR_INVALID_REPLY: Server sent an invalid reply
 * @WOCKY_AUTH_ERROR_NO_CREDENTIALS: Failure to provide user
 *   credentials
 * @WOCKY_AUTH_ERROR_FAILURE: Server sent a failure
 * @WOCKY_AUTH_ERROR_CONNRESET: disconnected
 * @WOCKY_AUTH_ERROR_STREAM: XMPP stream error while authing
 * @WOCKY_AUTH_ERROR_RESOURCE_CONFLICT: Resource conflict (relevant in
 *   in jabber auth)
 * @WOCKY_AUTH_ERROR_NOT_AUTHORIZED: Provided credentials are not
 *   valid
 *
 * #WockyAuthRegistry specific errors.
 */
typedef enum
{
  WOCKY_AUTH_ERROR_INIT_FAILED,
  WOCKY_AUTH_ERROR_NOT_SUPPORTED,
  WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS,
  WOCKY_AUTH_ERROR_NETWORK,
  WOCKY_AUTH_ERROR_INVALID_REPLY,
  WOCKY_AUTH_ERROR_NO_CREDENTIALS,
  WOCKY_AUTH_ERROR_FAILURE,
  WOCKY_AUTH_ERROR_CONNRESET,
  WOCKY_AUTH_ERROR_STREAM,
  WOCKY_AUTH_ERROR_RESOURCE_CONFLICT,
  WOCKY_AUTH_ERROR_NOT_AUTHORIZED,
} WockyAuthError;

#define WOCKY_AUTH_MECH_JABBER_DIGEST "X-WOCKY-JABBER-DIGEST"
#define WOCKY_AUTH_MECH_JABBER_PASSWORD "X-WOCKY-JABBER-PASSWORD"
#define WOCKY_AUTH_MECH_SASL_DIGEST_MD5 "DIGEST-MD5"
#define WOCKY_AUTH_MECH_SASL_PLAIN "PLAIN"
#define WOCKY_AUTH_MECH_SASL_SCRAM_SHA_1 "SCRAM-SHA-1"

/**
 * WockyAuthRegistryStartData:
 * @mechanism: the name of the mechanism
 * @initial_response: the data in the response
 *
 * A structure to hold the mechanism and response data.
 */
typedef struct {
  gchar *mechanism;
  GString *initial_response;
} WockyAuthRegistryStartData;

#define WOCKY_TYPE_AUTH_REGISTRY wocky_auth_registry_get_type()

#define WOCKY_AUTH_REGISTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  WOCKY_TYPE_AUTH_REGISTRY, WockyAuthRegistry))

#define WOCKY_AUTH_REGISTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  WOCKY_TYPE_AUTH_REGISTRY, WockyAuthRegistryClass))

#define WOCKY_IS_AUTH_REGISTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  WOCKY_TYPE_AUTH_REGISTRY))

#define WOCKY_IS_AUTH_REGISTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  WOCKY_TYPE_AUTH_REGISTRY))

#define WOCKY_AUTH_REGISTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  WOCKY_TYPE_AUTH_REGISTRY, WockyAuthRegistryClass))

typedef struct _WockyAuthRegistry WockyAuthRegistry;

/**
 * WockyAuthRegistryClass:
 * @start_auth_async_func: a function to call to start an asynchronous
 *   start auth operation; see wocky_auth_registry_start_auth_async() for
 *   more details.
 * @start_auth_finish_func: a function to call to finish an
 *   asynchronous start auth operation; see
 *   wocky_auth_registry_start_auth_finish() for more details.
 * @challenge_async_func: a function to call to start an asynchronous
 *   challenge operation; see wocky_auth_registry_challenge_async() for
 *   more details.
 * @challenge_finish_func: a function to call to finish an asynchronous
 *   challenge operation; see wocky_auth_registry_challenge_finish() for
 *   more details.
 * @success_async_func: a function to call to start an asynchronous
 *   success operation; see wocky_auth_registry_success_async() for
 *   more details.
 * @success_finish_func: a function to call to finish an asynchronous
 *   success operation; see wocky_auth_registry_success_finish() for
 *   more details.
 * @failure_func: a function to call on failure; see
 *   wocky_auth_registry_failure() for more details.
 *
 * The class of a #WockyAuthRegistry.
 */
typedef struct _WockyAuthRegistryClass WockyAuthRegistryClass;
typedef struct _WockyAuthRegistryPrivate WockyAuthRegistryPrivate;

/**
 * WockyAuthRegistryStartAuthAsyncFunc:
 * @self: a #WockyAuthRegistry object
 * @mechanisms: a list of avahilable mechanisms
 * @allow_plain: %TRUE if PLAIN is allowed, otherwise %FALSE
 * @is_secure_channel: %TRUE if channel is secure, otherwise %FALSE
 * @username: the username
 * @password: the password
 * @server: the server
 * @session_id: the session ID
 * @callback: a callback to be called when finished
 * @user_data: data to pass to @callback
 *
 * Starts a async authentication: chooses mechanism and gets initial data.
 * The default function chooses a #WockyAuthHandler by which mechanism it
 * supports and gets the initial data from the chosen handler.
 */
typedef void (*WockyAuthRegistryStartAuthAsyncFunc) (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GAsyncReadyCallback callback,
    gpointer user_data);

/**
 * WockyAuthRegistryStartAuthFinishFunc:
 * @self: a #WockyAuthRegistry object
 * @result: a #GAsyncResult object
 * @start_data: a location to fill with a #WockyAuthRegistryStartData structure
 * @error: a location to fill with a #GError if an error is hit, or %NULL
 *
 * Called to finish the #GAsyncResult task for authentication
 * start. By default, it extracts a #WockyAuthRegistryStartData
 * pointer from a given #GSimpleAsyncResult and copies it to the out
 * param.
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
typedef gboolean (*WockyAuthRegistryStartAuthFinishFunc) (
    WockyAuthRegistry *self,
    GAsyncResult *result,
    WockyAuthRegistryStartData **start_data,
    GError **error);

/**
 * WockyAuthRegistryChallengeAsyncFunc:
 * @self: a #WockyAuthRegistry object
 * @challenge_data: the challenge data string
 * @callback: a callback to call when finished
 * @user_data: data to pass to @callback
 *
 * Recieves a challenge and asynchronously provides a reply. By
 * default the challenge is passed on to the chosen #WockyAuthHandler.
 */
typedef void (*WockyAuthRegistryChallengeAsyncFunc) (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data);

/**
 * WockyAuthRegistryChallengeFinishFunc:
 * @self: a #WockyAuthRegistry object
 * @result: a #GAsyncResult object
 * @response: a location to be filled with the response string
 * @error: a location to fill with a #GError if an error is hit, or %NULL
 *
 * Finishes a #GAsyncResult from
 * #WockyAuthRegistryChallengeAsyncFunc. By default it extracts a
 * #GString response from the given #GSimpleAsyncResult and copies it
 * to the out param.
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
typedef gboolean (*WockyAuthRegistryChallengeFinishFunc) (
    WockyAuthRegistry *self,
    GAsyncResult *result,
    GString **response,
    GError **error);

/**
 * WockyAuthRegistrySuccessAsyncFunc:
 * @self: a #WockyAuthRegistry object
 * @callback: a callback to be called when finished
 * @user_data: data to pass to @callback
 *
 * Notifies the registry of authentication success, and allows a last ditch
 * attempt at aborting the authentication at the client's discretion.
 */
typedef void (*WockyAuthRegistrySuccessAsyncFunc) (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

/**
 * WockyAuthRegistrySuccessFinishFunc:
 * @self: a #WockyAuthRegistry object
 * @result: a #GAsyncResult object
 * @error: a location to fill with a #GError if an error is hit, or %NULL
 *
 * Finishes a #GAsyncResult from
 * #WockyAuthRegistrySuccessAsyncFunc. It checks for any errors set on
 * the given #GSimpleAsyncResult, copies the #GError to an out param
 * and returns %FALSE if there was an error.
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
typedef gboolean (*WockyAuthRegistrySuccessFinishFunc) (
    WockyAuthRegistry *self,
    GAsyncResult *result,
    GError **error);

/**
 * WockyAuthRegistryFailureFunc:
 * @self: a #WockyAuthRegistry object
 * @error: a #GError describing the failure
 *
 * Notifies the client of a server-side error. By default this is not
 * implemented.
 *
 */
typedef void (*WockyAuthRegistryFailureFunc) (WockyAuthRegistry *self,
    GError *error);

struct _WockyAuthRegistry
{
  /*<private>*/
  GObject parent;

  WockyAuthRegistryPrivate *priv;
};

struct _WockyAuthRegistryClass
{
  /*<private>*/
  GObjectClass parent_class;

  /*<public>*/
  WockyAuthRegistryStartAuthAsyncFunc start_auth_async_func;
  WockyAuthRegistryStartAuthFinishFunc start_auth_finish_func;

  WockyAuthRegistryChallengeAsyncFunc challenge_async_func;
  WockyAuthRegistryChallengeFinishFunc challenge_finish_func;

  WockyAuthRegistrySuccessAsyncFunc success_async_func;
  WockyAuthRegistrySuccessFinishFunc success_finish_func;

  WockyAuthRegistryFailureFunc failure_func;
};

GType wocky_auth_registry_get_type (void) G_GNUC_CONST;

WockyAuthRegistry *wocky_auth_registry_new (void);

void wocky_auth_registry_start_auth_async (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_auth_registry_start_auth_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    WockyAuthRegistryStartData **start_data,
    GError **error);

void wocky_auth_registry_challenge_async (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_auth_registry_challenge_finish (WockyAuthRegistry *self,
    GAsyncResult *res,
    GString **response,
    GError **error);

void wocky_auth_registry_success_async (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_auth_registry_success_finish (WockyAuthRegistry *self,
    GAsyncResult *res,
    GError **error);

void wocky_auth_registry_add_handler (WockyAuthRegistry *self,
    WockyAuthHandler *handler);

void wocky_auth_registry_start_data_free (
    WockyAuthRegistryStartData *start_data);

WockyAuthRegistryStartData * wocky_auth_registry_start_data_new (
    const gchar *mechanism,
    const GString *initial_response);

WockyAuthRegistryStartData * wocky_auth_registry_start_data_dup (
    WockyAuthRegistryStartData *start_data);

void wocky_auth_registry_failure (WockyAuthRegistry *self,
    GError *error);

gboolean wocky_auth_registry_supports_one_of (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain);

G_END_DECLS

#endif /* _WOCKY_AUTH_REGISTRY_H */
