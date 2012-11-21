/*
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 * Copyright © 2008-2009 Codethink Limited
 * Copyright © 2009-2010 Collabora Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * Authors: Vivek Dasmohapatra <vivek@collabora.co.uk>
 *          Ryan Lortie <desrt@desrt.ca>
 *          Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 *
 * Based on wocky-tls.c, which was in turn based on an unmerged gnio feature.
 * See wocky-tls.c for details.
 *
 * This file follows the original coding style from upstream, not collabora
 * house style. See wocky-tls.c for details.
 */

/**
 * SECTION: wocky-tls
 * @title: Wocky OpenSSL TLS
 * @short_description: Establish TLS sessions
 *
 * The WOCKY_TLS_DEBUG_LEVEL environment variable can be used to print debug
 * output from OpenSSL. To enable it, set it to a value from 1 to 9.
 * Higher values will print more information.
 *
 * Increasing the value past certain thresholds will also trigger increased
 * debugging output from within wocky-openssl.c as well.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-tls.h"

/* Apparently an implicit requirement of OpenSSL's headers... */
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <openssl/ssl.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/x509v3.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_TLS
#define DEBUG_HANDSHAKE_LEVEL 5
#define DEBUG_ASYNC_DETAIL_LEVEL 6

#include "wocky-debug-internal.h"
#include "wocky-utils.h"

#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

/* SSL_CTX_set_cipher_list() allows to restrict/alter the list of supported
 * ciphers; see ciphers(1) for documentation on the format.
 * Usually the normal ciphers are ok, but on mobile phones we prefer RC4 as
 * it decreases the size of packets. The bandwidth difference is tiny, but
 * the difference in power consumption between small and very small packets
 * can be significant on 3G. */
#ifdef ENABLE_PREFER_STREAM_CIPHERS

#define CIPHER_LIST \
  "RC4-SHA:" \
  "RC4-MD5:" \
  "ECDHE-RSA-RC4-SHA:" \
  "ECDHE-ECDSA-RC4-SHA:" \
  "ECDH-RSA-RC4-SHA:" \
  "ECDH-ECDSA-RC4-SHA:" \
  "PSK-RC4-SHA:" \
  "ALL" /* fall-back to all the other algorithms */

#endif

enum
{
  PROP_S_NONE,
  PROP_S_STREAM,
  PROP_S_SERVER,
  PROP_S_DHBITS,
  PROP_S_KEYFILE,
  PROP_S_CERTFILE,
};

enum
{
  PROP_C_NONE,
  PROP_C_SESSION,
};

enum
{
  PROP_O_NONE,
  PROP_O_SESSION
};

enum
{
  PROP_I_NONE,
  PROP_I_SESSION
};

typedef enum
{
  WOCKY_TLS_OP_HANDSHAKE,
  WOCKY_TLS_OP_READ,
  WOCKY_TLS_OP_WRITE
} WockyTLSOperation;

/* from openssl docs: not clear if this is exported as a constant by openssl */
#define MAX_SSLV3_BLOCK_SIZE 0x4000

typedef struct
{
  gboolean active;

  gint io_priority;
  GCancellable *cancellable;
  GObject *source_object;
  GAsyncReadyCallback callback;
  gpointer user_data;
  gpointer source_tag;
  GError *error;
  gboolean sync_complete;
  gchar *buffer;
  gsize count;
  gchar rbuf[MAX_SSLV3_BLOCK_SIZE];
} WockyTLSJob;

typedef struct
{
  WockyTLSJob job;
  gulong state;
  gboolean done;
} WockyTLSHandshake;

typedef GIOStreamClass WockyTLSConnectionClass;
typedef GObjectClass WockyTLSSessionClass;
typedef GInputStreamClass WockyTLSInputStreamClass;
typedef GOutputStreamClass WockyTLSOutputStreamClass;

struct _WockyTLSSession
{
  GObject parent;

  GIOStream *stream;
  GCancellable *cancellable;
  GError *error;
  gboolean async;

  /* tls server support */
  gboolean server;
  guint dh_bits;
  gchar *key_file;
  gchar *cert_file;

  /* frontend jobs */
  struct
  {
    WockyTLSHandshake handshake;
    WockyTLSJob read;
    WockyTLSJob write;
  } job;

  /* openssl structures */
  BIO *rbio;
  BIO *wbio;
  SSL_METHOD *method;
  SSL_CTX *ctx;
  SSL *ssl;
};

typedef struct
{
  GInputStream parent;
  WockyTLSSession *session;
} WockyTLSInputStream;

typedef struct
{
  GOutputStream parent;
  WockyTLSSession *session;
} WockyTLSOutputStream;

struct _WockyTLSConnection
{
  GIOStream parent;

  WockyTLSSession *session;
  WockyTLSInputStream *input;
  WockyTLSOutputStream *output;
};

DH * get_dh4096 (void);
DH * get_dh2048 (void);
DH * get_dh1024 (void);
DH * get_dh512 (void);

static guint tls_debug_level = 0;

static GType wocky_tls_input_stream_get_type (void);
static GType wocky_tls_output_stream_get_type (void);
G_DEFINE_TYPE (WockyTLSConnection, wocky_tls_connection, G_TYPE_IO_STREAM);
G_DEFINE_TYPE (WockyTLSSession, wocky_tls_session, G_TYPE_OBJECT);
G_DEFINE_TYPE (WockyTLSInputStream, wocky_tls_input_stream, G_TYPE_INPUT_STREAM);
G_DEFINE_TYPE (WockyTLSOutputStream, wocky_tls_output_stream, G_TYPE_OUTPUT_STREAM);
#define WOCKY_TYPE_TLS_INPUT_STREAM (wocky_tls_input_stream_get_type ())
#define WOCKY_TYPE_TLS_OUTPUT_STREAM (wocky_tls_output_stream_get_type ())
#define WOCKY_TLS_INPUT_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst),   \
                                      WOCKY_TYPE_TLS_INPUT_STREAM,          \
                                      WockyTLSInputStream))
#define WOCKY_TLS_OUTPUT_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst),   \
                                       WOCKY_TYPE_TLS_OUTPUT_STREAM,         \
                                       WockyTLSOutputStream))

/* Ok: This function tries to retrieve the error that caused a problem from  *
 * bottom of the openssl error stack: The errnum argument is the error code  *
 * returned by the last openssl operation which MAY NOT have come from the   *
 * openssl error stack (cf SSL_get_error) and which MAY be SSL_ERROR_NONE:   *
 * it's not supposed to be SSL_ERROR_NONE if a problem occurred, but this is *
 * not actually guaranteed anywhere so we have to check for it here:         */
static const gchar *error_to_string (long error)
{
  static gchar ssl_error[256];
  int e;
  int x;
  /* SSL_ERROR_NONE from ERR_get_error means we have emptied the stack, *
   * in which case we should back up and use the last error we saw:     */
  for (e = x = error; x != SSL_ERROR_NONE; x = ERR_get_error ())
    e = x;

  /* we found an error in the stack, or were passed one in errnum: */
  if (e != SSL_ERROR_NONE)
    {
      ERR_error_string_n ((gulong) e, ssl_error, sizeof (ssl_error));
      return ssl_error;
    }

  /* No useful/informative/relevant error found */
  return NULL;
}

