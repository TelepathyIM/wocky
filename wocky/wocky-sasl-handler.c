
#include "wocky-sasl-handler.h"

WockySaslHandler *
wocky_sasl_handler_new (const gchar *mechanism,
    WockySaslChallengeFunc challenge_func, WockySaslSuccessFunc success_func,
    WockySaslFailureFunc failure_func, gpointer context)
{
  WockySaslHandler *handler;

  handler = g_slice_new0 (WockySaslHandler);
  handler->mechanism = g_strdup (mechanism);
  handler->challenge_func = challenge_func;
  handler->success_func = success_func;
  handler->failure_func = failure_func;
  handler->context = context;
  return handler;
}

void
wocky_sasl_handler_free (WockySaslHandler *handler)
{
  g_free (handler->mechanism);
  g_slice_free (WockySaslHandler, handler);
}

const gchar *
wocky_sasl_handler_get_mechanism (WockySaslHandler *handler)
{
  return handler->mechanism;
}

gchar *
wocky_sasl_handler_handle_challenge (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error)
{
  return handler->challenge_func (handler, stanza, error);
}

void
wocky_sasl_handler_handle_success (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error)
{
  handler->success_func (handler, stanza, error);
}

void
wocky_sasl_handler_handle_failure (
    WockySaslHandler *handler,
    WockyXmppStanza *stanza,
    GError **error)
{
  handler->failure_func (handler, stanza, error);
}
