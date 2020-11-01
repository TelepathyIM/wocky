/* wocky-auth-registry.c */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-auth-registry.h"
#include "wocky-auth-handler.h"
#include "wocky-sasl-scram.h"
#include "wocky-sasl-digest-md5.h"
#include "wocky-sasl-plain.h"
#include "wocky-jabber-auth-password.h"
#include "wocky-jabber-auth-digest.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"


enum
{
  PROP_CB_TYPE = 1,
  PROP_CB_DATA,
};

/* private structure */
struct _WockyAuthRegistryPrivate
{
  gboolean dispose_has_run;
  WockyTLSBindingType cb_type;
  gchar *cb_data;

  WockyAuthHandler *handler;
  GSList *handlers;
};

G_DEFINE_TYPE_WITH_CODE (WockyAuthRegistry, wocky_auth_registry, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (WockyAuthRegistry))

static void wocky_auth_registry_start_auth_async_func (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GAsyncReadyCallback callback,
    gpointer user_data);

static gboolean wocky_auth_registry_start_auth_finish_func (
    WockyAuthRegistry *self,
    GAsyncResult *result,
    WockyAuthRegistryStartData **start_data,
    GError **error);

static void wocky_auth_registry_challenge_async_func (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data);

static gboolean wocky_auth_registry_challenge_finish_func (
    WockyAuthRegistry *self,
    GAsyncResult *result,
    GString **response,
    GError **error);

