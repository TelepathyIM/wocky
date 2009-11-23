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


#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "wocky-sasl-auth.h"
#include "wocky-sasl-digest-md5.h"
#include "wocky-sasl-handler.h"
#include "wocky-sasl-plain.h"
#include "wocky-signals-marshal.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_SASL
#include "wocky-debug.h"

G_DEFINE_TYPE(WockySaslAuth, wocky_sasl_auth, G_TYPE_OBJECT)

enum
{
    PROP_SERVER = 1,
    PROP_USERNAME,
    PROP_PASSWORD,
    PROP_CONNECTION,
};

/* private structure */
typedef struct _WockySaslAuthPrivate WockySaslAuthPrivate;

struct _WockySaslAuthPrivate
{
  gboolean dispose_has_run;
  WockyXmppConnection *connection;
  gchar *username;
  gchar *password;
  gchar *server;
  WockySaslHandler *handler;
  GCancellable *cancel;
  GSimpleAsyncResult *result;
  GSList *handlers;
};

#define WOCKY_SASL_AUTH_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_SASL_AUTH, WockySaslAuthPrivate))

GQuark
wocky_sasl_auth_error_quark (void) {
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string ("wocky_sasl_auth_error");

  return quark;
}

static void
wocky_sasl_auth_init (WockySaslAuth *obj)
{
  //WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
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
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);

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
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);

  switch (property_id)
    {
      case PROP_SERVER:
        g_value_set_string (value, priv->server);
        break;
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
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

  object_class->dispose = wocky_sasl_auth_dispose;
  object_class->finalize = wocky_sasl_auth_finalize;

}

void
wocky_sasl_auth_dispose (GObject *object)
{
  WockySaslAuth *self = WOCKY_SASL_AUTH (object);
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (self);

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

  if (priv->connection != NULL)
    {
      g_object_unref (priv->connection);
    }

  if (G_OBJECT_CLASS (wocky_sasl_auth_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_sasl_auth_parent_class)->dispose (object);
}

void
wocky_sasl_auth_finalize (GObject *object)
{
  WockySaslAuth *self = WOCKY_SASL_AUTH (object);
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (self);

  /* free any data held directly by the object here */
  g_free (priv->server);
  g_free (priv->username);
  g_free (priv->password);

  G_OBJECT_CLASS (wocky_sasl_auth_parent_class)->finalize (object);
}

static void
auth_reset (WockySaslAuth *sasl)
{
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);

  g_free (priv->server);
  priv->server = NULL;

  if (priv->handler != NULL)
    {
      g_object_unref (priv->handler);
      priv->handler = NULL;
    }

  if (priv->connection != NULL)
    {
      g_object_unref (priv->connection);
      priv->connection = NULL;
    }
}

static void
auth_succeeded (WockySaslAuth *sasl)
{
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);
  GSimpleAsyncResult *r;

  DEBUG ("Authentication succeeded");
  auth_reset (sasl);

  r = priv->result;
  priv->result = NULL;

  g_simple_async_result_complete (r);
  g_object_unref (r);
}

static void
auth_failed (WockySaslAuth *sasl, gint error, const gchar *format, ...)
{
  gchar *message;
  va_list args;
  GSimpleAsyncResult *r;
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);

  auth_reset (sasl);

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  DEBUG ("Authentication failed!: %s", message);

  r = priv->result;
  priv->result = NULL;

  g_simple_async_result_set_error (r,
    WOCKY_SASL_AUTH_ERROR, error, "%s", message);

  g_simple_async_result_complete (r);
  g_object_unref (r);

  g_free (message);
}

