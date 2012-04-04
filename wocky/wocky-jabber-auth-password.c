#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-jabber-auth-password.h"

#include "wocky-auth-registry.h"
#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"

static void
auth_handler_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (WockyJabberAuthPassword, wocky_jabber_auth_password, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_AUTH_HANDLER, auth_handler_iface_init))

enum
{
  PROP_PASSWORD = 1
};

struct _WockyJabberAuthPasswordPrivate
{
  gchar *password;
};

static void
wocky_jabber_auth_password_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  WockyJabberAuthPassword *self = WOCKY_JABBER_AUTH_PASSWORD (object);
  WockyJabberAuthPasswordPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_PASSWORD:
        g_value_set_string (value, priv->password);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_jabber_auth_password_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  WockyJabberAuthPassword *self = WOCKY_JABBER_AUTH_PASSWORD (object);
  WockyJabberAuthPasswordPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_PASSWORD:
        g_free (priv->password);
        priv->password = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_jabber_auth_password_dispose (GObject *object)
{
  WockyJabberAuthPassword *self = WOCKY_JABBER_AUTH_PASSWORD (object);
  WockyJabberAuthPasswordPrivate *priv = self->priv;

  g_free (priv->password);
  G_OBJECT_CLASS (wocky_jabber_auth_password_parent_class)->dispose (object);
}

static void
wocky_jabber_auth_password_class_init (WockyJabberAuthPasswordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WockyJabberAuthPasswordPrivate));

  object_class->get_property = wocky_jabber_auth_password_get_property;
  object_class->set_property = wocky_jabber_auth_password_set_property;
  object_class->dispose = wocky_jabber_auth_password_dispose;

  g_object_class_install_property (object_class, PROP_PASSWORD,
      g_param_spec_string ("password", "password",
          "The password to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static gboolean
password_initial_response (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error);

static void
auth_handler_iface_init (gpointer g_iface)
{
  WockyAuthHandlerIface *iface = g_iface;

  iface->mechanism = WOCKY_AUTH_MECH_JABBER_PASSWORD;
  iface->plain = TRUE;
  iface->initial_response_func = password_initial_response;
}

static void
wocky_jabber_auth_password_init (WockyJabberAuthPassword *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (
      self, WOCKY_TYPE_JABBER_AUTH_PASSWORD, WockyJabberAuthPasswordPrivate);
}

WockyJabberAuthPassword *
wocky_jabber_auth_password_new (const gchar *password)
{
  return g_object_new (WOCKY_TYPE_JABBER_AUTH_PASSWORD,
      "password", password,
      NULL);
}

static gboolean
password_initial_response (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error)
{
  WockyJabberAuthPassword *self = WOCKY_JABBER_AUTH_PASSWORD (handler);
  WockyJabberAuthPasswordPrivate *priv = self->priv;

  if (priv->password == NULL)
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_NO_CREDENTIALS,
          "No password provided");
      return FALSE;
    }

  DEBUG ("Got password");

  *initial_data = g_string_new (priv->password);

  return TRUE;
}
