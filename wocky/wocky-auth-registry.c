/* wocky-auth-registry.c */

#include "wocky-auth-registry.h"
#include "wocky-auth-handler.h"
#include "wocky-sasl-digest-md5.h"
#include "wocky-sasl-plain.h"
#include "wocky-jabber-auth-password.h"
#include "wocky-jabber-auth-digest.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_AUTH
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyAuthRegistry, wocky_auth_registry, G_TYPE_OBJECT)

/* private structure */
struct _WockyAuthRegistryPrivate
{
  gboolean dispose_has_run;

  WockyAuthHandler *handler;
  GSList *handlers;
};

static void wocky_auth_registry_start_auth_func (WockyAuthRegistry *self,
    const GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GSimpleAsyncResult *result);

static void wocky_auth_registry_challenge_func (WockyAuthRegistry *self,
    const GString *challenge_data,
    GSimpleAsyncResult *result);

static void wocky_auth_registry_success_func (WockyAuthRegistry *self,
    GSimpleAsyncResult *result);

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
  switch (property_id)
    {
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
  switch (property_id)
    {
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

  g_type_class_add_private (klass, sizeof (WockyAuthRegistryPrivate));

  object_class->constructed = wocky_auth_registry_constructed;
  object_class->get_property = wocky_auth_registry_get_property;
  object_class->set_property = wocky_auth_registry_set_property;
  object_class->dispose = wocky_auth_registry_dispose;
  object_class->finalize = wocky_auth_registry_finalize;

  klass->start_auth_func = wocky_auth_registry_start_auth_func;
  klass->challenge_func = wocky_auth_registry_challenge_func;
  klass->success_func = wocky_auth_registry_success_func;
  klass->failure_func = NULL;
}

static void
wocky_auth_registry_init (WockyAuthRegistry *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_AUTH_REGISTRY,
      WockyAuthRegistryPrivate);
}

WockyAuthRegistry *
wocky_auth_registry_new (void)
{
  return g_object_new (WOCKY_TYPE_AUTH_REGISTRY, NULL);
}

static gboolean
wocky_auth_registry_has_mechanism (const GSList *list, const gchar *mech) {
  GSList *t;

  t = g_slist_find_custom ((GSList *) list, mech, (GCompareFunc) g_strcmp0);

  return (t != NULL);
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

static WockyAuthHandler *
wocky_auth_registry_select_handler (WockyAuthRegistry *self,
    gboolean allow_plain, const GSList *mechanisms)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSList *k;

  for (k = priv->handlers; k != NULL; k = k->next)
    {
      WockyAuthHandler *handler = k->data;
      const gchar *handler_mech = wocky_auth_handler_get_mechanism (handler);

      if (wocky_auth_handler_is_plain (handler) && !allow_plain)
        continue;

      if (wocky_auth_registry_has_mechanism (mechanisms, handler_mech))
        {
          g_object_ref (handler);
          return handler;
        }
    }

  return NULL;
}

static void
wocky_auth_registry_start_auth_func (WockyAuthRegistry *self,
    const GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    const gchar *session_id,
    GSimpleAsyncResult *result)
{
  WockyAuthRegistryPrivate *priv = self->priv;

  g_assert (priv->handler == NULL);

  priv->handler = wocky_auth_registry_select_handler (self, allow_plain,
      mechanisms);

  if (priv->handler == NULL)
    {
      if (wocky_auth_registry_has_mechanism (mechanisms, MECH_SASL_DIGEST_MD5))
        {
          /* XXX: check for username and password here? */
          priv->handler = WOCKY_AUTH_HANDLER (wocky_sasl_digest_md5_new (
                  server, username, password));
        }
      else if (wocky_auth_registry_has_mechanism (mechanisms,
              MECH_JABBER_DIGEST))
        {
          priv->handler = WOCKY_AUTH_HANDLER (wocky_jabber_auth_digest_new (
                  session_id, password));
        }
      else if (allow_plain && wocky_auth_registry_has_mechanism (mechanisms,
              MECH_SASL_PLAIN))
        {
          /* XXX: check for username and password here? */
          DEBUG ("Choosing PLAIN as auth mechanism");
          priv->handler = WOCKY_AUTH_HANDLER (wocky_sasl_plain_new (
                  username, password));
        }
      else if (allow_plain && wocky_auth_registry_has_mechanism (mechanisms,
              MECH_JABBER_PASSWORD))
        {
          priv->handler = WOCKY_AUTH_HANDLER (wocky_jabber_auth_password_new (
                  password));
        }
    }

  if (priv->handler == NULL)
    {
      g_simple_async_result_set_error (result, WOCKY_AUTH_ERROR,
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
          g_simple_async_result_set_from_error (result, error);
          g_error_free (error);
        }
      else
        {
          WockyAuthRegistryStartData *start_data =
            wocky_auth_registry_start_data_new (
                wocky_auth_handler_get_mechanism (priv->handler),
                initial_data);

          g_simple_async_result_set_op_res_gpointer (result, start_data,
              (GDestroyNotify) wocky_auth_registry_start_data_free);

          wocky_g_string_free (initial_data);
        }
    }

  g_simple_async_result_complete_in_idle (result);
}

void
wocky_auth_registry_start_auth_async (WockyAuthRegistry *self,
    const GSList *mechanisms,
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
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      wocky_auth_registry_start_auth_finish);

  cls->start_auth_func (self, mechanisms, allow_plain, is_secure_channel,
      username, password, server, session_id, result);

  g_object_unref (result);
}

gboolean
wocky_auth_registry_start_auth_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    WockyAuthRegistryStartData **start_data,
    GError **error)
{
  wocky_implement_finish_copy_pointer (self,
      wocky_auth_registry_start_auth_finish,
      wocky_auth_registry_start_data_dup,
      start_data);
}

static void
wocky_auth_registry_challenge_func (WockyAuthRegistry *self,
    const GString *challenge_data,
    GSimpleAsyncResult *result)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GString *response = NULL;
  GError *error = NULL;

  g_assert (priv->handler != NULL);

  if (!wocky_auth_handler_handle_auth_data (priv->handler, challenge_data,
          &response, &error))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (result, response,
          (GDestroyNotify) wocky_g_string_free);
    }

  g_simple_async_result_complete_in_idle (result);
}

void
wocky_auth_registry_challenge_async (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_auth_registry_challenge_finish);

  cls->challenge_func (self, challenge_data, result);

  g_object_unref (result);
}

gboolean
wocky_auth_registry_challenge_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    GString **response,
    GError **error)
{
  wocky_implement_finish_copy_pointer (self,
      wocky_auth_registry_challenge_finish,
      wocky_g_string_dup,
      response);
}

static void
wocky_auth_registry_success_func (WockyAuthRegistry *self,
    GSimpleAsyncResult *result)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GError *error = NULL;

  g_assert (priv->handler != NULL);

  if (!wocky_auth_handler_handle_success (priv->handler, &error))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }

  g_simple_async_result_complete_in_idle (result);
}


void
wocky_auth_registry_success_async (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryClass *cls = WOCKY_AUTH_REGISTRY_GET_CLASS (self);
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_auth_registry_success_finish);

  cls->success_func (self, result);

  g_object_unref (result);
}

gboolean
wocky_auth_registry_success_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self, wocky_auth_registry_success_finish);
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
