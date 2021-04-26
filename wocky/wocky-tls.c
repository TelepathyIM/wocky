/*
 * Wocky TLS integration - GIO-TLS implementation
 * (just named -tls for historical reasons)
 *
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 * Copyright © 2008-2009 Codethink Limited
 * Copyright © 2009 Collabora Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 *          Vivek Dasmohapatra <vivek@collabora.co.uk>
 *          Dan Winship <danw@gnome.org>
 */

/**
 * SECTION: wocky-tls
 * @title: Wocky TLS
 * @short_description: Establish TLS sessions
 *
 * Since version 0.19 this implementation is a wrapper around GIO
 * GTlsConnection - where it all started. Glib-networking is then
 * providing actual TLS backends - GnuTLS (default) and OpenSSL (optional).
 *
 * #WockyTLSession thus became simple alias to GTlsConnection class. See
 * <ulink url="https://developer.gnome.org/gio/stable/GTlsConnection.html">
 * GIO GTlsConnection API Reference</ulink> for generic methods.
 *
 * The below environment variables can be used to print debug output from GIO
 * and GNU TLS.
 *
 * * `G_MESSAGES_DEBUG=GLib-Net|all`: Overall glib debug (could be very chatty)
 * * `GNUTLS_DEBUG_LEVEL=[0..99]`: low level GnuTLS debug messages (similar)
 *
 * Higher values will print more information. See the documentation of
 * `gnutls_global_set_log_level` for more details.
 *
 * * `WOCKY_DEBUG=tls|all` will trigger increased debugging output from within
 * wocky-tls.c as well.
 *
 * The `G_TLS_GNUTLS_PRIORITY` environment variable can be set to a gnutls
 * priority string [See gnutls-cli(1) or the `gnutls_priority_init` docs]
 * to control most tls protocol details. An empty or unset value is roughly
 * equivalent to a priority string `"NORMAL:%COMPAT:-VERS-TLS1.0:-VERS-TLS1.1"`.
 *
 * To control OpenSSL backend's parameters following glib-networking variables
 * could be set in the environment:
 * * `G_TLS_OPENSSL_MAX_PROTO`: Sets OpenSSL Max allowed protocol.
 * * `G_TLS_OPENSSL_CIPHER_LIST`: Sets allowed ciphers (cipherstring).
 * * `G_TLS_OPENSSL_CURVE_LIST`: Similarly restricts allowed EC curve list.
 * * `G_TLS_OPENSSL_SIGNATURE_ALGORITHM_LIST`: Controls allowed hash functions.
 *
 * For instance to restrict to TLSv1.2 protocol you can export following
 * environment variable: `export G_TLS_OPENSSL_MAX_PROTO=0x0303`
 *
 * Finally `GIO_USE_TLS=gnutls|openssl` could be used to control which backend
 * will be selected (if available).
 *
 * See glib-networking docs for details on controlling backends and their
 * behaviour.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-tls.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_TLS

#define VERIFY_STRICT G_TLS_CERTIFICATE_VALIDATE_ALL
#define VERIFY_NORMAL G_TLS_CERTIFICATE_VALIDATE_ALL
#define VERIFY_LENIENT 0

#include "wocky-debug-internal.h"
#include "wocky-utils.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

static void
free_cas (gpointer cas)
{
  GList *c;

  for (c = cas; c; c = c->next)
    g_object_unref (c->data);
  g_list_free (cas);
}

/* ************************************************************************* */
/* adding CA certificates lists for peer certificate verification    */

