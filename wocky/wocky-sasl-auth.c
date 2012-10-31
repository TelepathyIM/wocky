/*
 * wocky-sasl-auth.c - Source for WockySaslAuth
 * Copyright (C) 2006-2009 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "wocky-sasl-auth.h"
#include "wocky-signals-marshal.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"

G_DEFINE_TYPE(WockySaslAuth, wocky_sasl_auth, G_TYPE_OBJECT)

enum
{
    PROP_SERVER = 1,
    PROP_USERNAME,
    PROP_PASSWORD,
    PROP_CONNECTION,
    PROP_AUTH_REGISTRY,
};

/* private structure */
struct _WockySaslAuthPrivate
{
  gboolean dispose_has_run;
  WockyXmppConnection *connection;
  gchar *username;
  gchar *password;
  gchar *server;
  GCancellable *cancel;
  GSimpleAsyncResult *result;
  WockyAuthRegistry *auth_registry;
};

static void sasl_auth_stanza_received (GObject *source, GAsyncResult *res,
    gpointer user_data);

static void
wocky_sasl_auth_init (WockySaslAuth *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_SASL_AUTH,
      WockySaslAuthPrivate);
}

static void wocky_sasl_auth_dispose (GObject *object);
static void wocky_sasl_auth_finalize (GObject *object);

