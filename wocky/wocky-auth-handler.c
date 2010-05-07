
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

const gchar *
wocky_auth_handler_get_mechanism (WockyAuthHandler *handler)
{
  return WOCKY_AUTH_HANDLER_GET_IFACE (handler)->mechanism;
}

gboolean
wocky_auth_handler_is_plain (WockyAuthHandler *handler)
{
  return WOCKY_AUTH_HANDLER_GET_IFACE (handler)->plain;
}

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