void
wocky_tls_session_add_ca (GTlsConnection *conn,
                          const gchar *ca_path)
{
  GList *cas, *certs, *c;
  int n = 0;
  struct stat target;

  cas = g_object_get_data (G_OBJECT (conn), "wocky-ca-list");
  if (cas)
    {
      /* Copy, since the old value will be freed when we set the
       * new value.
       */
      for (c = cas; c; c = c->next)
        g_object_ref (c->data);
      cas = g_list_copy (cas);
    }
  DEBUG ("adding CA CERT path '%s'", (gchar *) ca_path);

  if (stat (ca_path, &target) != 0)
    {
      DEBUG ("CA file '%s': stat failed)", ca_path);
      return;
    }

  if (S_ISDIR (target.st_mode))
    {
      DIR *dir;
      struct dirent *entry;

      if ((dir = opendir (ca_path)) == NULL)
        return;

      for (entry = readdir (dir); entry != NULL; entry = readdir (dir))
        {
          struct stat file;
          gchar *path = g_build_path ("/", ca_path, entry->d_name, NULL);

          if ((stat (path, &file) == 0) && S_ISREG (file.st_mode))
            {
              certs = g_tls_certificate_list_new_from_file (path, NULL);
              n += g_list_length (certs);
              cas = g_list_concat (cas, certs);
            }

          g_free (path);
        }

      DEBUG ("+ %s: %d certs from dir", ca_path, n);
      closedir (dir);
    }
  else if (S_ISREG (target.st_mode))
    {
      certs = g_tls_certificate_list_new_from_file (ca_path, NULL);
      n += g_list_length (certs);
      cas = g_list_concat (cas, certs);
      DEBUG ("+ %s: %d certs from file", ca_path, n);
    }
  g_object_set_data_full (G_OBJECT (conn), "wocky-ca-list", cas, free_cas);
  if (cas)
    {
      /* Generating GTlsDatabase for cert verification */
      GFileIOStream *fio = NULL;
      GFile *anchor = g_file_new_tmp (NULL, &fio, NULL);
      GOutputStream *o = g_io_stream_get_output_stream (G_IO_STREAM (fio));
      for (c = cas; c; c = c->next)
        {
          char *buf = NULL;
          g_object_get (G_OBJECT (c->data), "certificate-pem", &buf, NULL);
          if (!buf || !g_output_stream_write_all (o, buf, strlen (buf), NULL, NULL, NULL))
            {
              DEBUG ("Cannot write temp file to aggregate Trust Anchor");
              // Flag failed anchor aggregation
              o = NULL;
              g_free (buf);
              break;
            }
          g_free (buf);
        }
      g_io_stream_close (G_IO_STREAM (fio), NULL, NULL);
      if (o)
        {
          char *fn = g_file_get_path (anchor);
          GTlsDatabase *fdb = g_tls_file_database_new (fn, NULL);
          g_free (fn);
          if (fdb)
            g_tls_connection_set_database (G_TLS_CONNECTION (conn), fdb);
          g_object_unref (fdb);
        }
      g_file_delete (anchor, NULL, NULL);
      g_object_unref (anchor);
      g_object_unref (fio);
    }
}

void
wocky_tls_session_add_crl (WockyTLSSession *session, const gchar *crl_path)
{
  DEBUG ("GIO-TLS currently doesn't support custom CRLs, ignoring %s", crl_path);
}

/**
 * wocky_tls_session_get_peers_certificate:
 * @session: a #GTlsConnection
 * @type: (out): a location for #WockyTLSCertType
 *
 * Obtains all peer certificates - which supposed to be server cert and the
 * optional chain. The @type param is set with the type of retrieved certs,
 * and should always be WOCKY_TLS_CERT_TYPE_X509 (since pgp is deprecated).
 * Returned certificates are binary blobs of DER certeficate data.
 *
 * Returns: (transfer full)(element-type GByteArray): a #GPtrArray of #GByteArray
 * DER blobs.
 */
GPtrArray *
wocky_tls_session_get_peers_certificate (GTlsConnection *session,
    WockyTLSCertType *type)
{
  GTlsCertificate *tlscert;
  GPtrArray *certificates;

  tlscert = g_tls_connection_get_peer_certificate (session);
  if (tlscert == NULL)
    return NULL;

  certificates =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);

  while (tlscert)
    {
      GArray *cert;
      GByteArray *der_data;

      g_object_get (G_OBJECT (tlscert),
          "certificate", &der_data,
          NULL);
      cert = g_array_sized_new (TRUE, TRUE, sizeof (guchar), der_data->len);
      g_array_append_vals (cert, der_data->data, der_data->len);
      g_byte_array_unref (der_data);
      g_ptr_array_add (certificates, cert);

      g_object_get (G_OBJECT (tlscert),
          "issuer", &tlscert,
          NULL);
    }

  if (type != NULL)
    *type = WOCKY_TLS_CERT_TYPE_X509;

  return certificates;
}

static inline gboolean
contains_illegal_wildcard (const char *name, int size)
{
  if (name[0] == '*' && name[1] == '.')
    {
      name += 2;
      size -= 2;
    }

  if (memchr (name, '*', size) != NULL)
    return TRUE;

  return FALSE;
}