static gboolean
stream_error (WockySaslAuth *sasl, WockyXmppStanza *stanza)
{
  WockyStanzaType type = WOCKY_STANZA_TYPE_NONE;
  WockyXmppNode *xmpp = NULL;
  GSList *item = NULL;
  const gchar *msg = NULL;
  const gchar *err = NULL;
  WockyXmppNode *cond = NULL;
  WockyXmppNode *text = NULL;

  if (stanza == NULL)
    {
      auth_failed (sasl, WOCKY_SASL_AUTH_ERROR_CONNRESET, "Disconnected");
      return TRUE;
    }

  wocky_xmpp_stanza_get_type_info (stanza, &type, NULL);

  if (type == WOCKY_STANZA_TYPE_STREAM_ERROR)
    {
      xmpp = stanza->node;
      for (item = xmpp->children; item != NULL; item = g_slist_next (item))
        {
          WockyXmppNode *child = item->data;
          const gchar *cns = wocky_xmpp_node_get_ns (child);

          if (wocky_strdiff (cns, WOCKY_XMPP_NS_STREAMS))
            continue;

          if (!wocky_strdiff (child->name, "text"))
            text = child;
          else
            cond = child;
        }

      if (text != NULL)
        msg = text->content;
      else if (cond != NULL)
        msg = cond->name;
      else
        msg = "-";

      err = (cond != NULL) ? cond->name : "unknown-error";

      auth_failed (sasl, WOCKY_SASL_AUTH_ERROR_STREAM, "%s: %s", err, msg);
      return TRUE;
    }

  return FALSE;
}

WockySaslAuth *
wocky_sasl_auth_new (const gchar *server,
    const gchar *username,
    const gchar *password,
    WockyXmppConnection *connection)
{
  return g_object_new (WOCKY_TYPE_SASL_AUTH,
      "server", server,
      "username", username,
      "password", password,
      "connection", connection,
      NULL);
}

static gboolean
each_mechanism (WockyXmppNode *node, gpointer user_data)
{
  GSList **list = (GSList **)user_data;
  if (wocky_strdiff (node->name, "mechanism"))
    {
      return TRUE;
    }
  *list = g_slist_append (*list, g_strdup (node->content));
  return TRUE;
}

static GSList *
wocky_sasl_auth_mechanisms_to_list (WockyXmppNode *mechanisms)
{
  GSList *result = NULL;

  if (mechanisms == NULL)
    return NULL;

  wocky_xmpp_node_each_child (mechanisms, each_mechanism, &result);
  return result;
}

static gboolean
wocky_sasl_auth_has_mechanism (GSList *list, const gchar *mech) {
  GSList *t;
  for (t = list ; t != NULL ; t = g_slist_next (t)) {
    if (!wocky_strdiff ((gchar *) t->data, mech)) {
      return TRUE;
    }
  }
  return FALSE;
}