static void wocky_auth_registry_success_async_func (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

static gboolean wocky_auth_registry_success_finish_func (
    WockyAuthRegistry *self,
    GAsyncResult *result,
    GError **error);

GQuark
wocky_auth_error_quark (void) {
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string ("wocky_auth_error");

  return quark;
}

static void
wocky_auth_registry_constructed (GObject *object)
{
}

static void
wocky_auth_registry_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  WockyAuthRegistry *self = WOCKY_AUTH_REGISTRY (object);
  WockyAuthRegistryPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_CB_TYPE:
      g_value_set_enum (value, priv->cb_type);
      break;

    case PROP_CB_DATA:
      g_value_set_string (value, priv->cb_data);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_auth_registry_set_property (GObject      *object,
    guint         property_id,
    const GValue *value,
    GParamSpec   *pspec)
{
  WockyAuthRegistry *self = WOCKY_AUTH_REGISTRY (object);
  WockyAuthRegistryPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_CB_TYPE:
      priv->cb_type = g_value_get_enum (value);
      break;

    case PROP_CB_DATA:
      g_free (priv->cb_data);
      priv->cb_data = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_auth_registry_dispose (GObject *object)
{
  WockyAuthRegistry *self = WOCKY_AUTH_REGISTRY (object);
  WockyAuthRegistryPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_free (priv->cb_data);
  /* release any references held by the object here */
  if (priv->handler != NULL)
    {
      g_object_unref (priv->handler);
    }

  if (priv->handlers != NULL)
    {
      g_slist_foreach (priv->handlers, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->handlers);
    }

  G_OBJECT_CLASS (wocky_auth_registry_parent_class)->dispose (object);
}

static void
wocky_auth_registry_finalize (GObject *object)
{
  G_OBJECT_CLASS (wocky_auth_registry_parent_class)->finalize (object);
}

static void
wocky_auth_registry_class_init (WockyAuthRegistryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = wocky_auth_registry_constructed;
  object_class->get_property = wocky_auth_registry_get_property;
  object_class->set_property = wocky_auth_registry_set_property;
  g_object_class_install_property (object_class, PROP_CB_TYPE,
      g_param_spec_enum ("tls-binding-type", "tls channel binding type",
          "The type of the TLS Channel Binding to use in SASL negotiation",
          WOCKY_TYPE_TLS_BINDING_TYPE, WOCKY_TLS_BINDING_DISABLED,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_CB_DATA,
      g_param_spec_string ("tls-binding-data", "tls channel binding data",
          "Base64 encoded TLS Channel binding data for the set type", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  object_class->dispose = wocky_auth_registry_dispose;
  object_class->finalize = wocky_auth_registry_finalize;

  klass->start_auth_async_func = wocky_auth_registry_start_auth_async_func;
  klass->start_auth_finish_func = wocky_auth_registry_start_auth_finish_func;

  klass->challenge_async_func = wocky_auth_registry_challenge_async_func;
  klass->challenge_finish_func = wocky_auth_registry_challenge_finish_func;

  klass->success_async_func = wocky_auth_registry_success_async_func;
  klass->success_finish_func = wocky_auth_registry_success_finish_func;

  klass->failure_func = NULL;
}

static void
wocky_auth_registry_init (WockyAuthRegistry *self)
{
  self->priv = wocky_auth_registry_get_instance_private (self);
}

WockyAuthRegistry *
wocky_auth_registry_new (void)
{
  return g_object_new (WOCKY_TYPE_AUTH_REGISTRY, NULL);
}

static gboolean
wocky_auth_registry_has_mechanism (
    GSList *list,
    const gchar *mech)
{
  return (g_slist_find_custom (list, mech, (GCompareFunc) g_strcmp0) != NULL);
}

WockyAuthRegistryStartData *
wocky_auth_registry_start_data_new (const gchar *mechanism,
    const GString *initial_response)
{
  WockyAuthRegistryStartData *start_data = g_slice_new0 (
      WockyAuthRegistryStartData);

  start_data->mechanism = g_strdup (mechanism);
  start_data->initial_response = wocky_g_string_dup (initial_response);

  return start_data;
}

WockyAuthRegistryStartData *
wocky_auth_registry_start_data_dup (WockyAuthRegistryStartData *start_data)
{
  return wocky_auth_registry_start_data_new (
      start_data->mechanism,
      start_data->initial_response);
}

void
wocky_auth_registry_start_data_free (WockyAuthRegistryStartData *start_data)
{
  g_free (start_data->mechanism);

  if (start_data->initial_response != NULL)
    g_string_free (start_data->initial_response, TRUE);

  g_slice_free (WockyAuthRegistryStartData, start_data);
}

static gboolean
wocky_auth_registry_select_handler (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    WockyAuthHandler **out_handler)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSList *k;
  /* Define order of SCRAM hashing algorithm preferences according to ...   *
   * ... various recommendations                                            */
  struct {
    gchar *mech;
    gboolean is_plus;
    GChecksumType algo;
  } scram_handlers[] = {
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_512_PLUS, TRUE, G_CHECKSUM_SHA512 },
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_512, FALSE, G_CHECKSUM_SHA512 },
#ifdef WOCKY_AUTH_MECH_SASL_SCRAM_SHA_384
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_384_PLUS, TRUE, G_CHECKSUM_SHA384 },
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_384, FALSE, G_CHECKSUM_SHA384 },
#endif
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_256_PLUS, TRUE, G_CHECKSUM_SHA256 },
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_256, FALSE, G_CHECKSUM_SHA256 },
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_1_PLUS, TRUE, G_CHECKSUM_SHA1 },
    { WOCKY_AUTH_MECH_SASL_SCRAM_SHA_1, FALSE, G_CHECKSUM_SHA1 },
    { NULL, FALSE, G_CHECKSUM_SHA1 }
  };

  for (k = priv->handlers; k != NULL; k = k->next)
    {
      WockyAuthHandler *handler = k->data;
      const gchar *handler_mech = wocky_auth_handler_get_mechanism (handler);

      if (wocky_auth_handler_is_plain (handler) && !allow_plain)
        continue;

      if (wocky_auth_registry_has_mechanism (mechanisms, handler_mech))
        {
          if (out_handler != NULL)
            *out_handler = g_object_ref (handler);

          return TRUE;
        }
    }

  /* All the below mechanisms require password so if we have none
   * let's just stop here */
  g_return_val_if_fail (out_handler == NULL || password != NULL, FALSE);

  for (int i = 0; scram_handlers[i].mech != NULL ; i++)
    {
      if (wocky_auth_registry_has_mechanism (mechanisms,
                                       scram_handlers[i].mech))
        {
          if (out_handler != NULL && username != NULL)
            {
              /* For PLUS it's whatever we found/support, otherwise NONE or  *
               * DISABLED. NONE is when we support some but server doesn't.  */
              WockyTLSBindingType cb_type = (scram_handlers[i].is_plus ?
                               priv->cb_type
                             : MIN (priv->cb_type, WOCKY_TLS_BINDING_NONE));
              DEBUG ("Choosing %s as auth mechanism", scram_handlers[i].mech);
              *out_handler = WOCKY_AUTH_HANDLER (wocky_sasl_scram_new (
                  server, username, password));
              WOCKY_AUTH_HANDLER_GET_IFACE (*out_handler)->mechanism =
                                                       scram_handlers[i].mech;
              g_object_set (G_OBJECT (*out_handler),
                  "hash-algo", scram_handlers[i].algo,
                  "cb-type", cb_type,
                  "cb-data", priv->cb_data,
                  NULL);
            }
          return TRUE;
        }
    }

  if (wocky_auth_registry_has_mechanism (mechanisms,
          WOCKY_AUTH_MECH_SASL_DIGEST_MD5))
    {
      if (out_handler != NULL)
        {
          /* XXX: check for username and password here? */
          *out_handler = WOCKY_AUTH_HANDLER (wocky_sasl_digest_md5_new (
                  server, username, password));
        }
      return TRUE;
    }

  if (wocky_auth_registry_has_mechanism (mechanisms,
          WOCKY_AUTH_MECH_JABBER_DIGEST))
    {
      if (out_handler != NULL)
        {
          *out_handler = WOCKY_AUTH_HANDLER (wocky_jabber_auth_digest_new (
                  session_id, password));
        }
      return TRUE;
    }

  if (allow_plain && wocky_auth_registry_has_mechanism (mechanisms,
          WOCKY_AUTH_MECH_SASL_PLAIN))
    {
      if (out_handler != NULL)
        {
          /* XXX: check for username and password here? */
          DEBUG ("Choosing PLAIN as auth mechanism");
          *out_handler = WOCKY_AUTH_HANDLER (wocky_sasl_plain_new (
                  username, password));
        }
      return TRUE;
    }

  if (allow_plain && wocky_auth_registry_has_mechanism (mechanisms,
          WOCKY_AUTH_MECH_JABBER_PASSWORD))
    {
      if (out_handler != NULL)
        {
          *out_handler = WOCKY_AUTH_HANDLER (wocky_jabber_auth_password_new (
                  password));
        }
      return TRUE;
    }

  if (out_handler)
    *out_handler = NULL;

  return FALSE;
}


