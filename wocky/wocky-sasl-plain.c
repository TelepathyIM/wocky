
#include "wocky-sasl-plain.h"

#include "wocky-sasl-auth.h"

#define DEBUG_FLAG DEBUG_SASL
#include "wocky-debug.h"

static void
sasl_handler_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (WockySaslPlain, wocky_sasl_plain, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_SASL_HANDLER, sasl_handler_iface_init))

enum
{
  PROP_USERNAME = 1,
  PROP_PASSWORD
};

struct _WockySaslPlainPrivate
{
  gchar *username;
  gchar *password;
};

static void
wocky_sasl_plain_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  WockySaslPlain *self = WOCKY_SASL_PLAIN (object);
  WockySaslPlainPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_USERNAME:
        g_value_set_string (value, priv->username);
        break;

      case PROP_PASSWORD:
        g_value_set_string (value, priv->password);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_sasl_plain_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  WockySaslPlain *self = WOCKY_SASL_PLAIN (object);
  WockySaslPlainPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_USERNAME:
        g_free (priv->username);
        priv->username = g_value_dup_string (value);
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
wocky_sasl_plain_dispose (GObject *object)
{
  WockySaslPlain *self = WOCKY_SASL_PLAIN (object);
  WockySaslPlainPrivate *priv = self->priv;

  g_free (priv->username);
  g_free (priv->password);
  G_OBJECT_CLASS (wocky_sasl_plain_parent_class)->dispose (object);
}

static void
wocky_sasl_plain_class_init (WockySaslPlainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WockySaslPlainPrivate));

  object_class->get_property = wocky_sasl_plain_get_property;
  object_class->set_property = wocky_sasl_plain_set_property;
  object_class->dispose = wocky_sasl_plain_dispose;

  g_object_class_install_property (object_class, PROP_USERNAME,
      g_param_spec_string ("username", "username",
          "The username to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PASSWORD,
      g_param_spec_string ("password", "password",
          "The password to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static gchar *
plain_handle_challenge (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error);

static void
plain_handle_success (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error);

static void
plain_handle_failure (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error);

static void
sasl_handler_iface_init (gpointer g_iface)
{
  WockySaslHandlerIface *iface = g_iface;

  iface->mechanism = "PLAIN";
  iface->plain = TRUE;
  iface->challenge_func = plain_handle_challenge;
  iface->success_func = plain_handle_success;
  iface->failure_func = plain_handle_failure;
}

static void
wocky_sasl_plain_init (WockySaslPlain *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (
      self, WOCKY_TYPE_SASL_PLAIN, WockySaslPlainPrivate);
}

WockySaslPlain*
wocky_sasl_plain_new (const gchar *username, const gchar *password)
{
  return g_object_new (WOCKY_TYPE_SASL_PLAIN,
      "username", username,
      "password", password,
      NULL);
}

static gchar *
plain_generate_initial_response (const gchar *username, const gchar *password)
{
  GString *str = g_string_new ("");
  gchar *cstr;

  g_string_append_c (str, '\0');
  g_string_append (str, username);
  g_string_append_c (str, '\0');
  g_string_append (str, password);
  cstr = g_base64_encode ((guchar *) str->str, str->len);
  g_string_free (str, TRUE);
  return cstr;
}

static gchar *
plain_handle_challenge (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error)
{
  WockySaslPlain *self = WOCKY_SASL_PLAIN (handler);
  WockySaslPlainPrivate *priv = self->priv;

  if (stanza == NULL)
    {
      if (priv->username == NULL || priv->password == NULL)
        {
          g_set_error (error, WOCKY_SASL_AUTH_ERROR,
              WOCKY_SASL_AUTH_ERROR_NO_CREDENTIALS,
              "No username or password provided");
          return NULL;
        }

      DEBUG ("Got username and password");
      return plain_generate_initial_response (priv->username, priv->password);
    }

  g_set_error (error, WOCKY_SASL_AUTH_ERROR,
      WOCKY_SASL_AUTH_ERROR_INVALID_REPLY,
      "Server sent an unexpected challenge");
  return NULL;
}

static void
plain_handle_success (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error)
{
}

static void
plain_handle_failure (WockySaslHandler *handler, WockyXmppStanza *stanza,
    GError **error)
{
  WockyXmppNode *reason = NULL;

  if (stanza->node->children != NULL)
    {
      /* TODO add a wocky xmpp node utility to either get the first child or
       * iterate the children list */
      reason = (WockyXmppNode *) stanza->node->children->data;
    }
    /* TODO Handle the different error cases in a different way. i.e.
     * make it clear for the user if it's credentials were wrong, if the server
     * just has a temporary error or if the authentication procedure itself was
     * at fault (too weak, invalid mech etc) */

  g_set_error (error, WOCKY_SASL_AUTH_ERROR, WOCKY_SASL_AUTH_ERROR_FAILURE,
      "Authentication failed: %s",
      reason == NULL ? "Unknown reason" : reason->name);
}