static GSimpleAsyncResult *
wocky_tls_job_make_result (WockyTLSJob *job,
                           gssize   result)
{
  GSimpleAsyncResult *simple;

  simple = g_simple_async_result_new (job->source_object,
                                      job->callback,
                                      job->user_data,
                                      job->source_tag);
  if (job->error != NULL)
    {
      DEBUG ("setting error from job '%s'", job->error->message);
      g_simple_async_result_set_from_error (simple, job->error);
      g_error_free (job->error);
      job->error = NULL;
    }

  if (job->source_object != NULL)
    g_object_unref (job->source_object);

  job->source_object = NULL;

  if (job->cancellable != NULL)
    g_object_unref (job->cancellable);

  job->cancellable = NULL;

  job->active = FALSE;

  return simple;
}

static void
wocky_tls_job_result_gssize (WockyTLSJob *job,
                             gssize   result)
{
  GSimpleAsyncResult *simple;

  if ((simple = wocky_tls_job_make_result (job, result)))
    {
      if (result >= 0)
        g_simple_async_result_set_op_res_gssize (simple, result);

      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
}

/* only used for handshake results: read + write use result_gssize */
static void
wocky_tls_job_result_boolean (WockyTLSJob *job,
                              gint     result)
{
  GSimpleAsyncResult *simple;

  if ((simple = wocky_tls_job_make_result (job, result)))
    {
      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
}

/* ************************************************************************* */
static void
wocky_tls_session_try_operation (WockyTLSSession   *session,
                                 WockyTLSOperation  operation);

static void
wocky_tls_session_write_ready (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data);
static void
wocky_tls_session_read_ready (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data);

/* writes to the internal BIO should always succeed, so we should never
 * receive SSL_ERROR_WANT_WRITE: reads, on the other hand, obviously
 * depend on how much data we have buffered, so SSL_ERROR_WANT_READ can
 * clearly happen */
static void
handshake_write (WockyTLSSession *session)
{
  gchar *wbuf;
  WockyTLSJob *handshake = &(session->job.handshake.job);
  GCancellable *cancel = handshake->cancellable;
  gint prio = handshake->io_priority;
  GOutputStream *output = g_io_stream_get_output_stream (session->stream);
  long wsize = BIO_get_mem_data (session->wbio, &wbuf);

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  g_output_stream_write_async (output, wbuf, wsize, prio, cancel,
                               wocky_tls_session_write_ready, session);
}

static void
handshake_read (WockyTLSSession *session)
{
  GInputStream *input = g_io_stream_get_input_stream (session->stream);
  WockyTLSJob *handshake = (WockyTLSJob *) &session->job.handshake.job;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  g_input_stream_read_async (input,
                             &(handshake->rbuf),
                             MAX_SSLV3_BLOCK_SIZE,
                             handshake->io_priority,
                             handshake->cancellable,
                             wocky_tls_session_read_ready,
                             session);
}

static int
ssl_handshake (WockyTLSSession *session)
{
  gint result = 1;
  gulong errnum = SSL_ERROR_NONE;
  gboolean want_read = FALSE;
  gboolean want_write = FALSE;
  const gchar *errstr = NULL;
  gboolean done = session->job.handshake.done;
  gboolean fatal = FALSE;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  if (!done)
    {
      const gchar *method;

      if (session->server)
        {
          method = "SSL_accept";
          result = SSL_accept (session->ssl);
        }
      else
        {
          method = "SSL_connect";
          result = SSL_connect (session->ssl);
        }
      errnum = SSL_get_error (session->ssl, result);
      done = (result == 1);
      errstr = error_to_string (errnum);
      fatal = (errnum != SSL_ERROR_WANT_READ &&
               errnum != SSL_ERROR_WANT_WRITE &&
               errnum != SSL_ERROR_NONE);
      DEBUG ("%s - result: %d; error: %ld", method, result, errnum);
      DEBUG ("%s         : %s", method, errstr);
    }

  /* buffered write data means we need to write */
  want_write = BIO_pending (session->wbio) > 0;

  /* check to see if there's data waiting to go out:                *
   * since writes to a BIO should always succeed, it is possible to *
   * have buffered write data after a successful return, but not    *
   * possible to be waiting on a read, since SSL_connect should not *
   * return success if waiting for data to come in                  */
  if (done)
    {
      session->job.handshake.done = TRUE;

      if (!want_write)
        {
          DEBUG ("Handshake completed");
          errnum = session->job.handshake.state = SSL_ERROR_NONE;
        }
      else
        {
          DEBUG ("Handshake completed (IO incomplete)");
          g_assert (errnum != SSL_ERROR_WANT_READ);
          errnum = SSL_ERROR_WANT_WRITE;
        }
    }
  else
    {
      DEBUG ("Handshake state: %ld", errnum);
      session->job.handshake.state = errnum;
      want_read = (errnum == SSL_ERROR_WANT_READ);
    }
  /* sif we want both a write (buffered data in the BIO) AND a read        *
   * (SSL_ERROR_WANT_READ) then this will happen when the handshake_write *
   * invokes wocky_tls_session_write_ready which will in turn call        *
   * wocky_tls_session_try_operation which will re-enter handshake        *
   * and then proceed to fall back through to this block of code          */

  if (!fatal)
    {
      DEBUG ("want write: %d; want read: %d;", want_write, want_read);
      if (want_write)
        handshake_write (session);
      else if (want_read)
        handshake_read (session);
      else
        wocky_tls_session_try_operation (session, WOCKY_TLS_OP_HANDSHAKE);
    }
  else
    {
      DEBUG ("Handshake failed: [%d:%ld] %s", result, errnum, errstr);
      if (session->job.handshake.job.error != NULL)
        {
          g_error_free (session->job.handshake.job.error);
          session->job.handshake.job.error = NULL;
        }
      g_set_error (&(session->job.handshake.job.error), WOCKY_TLS_ERROR, result,
                   "Handshake failed: %s", errstr);
      wocky_tls_session_try_operation (session, WOCKY_TLS_OP_HANDSHAKE);
    }

  return errnum;
}

static void
ssl_fill (WockyTLSSession *session)
{
  GInputStream *input = g_io_stream_get_input_stream (session->stream);
  gchar *rbuf = session->job.read.rbuf;
  gint prio = session->job.read.io_priority;
  GCancellable *cancel = session->job.read.cancellable;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  g_input_stream_read_async (input, rbuf, MAX_SSLV3_BLOCK_SIZE, prio, cancel,
                             wocky_tls_session_read_ready, session);
}

static void
ssl_flush (WockyTLSSession *session)
{
  long wsize;
  gchar *wbuf;
  gint prio = session->job.read.io_priority;
  GOutputStream *output = g_io_stream_get_output_stream (session->stream);
  GCancellable *cancel = session->job.read.cancellable;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  wsize = BIO_get_mem_data (session->wbio, &wbuf);

  if (wsize > 0)
    g_output_stream_write_async (output, wbuf, wsize, prio, cancel,
                                 wocky_tls_session_write_ready, session);
}

/* FALSE indicates we should go round again and try to get more data */
static gboolean
ssl_read_is_complete (WockyTLSSession *session, gint result)
{
  /* if the job error is set, we should bail out now, we have failed   *
   * otherwise:                                                        *
   * a -ve return with an SSL error of WANT_READ implies an incomplete *
   * crypto record: we need to go round again and get more data        *
   * or:                                                               *
   * a 0 return means the SSL connection was shut down cleanly         */
  if ((session->job.read.error == NULL) && (result <= 0))
    {
      int err = SSL_get_error (session->ssl, result);

      switch (err)
        {
        case SSL_ERROR_WANT_READ:
          DEBUG ("Incomplete SSL record, read again");
          return FALSE;
        case SSL_ERROR_WANT_WRITE:
          g_warning ("read caused write: unsupported TLS re-negotiation?");
          /* deliberately falling through to the default case, having logged a
           * more specific warning.
           */
        default:
          g_set_error (&session->job.read.error, WOCKY_TLS_ERROR, err,
                       "OpenSSL read: protocol error %d", err);
        }
    }

  return TRUE;
}

static void
wocky_tls_session_try_operation (WockyTLSSession   *session,
                                 WockyTLSOperation  operation)
{
  WockyTLSJob *handshake = &(session->job.handshake.job);

  if (handshake->active || operation == WOCKY_TLS_OP_HANDSHAKE)
    {
      gint result = session->job.handshake.state;
      DEBUG ("async job handshake");

      if (tls_debug_level >= DEBUG_HANDSHAKE_LEVEL)
        DEBUG ("async job handshake: %d", result);

      switch (result)
        {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
          DEBUG ("Handshake incomplete...");
          ssl_handshake (session);
          break;
        case SSL_ERROR_NONE:
          DEBUG ("Handshake complete (success): %d", result);
          wocky_tls_job_result_boolean (handshake, result);
          break;
        default:
          DEBUG ("Handshake complete (failure): %d", result);
          if (handshake->error == NULL)
            handshake->error =
              g_error_new (WOCKY_TLS_ERROR, result, "Handshake Error");
          wocky_tls_job_result_boolean (handshake, result);
        }
    }
  else if (operation == WOCKY_TLS_OP_READ)
    {
      gssize result = 0;
      gulong pending = 0;
      gsize wanted = 0;

      if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
        DEBUG ("async job OP_READ");

      /* cipherbytes in the BIO != clearbytes after SSL_read */
      wanted = session->job.read.count;
      pending = (gulong)BIO_pending (session->rbio);
      result = SSL_read (session->ssl, session->job.read.buffer, wanted);
      DEBUG ("read %" G_GSSIZE_FORMAT " clearbytes (from %ld cipherbytes)",
          result, pending);

      if (ssl_read_is_complete (session, result))
        wocky_tls_job_result_gssize (&session->job.read, result);
      else
        ssl_fill (session);
    }

  else
    { /* we have no useful way of mapping SSL cipherbytes to raw *
       * clearbytes: it should always be a complete write unless *
       * there's been a network error, in which case the utility *
       * of a byte count is debatable anyway                     */
      gssize result = session->job.write.count;

      if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
        DEBUG ("async job OP_WRITE");

      g_assert (operation == WOCKY_TLS_OP_WRITE);
      DEBUG ("wrote %" G_GSSIZE_FORMAT " clearbytes", result);
      wocky_tls_job_result_gssize (&session->job.write, result);
    }
}

static void
wocky_tls_job_start (WockyTLSJob             *job,
                     gpointer             source_object,
                     gint                 io_priority,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data,
                     gpointer             source_tag)
{
  g_assert (job->active == FALSE);
  g_assert (job->cancellable == NULL);

  /* this is always a circular reference, so it will keep the
   * session alive for as long as the job is running.
   */
  job->source_object = g_object_ref (source_object);

  job->io_priority = io_priority;
  if (cancellable != NULL)
    job->cancellable = g_object_ref (cancellable);
  job->callback = callback;
  job->user_data = user_data;
  job->source_tag = source_tag;
  job->error = NULL;
  job->active = TRUE;
}

typedef gint (*ssl_handler) (SSL *ssl);

WockyTLSConnection *
wocky_tls_session_handshake (WockyTLSSession   *session,
                             GCancellable  *cancellable,
                             GError       **error)
{
  gint result = -1;
  gboolean go = TRUE;
  gboolean done = FALSE;
  ssl_handler handler = session->server ? SSL_accept : SSL_connect;
  gboolean want_write = FALSE;
  gboolean want_read = FALSE;
  gint errnum = SSL_ERROR_NONE;
  const gchar *errstr = NULL;

  while (go)
    {
      DEBUG ("sync SSL handshake loop");

      if (!done)
        {
          result = handler (session->ssl);
          errnum = SSL_get_error (session->ssl, result);
          done = (result == 1);
          DEBUG ("SSL_%s: %d:%d",
                 (handler == SSL_accept) ? "accept" : "connect",
                 result, errnum);
          if (errnum != SSL_ERROR_NONE &&
              errnum != SSL_ERROR_WANT_READ &&
              errnum != SSL_ERROR_WANT_WRITE)
            {
              errstr = error_to_string (errnum);
              DEBUG ("SSL handshake error: [%d:%d] %s", result, errnum, errstr);
            }
        }

      want_write = BIO_pending (session->wbio) > 0;
      want_read = (errnum == SSL_ERROR_WANT_READ);

      if (want_write)
        {
          gchar *wbuf;
          GOutputStream *out = g_io_stream_get_output_stream (session->stream);
          long wsize = BIO_get_mem_data (session->wbio, &wbuf);
          gssize sent = 0;
          DEBUG ("sending %ld cipherbytes", wsize);
          if (wsize > 0)
            sent = g_output_stream_write (out, wbuf, wsize, NULL, error);
          DEBUG ("sent %" G_GSSIZE_FORMAT " cipherbytes", sent);
          (void) BIO_reset (session->wbio);
        }

      if (want_read)
        {
          char rbuf[MAX_SSLV3_BLOCK_SIZE];
          GInputStream *in = g_io_stream_get_input_stream (session->stream);
          gssize bytes =
            g_input_stream_read (in, &rbuf, sizeof(rbuf), NULL, error);
          DEBUG ("read %" G_GSSIZE_FORMAT " cipherbytes", bytes);
          BIO_write (session->rbio, &rbuf, bytes);
        }

      switch (errnum)
        {
        case SSL_ERROR_WANT_WRITE:
          /* WANT_WRITE is theoretically impossible, but what the hell */
        case SSL_ERROR_WANT_READ:
          break;
        case SSL_ERROR_NONE:
          DEBUG ("handshake complete, all IO done");
          go = FALSE;
          break;
        default:
          DEBUG ("SSL handshake error: [%d:%d] %s", result, errnum, errstr);
          *error =
            g_error_new (WOCKY_TLS_ERROR, errnum, "Handshake: %s", errstr);
          go = FALSE;
        }
    }

  if (done)
    return g_object_new (WOCKY_TYPE_TLS_CONNECTION, "session", session, NULL);

  return NULL;
}

/* ************************************************************************* */
/* adding CA certificates & CRL lists for peer certificate verification      */

void
wocky_tls_session_add_ca (WockyTLSSession *session,
                          const gchar *path)
{
  gboolean ok = FALSE;

  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      DEBUG ("CA file or path '%s' not accessible", path);
      return;
    }

  if (g_file_test (path, G_FILE_TEST_IS_DIR))
    {
      DEBUG ("Loading CA directory");
      ok = SSL_CTX_load_verify_locations (session->ctx, NULL, path);
    }

  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      DEBUG ("Loading CA file");
      ok = SSL_CTX_load_verify_locations (session->ctx, path, NULL);
    }

  if (!ok)
    {
      gulong e, f;
      for (f = e = ERR_get_error (); e != 0; e = ERR_get_error ())
        f = e;
      DEBUG ("CA '%s' failed: %s", path, ERR_error_string (f, NULL));
    }
  else
    DEBUG ("CA '%s' loaded", path);
}