static void
wocky_sasl_auth_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (object);
  WockySaslAuthPrivate *priv = sasl->priv;

  switch (property_id)
    {
      case PROP_SERVER:
        g_free (priv->server);
        priv->server = g_value_dup_string (value);
        break;
      case PROP_USERNAME:
        g_free (priv->username);
        priv->username = g_value_dup_string (value);
        break;
      case PROP_PASSWORD:
        g_free (priv->password);
        priv->password = g_value_dup_string (value);
        break;
      case PROP_CONNECTION:
        priv->connection = g_value_dup_object (value);
        break;
      case PROP_AUTH_REGISTRY:
        if (g_value_get_object (value) == NULL)
          priv->auth_registry = wocky_auth_registry_new ();
        else
          priv->auth_registry = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_sasl_auth_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (object);
  WockySaslAuthPrivate *priv = sasl->priv;

  switch (property_id)
    {
      case PROP_SERVER:
        g_value_set_string (value, priv->server);
        break;
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;
      case PROP_AUTH_REGISTRY:
        g_value_set_object (value, priv->auth_registry);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_sasl_auth_class_init (WockySaslAuthClass *wocky_sasl_auth_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_sasl_auth_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_sasl_auth_class,
      sizeof (WockySaslAuthPrivate));

  object_class->set_property = wocky_sasl_auth_set_property;
  object_class->get_property = wocky_sasl_auth_get_property;

  spec = g_param_spec_string ("server",
    "server",
    "The name of the server",
    NULL,
    G_PARAM_READWRITE|G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_SERVER, spec);

  spec = g_param_spec_string ("username",
    "username",
    "The username to authenticate with",
    NULL,
    G_PARAM_WRITABLE|G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_USERNAME, spec);

  spec = g_param_spec_string ("password",
    "password",
    "The password to authenticate with",
    NULL,
    G_PARAM_WRITABLE|G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_PASSWORD, spec);

  spec = g_param_spec_object ("connection",
    "connection",
    "The Xmpp connection to user",
    WOCKY_TYPE_XMPP_CONNECTION,
    G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_CONNECTION, spec);

  spec = g_param_spec_object ("auth-registry",
    "Authentication Registry",
    "Authentication Registry",
    WOCKY_TYPE_AUTH_REGISTRY,
    G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_AUTH_REGISTRY, spec);

  object_class->dispose = wocky_sasl_auth_dispose;
  object_class->finalize = wocky_sasl_auth_finalize;

}

void
wocky_sasl_auth_dispose (GObject *object)
{
  WockySaslAuth *self = WOCKY_SASL_AUTH (object);
  WockySaslAuthPrivate *priv = self->priv;

  if (priv->connection != NULL)
    g_object_unref (priv->connection);

  if (priv->auth_registry != NULL)
    g_object_unref (priv->auth_registry);

  if (G_OBJECT_CLASS (wocky_sasl_auth_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_sasl_auth_parent_class)->dispose (object);
}

void
wocky_sasl_auth_finalize (GObject *object)
{
  WockySaslAuth *self = WOCKY_SASL_AUTH (object);
  WockySaslAuthPrivate *priv = self->priv;

  /* free any data held directly by the object here */
  g_free (priv->server);
  g_free (priv->username);
  g_free (priv->password);

  G_OBJECT_CLASS (wocky_sasl_auth_parent_class)->finalize (object);
}

static void
auth_reset (WockySaslAuth *sasl)
{
  WockySaslAuthPrivate *priv = sasl->priv;

  g_free (priv->server);
  priv->server = NULL;

  if (priv->connection != NULL)
    {
      g_object_unref (priv->connection);
      priv->connection = NULL;
    }

  if (priv->cancel != NULL)
    {
      g_object_unref (priv->cancel);
      priv->cancel = NULL;
    }
}

static void
auth_succeeded (WockySaslAuth *sasl)
{
  WockySaslAuthPrivate *priv = sasl->priv;
  GSimpleAsyncResult *r;

  DEBUG ("Authentication succeeded");
  auth_reset (sasl);

  r = priv->result;
  priv->result = NULL;

  g_simple_async_result_complete (r);
  g_object_unref (r);
}

static void
auth_failed (WockySaslAuth *sasl, gint code, const gchar *format, ...)
{
  gchar *message;
  va_list args;
  GSimpleAsyncResult *r;
  GError *error = NULL;
  WockySaslAuthPrivate *priv = sasl->priv;

  auth_reset (sasl);

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  DEBUG ("Authentication failed!: %s", message);

  r = priv->result;
  priv->result = NULL;

  error = g_error_new_literal (WOCKY_AUTH_ERROR, code, message);

  g_simple_async_result_set_from_error (r, error);

  wocky_auth_registry_failure (priv->auth_registry, error);

  g_simple_async_result_complete (r);
  g_object_unref (r);

  g_error_free (error);
  g_free (message);
}

static gboolean
stream_error (WockySaslAuth *sasl, WockyStanza *stanza)
{
  WockySaslAuthPrivate *priv = sasl->priv;
  GError *error = NULL;

  if (stanza == NULL)
    {
      auth_failed (sasl, WOCKY_AUTH_ERROR_CONNRESET, "Disconnected");
      return TRUE;
    }

  if (wocky_stanza_extract_stream_error (stanza, &error))
    {
      auth_failed (sasl, WOCKY_AUTH_ERROR_STREAM, "%s: %s",
          wocky_enum_to_nick (WOCKY_TYPE_XMPP_STREAM_ERROR, error->code),
          error->message);

      g_error_free (error);

      return TRUE;
    }

  if (g_cancellable_is_cancelled (priv->cancel))
    {
      /* We got disconnected but we still had this stanza to process. Don't
       * bother with it. */
      auth_failed (sasl, WOCKY_AUTH_ERROR_CONNRESET, "Disconnected");
      return TRUE;
    }

  return FALSE;
}

WockySaslAuth *
wocky_sasl_auth_new (const gchar *server,
    const gchar *username,
    const gchar *password,
    WockyXmppConnection *connection,
    WockyAuthRegistry *auth_registry)
{
  return g_object_new (WOCKY_TYPE_SASL_AUTH,
      "server", server,
      "username", username,
      "password", password,
      "connection", connection,
      "auth-registry", auth_registry,
      NULL);
}

static GSList *
wocky_sasl_auth_mechanisms_to_list (WockyNode *mechanisms)
{
  GSList *result = NULL;
  WockyNode *mechanism;
  WockyNodeIter iter;

  if (mechanisms == NULL)
    return NULL;

  wocky_node_iter_init (&iter, mechanisms, "mechanism", NULL);
  while (wocky_node_iter_next (&iter, &mechanism))
    result = g_slist_append (result, g_strdup (mechanism->content));

  return result;
}

static void
sasl_auth_got_failure (WockySaslAuth *sasl,
  WockyStanza *stanza,
  GError **error)
{
  WockyNode *reason = NULL;

  if (wocky_stanza_get_top_node (stanza)->children != NULL)
    {
      /* TODO add a wocky xmpp node utility to either get the first child or
       * iterate the children list */
      reason = (WockyNode *)
          wocky_stanza_get_top_node (stanza)->children->data;
    }
    /* TODO Handle the different error cases in a different way. i.e.
     * make it clear for the user if it's credentials were wrong, if the server
     * just has a temporary error or if the authentication procedure itself was
     * at fault (too weak, invalid mech etc) */

  g_set_error (error, WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_FAILURE,
      "Authentication failed: %s",
      reason == NULL ? "Unknown reason" : reason->name);
}

static GString *
wocky_sasl_auth_decode_challenge (const gchar *challenge)
{
  gchar *challenge_str;
  GString *challenge_data;
  gsize len;

  if (challenge == NULL)
    return g_string_new_len ("", 0);

  challenge_str = (gchar *) g_base64_decode (challenge, &len);

  challenge_data = g_string_new_len (challenge_str, len);

  g_free (challenge_str);

  return challenge_data;
}

static gchar *
wocky_sasl_auth_encode_response (const GString *response_data)
{
  if (response_data != NULL && response_data->len > 0)
    return g_base64_encode ((guchar *) response_data->str,
        response_data->len);

  return NULL;
}

static void
wocky_sasl_auth_success_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  WockySaslAuth *self = (WockySaslAuth *) user_data;
  WockySaslAuthPrivate *priv = self->priv;
  GError *error = NULL;

  if (!wocky_auth_registry_success_finish (priv->auth_registry, res, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
    }
  else
    {
      auth_succeeded (self);
    }
}

static void
wocky_sasl_auth_response_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  WockySaslAuth *self = (WockySaslAuth *) user_data;
  WockySaslAuthPrivate *priv = self->priv;
  WockyStanza *response_stanza;
  GString *response_data = NULL;
  gchar *response;
  GError *error = NULL;

  if (!wocky_auth_registry_challenge_finish (priv->auth_registry, res,
          &response_data, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
      return;
    }

  response = wocky_sasl_auth_encode_response (response_data);

  response_stanza = wocky_stanza_new ("response", WOCKY_XMPP_NS_SASL_AUTH);
  wocky_node_set_content (wocky_stanza_get_top_node (response_stanza),
      response);

  /* FIXME handle send error */
  wocky_xmpp_connection_send_stanza_async (
      priv->connection, response_stanza, NULL, NULL, NULL);

  wocky_xmpp_connection_recv_stanza_async (priv->connection,
      NULL, sasl_auth_stanza_received, self);

  if (response_data != NULL)
    g_string_free (response_data, TRUE);
  g_free (response);
  g_object_unref (response_stanza);
}

static void
wocky_sasl_auth_success_response_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  WockySaslAuth *self = (WockySaslAuth *) user_data;
  WockySaslAuthPrivate *priv = self->priv;
  GError *error = NULL;
  GString *response_data;

  if (!wocky_auth_registry_challenge_finish (priv->auth_registry, res,
          &response_data, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
      return;
    }

  if (response_data != NULL)
    {
      auth_failed (self, WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Got success from the server while we still had more data to "
          "send");

      g_string_free (response_data, TRUE);
      return;
    }

  wocky_auth_registry_success_async (priv->auth_registry,
      wocky_sasl_auth_success_cb, self);
}

static void
sasl_auth_stanza_received (GObject *source,
  GAsyncResult *res,
  gpointer user_data)
{
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (user_data);
  WockySaslAuthPrivate *priv = sasl->priv;
  WockyStanza *stanza;
  GError *error = NULL;

  stanza = wocky_xmpp_connection_recv_stanza_finish (
    WOCKY_XMPP_CONNECTION (priv->connection), res, NULL);

  if (stream_error (sasl, stanza))
    return;

  if (wocky_strdiff (
      wocky_node_get_ns (wocky_stanza_get_top_node (stanza)),
          WOCKY_XMPP_NS_SASL_AUTH))
    {
      auth_failed (sasl, WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server sent a reply not in the %s namespace",
          WOCKY_XMPP_NS_SASL_AUTH);
      return;
    }

  /* If the SASL async result is _complete()d in the handler, the SASL object *
   * will be unref'd, which means the ref count could fall to zero while we   *
   * are still using it. grab  aref to it and drop it after we are sure that  *
   * we don't need it anymore:
   */
  g_object_ref (sasl);

  if (!wocky_strdiff (wocky_stanza_get_top_node (stanza)->name, "challenge"))
    {
      GString *challenge;

      challenge = wocky_sasl_auth_decode_challenge (
          wocky_stanza_get_top_node (stanza)->content);

      wocky_auth_registry_challenge_async (priv->auth_registry, challenge,
          wocky_sasl_auth_response_cb, sasl);

      g_string_free (challenge, TRUE);
    }
  else if (!wocky_strdiff (wocky_stanza_get_top_node (stanza)->name, "success"))
    {
      if (wocky_stanza_get_top_node (stanza)->content != NULL)
        {
          GString *challenge;

          challenge = wocky_sasl_auth_decode_challenge (
              wocky_stanza_get_top_node (stanza)->content);

          wocky_auth_registry_challenge_async (priv->auth_registry, challenge,
              wocky_sasl_auth_success_response_cb, sasl);

          g_string_free (challenge, TRUE);
        }
      else
        {
          wocky_auth_registry_success_async (priv->auth_registry,
              wocky_sasl_auth_success_cb, sasl);
        }
    }
  else if (!wocky_strdiff (wocky_stanza_get_top_node (stanza)->name, "failure"))
    {
      sasl_auth_got_failure (sasl, stanza, &error);
      g_assert (error != NULL);
      auth_failed (sasl, error->code, error->message);
      g_error_free (error);
    }
  else
    {
      auth_failed (sasl, WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server sent an invalid reply (%s)",
          wocky_stanza_get_top_node (stanza)->name);
    }

  g_object_unref (sasl);
  g_object_unref (stanza);
  return;
}

static void
sasl_auth_stanza_sent (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (source);
  WockySaslAuth *self = user_data;
  WockySaslAuthPrivate *priv = self->priv;

  if (!wocky_xmpp_connection_send_stanza_finish (connection, res, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (priv->connection,
    priv->cancel, sasl_auth_stanza_received, self);
}

gboolean
wocky_sasl_auth_authenticate_finish (WockySaslAuth *sasl,
  GAsyncResult *result,
  GError **error)
{
  wocky_implement_finish_void (sasl, wocky_sasl_auth_authenticate_async);
}

static void
wocky_sasl_auth_start_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  WockySaslAuth *self = (WockySaslAuth *) user_data;
  WockySaslAuthPrivate *priv = self->priv;
  WockyStanza *stanza;
  GError *error = NULL;
  WockyAuthRegistryStartData *start_data = NULL;

  if (!wocky_auth_registry_start_auth_finish (priv->auth_registry, res,
          &start_data, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
      return;
    }

  stanza = wocky_stanza_new ("auth", WOCKY_XMPP_NS_SASL_AUTH);

  /* google JID domain discovery - client sets a namespaced attribute */
  wocky_node_set_attribute_ns (wocky_stanza_get_top_node (stanza),
      "client-uses-full-bind-result", "true", WOCKY_GOOGLE_NS_AUTH);

  if (start_data->initial_response != NULL)
    {
      gchar *initial_response_str = wocky_sasl_auth_encode_response (
          start_data->initial_response);

      wocky_node_set_content (
        wocky_stanza_get_top_node (stanza),
        initial_response_str);

      g_free (initial_response_str);
    }

  wocky_node_set_attribute (wocky_stanza_get_top_node (stanza),
    "mechanism", start_data->mechanism);
  wocky_xmpp_connection_send_stanza_async (priv->connection, stanza,
    priv->cancel, sasl_auth_stanza_sent, self);

  wocky_auth_registry_start_data_free (start_data);
  g_object_unref (stanza);
}

/* Initiate sasl auth. features should contain the stream features stanza as
 * receiver from the server */
void
wocky_sasl_auth_authenticate_async (WockySaslAuth *sasl,
    WockyStanza *features,
    gboolean allow_plain,
    gboolean is_secure,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockySaslAuthPrivate *priv = sasl->priv;
  WockyNode *mech_node;
  GSList *mechanisms, *t;

  g_assert (sasl != NULL);
  g_assert (features != NULL);

  mech_node = wocky_node_get_child_ns (
    wocky_stanza_get_top_node (features),
    "mechanisms", WOCKY_XMPP_NS_SASL_AUTH);

  mechanisms = wocky_sasl_auth_mechanisms_to_list (mech_node);

  if (G_UNLIKELY (mechanisms == NULL))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (sasl),
          callback, user_data,
          WOCKY_AUTH_ERROR, WOCKY_AUTH_ERROR_NOT_SUPPORTED,
          "Server doesn't have any sasl mechanisms");
      goto out;
    }

  priv->result = g_simple_async_result_new (G_OBJECT (sasl),
    callback, user_data, wocky_sasl_auth_authenticate_async);

  if (cancellable != NULL)
    priv->cancel = g_object_ref (cancellable);

  wocky_auth_registry_start_auth_async (priv->auth_registry, mechanisms,
      allow_plain, is_secure, priv->username, priv->password, priv->server,
      NULL, wocky_sasl_auth_start_cb, sasl);

out:
  for (t = mechanisms ; t != NULL; t = g_slist_next (t))
    {
      g_free (t->data);
    }

  g_slist_free (mechanisms);
}