static void
sasl_auth_stanza_received (GObject *source,
  GAsyncResult *res,
  gpointer user_data)
{
  WockySaslAuth *sasl = WOCKY_SASL_AUTH (user_data);
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);
  WockyXmppStanza *stanza;
  GError *error = NULL;
  gchar *response = NULL;

  stanza = wocky_xmpp_connection_recv_stanza_finish (
    WOCKY_XMPP_CONNECTION (priv->connection), res, NULL);

  if (stream_error (sasl, stanza))
    return;

  if (wocky_strdiff (
      wocky_xmpp_node_get_ns (stanza->node), WOCKY_XMPP_NS_SASL_AUTH))
    {
      auth_failed (sasl, WOCKY_SASL_AUTH_ERROR_INVALID_REPLY,
          "Server sent a reply not in the %s namespace",
          WOCKY_XMPP_NS_SASL_AUTH);
      return;
    }

  /* If the SASL async result is _complete()d in the handler, the SASL object *
   * will be unref'd, which means the ref count could fall to zero while we   *
   * are still using it. grab  aref to it and drop it after we are sure that  *
   * we don't need it anymore:                                                */
  g_object_ref (sasl);

  if (!wocky_strdiff (stanza->node->name, "challenge"))
    {
      response = wocky_sasl_handler_handle_challenge (
          priv->handler, stanza, &error);

      if (response != NULL && error != NULL)
        {
          g_warning ("SASL handler returned both a response and an error (%s)",
              error->message);
          g_free (response);
        }
      else if (response == NULL && error == NULL)
        {
          g_warning ("SASL handler returned no result and no error");
        }
    }
  else if (!wocky_strdiff (stanza->node->name, "success"))
    {
      wocky_sasl_handler_handle_success (priv->handler, stanza, &error);
    }
  else if (!wocky_strdiff (stanza->node->name, "failure"))
    {
      wocky_sasl_handler_handle_failure (priv->handler, stanza, &error);
    }
  else
    {
      auth_failed (sasl, WOCKY_SASL_AUTH_ERROR_INVALID_REPLY,
          "Server sent an invalid reply (%s)",
          stanza->node->name);
    }

  if (error != NULL)
    {
      auth_failed (sasl, error->code, error->message);
      g_error_free (error);
    }
  else if (!wocky_strdiff (stanza->node->name, "success"))
    {
      auth_succeeded (sasl);
    }
  else if (!wocky_strdiff (stanza->node->name, "failure"))
    {
      /* Handler didn't return error from failure function. Fail with a
       * general error.
       */
      auth_failed (sasl, WOCKY_SASL_AUTH_ERROR_FAILURE,
          "Authentication failed.");
    }
  else
    {
      if (response != NULL)
        {
          WockyXmppStanza *response_stanza;

          response_stanza = wocky_xmpp_stanza_new ("response");
          wocky_xmpp_node_set_ns (
              response_stanza->node, WOCKY_XMPP_NS_SASL_AUTH);
          wocky_xmpp_node_set_content (response_stanza->node, response);

          /* FIXME handle send error */
          wocky_xmpp_connection_send_stanza_async (
              priv->connection, response_stanza, NULL, NULL, NULL);
          g_object_unref (response_stanza);
          g_free (response);
        }

      wocky_xmpp_connection_recv_stanza_async (priv->connection,
          NULL, sasl_auth_stanza_received, sasl);
    }

  g_object_unref (sasl);
  g_object_unref (stanza);
  return;
}

static gboolean
wocky_sasl_auth_start_mechanism (WockySaslAuth *sasl,
    WockySaslHandler *handler)
{
  WockyXmppStanza *stanza;
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE(sasl);
  gboolean ret = TRUE;
  gchar *initial_response;
  GError *error = NULL;

  priv->handler = handler;

  stanza = wocky_xmpp_stanza_new ("auth");
  wocky_xmpp_node_set_ns (stanza->node, WOCKY_XMPP_NS_SASL_AUTH);

  /* google JID domain discovery - client sets a namespaced attribute */
  wocky_xmpp_node_set_attribute_ns (stanza->node,
      "client-uses-full-bind-result", "true", WOCKY_GOOGLE_NS_AUTH);

  initial_response =
      wocky_sasl_handler_handle_challenge ( priv->handler, NULL, &error);

  if (error != NULL)
    {
      auth_failed (sasl, error->domain, error->message);
      goto out;
    }

  if (initial_response != NULL)
    {
      wocky_xmpp_node_set_content (stanza->node, initial_response);
      g_free (initial_response);
    }

  /* FIXME handle send error */
  wocky_xmpp_node_set_attribute (stanza->node, "mechanism",
    wocky_sasl_handler_get_mechanism (priv->handler));
  wocky_xmpp_connection_send_stanza_async (priv->connection, stanza,
    NULL, NULL, NULL);
  wocky_xmpp_connection_recv_stanza_async (priv->connection,
    NULL, sasl_auth_stanza_received, sasl);

out:
  g_object_unref (stanza);

  return ret;
}

gboolean
wocky_sasl_auth_authenticate_finish (WockySaslAuth *sasl,
  GAsyncResult *result,
  GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (sasl), wocky_sasl_auth_authenticate_finish), FALSE);

  return TRUE;
}