int
wocky_tls_session_verify_peer (WockyTLSSession    *session,
                               GStrv               extra_identities,
                               WockyTLSVerificationLevel level,
                               WockyTLSCertStatus *status)
{
  int rval = 0;
  guint peer_cert_status = 0;
  GTlsCertificateFlags check;

  /* list gio cert error conditions in descending order of noteworthiness    *
   * and map them to wocky cert error conditions                             */
  static const struct
  {
    GTlsCertificateFlags gio;
    WockyTLSCertStatus wocky;
  } status_map[] =
    { { G_TLS_CERTIFICATE_BAD_IDENTITY, WOCKY_TLS_CERT_NAME_MISMATCH       },
      { G_TLS_CERTIFICATE_REVOKED,      WOCKY_TLS_CERT_REVOKED             },
      { G_TLS_CERTIFICATE_NOT_ACTIVATED,WOCKY_TLS_CERT_NOT_ACTIVE          },
      { G_TLS_CERTIFICATE_EXPIRED,      WOCKY_TLS_CERT_EXPIRED             },
      { G_TLS_CERTIFICATE_UNKNOWN_CA,   WOCKY_TLS_CERT_SIGNER_UNKNOWN      },
      { G_TLS_CERTIFICATE_INSECURE,     WOCKY_TLS_CERT_INSECURE            },
      { G_TLS_CERTIFICATE_GENERIC_ERROR,WOCKY_TLS_CERT_INVALID             },
      { ~((long) 0),                    WOCKY_TLS_CERT_UNKNOWN_ERROR       },
      { 0,                              WOCKY_TLS_CERT_OK                  } };

  g_assert (status != NULL);
  *status = WOCKY_TLS_CERT_OK;

  switch (level)
    {
    case WOCKY_TLS_VERIFY_STRICT:
      check = VERIFY_STRICT;
      break;
    case WOCKY_TLS_VERIFY_NORMAL:
      check = VERIFY_NORMAL;
      break;
    case WOCKY_TLS_VERIFY_LENIENT:
      check = VERIFY_LENIENT;
      break;
    default:
      g_warn_if_reached ();
      check = VERIFY_STRICT;
      break;
    }

  /* Use system default or custom provided Trust Anchor to perform certificate validation *
   * and then translate GIO validation error to Wocky language with the above dictionary  */
  peer_cert_status = g_tls_connection_get_peer_certificate_errors (G_TLS_CONNECTION (session));

  /* Force-override identity check if required */
  if (peer_cert_status & check & G_TLS_CERTIFICATE_BAD_IDENTITY
      && extra_identities && *extra_identities)
    {
      GTlsCertificate *cert = g_tls_connection_get_peer_certificate (
          G_TLS_CONNECTION (session));

      DEBUG ("Checking extra identities");
      for (int i=0; extra_identities[i] != NULL; i++)
        {
          GSocketConnectable *xid = g_network_address_new (extra_identities[i], 0);
          GTlsCertificateFlags flags = g_tls_certificate_verify (cert,
              xid, NULL);

          g_object_unref (xid);

          if (!(flags & G_TLS_CERTIFICATE_BAD_IDENTITY))
            {
              /* The certificate matches extra identity therefore disable
               * identity checks */
              DEBUG ("Certificate identity matches extra id %s", extra_identities[i]);
              check &= ~G_TLS_CERTIFICATE_BAD_IDENTITY;
              break;
            }
        }
    }

  if (peer_cert_status & check)
    { /* gio cert checking can return multiple errors bitwise &ed together    *
       * but we are realy only interested in the "most important" error:      */
      int x;
      for (x = 0; status_map[x].gio != 0; x++)
        {
          DEBUG ("checking gio error %d", status_map[x].gio);
          if (peer_cert_status & check & status_map[x].gio)
            {
              DEBUG ("gio error %d set", status_map[x].gio);
              *status = status_map[x].wocky;
              rval = -1;
              break;
            }
        }
    }

  return rval;
}

WockyTLSSession *
wocky_tls_session_new (GIOStream  *stream,
    const char *peername)
{
  GTlsClientConnection *conn;
  GSocketConnectable *peer;

  peer = peername ? g_network_address_new (peername, 0) : NULL;
  conn = (GTlsClientConnection *)
          g_tls_client_connection_new (stream, peer, NULL);
  if (peer)
    g_object_unref (peer);

  if (!conn)
    return NULL;

  g_object_set (G_OBJECT (conn),
      /* Accept everything; we'll check it afterwards */
      "validation-flags", 0,
      NULL);

  return WOCKY_TLS_SESSION (conn);
}

/**
 * wocky_tls_session_server_new:
 * @stream: a GIOStream on which we expect to receive the client TLS handshake
 * @dhbits: size of the DH parameters (Deprecated since GnuTLS 3.6.0)
 * @key: the path to the X509 PEM key file
 * @cert: the path to the X509 PEM certificate
 *
 * Create a new TLS server session
 *
 * Returns: a #WockyTLSSession object
 */
WockyTLSSession *
wocky_tls_session_server_new (GIOStream *stream, G_GNUC_UNUSED guint dhbits,
                              const gchar* key, const gchar* cert)
{
  GTlsServerConnection *conn;
  GTlsCertificate *tlscert;

  if (key && cert)
    tlscert = g_tls_certificate_new_from_files (cert, key, NULL);
  else
    tlscert = NULL;
  conn = (GTlsServerConnection *)
          g_tls_server_connection_new (stream, tlscert, NULL);
  if (tlscert)
    g_object_unref (tlscert);

  return WOCKY_TLS_SESSION (conn);
}