static void
wocky_auth_registry_start_auth_async_func (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GTask *task;

  task = g_task_new (G_OBJECT (self), NULL, callback, user_data);

  g_assert (priv->handler == NULL);

  if (!wocky_auth_registry_select_handler (self, mechanisms,
          allow_plain, username, password, server, session_id,
          &priv->handler))
    {
      g_task_return_new_error (task, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_NO_SUPPORTED_MECHANISMS,
          "No supported mechanisms found");
    }
  else
    {
      GString *initial_data;
      GError *error = NULL;

      if (!wocky_auth_handler_get_initial_response (priv->handler,
             &initial_data, &error))
        {
          g_task_return_error (task, error);
        }
      else
        {
          WockyAuthRegistryStartData *start_data =
            wocky_auth_registry_start_data_new (
                wocky_auth_handler_get_mechanism (priv->handler),
                initial_data);

          g_task_return_pointer (task, start_data,
              (GDestroyNotify) wocky_auth_registry_start_data_free);

          wocky_g_string_free (initial_data);
        }
    }

  g_object_unref (task);
}

void
wocky_auth_registry_start_auth_async (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  cls->start_auth_async_func (self, mechanisms, allow_plain, is_secure_channel,
      username, password, server, session_id, callback, user_data);
}

gboolean
wocky_auth_registry_start_auth_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    WockyAuthRegistryStartData **start_data,
    GError **error)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  return cls->start_auth_finish_func (self, result, start_data, error);
}

static gboolean
wocky_auth_registry_start_auth_finish_func (WockyAuthRegistry *self,
    GAsyncResult *result,
    WockyAuthRegistryStartData **start_data,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  *start_data = g_task_propagate_pointer (G_TASK (result), error);

  return !g_task_had_error (G_TASK (result));
}

static void
wocky_auth_registry_challenge_async_func (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GString *response = NULL;
  GError *error = NULL;
  GTask *task = g_task_new (G_OBJECT (self), NULL, callback, user_data);

  g_assert (priv->handler != NULL);

  if (!wocky_auth_handler_handle_auth_data (priv->handler, challenge_data,
          &response, &error))
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_pointer (task, response,
          (GDestroyNotify) wocky_g_string_free);
    }

  g_object_unref (task);
}

void
wocky_auth_registry_challenge_async (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  cls->challenge_async_func (self, challenge_data, callback, user_data);
}

gboolean
wocky_auth_registry_challenge_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    GString **response,
    GError **error)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  return cls->challenge_finish_func (self, result, response, error);
}

static gboolean
wocky_auth_registry_challenge_finish_func (WockyAuthRegistry *self,
    GAsyncResult *result,
    GString **response,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  *response = g_task_propagate_pointer (G_TASK (result), error);

  return !g_task_had_error (G_TASK (result));
}

static void
wocky_auth_registry_success_async_func (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GError *error = NULL;
  GTask *task = g_task_new (G_OBJECT (self), NULL, callback, user_data);

  g_assert (priv->handler != NULL);

  if (!wocky_auth_handler_handle_success (priv->handler, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
  g_clear_object (&priv->handler);
}


void
wocky_auth_registry_success_async (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  cls->success_async_func (self, callback, user_data);
}

gboolean
wocky_auth_registry_success_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    GError **error)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  return cls->success_finish_func (self, result, error);
}

static gboolean
wocky_auth_registry_success_finish_func (WockyAuthRegistry *self,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
wocky_auth_registry_failure (WockyAuthRegistry *self,
    GError *error)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);

  if (cls->failure_func != NULL)
    cls->failure_func (self, error);
}

void
wocky_auth_registry_add_handler (WockyAuthRegistry *self,
    WockyAuthHandler *handler)
{
  WockyAuthRegistryPrivate *priv = self->priv;

  g_object_ref (handler);
  priv->handlers = g_slist_append (priv->handlers, handler);
}

/**
 * wocky_auth_registry_supports_one_of:
 * @self: a #WockyAuthRegistry
 * @allow_plain: Whether auth in plain text is allowed
 * @mechanisms: a #GSList of gchar* of auth mechanisms
 *
 * Checks whether at least one of @mechanisms is supported by Wocky. At present,
 * Wocky itself only implements password-based authentication mechanisms.
 *
 * Returns: %TRUE if one of the @mechanisms is supported by wocky,
 *  %FALSE otherwise.
 */
gboolean
wocky_auth_registry_supports_one_of (WockyAuthRegistry *self,
    GSList *mechanisms,
    gboolean allow_plain)
{
  return wocky_auth_registry_select_handler (self, mechanisms,
          allow_plain, NULL, NULL, NULL, NULL, NULL);
}