void
wocky_tls_session_add_crl (WockyTLSSession *session,
                           const gchar *path)
{
  gboolean ok = FALSE;

  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      DEBUG ("CRL file or path '%s' not accessible", path);
      return;
    }

  if (g_file_test (path, G_FILE_TEST_IS_DIR))
    {
      X509_STORE *store = SSL_CTX_get_cert_store (session->ctx);
      X509_LOOKUP_METHOD *method = X509_LOOKUP_hash_dir ();
      X509_LOOKUP *lookup = X509_STORE_add_lookup (store, method);
      DEBUG ("Loading CRL directory");
      ok = X509_LOOKUP_add_dir (lookup, path, X509_FILETYPE_PEM) == 1;
    }

  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      X509_STORE *store = SSL_CTX_get_cert_store (session->ctx);
      X509_LOOKUP_METHOD *method = X509_LOOKUP_file ();
      X509_LOOKUP *lookup = X509_STORE_add_lookup (store, method);
      DEBUG ("Loading CRL file");
      ok = X509_LOOKUP_load_file (lookup, path, X509_FILETYPE_PEM) == 1;
    }

  if (!ok)
    {
      gulong e, f;
      for (f = e = ERR_get_error (); e != 0; e = ERR_get_error ())
        f = e;
      DEBUG ("'%s' failed: %s\n", path, ERR_error_string (f, NULL));
    }
  else
    DEBUG ("'%s' loaded\n", path);
}

