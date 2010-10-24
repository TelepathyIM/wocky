/*
 * Wocky TLS integration - GNUTLS implementation
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
 *
 * Upstream: git://git.gnome.org/gnio
 * Branched at: 42b00d143fcf644880456d06d3a20b6e990a7fa3
 *   "toss out everything that moved to glib"
 *
 * This file follows the original coding style from upstream, not house
 * collabora style: It is a copy of unmerged gnio TLS support with the
 * 'g' prefixes changes to 'wocky' and server-side TLS support added.
 */

/**
 * SECTION: wocky-tls
 * @title: Wocky GnuTLS TLS
 * @short_description: Establish TLS sessions
 *
 * The WOCKY_TLS_DEBUG_LEVEL environment variable can be used to print debug
 * output from GNU TLS. To enable it, set it to a value from 1 to 9.
 * Higher values will print more information. See the documentation of
 * gnutls_global_set_log_level for more details.
 *
 * Increasing the value past certain thresholds will also trigger increased
 * debugging output from within wocky-tls.c as well.
 *
 * The WOCKY_GNUTLS_OPTIONS environment variable can be set to a gnutls
 * priority string [See gnutls-cli(1) or the gnutls_priority_init docs]
 * to control most tls protocol details. An empty or unset value is roughly
 * equivalent to a priority string of "SECURE:+COMP-DEFLATE".
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-tls.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

/* DANWFIXME: allow configuring compression options */
#ifdef ENABLE_PREFER_STREAM_CIPHERS
#define DEFAULT_TLS_OPTIONS \
  /* start with nothing enabled by default */ \
  "NONE:" \
  /* enable all the normal algorithms */ \
  "+VERS-TLS-ALL:+SIGN-ALL:+MAC-ALL:+CTYPE-ALL:+RSA:" \
  /* prefer deflate compression, but fall back to null compression */ \
  "+COMP-DEFLATE:+COMP-NULL:" \
  /* our preferred stream ciphers */ \
  "+ARCFOUR-128:+ARCFOUR-40:" \
  /* all the other ciphers */ \
  "+AES-128-CBC:+AES-256-CBC:+3DES-CBC:+DES-CBC:+RC2-40:" \
  "+CAMELLIA-256-CBC:+CAMELLIA-128-CBC"
#else
#define DEFAULT_TLS_OPTIONS \
  "NORMAL:"        /* all secure algorithms */ \
  "-COMP-NULL:"    /* remove null compression */ \
  "+COMP-DEFLATE:" /* prefer deflate */ \
  "+COMP-NULL"     /* fall back to null */
#endif

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_TLS
#define DEBUG_HANDSHAKE_LEVEL 5
#define DEBUG_ASYNC_DETAIL_LEVEL 6

#ifdef DANWFIXME
#define VERIFY_STRICT  GNUTLS_VERIFY_DO_NOT_ALLOW_SAME
#define VERIFY_NORMAL  GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT
#define VERIFY_LENIENT ( GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT     | \
                         GNUTLS_VERIFY_ALLOW_ANY_X509_V1_CA_CRT | \
                         GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD2       | \
                         GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD5       | \
                         GNUTLS_VERIFY_DISABLE_TIME_CHECKS      | \
                         GNUTLS_VERIFY_DISABLE_CA_SIGN          )
#else
#define VERIFY_STRICT G_TLS_CERTIFICATE_VALIDATE_ALL
#define VERIFY_NORMAL G_TLS_CERTIFICATE_VALIDATE_ALL
#define VERIFY_LENIENT 0
#endif

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
}

void
wocky_tls_session_add_crl (WockyTLSSession *session, const gchar *crl_path)
{
  /* DANWFIXME */
}

