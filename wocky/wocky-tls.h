/*
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
 * This file follows the orignal coding style from upstream, not house
 * collabora style: It is a copy of unmerged gnio TLS support with the
 * 'g' prefixes changes to 'wocky' and server-side TLS support added.
 *
 */

#ifndef _wocky_tls_h_
#define _wocky_tls_h_

#include <gio/gio.h>
#include <gnutls/x509.h>
#include <gnutls/openpgp.h>

#define WOCKY_TYPE_TLS_CONNECTION (wocky_tls_connection_get_type ())
#define WOCKY_TYPE_TLS_SESSION    (wocky_tls_session_get_type ())
#define WOCKY_TLS_SESSION(inst)   (G_TYPE_CHECK_INSTANCE_CAST ((inst),   \
                                   WOCKY_TYPE_TLS_SESSION, WockyTLSSession))

#define WOCKY_TLS_CONNECTION(inst)(G_TYPE_CHECK_INSTANCE_CAST ((inst),   \
                                   WOCKY_TYPE_TLS_CONNECTION, \
                                   WockyTLSConnection))

typedef struct OPAQUE_TYPE__WockyTLSConnection WockyTLSConnection;
typedef struct OPAQUE_TYPE__WockyTLSSession WockyTLSSession;

#define WOCKY_TLS_VERIFY_STRICT  GNUTLS_VERIFY_DO_NOT_ALLOW_SAME
#define WOCKY_TLS_VERIFY_NORMAL  ( GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT | \
                                   GNUTLS_VERIFY_DO_NOT_ALLOW_SAME )
#define WOCKY_TLS_VERIFY_LENIENT ( GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT     | \
                                   GNUTLS_VERIFY_ALLOW_ANY_X509_V1_CA_CRT | \
                                   GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD2       | \
                                   GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD5       | \
                                   GNUTLS_VERIFY_DISABLE_TIME_CHECKS      | \
                                   GNUTLS_VERIFY_DISABLE_CA_SIGN          )

typedef enum
{
  WOCKY_TLS_CERT_OK = 0,
  WOCKY_TLS_CERT_INVALID,
  WOCKY_TLS_CERT_NAME_MISMATCH,
  WOCKY_TLS_CERT_REVOKED,
  WOCKY_TLS_CERT_SIGNER_UNKNOWN,
  WOCKY_TLS_CERT_SIGNER_UNAUTHORISED,
  WOCKY_TLS_CERT_INSECURE,
  WOCKY_TLS_CERT_NOT_ACTIVE,
  WOCKY_TLS_CERT_EXPIRED,
  WOCKY_TLS_CERT_UNKNOWN_ERROR,
} WockyTLSCertStatus;

GType wocky_tls_connection_get_type (void);
GType wocky_tls_session_get_type (void);

int wocky_tls_session_verify_peer (WockyTLSSession    *session,
                                   const gchar        *peername,
                                   long                flags,
                                   WockyTLSCertStatus *status);

WockyTLSConnection *wocky_tls_session_handshake (WockyTLSSession   *session,
                                                 GCancellable  *cancellable,
                                                 GError       **error);
void
wocky_tls_session_handshake_async (WockyTLSSession         *session,
                                   gint io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);
WockyTLSConnection *
wocky_tls_session_handshake_finish (WockyTLSSession   *session,
                                    GAsyncResult  *result,
                                    GError       **error);

WockyTLSSession *wocky_tls_session_new (GIOStream *stream,
                                        const gchar *ca,
                                        const gchar *crl);

WockyTLSSession *
wocky_tls_session_server_new (GIOStream *stream, guint dhbits,
                              const gchar* key, const gchar* cert,
                              const gchar* ca, const gchar* crl);

#endif

/* this file is "borrowed" from an unmerged gnio feature: */
/* Local Variables:                                       */
/* c-file-style: "gnu"                                    */
/* End:                                                   */