/* ************************************************************************* */

void
wocky_tls_session_handshake_async (WockyTLSSession         *session,
                                   gint                 io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  DEBUG ("");
  wocky_tls_job_start (&session->job.handshake.job, session,
                       io_priority, cancellable, callback, user_data,
                       wocky_tls_session_handshake_async);
  ssl_handshake (session);
}

WockyTLSConnection *
wocky_tls_session_handshake_finish (WockyTLSSession   *session,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  DEBUG ("");
  {
    GObject *source_object;

    source_object = g_async_result_get_source_object (result);
    g_object_unref (source_object);
    g_return_val_if_fail (G_OBJECT (session) == source_object, NULL);
  }

  g_return_val_if_fail (wocky_tls_session_handshake_async ==
                        g_simple_async_result_get_source_tag (simple), NULL);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  DEBUG ("connection OK");
  return g_object_new (WOCKY_TYPE_TLS_CONNECTION, "session", session, NULL);
}

static gboolean
compare_wildcarded_hostname (const char *hostname, const char *certname)
{
  DEBUG ("%s ~ %s", hostname, certname);

  if (g_ascii_strcasecmp (hostname, certname) == 0)
    return TRUE;

  /* We only allow leading '*.' wildcards. See the final bullet point of XMPP
   * Core §13.7.1.2.1
   * <http://xmpp.org/rfcs/rfc6120.html#security-certificates-generation-server>:
   *
   *   DNS domain names in server certificates MAY contain the wildcard
   *   character '*' as the complete left-most label within the identifier.
   */
  if (g_str_has_prefix (certname, "*."))
    {
      const gchar *certname_tail = certname + 2;
      const gchar *hostname_tail = index (hostname, '.');

      if (hostname_tail == NULL)
        return FALSE;

      hostname_tail++;
      DEBUG ("%s ~ %s", hostname_tail, certname_tail);
      return g_ascii_strcasecmp (hostname_tail, certname_tail) == 0;
    }

  return FALSE;
}

static gboolean
check_peer_name (const char *target, X509 *cert)
{
  int i;
  gboolean rval = FALSE;
  X509_NAME *subject = X509_get_subject_name (cert);
  X509_CINF *ci = cert->cert_info;
  static const long nid[] = { NID_commonName, NID_subject_alt_name, NID_undef };

  /* first, see if the x509 name contains the info we want: */
  for (i = 0; nid[i] != NID_undef; i++)
    {
      gssize len = X509_NAME_get_text_by_NID (subject, nid[i], NULL, -1);
      if (len > 0)
        {
          char *cname = g_new0 (gchar, len + 1);
          X509_NAME_get_text_by_NID (subject, nid[i], cname, len + 1);
          DEBUG ("got cname '%s' from x509 name, nid #%u", cname, i);
          rval = compare_wildcarded_hostname (target, cname);
          g_free (cname);
        }
    }

  /* ok, if that failed, we need to dive into the guts of the x509 structure *
   * and extract the subject_alt_name from the x509 v3 extensions: if that   *
   * extension is present, and a string, use that. If it is present, and     *
   * a multi-value stack, trawl it for the "DNS" entry and use that          */
  if (!rval && (ci->extensions != NULL))
    for (i = 0; i < sk_X509_EXTENSION_num(ci->extensions) && !rval; i++)
      {
        X509_EXTENSION *ext = sk_X509_EXTENSION_value (ci->extensions, i);
        ASN1_OBJECT *obj = X509_EXTENSION_get_object (ext);
        X509V3_EXT_METHOD *convert = NULL;
        long ni = OBJ_obj2nid (obj);
        const guchar *p;
        char *value = NULL;
        int len = ext->value->length;
        void *ext_str = NULL;

        if (ni != NID_subject_alt_name)
          continue;

        /* OpenSSL >= 1.0 returns a const here, but we need to be also   *
         * compatible with older versions that return a non-const value, *
         * hence the cast                                                */
        if ((convert = (X509V3_EXT_METHOD *) X509V3_EXT_get (ext)) == NULL)
          continue;

        p = ext->value->data;
        ext_str = ((convert->it != NULL) ?
                   ASN1_item_d2i (NULL, &p, len, ASN1_ITEM_ptr(convert->it)) :
                   convert->d2i (NULL, &p, len) );

        if (ext_str == NULL)
          continue;

        if (convert->i2s != NULL)
          {
            value = convert->i2s (convert, ext_str);
            DEBUG ("got cname '%s' from subject_alt_name, which is a string",
                value);
            rval = compare_wildcarded_hostname (target, value);
            OPENSSL_free (value);
          }
        else if (convert->i2v != NULL)
          {
            int j;
            STACK_OF(CONF_VALUE) *nval = convert->i2v(convert, ext_str, NULL);
            for (j = 0; j < sk_CONF_VALUE_num (nval); j++)
              {
                CONF_VALUE *v = sk_CONF_VALUE_value(nval, j);
                if (!wocky_strdiff (v->name, "DNS"))
                  {
                    DEBUG ("Got cname '%s' from subject_alt_name, which is a "
                        "multi-value stack with a 'DNS' entry", v->value);
                    rval = compare_wildcarded_hostname (target, v->value);
                  }
              }
            sk_CONF_VALUE_pop_free(nval, X509V3_conf_free);
          }

        if (convert->it)
          ASN1_item_free (ext_str, ASN1_ITEM_ptr (convert->it));
        else
          convert->ext_free (ext_str);
      }

  return rval;
}

static gboolean
check_peer_names (const char *peer_name,
    GStrv extra_identities,
    X509 *cert)
{
  gboolean tried = FALSE;

  if (peer_name != NULL)
    {
      if (check_peer_name (peer_name, cert))
        return TRUE;

      tried = TRUE;
    }

  if (extra_identities != NULL)
    {
      gint i;

      for (i = 0; extra_identities[i] != NULL; i++)
        {
          if (wocky_strdiff (extra_identities[i], peer_name))
            {
              if (check_peer_name (extra_identities[i], cert))
                return TRUE;

              tried = TRUE;
            }
        }
    }

  /* If no peer names were passed it means we didn't want to check the
   * certificate against anything.
   * If some attempts were made then it means the check failed. */
  return !tried;
}