static WockySaslHandler *
wocky_sasl_auth_select_handler (
    WockySaslAuth *sasl, gboolean allow_plain, GSList *mechanisms)
{
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);
  GSList *i, *k;

  for (k = priv->handlers; k != NULL; k = k->next)
    {
      WockySaslHandler *handler = k->data;
      const gchar *handler_mech = wocky_sasl_handler_get_mechanism (handler);

      if (wocky_sasl_handler_is_plain (handler) && !allow_plain)
        continue;

      for (i = mechanisms; i != NULL; i = i->next)
        {
          const gchar *mechanism = i->data;

          if (!wocky_strdiff (handler_mech, mechanism))
            {
              g_object_ref (handler);
              return handler;
            }
        }
    }

  if (wocky_sasl_auth_has_mechanism (mechanisms, "DIGEST-MD5"))
    {
      /* XXX: check for username and password here? */
      DEBUG ("Choosing DIGEST-MD5 as auth mechanism");
      return WOCKY_SASL_HANDLER (wocky_sasl_digest_md5_new (
          priv->server, priv->username, priv->password));
    }
  else if (allow_plain &&
      wocky_sasl_auth_has_mechanism (mechanisms, "PLAIN"))
    {
      /* XXX: check for username and password here? */
      DEBUG ("Choosing PLAIN as auth mechanism");
      return WOCKY_SASL_HANDLER (wocky_sasl_plain_new (
          priv->username, priv->password));
    }
  else
    {
      return NULL;
    }
}

/* Initiate sasl auth. features should contain the stream features stanza as
 * receiver from the server */
void
wocky_sasl_auth_authenticate_async (WockySaslAuth *sasl,
    WockyXmppStanza *features, gboolean allow_plain,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (sasl);
  WockyXmppNode *mech_node;
  GSList *mechanisms, *t;
  WockySaslHandler *handler = NULL;

  g_assert (sasl != NULL);
  g_assert (features != NULL);

  mech_node = wocky_xmpp_node_get_child_ns (features->node, "mechanisms",
      WOCKY_XMPP_NS_SASL_AUTH);

  mechanisms = wocky_sasl_auth_mechanisms_to_list (mech_node);

  if (G_UNLIKELY (mechanisms == NULL))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (sasl),
          callback, user_data,
          WOCKY_SASL_AUTH_ERROR, WOCKY_SASL_AUTH_ERROR_SASL_NOT_SUPPORTED,
          "Server doesn't have any sasl mechanisms");
      goto out;
    }

  priv->result = g_simple_async_result_new (G_OBJECT (sasl),
    callback, user_data, wocky_sasl_auth_authenticate_finish);
  handler = wocky_sasl_auth_select_handler (sasl, allow_plain, mechanisms);

  if (handler == NULL)
    {
      DEBUG ("No supported mechanisms found");

      g_object_unref (priv->result);
      priv->result = NULL;

      g_simple_async_report_error_in_idle (G_OBJECT (sasl),
          callback, user_data,
          WOCKY_SASL_AUTH_ERROR,
          WOCKY_SASL_AUTH_ERROR_NO_SUPPORTED_MECHANISMS,
          "No supported mechanisms found");

      goto out;
    }

  wocky_sasl_auth_start_mechanism (sasl, handler);

out:
  for (t = mechanisms ; t != NULL; t = g_slist_next (t))
    {
      g_free (t->data);
    }

  g_slist_free (mechanisms);
}

/**
 * wocky_sasl_auth_add_handler:
 * Provide an external SASL handler to be used during authentication. Handlers
 * (and therefore SASL mechanisms) are prioritised in the order they are
 * added (handlers added earlier take precedence over those added later).
 **/
void
wocky_sasl_auth_add_handler (WockySaslAuth *auth, WockySaslHandler *handler)
{
  WockySaslAuthPrivate *priv = WOCKY_SASL_AUTH_GET_PRIVATE (auth);

  g_object_ref (handler);
  priv->handlers = g_slist_append (priv->handlers, handler);
}
