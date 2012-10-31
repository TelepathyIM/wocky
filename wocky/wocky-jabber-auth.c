/*
 * wocky-jabber-auth.c - Source for WockyJabberAuth
 * Copyright (C) 2009-2010 Collabora Ltd.
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

#include "wocky-jabber-auth.h"
#include "wocky-signals-marshal.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"

G_DEFINE_TYPE(WockyJabberAuth, wocky_jabber_auth, G_TYPE_OBJECT)

enum
{
    PROP_SESSION_ID = 1,
    PROP_USERNAME,
    PROP_RESOURCE,
    PROP_PASSWORD,
    PROP_CONNECTION,
    PROP_AUTH_REGISTRY,
};

/* private structure */
struct _WockyJabberAuthPrivate
{
  gboolean dispose_has_run;
  WockyXmppConnection *connection;
  gchar *username;
  gchar *resource;
  gchar *password;
  gchar *session_id;
  GCancellable *cancel;
  GSimpleAsyncResult *result;
  WockyAuthRegistry *auth_registry;
  gboolean allow_plain;
  gboolean is_secure;
};

static void
wocky_jabber_auth_init (WockyJabberAuth *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_JABBER_AUTH,
      WockyJabberAuthPrivate);
}

static void wocky_jabber_auth_dispose (GObject *object);
static void wocky_jabber_auth_finalize (GObject *object);

static void
wocky_jabber_auth_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyJabberAuth *self = WOCKY_JABBER_AUTH (object);
  WockyJabberAuthPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_SESSION_ID:
        g_free (priv->session_id);
        priv->session_id = g_value_dup_string (value);
        break;
      case PROP_USERNAME:
        g_free (priv->username);
        priv->username = g_value_dup_string (value);
        break;
      case PROP_RESOURCE:
        g_free (priv->resource);
        priv->resource = g_value_dup_string (value);
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
wocky_jabber_auth_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyJabberAuth *self = WOCKY_JABBER_AUTH (object);
  WockyJabberAuthPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_SESSION_ID:
        g_value_set_string (value, priv->session_id);
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
wocky_jabber_auth_class_init (WockyJabberAuthClass *wocky_jabber_auth_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_jabber_auth_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_jabber_auth_class,
      sizeof (WockyJabberAuthPrivate));

  object_class->set_property = wocky_jabber_auth_set_property;
  object_class->get_property = wocky_jabber_auth_get_property;

  spec = g_param_spec_string ("session-id",
    "session-id",
    "The XMPP session ID",
    NULL,
    G_PARAM_READWRITE|G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_SESSION_ID, spec);

  spec = g_param_spec_string ("username",
    "username",
    "The username to authenticate with",
    NULL,
    G_PARAM_WRITABLE|G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_USERNAME, spec);

  spec = g_param_spec_string ("resource",
    "resource",
    "The XMPP resource to bind to",
    NULL,
    G_PARAM_WRITABLE|G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_RESOURCE, spec);

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

  object_class->dispose = wocky_jabber_auth_dispose;
  object_class->finalize = wocky_jabber_auth_finalize;

}