GPtrArray *
wocky_tls_session_get_peers_certificate (WockyTLSSession *session,
    WockyTLSCertType *type)
{
  STACK_OF(X509) *cert_chain = NULL;
  guint cls = 0;
  GPtrArray *certificates;

  certificates =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);

  cert_chain = SSL_get_peer_cert_chain (session->ssl);

  if (cert_chain == NULL)
    return NULL;

  if (type != NULL)
    *type = WOCKY_TLS_CERT_TYPE_X509;

  cls = sk_X509_num (cert_chain);

  for (guint i = 0; i < cls; i++)
    {
      GArray *certificate;
      X509 *peer;
      gint peer_len;
      guchar *peer_buffer;

      peer = sk_X509_value (cert_chain, i);
      peer_len = i2d_X509 (peer, NULL);

      certificate = g_array_sized_new (TRUE, TRUE, sizeof (guchar), peer_len);

      peer_buffer = g_malloc (peer_len);
      i2d_X509 (peer, &peer_buffer);
      peer_buffer -= peer_len;

      g_array_append_vals (certificate, peer_buffer, peer_len);
      g_ptr_array_add (certificates, certificate);

      g_free (peer_buffer);
    }

  return certificates;
}

static WockyTLSCertStatus
_cert_status (WockyTLSSession *session,
         int ssl_code,
         WockyTLSVerificationLevel level,
         int old_code)
{
  switch (ssl_code)
    {
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
      return WOCKY_TLS_CERT_SIGNER_UNKNOWN;
      break;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_INVALID_PURPOSE:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      return WOCKY_TLS_CERT_INVALID;
      break;
    case X509_V_ERR_CERT_REVOKED:
      return WOCKY_TLS_CERT_REVOKED;
      break;
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_CERT_NOT_YET_VALID:
      return WOCKY_TLS_CERT_NOT_ACTIVE;
      break;
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_CERT_HAS_EXPIRED:
      return WOCKY_TLS_CERT_EXPIRED;
      break;
    case X509_V_ERR_OUT_OF_MEM:
      return WOCKY_TLS_CERT_INTERNAL_ERROR;
      break;
    case X509_V_ERR_INVALID_CA:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_AKID_SKID_MISMATCH:
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
      return WOCKY_TLS_CERT_SIGNER_UNAUTHORISED;
      break;
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
      return WOCKY_TLS_CERT_MAYBE_DOS;
      break;
    case X509_V_ERR_UNABLE_TO_GET_CRL:
      /* if we are in STRICT mode, being unable to see the CRL is a
       * terminal condition: in NORMAL or LENIENT we can live with it.
       * Also, if we re-tried and got the same error, we're just going
       * to loop indefinitely, so bail out with the original error.
       * NOTE: 'unable to fetch' a CRL is not the same as CRL invalidated
       * the certificate, or we'd just turn the CRL checks off when in
       * NORMAL or LENIENT mode */
      if (level == WOCKY_TLS_VERIFY_STRICT ||
          old_code == X509_V_ERR_UNABLE_TO_GET_CRL)
        {
          return WOCKY_TLS_CERT_INSECURE;
        }
      else
        {
          WockyTLSCertStatus status = WOCKY_TLS_CERT_OK;
          X509_STORE_CTX *xctx = X509_STORE_CTX_new();
          X509_STORE *store = SSL_CTX_get_cert_store(session->ctx);
          X509 *cert = SSL_get_peer_certificate (session->ssl);
          STACK_OF(X509) *chain = SSL_get_peer_cert_chain (session->ssl);
          long old_flags = store->param->flags;
          long new_flags = old_flags;
          DEBUG("No CRL available, but not in strict mode - re-verifying");

          new_flags &= ~(X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);

          store->param->flags = new_flags;
          X509_STORE_CTX_init (xctx, store, cert, chain);
          X509_STORE_CTX_set_flags (xctx, new_flags);

          if( X509_verify_cert (xctx) < 1 )
            {
              int new_code = X509_STORE_CTX_get_error (xctx);
              status = _cert_status (session, new_code, level, ssl_code);
            }

          store->param->flags = old_flags;
          X509_STORE_CTX_free (xctx);
          X509_free (cert);

          return status;
        }
      break;
    default:
      return WOCKY_TLS_CERT_UNKNOWN_ERROR;
    }
}

int
wocky_tls_session_verify_peer (WockyTLSSession    *session,
                               const gchar        *peername,
                               GStrv               extra_identities,
                               WockyTLSVerificationLevel level,
                               WockyTLSCertStatus *status)
{
  int rval = -1;
  X509 *cert;
  gboolean lenient = (level == WOCKY_TLS_VERIFY_LENIENT);

  DEBUG ("");
  g_assert (status != NULL);
  *status = WOCKY_TLS_CERT_OK;

  switch (level)
    {
    case WOCKY_TLS_VERIFY_STRICT:
    case WOCKY_TLS_VERIFY_NORMAL:
    case WOCKY_TLS_VERIFY_LENIENT:
      break;
    default:
      g_warn_if_reached ();
      level = WOCKY_TLS_VERIFY_STRICT;
    }

  DEBUG ("setting ssl verify flags level to: %s",
      wocky_enum_to_nick (WOCKY_TYPE_TLS_VERIFICATION_LEVEL, level));
  cert = SSL_get_peer_certificate (session->ssl);
  rval = SSL_get_verify_result (session->ssl);
  DEBUG ("X509 cert: %p; verified: %d", cert, rval);

  /* If no certificate is presented, SSL_get_verify_result() always returns
   * X509_V_OK. This is listed as a bug in `man 3 SSL_get_verify_result`. To
   * future-proof against that bug being fixed, we don't assume that behaviour.
   */
  if (cert == NULL)
    {
      if (lenient)
        {
          *status = WOCKY_TLS_CERT_OK;
          return X509_V_OK;
        }
      else if (rval == X509_V_OK)
        {
          DEBUG ("Anonymous SSL handshake");
          rval = X509_V_ERR_CERT_UNTRUSTED;
        }
    }
  else if (!check_peer_names (peername, extra_identities, cert))
    {
      /* Irrespective of whether the certificate is valid, if it's for the
       * wrong host that's arguably a more useful error condition to report.
       */
      *status = WOCKY_TLS_CERT_NAME_MISMATCH;
      return X509_V_ERR_APPLICATION_VERIFICATION;
    }

  if (rval != X509_V_OK)
    {
      DEBUG ("cert verification error: %d", rval);
      *status = _cert_status (session, rval, level, X509_V_OK);

      /* some conditions are to be ignored when lenient, others still matter */
      if (lenient)
        switch (*status)
          {
          case WOCKY_TLS_CERT_INTERNAL_ERROR:
          case WOCKY_TLS_CERT_REVOKED:
          case WOCKY_TLS_CERT_MAYBE_DOS:
            DEBUG ("this error matters, even though we're in lenient mode");
            break;
          default:
            DEBUG ("ignoring errors: we're in lenient mode");
            rval = X509_V_OK;
            *status = WOCKY_TLS_CERT_OK;
          }
    }

  return rval;
}

static gssize
wocky_tls_input_stream_read (GInputStream  *stream,
                             void          *buffer,
                             gsize          count,
                             GCancellable  *cancellable,
                             GError       **error)
{
  /* WockyTLSSession *session = WOCKY_TLS_INPUT_STREAM (stream)->session; */

  DEBUG ("sync read - not implmented");
  g_assert_not_reached ();

  return 0;
}

