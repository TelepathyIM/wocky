#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-auth-handler.h"
#include "wocky-auth-registry.h"

GType
wocky_auth_handler_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      const GTypeInfo info =
      {
        /* class_size */ sizeof (WockyAuthHandlerIface),
        /* base_init */ NULL,
        /* base_finalize */ NULL,
        /* class_init */ NULL,
        /* class_finalize */ NULL,
        /* class_data */ NULL,
        /* instance_size */ 0,
        /* n_preallocs */ 0,
        /* instance_init */ NULL,
        /* value_table */ NULL
      };
      GType g_define_type_id = g_type_register_static (
          G_TYPE_INTERFACE, "WockyAuthHandler", &info, 0);

      g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_OBJECT);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

/**
 * wocky_auth_handler_get_mechanism:
 * @handler: a handler for a SASL mechanism.
 *
 * Returns the name of the SASL mechanism @handler implements.
 *
 * Returns: the name of the SASL mechanism @handler implements.
 */
const gchar *
wocky_auth_handler_get_mechanism (WockyAuthHandler *handler)
{
  return WOCKY_AUTH_HANDLER_GET_IFACE (handler)->mechanism;
}

/**
 * wocky_auth_handler_is_plain:
 * @handler: a handler for a SASL mechanism.
 *
 * Checks whether @handler sends secrets in plaintext. This may be used to
 * decide whether to use @handler on an insecure XMPP connection.
 *
 * Returns: %TRUE if @handler sends secrets in plaintext.
 */
gboolean
wocky_auth_handler_is_plain (WockyAuthHandler *handler)
{
  return WOCKY_AUTH_HANDLER_GET_IFACE (handler)->plain;
}

/**
 * wocky_auth_handler_get_initial_response:
 * @handler: a handler for a SASL mechanism
 * @initial_data: (out) (transfer full): initial data to send to the server, if
 *  any
 * @error: an optional location for a #GError to fill, or %NULL
 *
 * Called when authentication begins to fetch the initial data to send to the
 * server in the <code>&lt;auth/&gt;</code> stanza.
 *
 * If this function returns %TRUE, @initial_data will be non-%NULL if @handler
 * provides an initial response, and %NULL otherwise.
 *
 * Returns: %TRUE on success; %FALSE otherwise.
 */
gboolean
wocky_auth_handler_get_initial_response (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error)
{
  WockyAuthInitialResponseFunc func =
    WOCKY_AUTH_HANDLER_GET_IFACE (handler)->initial_response_func;

  g_assert (initial_data != NULL);
  *initial_data = NULL;

  if (func == NULL)
    return TRUE;

  return func (handler, initial_data, error);
}

/**
 * wocky_auth_handler_handle_auth_data:
 * @handler: a #WockyAuthHandler object
 * @data: the challenge string
 * @response: (out) (transfer full): a location to fill with a challenge
 *  response in a #GString
 * @error: an optional location for a #GError to fill, or %NULL
 *
 * Asks @handler to respond to a <code>&lt;challenge/&gt;</code> stanza or a
 * <code>&lt;success/&gt;</code> with data. On success, @handler will put
 * response data into @response, Base64-encoding it if appropriate.
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean
wocky_auth_handler_handle_auth_data (
    WockyAuthHandler *handler,
    const GString *data,
    GString **response,
    GError **error)
{
  WockyAuthAuthDataFunc func =
    WOCKY_AUTH_HANDLER_GET_IFACE (handler)->auth_data_func;

  g_assert (response != NULL);
  *response = NULL;

  if (func == NULL)
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server send a challenge, but the mechanism didn't expect any");
      return FALSE;
    }

  return func (handler, data, response, error);
}

/**
 * wocky_auth_handler_handle_success:
 * @handler: a #WockyAuthHandler object
 * @error: an optional location for a #GError to fill, or %NULL
 *
 * Called when a <code>&lt;success/&gt;</code> stanza is received
 * during authentication. If no error is returned, then authentication
 * is considered finished. (Typically, an error is only raised if the
 * <code>&lt;success/&gt;</code> stanza was received earlier than
 * expected)
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean
wocky_auth_handler_handle_success (
    WockyAuthHandler *handler,
    GError **error)
{
  WockyAuthSuccessFunc func =
    WOCKY_AUTH_HANDLER_GET_IFACE (handler)->success_func;

  if (func == NULL)
    return TRUE;
  else
   return func (handler, error);
}
