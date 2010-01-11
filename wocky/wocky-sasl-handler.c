
#include "wocky-sasl-handler.h"
#include "wocky-sasl-auth.h"

GType
wocky_sasl_handler_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      const GTypeInfo info =
      {
        .class_size = sizeof (WockySaslHandlerIface),
        .base_init = NULL,
        .base_finalize = NULL,
        .class_init = NULL,
        .class_finalize = NULL,
        .class_data = NULL,
        .instance_size = 0,
        .n_preallocs = 0,
        .instance_init = NULL
      };
      GType g_define_type_id = g_type_register_static (
          G_TYPE_INTERFACE, "WockySaslHandler", &info, 0);

      g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_OBJECT);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

const gchar *
wocky_sasl_handler_get_mechanism (WockySaslHandler *handler)
{
  return WOCKY_SASL_HANDLER_GET_IFACE (handler)->mechanism;
}

gboolean
wocky_sasl_handler_is_plain (WockySaslHandler *handler)
{
  return WOCKY_SASL_HANDLER_GET_IFACE (handler)->plain;
}

gboolean
wocky_sasl_handler_get_initial_response (WockySaslHandler *handler,
    gchar **initial_data,
    GError **error)
{
  WockySaslInitialResponseFunc func =
    WOCKY_SASL_HANDLER_GET_IFACE (handler)->initial_response_func;

  g_assert (initial_data != NULL);
  *initial_data = NULL;

  if (func == NULL)
    return TRUE;

  return func (handler, initial_data, error);
}

gchar *
wocky_sasl_handler_handle_challenge (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error)
{
  WockySaslChallengeFunc func =
    WOCKY_SASL_HANDLER_GET_IFACE (handler)->challenge_func;

  if (func == NULL)
    {
      g_set_error (error, WOCKY_SASL_AUTH_ERROR,
          WOCKY_SASL_AUTH_ERROR_INVALID_REPLY,
          "Server send a challenge, but the mechanism didn't expect any");
      return NULL;
    }

  return func (handler, stanza, error);
}

gboolean
wocky_sasl_handler_handle_success (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error)
{
  return WOCKY_SASL_HANDLER_GET_IFACE (handler)->success_func (
      handler, stanza, error);
}

void
wocky_sasl_handler_handle_failure (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error)
{
  WOCKY_SASL_HANDLER_GET_IFACE (handler)->failure_func (
      handler, stanza, error);
}
