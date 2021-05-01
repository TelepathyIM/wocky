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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef _wocky_tls_h_
#define _wocky_tls_h_

#include <gio/gio.h>

#include "wocky-enumtypes.h"

#define WOCKY_TYPE_TLS_SESSION    G_TYPE_TLS_CONNECTION
#define WOCKY_TLS_SESSION(inst)   G_TLS_CONNECTION(inst)

typedef struct _GTlsConnection WockyTLSSession;

typedef enum
{
  WOCKY_TLS_VERIFY_STRICT = 0,
  WOCKY_TLS_VERIFY_NORMAL,
  WOCKY_TLS_VERIFY_LENIENT,
} WockyTLSVerificationLevel;

GQuark wocky_tls_cert_error_quark (void);
#define WOCKY_TLS_CERT_ERROR (wocky_tls_cert_error_quark ())
/*
GQuark wocky_tls_error_quark (void);
#define WOCKY_TLS_ERROR (wocky_tls_error_quark ())
*/
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
  WOCKY_TLS_CERT_NO_CERTIFICATE,
  WOCKY_TLS_CERT_MAYBE_DOS,
  WOCKY_TLS_CERT_INTERNAL_ERROR,
  WOCKY_TLS_CERT_UNKNOWN_ERROR,
} WockyTLSCertStatus;

typedef enum
{
  WOCKY_TLS_CERT_TYPE_NONE = 0,
  WOCKY_TLS_CERT_TYPE_X509,
  WOCKY_TLS_CERT_TYPE_OPENPGP, /* DANWFIXME: unused */
} WockyTLSCertType;

int wocky_tls_session_verify_peer (WockyTLSSession *session,
    GStrv extra_identities,
    WockyTLSVerificationLevel level,
    WockyTLSCertStatus *status);
GPtrArray *wocky_tls_session_get_peers_certificate (WockyTLSSession *session,
    WockyTLSCertType *type);

#define wocky_tls_session_handshake g_tls_connection_handshake
#define wocky_tls_session_handshake_async g_tls_connection_handshake_async
#define wocky_tls_session_handshake_finish g_tls_connection_handshake_finish

void wocky_tls_session_add_ca (WockyTLSSession *session, const gchar *path);
void wocky_tls_session_add_crl (WockyTLSSession *session, const gchar *path);

WockyTLSSession *wocky_tls_session_new (GIOStream *stream,
    const gchar *peername);

WockyTLSSession *wocky_tls_session_server_new (GIOStream   *stream,
                                               guint        dhbits,
                                               const gchar* key,
                                               const gchar* cert);

#endif /* _wocky_tls_h_ */
