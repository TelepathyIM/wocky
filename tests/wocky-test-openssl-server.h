/*
 * Copyright © 2009 Collabora Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * Authors: Vivek Dasmohapatra <vivek@collabora.co.uk>
 *
 * See wocky-tls.c
 * OpenSSL substituted for gnutls by use of a sufficiently large hammer.
 *
 * This file follows the orignal coding style from upstream, not house
 * collabora style: It is a copy of unmerged gnio TLS support with the
 * 'g' prefixes changes to 'wocky' and server-side TLS support added,
 * and then OpenSSL inserted forcefully into a gnutls shaped hole
 */

/* WARNING:
 *
 * This copy is then further hacked to make sure certain sub-optimal
 * SSL record / network write operations happen predictably.
 * it mangles its output data stream, so don't use it unless you know
 * exactly what you're getting into.
 */

#ifndef _test_openssl_h_
#define _test_openssl_h_

#define TEST_SSL_RECORD_MARKER "SSL-RECORD\n"
#define TEST_SSL_RECORD_MARKER_LEN sizeof (TEST_SSL_RECORD_MARKER)
#define TEST_SSL_DATA \
  "Brillineggiava, ed i tovoli slati"               "\n"\
  "        girlavano ghimbanti nella vaba;"         "\n"\
  "i borogovi eran tutti mimanti"                   "\n"\
  "        e la moma radeva fuorigraba."            "\n"\
  "Figliuolo mio, sta' attento al Gibrovacco,"      "\n"\
  "        dagli artigli e dal morso lacerante;"    "\n"\
  "fuggi l'uccello Giuggiolo, e nel sacco"          "\n"\
  "        metti infine il frumioso Bandifante."    "\n"
#define TEST_SSL_DATA_LEN sizeof (TEST_SSL_DATA)


#include <gio/gio.h>
#include <openssl/ssl.h>

#define TEST_TYPE_TLS_CONNECTION (test_tls_connection_get_type ())
#define TEST_TYPE_TLS_SESSION    (test_tls_session_get_type ())
#define TEST_TLS_SESSION(inst)   (G_TYPE_CHECK_INSTANCE_CAST ((inst),   \
                                  TEST_TYPE_TLS_SESSION, TestTLSSession))

#define TEST_TLS_CONNECTION(inst)(G_TYPE_CHECK_INSTANCE_CAST ((inst),   \
                                  TEST_TYPE_TLS_CONNECTION, \
                                  TestTLSConnection))

typedef struct OPAQUE_TYPE__TestTLSConnection TestTLSConnection;
typedef struct OPAQUE_TYPE__TestTLSSession TestTLSSession;

#define TEST_TLS_VERIFY_STRICT  4
#define TEST_TLS_VERIFY_NORMAL  2
#define TEST_TLS_VERIFY_LENIENT 1

GQuark test_tls_cert_error_quark (void);
#define TEST_TLS_CERT_ERROR (test_tls_cert_error_quark ())

GQuark test_tls_error_quark (void);
#define TEST_TLS_ERROR (test_tls_error_quark ())

typedef enum
  {
    TEST_TLS_CERT_OK = 0,
    TEST_TLS_CERT_INVALID,
    TEST_TLS_CERT_NAME_MISMATCH,
    TEST_TLS_CERT_REVOKED,
    TEST_TLS_CERT_SIGNER_UNKNOWN,
    TEST_TLS_CERT_SIGNER_UNAUTHORISED,
    TEST_TLS_CERT_INSECURE,
    TEST_TLS_CERT_NOT_ACTIVE,
    TEST_TLS_CERT_EXPIRED,
    TEST_TLS_CERT_NO_CERTIFICATE,
    TEST_TLS_CERT_MAYBE_DOS,
    TEST_TLS_CERT_INTERNAL_ERROR,
    TEST_TLS_CERT_UNKNOWN_ERROR,
  } TestTLSCertStatus;

GType test_tls_connection_get_type (void);
GType test_tls_session_get_type (void);

int test_tls_session_verify_peer (TestTLSSession    *session,
                                  const gchar        *peername,
                                  long                flags,
                                  TestTLSCertStatus *status);

TestTLSConnection *test_tls_session_handshake (TestTLSSession   *session,
                                               GCancellable  *cancellable,
                                               GError       **error);
void
test_tls_session_handshake_async (TestTLSSession         *session,
                                  gint io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);
TestTLSConnection *
test_tls_session_handshake_finish (TestTLSSession   *session,
                                   GAsyncResult  *result,
                                   GError       **error);

void test_tls_session_add_ca (TestTLSSession *session, const gchar *path);
void test_tls_session_add_crl (TestTLSSession *session, const gchar *path);

TestTLSSession *test_tls_session_new (GIOStream *stream);

TestTLSSession *test_tls_session_server_new (GIOStream   *stream,
                                             guint        dhbits,
                                             const gchar* key,
                                             const gchar* cert);
#endif /* _test_openssl_h_ */

/* this file is based on an unmerged gnio feature  */
/* Local Variables:                                */
/* c-file-style: "gnu"                             */
/* End:                                            */
