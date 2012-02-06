/*
 * wocky-tls-handler.c - Source for WockyTLSHandler
 * Copyright (C) 2010 Collabora Ltd.
 * @author Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 * @author Vivek Dasmohapatra <vivek@collabora.co.uk>
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

#include <config.h>

#include "wocky-tls-handler.h"
#include "wocky-utils.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_TLS
#include "wocky-debug-internal.h"

static void
real_verify_async (WockyTLSHandler *self,
    WockyTLSSession *tls_session,
    const gchar *peername,
    GStrv extra_identities,
    GAsyncReadyCallback callback,
    gpointer user_data);

static gboolean
real_verify_finish (WockyTLSHandler *self,
    GAsyncResult *result,
    GError **error);

G_DEFINE_TYPE (WockyTLSHandler, wocky_tls_handler, G_TYPE_OBJECT)

enum {
  PROP_TLS_INSECURE_OK = 1,
};

struct _WockyTLSHandlerPrivate {
  gboolean ignore_ssl_errors;

  GSList *cas;
  GSList *crls;
};

static void
wocky_tls_handler_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyTLSHandler *self = WOCKY_TLS_HANDLER (object);

  switch (property_id)
    {
      case PROP_TLS_INSECURE_OK:
        g_value_set_boolean (value, self->priv->ignore_ssl_errors);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_tls_handler_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyTLSHandler *self = WOCKY_TLS_HANDLER (object);

  switch (property_id)
    {
      case PROP_TLS_INSECURE_OK:
        self->priv->ignore_ssl_errors = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_tls_handler_finalize (GObject *object)
{
  WockyTLSHandler *self = WOCKY_TLS_HANDLER (object);

  if (self->priv->cas != NULL)
    {
      g_slist_foreach (self->priv->cas, (GFunc) g_free, NULL);
      g_slist_free (self->priv->cas);
    }

  if (self->priv->crls != NULL)
    {
      g_slist_foreach (self->priv->crls, (GFunc) g_free, NULL);
      g_slist_free (self->priv->crls);
    }

  G_OBJECT_CLASS (wocky_tls_handler_parent_class)->finalize (object);
}

static void
wocky_tls_handler_class_init (WockyTLSHandlerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (WockyTLSHandlerPrivate));

  klass->verify_async_func = real_verify_async;
  klass->verify_finish_func = real_verify_finish;

  oclass->get_property = wocky_tls_handler_get_property;
  oclass->set_property = wocky_tls_handler_set_property;
  oclass->finalize = wocky_tls_handler_finalize;

  /**
   * WockyTLSHandler:ignore-ssl-errors:
   *
   * Whether to ignore recoverable SSL errors (certificate
   * insecurity/expiry etc).
   */
  pspec = g_param_spec_boolean ("ignore-ssl-errors", "ignore-ssl-errors",
      "Whether recoverable TLS errors should be ignored", TRUE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_TLS_INSECURE_OK, pspec);
}

static void
wocky_tls_handler_init (WockyTLSHandler *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_TLS_HANDLER,
      WockyTLSHandlerPrivate);
}

static void
real_verify_async (WockyTLSHandler *self,
    WockyTLSSession *tls_session,
    const gchar *peername,
    GStrv extra_identities,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  glong flags = WOCKY_TLS_VERIFY_NORMAL;
  WockyTLSCertStatus status = WOCKY_TLS_CERT_UNKNOWN_ERROR;
  const gchar *verify_peername = NULL;
  GStrv verify_extra_identities = NULL;

  result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, wocky_tls_handler_verify_async);

  /* When ignore_ssl_errors is true, don't check the peername. Otherwise:
   * - Under legacy SSL, the connect hostname is the preferred peername;
   * - Under STARTTLS, we check the domain regardless of the connect server.
   */
  if (self->priv->ignore_ssl_errors)
    {
      flags = WOCKY_TLS_VERIFY_LENIENT;
    }
  else
    {
      verify_peername = peername;
      verify_extra_identities = extra_identities;
    }

  DEBUG ("Verifying certificate (peername: %s)",
      (verify_peername == NULL) ? "-" : verify_peername);

  wocky_tls_session_verify_peer (tls_session, verify_peername,
      verify_extra_identities, flags, &status);

  if (status != WOCKY_TLS_CERT_OK)
    {
      gboolean ok_when_lenient = FALSE;
      const gchar *msg = NULL;
      switch (status)
        {
          case WOCKY_TLS_CERT_NAME_MISMATCH:
            msg = "SSL Certificate does not match name '%s'";
            break;
          case WOCKY_TLS_CERT_REVOKED:
            msg = "SSL Certificate for %s has been revoked";
            break;
          case WOCKY_TLS_CERT_SIGNER_UNKNOWN:
            ok_when_lenient = TRUE;
            msg = "SSL Certificate for %s is insecure (unknown signer)";
            break;
          case WOCKY_TLS_CERT_SIGNER_UNAUTHORISED:
            msg = "SSL Certificate for %s is insecure (unauthorised signer)";
            break;
          case WOCKY_TLS_CERT_INSECURE:
            msg = "SSL Certificate for %s is insecure (weak crypto)";
            break;
          case WOCKY_TLS_CERT_NOT_ACTIVE:
            msg = "SSL Certificate for %s not active yet";
            break;
          case WOCKY_TLS_CERT_EXPIRED:
            msg = "SSL Certificate for %s expired";
            break;
          case WOCKY_TLS_CERT_INVALID:
            msg = "SSL Certificate for %s invalid";
            ok_when_lenient = TRUE;
            break;
          /* Handle UNKNOWN_ERROR and any other unexpected values equivalently
           */
          case WOCKY_TLS_CERT_UNKNOWN_ERROR:
          default:
            msg = "SSL Certificate Verification Error for %s";
        }

      if (!(self->priv->ignore_ssl_errors && ok_when_lenient))
        {
          GError *cert_error = NULL;

          cert_error = g_error_new (WOCKY_TLS_CERT_ERROR, status, msg,
              peername);
          g_simple_async_result_set_from_error (result, cert_error);

          g_error_free (cert_error);
          g_simple_async_result_complete_in_idle (result);
          g_object_unref (result);

          return;
        }
      else
        {
          gchar *err;

          err = g_strdup_printf (msg, peername);
          DEBUG ("Cert error: '%s', but ignore-ssl-errors is set", err);
          g_free (err);
        }
    }

  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);
}

