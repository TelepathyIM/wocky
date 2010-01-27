
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

static gboolean
test_initial_response (WockySaslHandler *handler,
    gchar **initial_data,
    GError **error);

static void
sasl_handler_iface_init (gpointer g_iface)
{
  WockySaslHandlerIface *iface = g_iface;

  iface->mechanism = "X-TEST";
  iface->plain = FALSE;
  iface->initial_response_func = test_initial_response;
}

WockyTestSaslHandler *
wocky_test_sasl_handler_new (void)
{
  return g_object_new (WOCKY_TYPE_TEST_SASL_HANDLER, NULL);
}

static gboolean
test_initial_response (WockySaslHandler *handler,
    gchar **initial_data,
    GError **error)
{
  *initial_data = g_base64_encode ((guchar *) "open sesame", 11);
  return TRUE;
}