GPtrArray *
wocky_tls_session_get_peers_certificate (GTlsConnection *conn,
    WockyTLSCertType *type)
{
  GTlsCertificate *tlscert;
  GPtrArray *certificates;

  tlscert = g_tls_connection_get_peer_certificate (conn);
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

#define OID_X520_COMMON_NAME "2.5.4.3"

static gboolean
cert_names_are_valid (gnutls_x509_crt_t cert)
{
  char name[256];
  size_t size;
  gboolean found = FALSE;
  int type = 0;
  int i = 0;

  /* GNUTLS allows wildcards anywhere within the certificate name, but XMPP only
   * permits a single leading "*.".
   */
  for (i = 0; type >= 0; i++)
    {
      size = sizeof (name);
      type = gnutls_x509_crt_get_subject_alt_name (cert, i, name, &size, NULL);

      switch (type)
        {
        case GNUTLS_SAN_DNSNAME:
        case GNUTLS_SAN_IPADDRESS:
          found = TRUE;
          if (contains_illegal_wildcard (name, size))
              return FALSE;
          break;
        default:
          break;
        }
    }

  if (!found)
    {
      size = sizeof (name);

      /* cert has no names at all? bizarro! */
      if (gnutls_x509_crt_get_dn_by_oid (cert, OID_X520_COMMON_NAME, 0,
                                         0, name, &size) < 0)
          return FALSE;

      found = TRUE;

      if (contains_illegal_wildcard (name, size))
          return FALSE;

    }

  /* found a name, wasn't a duff wildcard */
  return found;
}

int
wocky_tls_session_verify_peer (WockyTLSSession    *session,
                               GStrv               extra_identities,
                               WockyTLSVerificationLevel level,
                               WockyTLSCertStatus *status)
{
  int rval = -1;
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
#ifdef DANWFIXME /* does it matter? */
      { GNUTLS_CERT_SIGNER_NOT_CA,      WOCKY_TLS_CERT_SIGNER_UNAUTHORISED },
#endif
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

#ifdef DANWFIXME
  DEBUG ("setting gnutls verify flags level to: %s",
      wocky_enum_to_nick (WOCKY_TYPE_TLS_VERIFICATION_LEVEL, level));
  gnutls_certificate_set_verify_flags (session->gnutls_cert_cred, check);
  rval = gnutls_certificate_verify_peers2 (session->session, &peer_cert_status);

  if (rval != GNUTLS_E_SUCCESS)
    {
      switch (rval)
        {
        case GNUTLS_E_NO_CERTIFICATE_FOUND:
        case GNUTLS_E_INVALID_REQUEST:
          *status = WOCKY_TLS_CERT_NO_CERTIFICATE;
          break;
        case GNUTLS_E_INSUFFICIENT_CREDENTIALS:
          *status = WOCKY_TLS_CERT_INSECURE;
          break;
        case GNUTLS_E_CONSTRAINT_ERROR:
          *status = WOCKY_TLS_CERT_MAYBE_DOS;
          break;
        case GNUTLS_E_MEMORY_ERROR:
          *status = WOCKY_TLS_CERT_INTERNAL_ERROR;
          break;
        default:
          *status = WOCKY_TLS_CERT_UNKNOWN_ERROR;
	}

      return rval;
    }
#endif


  peer_cert_status = g_tls_connection_get_peer_certificate_errors (G_TLS_CONNECTION (session));
  if (check & G_TLS_CERTIFICATE_UNKNOWN_CA)
    {
      GTlsCertificate *peer = g_tls_connection_get_peer_certificate (G_TLS_CONNECTION (session));
      GList *cas = g_object_get_data (G_OBJECT (session), "wocky-ca-list"), *c;
      GTlsCertificateFlags flags;

      for (c = cas; c; c = c->next)
	{
	  flags = g_tls_certificate_verify (peer, NULL, c->data);
	  if (flags == 0)
	    {
	      peer_cert_status &= ~G_TLS_CERTIFICATE_UNKNOWN_CA;
	      break;
	    }
	  else if (flags & G_TLS_CERTIFICATE_GENERIC_ERROR)
	    {
	      peer_cert_status = flags;
	      break;
	    }
	}
    }

  if (peer_cert_status & check)
    { /* gio cert checking can return multiple errors bitwise &ed together    *
       * but we are realy only interested in the "most important" error:      */
      int x;
      *status = WOCKY_TLS_CERT_OK;
      for (x = 0; status_map[x].gio != 0; x++)
        {
          DEBUG ("checking gio error %d", status_map[x].gio);
          if (_stat & status_map[x].gio)
            {
              DEBUG ("gio error %d set", status_map[x].gio);
              *status = status_map[x].wocky;
              rval = -1;
              break;
            }
        }
    }

  return 0;
}

#ifdef DANWFIXME
static void
tls_debug (int level,
    const char *msg)
{
  DEBUG ("[%d] [%02d] %s", getpid(), level, msg);
}

static const char *
tls_options (void)
{
  const char *options = g_getenv ("WOCKY_GNUTLS_OPTIONS");
  return (options != NULL && *options != '\0') ? options : DEFAULT_TLS_OPTIONS;
}
#endif

WockyTLSSession *
wocky_tls_session_new (GIOStream  *stream,
		       const char *peername)
{
  GTlsClientConnection *conn;
  GSocketConnectable *peer;

  peer = peername ? g_network_address_new (peername, 0) : NULL;
  conn = g_tls_client_connection_new (stream, peer, NULL);
  if (peer)
    g_object_unref (peer);

  if (!conn)
    return NULL;

  g_object_set (G_OBJECT (conn),

		/* FIXME: just use the system certdb rather than
		 * reimplementing it ourselves.
		 */
		"use-system-certdb", FALSE,

		/* Accept everything; we'll check it afterwards */
		"validation-flags", 0,

		NULL);

  return WOCKY_TLS_SESSION (conn);
}

/**
 * wocky_tls_session_server_new:
 * @stream: a GIOStream on which we expect to receive the client TLS handshake
 * @dhbits: size of the DH parameters (see gnutls for valid settings)
 * @key: the path to the X509 PEM key file
 * @cert: the path to the X509 PEM certificate
 *
 * Create a new TLS server session
 *
 * Returns: a #WockyTLSSession object
 */
WockyTLSSession *
wocky_tls_session_server_new (GIOStream *stream, guint dhbits,
                              const gchar* key, const gchar* cert)
{
  GTlsServerConnection *conn;
  GTlsCertificate *tlscert;

  /* DANWFIXME: dhbits */

  if (key && cert)
    tlscert = g_tls_certificate_new_from_files (cert, key, NULL);
  else
    tlscert = NULL;
  conn = g_tls_server_connection_new (stream, tlscert, NULL);
  if (tlscert)
    g_object_unref (tlscert);

  return WOCKY_TLS_SESSION (conn);
}
