
#include "wocky-test-sasl-handler.h"

#include <wocky/wocky-sasl-handler.h>

static void
sasl_handler_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (WockyTestSaslHandler, wocky_test_sasl_handler,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_SASL_HANDLER, sasl_handler_iface_init))

static void
wocky_test_sasl_handler_class_init (WockyTestSaslHandlerClass *klass)
{
}

static void
wocky_test_sasl_handler_init (WockyTestSaslHandler *self)
{
}

static gchar *
test_handle_challenge (WockySaslHandler *handler,
    WockyXmppStanza *stanza, GError **error);

static void
test_handle_success (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error);

static void
test_handle_failure (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error);

static void
sasl_handler_iface_init (gpointer g_iface)
{
  WockySaslHandlerIface *iface = g_iface;

  iface->mechanism = "X-TEST";
  iface->plain = FALSE;
  iface->challenge_func = test_handle_challenge;
  iface->success_func = test_handle_success;
  iface->failure_func = test_handle_failure;
}

WockyTestSaslHandler *
wocky_test_sasl_handler_new (void)
{
  return g_object_new (WOCKY_TYPE_TEST_SASL_HANDLER, NULL);
}

static gchar *
test_handle_challenge (WockySaslHandler *handler,
    WockyXmppStanza *stanza, GError **error)
{
  return g_base64_encode ((guchar *) "open sesame", 11);
}

static void
test_handle_success (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error)
{
}

static void
test_handle_failure (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error)
{
}