static gboolean
real_verify_finish (WockyTLSHandler *self,
    GAsyncResult *result,
    GError **error)
{
  wocky_implement_finish_void (self, wocky_tls_handler_verify_async);
}

void
wocky_tls_handler_verify_async (WockyTLSHandler *self,
    WockyTLSSession *session,
    const gchar *peername,
    GStrv extra_identities,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyTLSHandlerClass *klass = WOCKY_TLS_HANDLER_GET_CLASS (self);

  klass->verify_async_func (self, session, peername, extra_identities,
      callback, user_data);
}

gboolean
wocky_tls_handler_verify_finish (WockyTLSHandler *self,
    GAsyncResult *res,
    GError **error)
{
  WockyTLSHandlerClass *klass = WOCKY_TLS_HANDLER_GET_CLASS (self);

  return klass->verify_finish_func (self, res, error);
}

WockyTLSHandler *
wocky_tls_handler_new (gboolean ignore_ssl_errors)
{
  return g_object_new (WOCKY_TYPE_TLS_HANDLER,
      "ignore-ssl-errors", ignore_ssl_errors, NULL);
}

/**
 * wocky_tls_handler_add_ca:
 * @self: a #WockyTLSHandler instance
 * @path: a path to a directory or file containing PEM encoded CA certificates
 *
 * Sensible default paths (under Debian derived distributions) are:
 *
 * * for gnutls:  /etc/ssl/certs/ca-certificates.crt
 * * for openssl: /etc/ssl/certs
 *
 * Certificates my also be found under /usr/share/ca-certificates/...
 * if the user wishes to pick and choose which CAs to use.
 *
 * Returns: a #gboolean indicating whether the path was resolved.
 * Does not indicate that there was actually a file or directory there
 * or that any CAs were actually found. The CAs won't actually be loaded
 * until just before the TLS session setup is attempted.
 */
gboolean
wocky_tls_handler_add_ca (WockyTLSHandler *self,
    const gchar *path)
{
  gchar *abspath = wocky_absolutize_path (path);

  if (abspath != NULL)
    self->priv->cas = g_slist_prepend (self->priv->cas, abspath);

  return abspath != NULL;
}

/**
 * wocky_tls_handler_add_crl:
 * @self: a #WockyTLSHandler instance
 * @path: a path to a directory or file containing PEM encoded CRLs
 *
 * This function does not descend subdirectories automatically.
 *
 * Returns: a #gboolean indicating whether the path was resolved.
 * Does not indicate that there was actually a file or directory there
 * or that any CRLs were actually found. The CRLs won't actually be loaded
 * until just before the TLS session setup is attempted.
 */
gboolean
wocky_tls_handler_add_crl (WockyTLSHandler *self,
    const gchar *path)
{
  gchar *abspath = wocky_absolutize_path (path);

  if (abspath != NULL)
    self->priv->crls = g_slist_prepend (self->priv->crls, abspath);

  return abspath != NULL;
}

GSList *
wocky_tls_handler_get_cas (WockyTLSHandler *self)
{
  g_assert (WOCKY_IS_TLS_HANDLER (self));

  return self->priv->cas;
}

GSList *
wocky_tls_handler_get_crls (WockyTLSHandler *self)
{
  g_assert (WOCKY_IS_TLS_HANDLER (self));

  return self->priv->crls;
}