static void
wocky_tls_input_stream_read_async (GInputStream        *stream,
                                   void                *buffer,
                                   gsize                count,
                                   gint                 io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  WockyTLSSession *session = WOCKY_TLS_INPUT_STREAM (stream)->session;
  int ret;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  g_assert (session->job.read.active == FALSE);

  /* It is possible for a complete SSL record to be present in the read BIO *
   * already as a result of a previous read, since SSL_read may extract     *
   * just the first complete record, or some or all of them:                *
   * as a result, we may not want to issue an actual read request as the    *
   * data we are expecting may already have been read, causing us to wait   *
   * until the next block of data arrives over the network (which may not   *
   * ever happen): short-circuit the actual read if this is the case:       */
  ret = SSL_read (session->ssl, buffer, count);

  if (ssl_read_is_complete (session, ret))
    {
      GSimpleAsyncResult *r;

      if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
        DEBUG ("already have %d clearbytes buffered", ret);

      r = g_simple_async_result_new (G_OBJECT (stream),
                                     callback,
                                     user_data,
                                     wocky_tls_input_stream_read_async);

      if (session->job.read.error == NULL)
        g_simple_async_result_set_op_res_gssize (r, ret);
      else
        g_simple_async_result_set_from_error (r, session->job.read.error);

      g_simple_async_result_complete_in_idle (r);
      g_object_unref (r);
      return;
    }

  wocky_tls_job_start (&session->job.read, stream,
                       io_priority, cancellable, callback, user_data,
                       wocky_tls_input_stream_read_async);

  session->job.read.buffer = buffer;
  session->job.read.count = count;
  ssl_fill (session);
}

static gssize
wocky_tls_input_stream_read_finish (GInputStream  *stream,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (stream), wocky_tls_input_stream_read_async), -1);

  if (g_simple_async_result_propagate_error (simple, error))
    return -1;

  return g_simple_async_result_get_op_res_gssize (simple);
}

static gssize
wocky_tls_output_stream_write (GOutputStream  *stream,
                               const void     *buffer,
                               gsize           count,
                               GCancellable   *cancellable,
                               GError        **error)
{
  /* WockyTLSSession *session = WOCKY_TLS_OUTPUT_STREAM (stream)->session; */

  DEBUG ("sync write - not implemented");

  g_assert_not_reached ();

  return 0;
}

static void
wocky_tls_output_stream_write_async (GOutputStream       *stream,
                                     const void          *buffer,
                                     gsize                count,
                                     gint                 io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  int code;
  WockyTLSSession *session = WOCKY_TLS_OUTPUT_STREAM (stream)->session;

  DEBUG ("%" G_GSIZE_FORMAT " clearbytes to send", count);
  wocky_tls_job_start (&session->job.write, stream,
                       io_priority, cancellable, callback, user_data,
                       wocky_tls_output_stream_write_async);

  session->job.write.count = count;

  code = SSL_write (session->ssl, buffer, count);
  if (code < 0)
    {
      int error = SSL_get_error (session->ssl, code);
      switch (error)
        {
        case SSL_ERROR_WANT_WRITE:
          DEBUG ("Incomplete SSL write to BIO (theoretically impossible)");
          ssl_flush (session);
          return;
        case SSL_ERROR_WANT_READ:
          g_warning ("write caused read: unsupported TLS re-negotiation?");
          /* deliberately falling through to the default case, having logged a
           * more specific warning.
           */
        default:
          DEBUG ("SSL write failed, setting error %d", error);
          /* if we haven't already generated an error, set one here: */
          if(session->job.write.error == NULL)
            session->job.write.error =
              g_error_new (WOCKY_TLS_ERROR, error,
                           "OpenSSL write: protocol error %d", error);
          wocky_tls_session_try_operation (session, WOCKY_TLS_OP_WRITE);
          return;
        }
    }
  ssl_flush (session);
}

static gssize
wocky_tls_output_stream_write_finish (GOutputStream   *stream,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");
  {
    GObject *source_object;

    source_object = g_async_result_get_source_object (result);
    g_object_unref (source_object);
    g_return_val_if_fail (G_OBJECT (stream) == source_object, -1);
  }

  g_return_val_if_fail (wocky_tls_output_stream_write_async ==
                        g_simple_async_result_get_source_tag (simple), -1);

  if (g_simple_async_result_propagate_error (simple, error))
    return -1;

  return g_simple_async_result_get_op_res_gssize (simple);
}

static void
wocky_tls_output_stream_init (WockyTLSOutputStream *stream)
{
}

static void
wocky_tls_input_stream_init (WockyTLSInputStream *stream)
{
}

static void
wocky_tls_output_stream_set_property (GObject *object, guint prop_id,
                                      const GValue *value, GParamSpec *pspec)
{
  WockyTLSOutputStream *stream = WOCKY_TLS_OUTPUT_STREAM (object);

  switch (prop_id)
    {
     case PROP_C_SESSION:
      stream->session = g_value_dup_object (value);
      break;

     default:
      g_assert_not_reached ();
    }
}

static void
wocky_tls_output_stream_constructed (GObject *object)
{
  WockyTLSOutputStream *stream = WOCKY_TLS_OUTPUT_STREAM (object);

  g_assert (stream->session);
}

static void
wocky_tls_output_stream_finalize (GObject *object)
{
  WockyTLSOutputStream *stream = WOCKY_TLS_OUTPUT_STREAM (object);

  g_object_unref (stream->session);

  G_OBJECT_CLASS (wocky_tls_output_stream_parent_class)
    ->finalize (object);
}