void
wocky_jabber_auth_dispose (GObject *object)
{
  WockyJabberAuth *self = WOCKY_JABBER_AUTH (object);
  WockyJabberAuthPrivate *priv = self->priv;

  if (priv->connection != NULL)
    g_object_unref (priv->connection);

  if (priv->auth_registry != NULL)
    g_object_unref (priv->auth_registry);

  if (G_OBJECT_CLASS (wocky_jabber_auth_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_jabber_auth_parent_class)->dispose (object);
}

void
wocky_jabber_auth_finalize (GObject *object)
{
  WockyJabberAuth *self = WOCKY_JABBER_AUTH (object);
  WockyJabberAuthPrivate *priv = self->priv;

  /* free any data held directly by the object here */
  g_free (priv->session_id);
  g_free (priv->username);
  g_free (priv->resource);
  g_free (priv->password);

  G_OBJECT_CLASS (wocky_jabber_auth_parent_class)->finalize (object);
}

static void
auth_reset (WockyJabberAuth *self)
{
  WockyJabberAuthPrivate *priv = self->priv;

  g_free (priv->session_id);
  priv->session_id = NULL;

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
auth_succeeded (WockyJabberAuth *self)
{
  WockyJabberAuthPrivate *priv = self->priv;
  GSimpleAsyncResult *r;

  DEBUG ("Authentication succeeded");
  auth_reset (self);

  r = priv->result;
  priv->result = NULL;

  g_simple_async_result_complete (r);
  g_object_unref (r);
}

static void
auth_failed (WockyJabberAuth *self, gint code, const gchar *format, ...)
{
  gchar *message;
  va_list args;
  GSimpleAsyncResult *r;
  GError *error = NULL;
  WockyJabberAuthPrivate *priv = self->priv;

  auth_reset (self);

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
stream_error (WockyJabberAuth *self, WockyStanza *stanza)
{
  GError *error = NULL;

  if (stanza == NULL)
    {
      auth_failed (self, WOCKY_AUTH_ERROR_CONNRESET, "Disconnected");
      return TRUE;
    }

  if (wocky_stanza_extract_stream_error (stanza, &error))
    {
      auth_failed (self, WOCKY_AUTH_ERROR_STREAM, "%s: %s",
          wocky_enum_to_nick (WOCKY_TYPE_XMPP_STREAM_ERROR, error->code),
          error->message);

      g_error_free (error);

      return TRUE;
    }

  return FALSE;
}

WockyJabberAuth *
wocky_jabber_auth_new (const gchar *session_id,
    const gchar *username,
    const gchar *resource,
    const gchar *password,
    WockyXmppConnection *connection,
    WockyAuthRegistry *auth_registry)
{
  return g_object_new (WOCKY_TYPE_JABBER_AUTH,
      "session-id", session_id,
      "username", username,
      "resource", resource,
      "password", password,
      "connection", connection,
      "auth-registry", auth_registry,
      NULL);
}

gboolean
wocky_jabber_auth_authenticate_finish (WockyJabberAuth *self,
  GAsyncResult *result,
  GError **error)
{
  wocky_implement_finish_void (self, wocky_jabber_auth_authenticate_async);
}

static void
wocky_jabber_auth_success_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyJabberAuth *self = (WockyJabberAuth *) user_data;
  WockyJabberAuthPrivate *priv = self->priv;
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
jabber_auth_reply (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyJabberAuth *self = (WockyJabberAuth *) user_data;
  WockyJabberAuthPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->connection;
  GError *error = NULL;
  WockyStanza *reply = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType sub = WOCKY_STANZA_SUB_TYPE_NONE;

  DEBUG ("");
  reply = wocky_xmpp_connection_recv_stanza_finish (conn, res, &error);

  if (stream_error (self, reply))
    return;

  wocky_stanza_get_type_info (reply, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      auth_failed (self, WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Jabber Auth Reply: Response Invalid");
      goto out;
    }

  switch (sub)
    {
      WockyAuthError code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (reply, NULL, &error, NULL, NULL);

        switch (error->code)
          {
          case WOCKY_XMPP_ERROR_NOT_AUTHORIZED:
            code = WOCKY_AUTH_ERROR_NOT_AUTHORIZED;
            break;
          case WOCKY_XMPP_ERROR_CONFLICT:
            code = WOCKY_AUTH_ERROR_RESOURCE_CONFLICT;
            break;
          case WOCKY_XMPP_ERROR_NOT_ACCEPTABLE:
            code = WOCKY_AUTH_ERROR_NO_CREDENTIALS;
            break;
          default:
            code = WOCKY_AUTH_ERROR_FAILURE;
          }

        auth_failed (self, code, "Authentication failed: %s", error->message);
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        wocky_auth_registry_success_async (priv->auth_registry,
            wocky_jabber_auth_success_cb, self);
        break;

      default:
        auth_failed (self,  WOCKY_AUTH_ERROR_INVALID_REPLY,
            "Bizarre response to Jabber Auth request");
        break;
    }

  out:
    g_object_unref (reply);

}

static void
jabber_auth_query (GObject *source, GAsyncResult *res, gpointer user_data)
{
  WockyJabberAuth *self = (WockyJabberAuth *) user_data;
  WockyJabberAuthPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->connection;
  GError *error = NULL;

  DEBUG ("");
  if (!wocky_xmpp_connection_send_stanza_finish (conn, res, &error))
    {
      auth_failed (self, error->code, "Jabber Auth IQ Set: %s",
          error->message);
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (conn, priv->cancel,
      jabber_auth_reply, user_data);
}

static void
wocky_jabber_auth_start_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyJabberAuth *self = (WockyJabberAuth *) user_data;
  WockyJabberAuthPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->connection;
  gchar *iqid;
  WockyStanza *iq;
  const gchar *auth_field;
  GError *error = NULL;
  WockyAuthRegistryStartData *start_data = NULL;

  if (!wocky_auth_registry_start_auth_finish (priv->auth_registry, res,
          &start_data, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
      return;
    }

  g_assert (start_data->mechanism != NULL);
  g_assert (start_data->initial_response != NULL);

  if (g_strcmp0 (start_data->mechanism, "X-WOCKY-JABBER-PASSWORD") == 0)
      auth_field = "password";
  else
      auth_field = "digest";

  iqid = wocky_xmpp_connection_new_id (conn);
  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_SET, NULL, NULL,
      '@', "id", iqid,
      '(', "query", ':', WOCKY_JABBER_NS_AUTH,
      '(', "username", '$', priv->username, ')',
      '(', auth_field, '$', start_data->initial_response->str, ')',
      '(', "resource", '$', priv->resource, ')',
      ')',
      NULL);

  wocky_xmpp_connection_send_stanza_async (conn, iq, priv->cancel,
      jabber_auth_query, self);

  g_free (iqid);
  g_object_unref (iq);
  wocky_auth_registry_start_data_free (start_data);
}

static void
jabber_auth_fields (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyJabberAuth *self = (WockyJabberAuth *) user_data;
  WockyJabberAuthPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->connection;
  GError *error = NULL;
  WockyStanza *fields = NULL;
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyStanzaSubType sub = WOCKY_STANZA_SUB_TYPE_NONE;

  fields = wocky_xmpp_connection_recv_stanza_finish (conn, res, &error);

  if (stream_error (self, fields))
    return;

  wocky_stanza_get_type_info (fields, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      auth_failed (self, WOCKY_AUTH_ERROR_FAILURE,
          "Jabber Auth Init: Response Invalid");
      goto out;
    }

  switch (sub)
    {
      WockyNode *node = NULL;
      WockyAuthError code;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (fields, NULL, &error, NULL, NULL);

        if (error->code == WOCKY_XMPP_ERROR_SERVICE_UNAVAILABLE)
          code = WOCKY_AUTH_ERROR_NOT_SUPPORTED;
        else
          code = WOCKY_AUTH_ERROR_FAILURE;

        auth_failed (self, code, "Jabber Auth: %s %s",
            wocky_xmpp_error_string (error->code),
            error->message);
        g_clear_error (&error);
        break;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        node = wocky_stanza_get_top_node (fields);
        node = wocky_node_get_child_ns (node, "query",
            WOCKY_JABBER_NS_AUTH);
        if ((node != NULL) &&
            (wocky_node_get_child (node, "resource") != NULL) &&
            (wocky_node_get_child (node, "username") != NULL))
          {
            GSList *mechanisms = NULL;

            if (wocky_node_get_child (node, "password") != NULL)
              mechanisms = g_slist_append (mechanisms, WOCKY_AUTH_MECH_JABBER_PASSWORD);

            if (wocky_node_get_child (node, "digest") != NULL)
              mechanisms = g_slist_append (mechanisms, WOCKY_AUTH_MECH_JABBER_DIGEST);

            wocky_auth_registry_start_auth_async (priv->auth_registry,
                mechanisms, priv->allow_plain, priv->is_secure,
                priv->username, priv->password, NULL, priv->session_id,
                wocky_jabber_auth_start_cb, self);

            g_slist_free (mechanisms);
          }
        break;

      default:
        auth_failed (self, WOCKY_AUTH_ERROR_FAILURE,
            "Bizarre response to Jabber Auth request");
        break;
    }

 out:
  g_object_unref (fields);
}

static void
jabber_auth_init_sent (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyJabberAuth *self = (WockyJabberAuth *) user_data;
  WockyJabberAuthPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->connection;
  GError *error = NULL;

  DEBUG ("");
  if (!wocky_xmpp_connection_send_stanza_finish (conn, res, &error))
    {
      auth_failed (self, error->code, error->message);
      g_error_free (error);
      return;
    }

  wocky_xmpp_connection_recv_stanza_async (conn, priv->cancel,
      jabber_auth_fields, user_data);
}

/* Initiate jabber auth. features should contain the stream features stanza as
 * receiver from the session_id */
void
wocky_jabber_auth_authenticate_async (WockyJabberAuth *self,
    gboolean allow_plain,
    gboolean is_secure,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyJabberAuthPrivate *priv = self->priv;
  WockyXmppConnection *conn = priv->connection;
  gchar *id = wocky_xmpp_connection_new_id (conn);
  WockyStanza *iq = NULL;

  DEBUG ("");

  priv->allow_plain = allow_plain;
  priv->is_secure = is_secure;

  priv->result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_jabber_auth_authenticate_async);

  if (cancellable != NULL)
    priv->cancel = g_object_ref (cancellable);

  iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_GET,
      NULL, NULL,
      '@', "id", id,
      '(', "query", ':', WOCKY_JABBER_NS_AUTH,
      /* This is a workaround for
       * <https://bugs.freedesktop.org/show_bug.cgi?id=24013>: while
       * <http://xmpp.org/extensions/xep-0078.html#usecases> doesn't require
       * us to include a username, it seems to be required by jabberd 1.4.
       */
      '(', "username",
      '$', priv->username,
      ')',
      ')',
      NULL);

  wocky_xmpp_connection_send_stanza_async (conn, iq, priv->cancel,
      jabber_auth_init_sent, self);

  g_free (id);
  g_object_unref (iq);
}
