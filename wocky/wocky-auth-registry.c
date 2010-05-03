/* wocky-auth-registry.c */

#include "wocky-auth-registry.h"
#include "wocky-sasl-digest-md5.h"
#include "wocky-sasl-handler.h"
#include "wocky-sasl-plain.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_SASL
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyAuthRegistry, wocky_auth_registry, G_TYPE_OBJECT)

/* private structure */
struct _WockyAuthRegistryPrivate
{
  gboolean dispose_has_run;
  WockySaslHandler *handler;
  GSList *handlers;
};

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

static void
wocky_auth_registry_free_response (GString *response)
{
  if (response != NULL)
    g_string_free (response, TRUE);
}

static GString *
wocky_auth_registry_copy_response (GString *response)
{
  if (response == NULL)
    return NULL;

  return g_string_new_len (response->str, response->len);
}

static WockySaslHandler *
wocky_auth_registry_select_handler (WockyAuthRegistry *self,
    gboolean allow_plain, const GSList *mechanisms)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSList *k;

  for (k = priv->handlers; k != NULL; k = k->next)
    {
      WockySaslHandler *handler = k->data;
      const gchar *handler_mech = wocky_sasl_handler_get_mechanism (handler);

      if (wocky_sasl_handler_is_plain (handler) && !allow_plain)
        continue;

      if (wocky_auth_registry_has_mechanism (mechanisms, handler_mech))
        {
          g_object_ref (handler);
          return handler;
        }
    }

  return NULL;
}

void
wocky_auth_registry_start_auth_async (WockyAuthRegistry *self,
    const GSList *mechanisms,
    gboolean allow_plain,
    gboolean is_secure_channel,
    const gchar *username,
    const gchar *password,
    const gchar *server,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSimpleAsyncResult *result;

  g_assert (priv->handler == NULL);

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      wocky_auth_registry_start_auth_finish);

  priv->handler = wocky_auth_registry_select_handler (self, allow_plain,
      mechanisms);

  if (priv->handler == NULL)
    {
      if (wocky_auth_registry_has_mechanism (mechanisms, "DIGEST-MD5"))
        {
          /* XXX: check for username and password here? */
          DEBUG ("Choosing DIGEST-MD5 as auth mechanism");
          priv->handler = WOCKY_SASL_HANDLER (wocky_sasl_digest_md5_new (
                  server, username, password));
        }
      else if (allow_plain &&
          wocky_auth_registry_has_mechanism (mechanisms, "PLAIN"))
        {
          /* XXX: check for username and password here? */
          DEBUG ("Choosing PLAIN as auth mechanism");
          priv->handler = WOCKY_SASL_HANDLER (wocky_sasl_plain_new (
                  username, password));
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

      if (!wocky_sasl_handler_get_initial_response (priv->handler,
             &initial_data, &error))
        {
          g_simple_async_result_set_from_error (result, error);
          g_error_free (error);
        }
      else
        {
          g_simple_async_result_set_op_res_gpointer (result, initial_data,
              (GDestroyNotify) wocky_auth_registry_free_response);
        }
    }

  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);
}

gboolean
wocky_auth_registry_start_auth_finish (WockyAuthRegistry *self,
    GAsyncResult *res,
    gchar **mechanism,
    GString **initial_data,
    GError **error)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);

  g_assert (priv->handler != NULL);

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (self),
          wocky_auth_registry_start_auth_finish), FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  if (mechanism != NULL)
    *mechanism = g_strdup (wocky_sasl_handler_get_mechanism (priv->handler));

  if (initial_data != NULL)
    *initial_data = wocky_auth_registry_copy_response (
        g_simple_async_result_get_op_res_gpointer (result));

  return TRUE;
}

void
wocky_auth_registry_challenge_async (WockyAuthRegistry *self,
    const GString *challenge_data,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_auth_registry_challenge_finish);
  GString *response = NULL;
  GError *error = NULL;

  g_assert (priv->handler != NULL);

  if (!wocky_sasl_handler_handle_auth_data (priv->handler, challenge_data,
          &response, &error))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (result, response,
          (GDestroyNotify) wocky_auth_registry_free_response);
    }

  g_simple_async_result_complete_in_idle (result);
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
      wocky_auth_registry_copy_response,
      response);
}

void
wocky_auth_registry_success_async (WockyAuthRegistry *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyAuthRegistryPrivate *priv = self->priv;
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_auth_registry_success_finish);
  GError *error = NULL;

  g_assert (priv->handler != NULL);

  if (!wocky_sasl_handler_handle_success (priv->handler, &error))
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }

  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);
}

gboolean
wocky_auth_registry_success_finish (WockyAuthRegistry *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self, wocky_auth_registry_success_finish);
}

void wocky_auth_registry_failure_notify (WockyAuthRegistry *self,
    GError *error)
{
}

void
wocky_auth_registry_add_handler (WockyAuthRegistry *self,
    WockySaslHandler *handler)
{
  WockyAuthRegistryPrivate *priv = self->priv;

  g_object_ref (handler);
  priv->handlers = g_slist_append (priv->handlers, handler);
}