static void
wocky_tls_output_stream_class_init (GOutputStreamClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  class->write_fn = wocky_tls_output_stream_write;
  class->write_async = wocky_tls_output_stream_write_async;
  class->write_finish = wocky_tls_output_stream_write_finish;
  obj_class->set_property = wocky_tls_output_stream_set_property;
  obj_class->constructed = wocky_tls_output_stream_constructed;
  obj_class->finalize = wocky_tls_output_stream_finalize;

  g_object_class_install_property (obj_class, PROP_O_SESSION,
    g_param_spec_object ("session", "TLS session",
                         "the TLS session object for this stream",
                         WOCKY_TYPE_TLS_SESSION, G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                         G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
wocky_tls_input_stream_set_property (GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec)
{
  WockyTLSInputStream *stream = WOCKY_TLS_INPUT_STREAM (object);

  switch (prop_id)
    {
     case PROP_C_SESSION:
      stream->session = g_value_dup_object (value);
      break;

     default:
      g_assert_not_reached ();
    }
}

static void
wocky_tls_input_stream_constructed (GObject *object)
{
  WockyTLSInputStream *stream = WOCKY_TLS_INPUT_STREAM (object);

  g_assert (stream->session);
}

static void
wocky_tls_input_stream_finalize (GObject *object)
{
  WockyTLSInputStream *stream = WOCKY_TLS_INPUT_STREAM (object);

  g_object_unref (stream->session);

  G_OBJECT_CLASS (wocky_tls_input_stream_parent_class)
    ->finalize (object);
}

static void
wocky_tls_input_stream_class_init (GInputStreamClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  class->read_fn = wocky_tls_input_stream_read;
  class->read_async = wocky_tls_input_stream_read_async;
  class->read_finish = wocky_tls_input_stream_read_finish;
  obj_class->set_property = wocky_tls_input_stream_set_property;
  obj_class->constructed = wocky_tls_input_stream_constructed;
  obj_class->finalize = wocky_tls_input_stream_finalize;

  g_object_class_install_property (obj_class, PROP_I_SESSION,
    g_param_spec_object ("session", "TLS session",
                         "the TLS session object for this stream",
                         WOCKY_TYPE_TLS_SESSION, G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                         G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
wocky_tls_connection_init (WockyTLSConnection *connection)
{
}

static void
wocky_tls_session_read_ready (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (user_data);
  GInputStream *input = G_INPUT_STREAM (object);
  GError **error = &(session->job.read.error);
  gssize rsize = 0;
  gchar *buf = session->job.handshake.job.active ?
    session->job.handshake.job.rbuf : session->job.read.rbuf;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  rsize = g_input_stream_read_finish (input, result, error);

  if (rsize > 0)
    {
      int x;
      int y;
      DEBUG ("received %" G_GSSIZE_FORMAT " cipherbytes, filling SSL BIO",
          rsize);
      BIO_write (session->rbio, buf, rsize);
      if (tls_debug_level > DEBUG_ASYNC_DETAIL_LEVEL + 1)
        for (x = 0; x < rsize; x += 16)
          {
            for (y = 0; y < 16 && x + y < rsize; y++)
              {
                char c = *(buf + x + y);
                char d = (g_ascii_isprint (c) && g_ascii_isgraph (c)) ? c : '.';
                fprintf (stderr, "%02x %c ", c & 0xff, d);
              }
            fprintf (stderr, "\n");
          }
    }
  /* note that we never issue a read of 0, so this _must_ be EOF (0) *
   * or a fatal error (-ve rval)                                     */
  else if (session->job.handshake.job.active)
    {
      if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
        DEBUG("read SSL cipherbytes (handshake) failed: %" G_GSSIZE_FORMAT,
            rsize);
      session->job.handshake.state = SSL_ERROR_SSL;
    }
  else
    {
      DEBUG ("read of SSL cipherbytes failed: %" G_GSSIZE_FORMAT, rsize);

      if ((*error != NULL) && ((*error)->domain == g_io_error_quark ()))
        {
          /* if there were any errors we could ignore, we'd do it like this: *
           * g_error_free (*error); *error = NULL;                           */
          DEBUG ("failed op: [%d] %s", (*error)->code, (*error)->message);
        }
      /* in order for non-handshake reads to return an error properly *
       * we need to make sure the error in the job is set             */
      else if (*error == NULL)
        {
          *error =
            g_error_new (WOCKY_TLS_ERROR, SSL_ERROR_SSL, "unknown error");
        }
    }

  wocky_tls_session_try_operation (session, WOCKY_TLS_OP_READ);
}

static void
wocky_tls_session_write_ready (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (user_data);
  gint buffered = BIO_pending (session->wbio);
  gssize written;

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    DEBUG ("");

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (object), result,
                                          &(session->job.write.error));

  if (written == buffered)
    {
      DEBUG ("%d bytes written, clearing write BIO", buffered);
      (void) BIO_reset (session->wbio);
      wocky_tls_session_try_operation (session, WOCKY_TLS_OP_WRITE);
    }
  else
    {
      gchar *buffer;
      long bsize = BIO_get_mem_data (session->wbio, &buffer);
      long psize = bsize - written;

      /* scrub the data we did manage to write from our buffer */
      if (written > 0)
        {
          gchar *pending = g_memdup (buffer + written, psize);

          (void) BIO_reset (session->wbio);
          (void) BIO_write (session->wbio, pending, psize);
          g_free (pending);
        }

      if (session->job.write.error != NULL)
        {
          if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
            DEBUG ("Incomplete async write [%" G_GSSIZE_FORMAT "/%d bytes]: "
                "%s:%u %s",
                written, buffered,
                g_quark_to_string (session->job.write.error->domain),
                session->job.write.error->code,
                session->job.write.error->message);

          /* if we have a  non-fatal error, erase it try again */
          if (g_error_matches (session->job.write.error,
                               G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
            g_clear_error (&(session->job.write.error));
        }

      /* no error here means retry the operation; otherwise bail out */
      if (session->job.write.error == NULL)
        ssl_flush (session);
      else
        wocky_tls_session_try_operation (session, WOCKY_TLS_OP_WRITE);
    }
}

static void
wocky_tls_session_init (WockyTLSSession *session)
{
  const char *level;
  guint lvl = 0;
  static gsize initialised;

  if G_UNLIKELY (g_once_init_enter (&initialised))
    {
      gint malloc_init_succeeded;

      DEBUG ("initialising SSL library and error strings");

      malloc_init_succeeded = CRYPTO_malloc_init ();
      g_warn_if_fail (malloc_init_succeeded);

      SSL_library_init ();
      SSL_load_error_strings ();
      OpenSSL_add_all_algorithms();
      ENGINE_load_builtin_engines ();
      g_once_init_leave (&initialised, 1);
    }

  if ((level = getenv ("WOCKY_TLS_DEBUG_LEVEL")) != NULL)
    lvl = atoi (level);

  tls_debug_level = lvl;
}

static void
wocky_tls_session_set_property (GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  switch (prop_id)
    {
     case PROP_S_STREAM:
      session->stream = g_value_dup_object (value);
      break;
    case PROP_S_SERVER:
      session->server = g_value_get_boolean (value);
      break;
    case PROP_S_DHBITS:
      session->dh_bits = g_value_get_uint (value);
      break;
    case PROP_S_KEYFILE:
      session->key_file = g_value_dup_string (value);
      break;
    case PROP_S_CERTFILE:
      session->cert_file = g_value_dup_string (value);
      break;
     default:
      g_assert_not_reached ();
    }
}

static void
set_dh_parameters (WockyTLSSession *session)
{
  DH *dh;

  switch (session->dh_bits)
    {
    case 4096:
      DEBUG ("get_dh4096");
      dh = get_dh4096 ();
      break;
    case 2048:
      DEBUG ("get_dh2048");
      dh = get_dh2048 ();
      break;
    case 1024:
      DEBUG ("get_dh1024");
      dh = get_dh1024 ();
      break;
    case 512:
      DEBUG ("get_dh512");
      dh = get_dh512 ();
      break;
    default:
      DEBUG ("Bad dh-bits setting: %d, reset to 1024", session->dh_bits);
      dh = get_dh1024 ();
    }

  SSL_CTX_set_tmp_dh (session->ctx, dh);
  DH_free (dh);
}

static void
set_ecdh_key (WockyTLSSession *session)
{
  EC_KEY *ecdh = EC_KEY_new_by_curve_name (NID_sect163r2);
  if (ecdh == NULL)
    {
      DEBUG ("unable to create elliptical crypto key for sect163r2 curve");
      return;
    }
  SSL_CTX_set_tmp_ecdh (session->ctx,ecdh);
  EC_KEY_free (ecdh);
}

static void
wocky_tls_session_constructed (GObject *object)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  if (session->server)
    {
      DEBUG ("I'm a server; using TLSv1_server_method");
      /* OpenSSL >= 1.0 returns a const here, but we need to be also   *
       * compatible with older versions that return a non-const value, *
       * hence the cast                                                */
      session->method = (SSL_METHOD *) TLSv1_server_method ();
    }
  else
    {
      DEBUG ("I'm a client; using TLSv1_client_method");
      session->method = (SSL_METHOD *) TLSv1_client_method ();
    }

  session->ctx = SSL_CTX_new (session->method);

  if (!SSL_CTX_set_default_verify_paths (session->ctx))
    g_warning ("SSL_CTX_set_default_verify_paths() failed");

  /* verification will be done manually after the handshake: */
  SSL_CTX_set_verify (session->ctx, SSL_VERIFY_NONE, NULL);
  SSL_CTX_set_options (session->ctx,
      SSL_OP_CIPHER_SERVER_PREFERENCE |
      /* It is usually safe to use SSL_OP_ALL to enable the bug workaround
       * options if compatibility with somewhat broken implementations is
       * desired.
       */
      SSL_OP_ALL |
      /* Set the NO_TICKET option on the context to be kind to the Google Talk
       * server, which seems unwilling to handle empty session tickets due to a
       * bug in Java.
       *
       * See http://twistedmatrix.com/trac/ticket/3463 and
       * http://loudmouth.lighthouseapp.com/projects/17276/tickets/28.
       */
      SSL_OP_NO_TICKET |
      /* SSLv2 is excessively quaint. We shouldn't be using it anyway, since
       * we're using TLSv1 methods, but...
       */
      SSL_OP_NO_SSLv2);
  X509_STORE_set_flags (SSL_CTX_get_cert_store (session->ctx),
                        X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);

#ifdef CIPHER_LIST
  SSL_CTX_set_cipher_list (session->ctx, CIPHER_LIST);
#endif

  if (session->server)
    {
      set_dh_parameters (session);
      set_ecdh_key (session);
    }

  if ((session->key_file != NULL) && (session->cert_file != NULL))
    {
      long errnum;
      DEBUG ("cert: %s", session->cert_file);
      DEBUG ("key : %s", session->key_file);
      SSL_CTX_use_certificate_file (session->ctx,
                                    session->cert_file,
                                    SSL_FILETYPE_PEM);
      SSL_CTX_use_PrivateKey_file (session->ctx,
                                   session->key_file,
                                   SSL_FILETYPE_PEM);
      if (!SSL_CTX_check_private_key (session->ctx))
        {
          errnum = ERR_get_error ();
          DEBUG ("cert/key check: %ld %s", errnum, error_to_string (errnum));
        }
      else
        DEBUG ("certificate loaded");
    }

  session->ssl = SSL_new (session->ctx);
  session->rbio = BIO_new (BIO_s_mem ());
  session->wbio = BIO_new (BIO_s_mem ());

  if (session->rbio == NULL)
    g_error ("Could not allocate memory BIO for SSL reads");

  if (session->wbio == NULL)
    g_error ("Could not allocate memory BIO for SSL writes");

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    {
      int x = 0;
      const char *c = SSL_get_cipher_list (session->ssl, x);
      for (; c != NULL; c = SSL_get_cipher_list (session->ssl, ++x))
        DEBUG ("%03d: %s", x, c);
    }

  if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
    {
      BIO_set_callback (session->rbio, BIO_debug_callback);
      BIO_set_callback (session->wbio, BIO_debug_callback);
    }

  BIO_set_mem_eof_return (session->rbio, -1);
  SSL_set_bio (session->ssl, session->rbio, session->wbio);

  DEBUG ("done");
}

static void
wocky_tls_session_finalize (GObject *object)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  /* the BIOs are freed by this call */
  SSL_free (session->ssl);
  /* free (session->method); handled by SSL_CTX_free */
  session->method = NULL;
  SSL_CTX_free (session->ctx);
  session->ctx = NULL;

  g_object_unref (session->stream);

  G_OBJECT_CLASS (wocky_tls_session_parent_class)->finalize (object);
}

static void
wocky_tls_session_dispose (GObject *object)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  g_free (session->key_file);
  session->key_file = NULL;

  g_free (session->cert_file);
  session->cert_file = NULL;

  G_OBJECT_CLASS (wocky_tls_session_parent_class)->dispose (object);
}

static void
wocky_tls_session_class_init (GObjectClass *class)
{
  class->set_property = wocky_tls_session_set_property;
  class->constructed = wocky_tls_session_constructed;
  class->finalize = wocky_tls_session_finalize;
  class->dispose = wocky_tls_session_dispose;

  g_object_class_install_property (class, PROP_S_STREAM,
    g_param_spec_object ("base-stream", "base stream",
                         "the stream that TLS communicates over",
                         G_TYPE_IO_STREAM, G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                         G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (class, PROP_S_SERVER,
    g_param_spec_boolean ("server", "server",
                          "whether this is a server",
                          FALSE, G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (class, PROP_S_DHBITS,
    g_param_spec_uint ("dh-bits", "Diffie-Hellman bits",
                       "Diffie-Hellmann bits: 512, 1024, 2048, or 4096",
                       512, 4096, 1024, G_PARAM_WRITABLE |
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                       G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (class, PROP_S_KEYFILE,
    g_param_spec_string ("x509-key", "x509 key",
                         "x509 PEM key file",
                         NULL, G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                         G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (class, PROP_S_CERTFILE,
    g_param_spec_string ("x509-cert", "x509 certificate",
                         "x509 PEM certificate file",
                         NULL, G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                         G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
wocky_tls_connection_set_property (GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
  WockyTLSConnection *connection = WOCKY_TLS_CONNECTION (object);

  switch (prop_id)
    {
     case PROP_C_SESSION:
      connection->session = g_value_dup_object (value);
      break;

     default:
      g_assert_not_reached ();
    }
}

static gboolean
wocky_tls_connection_close (GIOStream *stream, GCancellable *cancellable,
                            GError **error)
{
  WockyTLSConnection *connection = WOCKY_TLS_CONNECTION (stream);

  return g_io_stream_close (connection->session->stream, cancellable, error);
}

static GInputStream *
wocky_tls_connection_get_input_stream (GIOStream *io_stream)
{
  WockyTLSConnection *connection = WOCKY_TLS_CONNECTION (io_stream);

  if (connection->input == NULL)
    connection->input = g_object_new (WOCKY_TYPE_TLS_INPUT_STREAM,
                                      "session", connection->session,
                                      NULL);

  return (GInputStream *)connection->input;
}

static GOutputStream *
wocky_tls_connection_get_output_stream (GIOStream *io_stream)
{
  WockyTLSConnection *connection = WOCKY_TLS_CONNECTION (io_stream);

  if (connection->output == NULL)
    connection->output = g_object_new (WOCKY_TYPE_TLS_OUTPUT_STREAM,
                                       "session", connection->session,
                                       NULL);

  return (GOutputStream *)connection->output;
}

static void
wocky_tls_connection_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
  switch (prop_id)
    {
     default:
      g_assert_not_reached ();
    }
}

static void
wocky_tls_connection_constructed (GObject *object)
{
  WockyTLSConnection *connection = WOCKY_TLS_CONNECTION (object);

  g_assert (connection->session);
}

static void
wocky_tls_connection_finalize (GObject *object)
{
  WockyTLSConnection *connection = WOCKY_TLS_CONNECTION (object);

  g_object_unref (connection->session);

  if (connection->input != NULL)
    g_object_unref (connection->input);

  if (connection->output != NULL)
    g_object_unref (connection->output);

  G_OBJECT_CLASS (wocky_tls_connection_parent_class)
    ->finalize (object);
}

static void
wocky_tls_connection_class_init (WockyTLSConnectionClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (class);

  gobject_class->get_property = wocky_tls_connection_get_property;
  gobject_class->set_property = wocky_tls_connection_set_property;
  gobject_class->constructed = wocky_tls_connection_constructed;
  gobject_class->finalize = wocky_tls_connection_finalize;

  g_object_class_install_property (gobject_class, PROP_C_SESSION,
    g_param_spec_object ("session", "TLS session",
                         "the TLS session object for this connection",
                         WOCKY_TYPE_TLS_SESSION, G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  stream_class->get_input_stream = wocky_tls_connection_get_input_stream;
  stream_class->get_output_stream = wocky_tls_connection_get_output_stream;
  stream_class->close_fn = wocky_tls_connection_close;
}

WockyTLSSession *
wocky_tls_session_new (GIOStream *stream)
{
  return g_object_new (WOCKY_TYPE_TLS_SESSION,
                       "base-stream", stream,
                       "server", FALSE, NULL);
}

/**
 * wocky_tls_session_server_new:
 * @stream: a GIOStream on which we expect to receive the client TLS handshake
 * @dhbits: size of the DH parameters
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
  if (dhbits == 0)
    dhbits = 1024;
  return g_object_new (WOCKY_TYPE_TLS_SESSION, "base-stream", stream,
                       "dh-bits", dhbits, "x509-key", key, "x509-cert", cert,
                       "server", TRUE,
                       NULL);
}

/* this file is "borrowed" from an unmerged gnio feature: */
/* Local Variables:                                       */
/* c-file-style: "gnu"                                    */
/* End:                                                   */
