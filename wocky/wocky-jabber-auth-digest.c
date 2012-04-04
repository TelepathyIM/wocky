#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-jabber-auth-digest.h"

#include "wocky-auth-registry.h"
#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"

static void
auth_handler_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (WockyJabberAuthDigest, wocky_jabber_auth_digest, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_AUTH_HANDLER, auth_handler_iface_init))

enum
{
  PROP_SESSION_ID = 1,
  PROP_PASSWORD,
};

struct _WockyJabberAuthDigestPrivate
{
  gchar *session_id;
  gchar *password;
};

static void
wocky_jabber_auth_digest_get_property (GObject *object, guint property_id,
    GValue *value, GParamSpec *pspec)
{
  WockyJabberAuthDigest *self = WOCKY_JABBER_AUTH_DIGEST (object);
  WockyJabberAuthDigestPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_SESSION_ID:
        g_value_set_string (value, priv->session_id);
        break;

      case PROP_PASSWORD:
        g_value_set_string (value, priv->password);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_jabber_auth_digest_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
  WockyJabberAuthDigest *self = WOCKY_JABBER_AUTH_DIGEST (object);
  WockyJabberAuthDigestPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_SESSION_ID:
        g_free (priv->session_id);
        priv->session_id = g_value_dup_string (value);
        break;

      case PROP_PASSWORD:
        g_free (priv->password);
        priv->password = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_jabber_auth_digest_dispose (GObject *object)
{
  WockyJabberAuthDigest *self = WOCKY_JABBER_AUTH_DIGEST (object);
  WockyJabberAuthDigestPrivate *priv = self->priv;

  g_free (priv->session_id);
  g_free (priv->password);
  G_OBJECT_CLASS (wocky_jabber_auth_digest_parent_class)->dispose (object);
}

static void
wocky_jabber_auth_digest_class_init (WockyJabberAuthDigestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WockyJabberAuthDigestPrivate));

  object_class->get_property = wocky_jabber_auth_digest_get_property;
  object_class->set_property = wocky_jabber_auth_digest_set_property;
  object_class->dispose = wocky_jabber_auth_digest_dispose;

  g_object_class_install_property (object_class, PROP_SESSION_ID,
      g_param_spec_string ("session-id", "session-id",
          "The session_id to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PASSWORD,
      g_param_spec_string ("password", "password",
          "The password to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static gboolean
digest_initial_response (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error);

static void
auth_handler_iface_init (gpointer g_iface)
{
  WockyAuthHandlerIface *iface = g_iface;

  iface->mechanism = WOCKY_AUTH_MECH_JABBER_DIGEST;
  iface->plain = FALSE;
  iface->initial_response_func = digest_initial_response;
}

static void
wocky_jabber_auth_digest_init (WockyJabberAuthDigest *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (
      self, WOCKY_TYPE_JABBER_AUTH_DIGEST, WockyJabberAuthDigestPrivate);
}

WockyJabberAuthDigest *
wocky_jabber_auth_digest_new (const gchar *session_id, const gchar *password)
{
  return g_object_new (WOCKY_TYPE_JABBER_AUTH_DIGEST,
      "session-id", session_id,
      "password", password,
      NULL);
}

static GString *
digest_generate_initial_response (const gchar *session_id, const gchar *password)
{
  gchar *hsrc = g_strconcat (session_id, password, NULL);
  gchar *sha1 = g_compute_checksum_for_string (G_CHECKSUM_SHA1, hsrc, -1);
  GString *response = g_string_new (sha1);

  g_free (hsrc);
  g_free (sha1);

  return response;
}

static gboolean
digest_initial_response (WockyAuthHandler *handler,
    GString **initial_data,
    GError **error)
{
  WockyJabberAuthDigest *self = WOCKY_JABBER_AUTH_DIGEST (handler);
  WockyJabberAuthDigestPrivate *priv = self->priv;

  if (priv->password == NULL || priv->session_id == NULL)
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_NO_CREDENTIALS,
          "No session-id or password provided");
      return FALSE;
    }

  DEBUG ("Got session-id and password");

  *initial_data = digest_generate_initial_response (priv->session_id,
      priv->password);

  return TRUE;
}
