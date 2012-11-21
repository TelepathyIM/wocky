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

#include <gnutls/x509.h>
#include <gnutls/openpgp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

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

#define VERIFY_STRICT  GNUTLS_VERIFY_DO_NOT_ALLOW_SAME
#define VERIFY_NORMAL  GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT
#define VERIFY_LENIENT ( GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT     | \
                         GNUTLS_VERIFY_ALLOW_ANY_X509_V1_CA_CRT | \
                         GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD2       | \
                         GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD5       | \
                         GNUTLS_VERIFY_DISABLE_TIME_CHECKS      | \
                         GNUTLS_VERIFY_DISABLE_CA_SIGN          )

#include "wocky-debug-internal.h"
#include "wocky-utils.h"

#include <gnutls/gnutls.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
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
} WockyTLSJob;

typedef enum
{
  WOCKY_TLS_OP_READ,
  WOCKY_TLS_OP_WRITE
} WockyTLSOperation;

typedef enum
{
  WOCKY_TLS_OP_STATE_IDLE,
  WOCKY_TLS_OP_STATE_ACTIVE,
  WOCKY_TLS_OP_STATE_DONE
} WockyTLSOpState;

typedef struct
{
  WockyTLSJob job;
} WockyTLSJobHandshake;

typedef struct
{
  WockyTLSJob job;

  gconstpointer buffer;
  gsize count;
} WockyTLSJobWrite;

typedef struct
{
  WockyTLSJob job;

  gpointer buffer;
  gsize count;
} WockyTLSJobRead;

typedef struct
{
  WockyTLSOpState state;

  guint8 *buffer;
  gssize requested;
  gssize result;
  GError *error;
} WockyTLSOp;

typedef GIOStreamClass WockyTLSConnectionClass;
typedef GObjectClass WockyTLSSessionClass;
typedef GInputStreamClass WockyTLSInputStreamClass;
typedef GOutputStreamClass WockyTLSOutputStreamClass;

static gnutls_dh_params_t dh_0768 = NULL;
static gnutls_dh_params_t dh_1024 = NULL;
static gnutls_dh_params_t dh_2048 = NULL;
static gnutls_dh_params_t dh_3072 = NULL;
static gnutls_dh_params_t dh_4096 = NULL;

struct _WockyTLSSession
{
  GObject parent;

  GIOStream *stream;
  GCancellable *cancellable;
  GError *error;
  gboolean async;

  /* tls server support */
  gboolean server;
  gnutls_dh_params_t dh_params;
  guint dh_bits;
  gchar *key_file;
  gchar *cert_file;

  /* frontend jobs */
  WockyTLSJobHandshake handshake_job;
  WockyTLSJobRead      read_job;
  WockyTLSJobWrite     write_job;

  /* backend jobs */
  WockyTLSOp           read_op;
  WockyTLSOp           write_op;

  gnutls_session_t session;

  gnutls_certificate_credentials gnutls_cert_cred;
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

static const gchar *hdesc_to_string (long desc)
{
#define HDESC(x) case GNUTLS_HANDSHAKE_##x: return #x; break;
  switch (desc)
    {
      HDESC (HELLO_REQUEST);
      HDESC (CLIENT_HELLO);
      HDESC (SERVER_HELLO);
      HDESC (CERTIFICATE_PKT);
      HDESC (SERVER_KEY_EXCHANGE);
      HDESC (CERTIFICATE_REQUEST);
      HDESC (SERVER_HELLO_DONE);
      HDESC (CERTIFICATE_VERIFY);
      HDESC (CLIENT_KEY_EXCHANGE);
      HDESC (FINISHED);
      HDESC (SUPPLEMENTAL);
    }
  return "Unknown State";
}

static const gchar *error_to_string (long error)
{
  const gchar *result;

  result = gnutls_strerror_name (error);
  if (result != NULL)
    return result;

  return "Unknown Error";
}

static gboolean
wocky_tls_set_error (GError **error,
                     gssize   result)
{
  int code = (int) result;

  if (result < 0)
    g_set_error (error, WOCKY_TLS_ERROR, 0, "%d: %s",
        code, error_to_string (code));

  return result < 0;
}

static GSimpleAsyncResult *
wocky_tls_job_make_result (WockyTLSJob *job,
                           gssize   result)
{
  if (result != GNUTLS_E_AGAIN)
    {
      GSimpleAsyncResult *simple;
      GError *error = NULL;

      simple = g_simple_async_result_new (job->source_object,
                                          job->callback,
                                          job->user_data,
                                          job->source_tag);

      if (job->error != NULL)
        {
#ifdef WOCKY_TLS_STRICT_ERROR_ASSERTIONS
          g_assert (result == GNUTLS_E_PUSH_ERROR ||
                    result == GNUTLS_E_PULL_ERROR);
#endif
          g_simple_async_result_set_from_error (simple, job->error);
          g_error_free (job->error);
        }
      else if (wocky_tls_set_error (&error, result))
        {
          g_simple_async_result_set_from_error (simple, error);
          g_error_free (error);
        }

      if (job->cancellable != NULL)
        g_object_unref (job->cancellable);
      job->cancellable = NULL;

      g_object_unref (job->source_object);
      job->source_object = NULL;

      job->active = FALSE;

      return simple;
    }
  else
    {
      g_assert (job->active);
      return NULL;
    }
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

static void
wocky_tls_session_try_operation (WockyTLSSession   *session,
                                 WockyTLSOperation  operation)
{
  if (session->handshake_job.job.active)
    {
      gint result;
      DEBUG ("session %p: async job handshake", session);
      session->async = TRUE;
      result = gnutls_handshake (session->session);
      g_assert (result != GNUTLS_E_INTERRUPTED);

      if (tls_debug_level >= DEBUG_HANDSHAKE_LEVEL)
        {
          gnutls_handshake_description_t i;
          gnutls_handshake_description_t o;

          DEBUG ("session %p: async job handshake: %d %s", session,
              result, error_to_string(result));
          i = gnutls_handshake_get_last_in (session->session);
          o = gnutls_handshake_get_last_out (session->session);
          DEBUG ("session %p: async job handshake: { in: %s; out: %s }",
              session, hdesc_to_string (i), hdesc_to_string (o));
        }

      session->async = FALSE;

      wocky_tls_job_result_boolean (&session->handshake_job.job, result);
    }

  else if (operation == WOCKY_TLS_OP_READ)
    {
      gssize result = 0;

      if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
        DEBUG ("async job OP_READ");
      g_assert (session->read_job.job.active);

      /* If the read result is 0, the remote end disconnected us, no need to
       * pull data through gnutls_record_recv in that case */

      if (session->read_op.result != 0)
        {

          session->async = TRUE;
          result = gnutls_record_recv (session->session,
              session->read_job.buffer,
              session->read_job.count);
          g_assert (result != GNUTLS_E_INTERRUPTED);
          session->async = FALSE;
        }

      wocky_tls_job_result_gssize (&session->read_job.job, result);
    }

  else
    {
      gssize result;
      if (tls_debug_level >= DEBUG_ASYNC_DETAIL_LEVEL)
        DEBUG ("async job OP_WRITE: %"G_GSIZE_FORMAT,
          session->write_job.count);
      g_assert (operation == WOCKY_TLS_OP_WRITE);
      g_assert (session->write_job.job.active);


      session->async = TRUE;
      result = gnutls_record_send (session->session,
                                   session->write_job.buffer,
                                   session->write_job.count);
      g_assert (result != GNUTLS_E_INTERRUPTED);
      session->async = FALSE;

      wocky_tls_job_result_gssize (&session->write_job.job, result);
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

WockyTLSConnection *
wocky_tls_session_handshake (WockyTLSSession   *session,
                             GCancellable  *cancellable,
                             GError       **error)
{
  gint result;

  DEBUG ("sync job handshake");
  session->error = NULL;
  session->cancellable = cancellable;
  result = gnutls_handshake (session->session);
  g_assert (result != GNUTLS_E_INTERRUPTED);
  g_assert (result != GNUTLS_E_AGAIN);
  session->cancellable = NULL;

  if (tls_debug_level >= DEBUG_HANDSHAKE_LEVEL)
    DEBUG ("sync job handshake: %d %s", result, error_to_string (result));

  if (session->error != NULL)
    {
      g_assert (result == GNUTLS_E_PULL_ERROR ||
                result == GNUTLS_E_PUSH_ERROR);

      g_propagate_error (error, session->error);
      return NULL;
    }
  else if (wocky_tls_set_error (error, result))
    return NULL;

  return g_object_new (WOCKY_TYPE_TLS_CONNECTION, "session", session, NULL);
}

/* ************************************************************************* */
/* adding CA certificates lists for peer certificate verification    */

void
wocky_tls_session_add_ca (WockyTLSSession *session,
                          const gchar *ca_path)
{
  int n = 0;
  struct stat target;

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
            n += gnutls_certificate_set_x509_trust_file (
                session->gnutls_cert_cred, path, GNUTLS_X509_FMT_PEM);

          g_free (path);
        }

      DEBUG ("+ %s: %d certs from dir", ca_path, n);
      closedir (dir);
    }
  else if (S_ISREG (target.st_mode))
    {
      n = gnutls_certificate_set_x509_trust_file (session->gnutls_cert_cred,
          ca_path, GNUTLS_X509_FMT_PEM);
      DEBUG ("+ %s: %d certs from file", ca_path, n);
    }
}

void
wocky_tls_session_add_crl (WockyTLSSession *session, const gchar *crl_path)
{
  int n = 0;
  struct stat target;

  DEBUG ("adding CRL CERT path '%s'", (gchar *) crl_path);

  if (stat (crl_path, &target) != 0)
    {
      DEBUG ("CRL file '%s': stat failed)", crl_path);
      return;
    }

  if (S_ISDIR (target.st_mode))
    {
      DIR *dir;
      struct dirent *entry;

      if ((dir = opendir (crl_path)) == NULL)
        return;

      for (entry = readdir (dir); entry != NULL; entry = readdir (dir))
        {
          struct stat file;
          gchar *path = g_build_path ("/", crl_path, entry->d_name, NULL);

          if ((stat (path, &file) == 0) && S_ISREG (file.st_mode))
            {
              int x = gnutls_certificate_set_x509_crl_file (
                session->gnutls_cert_cred, path, GNUTLS_X509_FMT_PEM);

              if (x < 0)
                DEBUG ("Error loading %s: %d %s", path, x, gnutls_strerror (x));
              else
                n += x;
            }

          g_free (path);
        }

      DEBUG ("+ %s: %d certs from dir", crl_path, n);
      closedir (dir);
    }
  else if (S_ISREG (target.st_mode))
    {
      n = gnutls_certificate_set_x509_trust_file (session->gnutls_cert_cred,
          crl_path, GNUTLS_X509_FMT_PEM);

      if (n < 0)
        DEBUG ("Error loading '%s': %d %s", crl_path, n, gnutls_strerror(n));
      else
        DEBUG ("+ %s: %d certs from file", crl_path, n);
    }
}

/* ************************************************************************* */

void
wocky_tls_session_handshake_async (WockyTLSSession         *session,
                                   gint                 io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  wocky_tls_job_start (&session->handshake_job.job, session,
                       io_priority, cancellable, callback, user_data,
                       wocky_tls_session_handshake_async);
  wocky_tls_session_try_operation (session, 0);
}

WockyTLSConnection *
wocky_tls_session_handshake_finish (WockyTLSSession   *session,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

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

GPtrArray *
wocky_tls_session_get_peers_certificate (WockyTLSSession *session,
    WockyTLSCertType *type)
{
  guint idx;
  guint n_peers;
  const gnutls_datum_t *peers = NULL;
  GPtrArray *certificates;

  peers = gnutls_certificate_get_peers (session->session, &n_peers);

  if (peers == NULL)
    return NULL;

  certificates =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);

  for (idx = 0; idx < n_peers; idx++)
    {
      GArray *cert = g_array_sized_new (TRUE, TRUE, sizeof (guchar),
          peers[idx].size);
      g_array_append_vals (cert, peers[idx].data, peers[idx].size);
      g_ptr_array_add (certificates, cert);
    }

  if (type != NULL)
    {
      switch (gnutls_certificate_type_get (session->session))
        {
        case GNUTLS_CRT_X509:
          *type = WOCKY_TLS_CERT_TYPE_X509;
          break;
        case GNUTLS_CRT_OPENPGP:
          *type = WOCKY_TLS_CERT_TYPE_OPENPGP;
          break;
        default:
          *type = WOCKY_TLS_CERT_TYPE_NONE;
          break;
        }
    }

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
                               const gchar        *peername,
                               GStrv               extra_identities,
                               WockyTLSVerificationLevel level,
                               WockyTLSCertStatus *status)
{
  int rval = -1;
  guint peer_cert_status = 0;
  gboolean peer_name_ok = TRUE;
  gnutls_certificate_verify_flags check;

  /* list gnutls cert error conditions in descending order of noteworthiness *
   * and map them to wocky cert error conditions                             */
  static const struct
  {
    gnutls_certificate_status_t gnutls;
    WockyTLSCertStatus wocky;
  } status_map[] =
    { { GNUTLS_CERT_REVOKED,            WOCKY_TLS_CERT_REVOKED             },
      { GNUTLS_CERT_NOT_ACTIVATED,      WOCKY_TLS_CERT_NOT_ACTIVE          },
      { GNUTLS_CERT_EXPIRED,            WOCKY_TLS_CERT_EXPIRED             },
      { GNUTLS_CERT_SIGNER_NOT_FOUND,   WOCKY_TLS_CERT_SIGNER_UNKNOWN      },
      { GNUTLS_CERT_SIGNER_NOT_CA,      WOCKY_TLS_CERT_SIGNER_UNAUTHORISED },
      { GNUTLS_CERT_INSECURE_ALGORITHM, WOCKY_TLS_CERT_INSECURE            },
      { GNUTLS_CERT_INVALID,            WOCKY_TLS_CERT_INVALID             },
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

  /* if we get this far, we have a structurally valid certificate *
   * signed by _someone_: check the hostname matches the peername */
  if (peername != NULL || extra_identities != NULL)
    {
      const gnutls_datum_t *peers;
      guint n_peers;
      gnutls_x509_crt_t x509;
      gnutls_openpgp_crt_t opgp;

      /* we know these ops must succeed, or verify_peers2 would have *
       * failed before we got here: We just need to duplicate a bit  *
       * of what it does:                                            */
      peers = gnutls_certificate_get_peers (session->session, &n_peers);

      switch (gnutls_certificate_type_get (session->session))
        {
        case GNUTLS_CRT_X509:
          DEBUG ("checking X509 cert");
          if ((rval = gnutls_x509_crt_init (&x509)) == GNUTLS_E_SUCCESS)
            {
              gnutls_x509_crt_import (x509, &peers[0], GNUTLS_X509_FMT_DER);

              if (peername != NULL)
                {
                  if (!cert_names_are_valid (x509))
                    {
                      rval = 0;
                    }
                  else
                    {
                      rval = gnutls_x509_crt_check_hostname (x509, peername);
                      DEBUG ("gnutls_x509_crt_check_hostname: %s -> %d",
                             peername, rval);
                    }
                }
              else
                {
                  rval = 0;
                }

              if (rval == 0 && extra_identities != NULL)
                {
                  if (!cert_names_are_valid (x509))
                    {
                      rval = 0;
                    }
                  else
                    {
                      gint i;

                      for (i = 0; extra_identities[i] != NULL; i++)
                        {
                          rval = gnutls_x509_crt_check_hostname (x509,
                            extra_identities[i]);
                          DEBUG ("gnutls_x509_crt_check_hostname: %s -> %d",
                            extra_identities[i], rval);

                          if (rval != 0)
                            break;
                        }
                    }
                }

              rval = (rval == 0) ? -1 : GNUTLS_E_SUCCESS;

              gnutls_x509_crt_deinit (x509);
            }
          break;
        case GNUTLS_CRT_OPENPGP:
          DEBUG ("checking PGP cert");
          if ((rval = gnutls_openpgp_crt_init (&opgp)) == GNUTLS_E_SUCCESS)
            {
              gnutls_openpgp_crt_import (opgp, &peers[0], GNUTLS_OPENPGP_FMT_RAW);
              rval = gnutls_openpgp_crt_check_hostname (opgp, peername);
              DEBUG ("gnutls_openpgp_crt_check_hostname: %s -> %d", peername, rval);

              if (peername != NULL)
                {
                  rval = gnutls_openpgp_crt_check_hostname (opgp, peername);
                  DEBUG ("gnutls_openpgp_crt_check_hostname: %s -> %d",
                      peername, rval);
                }
              else
                {
                  rval = 0;
                }

              if (rval == 0 && extra_identities != NULL)
                {
                  gint i;

                  for (i = 0; extra_identities[i] != NULL; i++)
                    {
                      rval = gnutls_openpgp_crt_check_hostname (opgp,
                          extra_identities[i]);
                      DEBUG ("gnutls_openpgp_crt_check_hostname: %s -> %d",
                          extra_identities[i], rval);

                      if (rval != 0)
                        break;
                    }
                }

              rval = (rval == 0) ? -1 : GNUTLS_E_SUCCESS;

              gnutls_openpgp_crt_deinit (opgp);
            }
          break;
        default:
          /* theoretically, this can't happen if ...verify_peers2 is working: */
          DEBUG ("unknown cert type!");
          rval = GNUTLS_E_INVALID_REQUEST;
        }

      peer_name_ok = (rval == GNUTLS_E_SUCCESS);
    }

  DEBUG ("peer_name_ok: %d", peer_name_ok );

  /* if the hostname didn't match, we can just bail out with an error here *
   * otherwise we need to figure out which error (if any) our verification *
   * call failed with:                                                     */
  if (!peer_name_ok)
    *status = WOCKY_TLS_CERT_NAME_MISMATCH;
  else
    { /* Gnutls cert checking can return multiple errors bitwise &ed together *
       * but we are realy only interested in the "most important" error:      */
      int x;
      *status = WOCKY_TLS_CERT_OK;
      for (x = 0; status_map[x].gnutls != 0; x++)
        {
          DEBUG ("checking gnutls error %d", status_map[x].gnutls);
          if (peer_cert_status & status_map[x].gnutls)
            {
              DEBUG ("gnutls error %d set", status_map[x].gnutls);
              *status = status_map[x].wocky;
              rval = GNUTLS_E_CERTIFICATE_ERROR;
              break;
            }
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
  WockyTLSSession *session = WOCKY_TLS_INPUT_STREAM (stream)->session;
  gssize result;

  session->cancellable = cancellable;
  result = gnutls_record_recv (session->session, buffer, count);
  g_assert (result != GNUTLS_E_INTERRUPTED);
  g_assert (result != GNUTLS_E_AGAIN);
  session->cancellable = NULL;

  if (session->error != NULL)
    {
      g_assert (result == GNUTLS_E_PULL_ERROR);
      g_propagate_error (error, session->error);
      return -1;
    }
  else if (wocky_tls_set_error (error, result))
    return -1;

  return result;
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

  wocky_tls_job_start (&session->read_job.job, stream,
                       io_priority, cancellable, callback, user_data,
                       wocky_tls_input_stream_read_async);

  session->read_job.buffer = buffer;
  session->read_job.count = count;

  wocky_tls_session_try_operation (session, WOCKY_TLS_OP_READ);
}

static gssize
wocky_tls_input_stream_read_finish (GInputStream  *stream,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  {
    GObject *source_object;

    source_object = g_async_result_get_source_object (result);
    g_object_unref (source_object);
    g_return_val_if_fail (G_OBJECT (stream) == source_object, -1);
  }

  g_return_val_if_fail (wocky_tls_input_stream_read_async ==
                        g_simple_async_result_get_source_tag (simple), -1);

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
  WockyTLSSession *session = WOCKY_TLS_OUTPUT_STREAM (stream)->session;
  gssize result;

  session->cancellable = cancellable;
  result = gnutls_record_send (session->session, buffer, count);
  g_assert (result != GNUTLS_E_INTERRUPTED);
  g_assert (result != GNUTLS_E_AGAIN);
  session->cancellable = NULL;

  if (session->error != NULL)
    {
      g_assert (result == GNUTLS_E_PUSH_ERROR);
      g_propagate_error (error, session->error);
      return -1;
    }
  else if (wocky_tls_set_error (error, result))
    return -1;

  return result;
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
  WockyTLSSession *session = WOCKY_TLS_OUTPUT_STREAM (stream)->session;

  wocky_tls_job_start (&session->write_job.job, stream,
                   io_priority, cancellable, callback, user_data,
                   wocky_tls_output_stream_write_async);

  session->write_job.buffer = buffer;
  session->write_job.count = count;

  wocky_tls_session_try_operation (session, WOCKY_TLS_OP_WRITE);
}

static gssize
wocky_tls_output_stream_write_finish (GOutputStream   *stream,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

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

  g_assert (session->read_op.state == WOCKY_TLS_OP_STATE_ACTIVE);

  session->read_op.result =
    g_input_stream_read_finish (G_INPUT_STREAM (object), result,
                                &session->read_op.error);
  session->read_op.state = WOCKY_TLS_OP_STATE_DONE;

  /* don't recurse if the async handler is already running */
  if (!session->async)
    wocky_tls_session_try_operation (session, WOCKY_TLS_OP_READ);
}

static void
wocky_tls_session_write_ready (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (user_data);
  gssize ret;

  g_assert (session->write_op.state == WOCKY_TLS_OP_STATE_ACTIVE);

  ret = g_output_stream_write_finish (G_OUTPUT_STREAM (object), result,
                                  &session->write_op.error);
  if (ret > 0)
    {
      session->write_op.result += ret;

      if (session->write_op.result < session->write_op.requested)
        {
          GOutputStream *stream;
          WockyTLSJob *active_job;

          stream = g_io_stream_get_output_stream (session->stream);

          if (session->handshake_job.job.active)
              active_job = &session->handshake_job.job;
          else
              active_job = &session->write_job.job;

          g_output_stream_write_async (stream,
            session->write_op.buffer + session->write_op.result,
            session->write_op.requested - session->write_op.result,
            active_job->io_priority,
            active_job->cancellable,
            wocky_tls_session_write_ready,
            session);
          return;
        }
    }
  else
    {
      /* Error or EOF, we're done */
      session->write_op.result = ret;
    }

  session->write_op.state = WOCKY_TLS_OP_STATE_DONE;

  /* don't recurse if the async handler is already running */
  if (!session->async)
    wocky_tls_session_try_operation (session, WOCKY_TLS_OP_WRITE);
}

static ssize_t
wocky_tls_session_push_func (gpointer    user_data,
                             const void *buffer,
                             size_t      count)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (user_data);
  GOutputStream *stream;

  stream = g_io_stream_get_output_stream (session->stream);

  if (session->async)
    {
      WockyTLSJob *active_job;

      g_assert (session->handshake_job.job.active ||
                session->write_job.job.active);

      if (session->handshake_job.job.active)
        active_job = &session->handshake_job.job;
      else
        active_job = &session->write_job.job;

      g_assert (active_job->active);

      if (session->write_op.state == WOCKY_TLS_OP_STATE_IDLE)
        {
          session->write_op.state = WOCKY_TLS_OP_STATE_ACTIVE;
          session->write_op.buffer = g_memdup (buffer, count);
          session->write_op.requested = count;
          session->write_op.error = NULL;
          session->write_op.result = 0;

          g_output_stream_write_async (stream,
                                       session->write_op.buffer,
                                       session->write_op.requested,
                                       active_job->io_priority,
                                       active_job->cancellable,
                                       wocky_tls_session_write_ready,
                                       session);

          if G_UNLIKELY (session->write_op.state != WOCKY_TLS_OP_STATE_ACTIVE)
            g_warning ("The underlying stream '%s' used by the WockyTLSSession "
                       "called the GAsyncResultCallback recursively.  This "
                       "is an error in the underlying implementation: in "
                       "some cases it may lead to unbounded recursion.  "
                       "Result callbacks should always be dispatched from "
                       "the mainloop.",
                       G_OBJECT_TYPE_NAME (stream));
        }

      g_assert (session->write_op.state != WOCKY_TLS_OP_STATE_IDLE);
      g_assert_cmpint (session->write_op.requested, ==, count);
      g_assert (memcmp (session->write_op.buffer, buffer, count) == 0);

      if (session->write_op.state == WOCKY_TLS_OP_STATE_DONE)
        {
          session->write_op.state = WOCKY_TLS_OP_STATE_IDLE;
          g_free (session->write_op.buffer);

          if (session->write_op.result < 0)
            {
              active_job->error = session->write_op.error;
              gnutls_transport_set_errno (session->session, EIO);

              return -1;
            }
          else
            {
              g_assert_cmpint (session->write_op.result, <=, count);

              return session->write_op.result;
            }
        }

      gnutls_transport_set_errno (session->session, EAGAIN);

      return -1;
    }
  else
    {
      gssize result;

      result = g_output_stream_write (stream, buffer, count,
                                      session->cancellable,
                                      &session->error);

      if (result < 0)
        gnutls_transport_set_errno (session->session, EIO);

      return result;
    }
}

static ssize_t
wocky_tls_session_pull_func (gpointer  user_data,
                             void     *buffer,
                             size_t     count)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (user_data);
  GInputStream *stream;

  stream = g_io_stream_get_input_stream (session->stream);

  if (session->async)
    {
      WockyTLSJob *active_job;

      g_assert (session->handshake_job.job.active ||
                session->read_job.job.active);

      if (session->handshake_job.job.active)
        active_job = &session->handshake_job.job;
      else
        active_job = &session->read_job.job;

      g_assert (active_job->active);

      if (session->read_op.state == WOCKY_TLS_OP_STATE_IDLE)
        {
          session->read_op.state = WOCKY_TLS_OP_STATE_ACTIVE;
          session->read_op.buffer = g_malloc (count);
          session->read_op.requested = count;
          session->read_op.error = NULL;

          g_input_stream_read_async (stream,
                                     session->read_op.buffer,
                                     session->read_op.requested,
                                     active_job->io_priority,
                                     active_job->cancellable,
                                     wocky_tls_session_read_ready,
                                     session);

          if G_UNLIKELY (session->read_op.state != WOCKY_TLS_OP_STATE_ACTIVE)
            g_warning ("The underlying stream '%s' used by the WockyTLSSession "
                       "called the GAsyncResultCallback recursively.  This "
                       "is an error in the underlying implementation: in "
                       "some cases it may lead to unbounded recursion.  "
                       "Result callbacks should always be dispatched from "
                       "the mainloop.",
                       G_OBJECT_TYPE_NAME (stream));
        }

      g_assert (session->read_op.state != WOCKY_TLS_OP_STATE_IDLE);
      g_assert_cmpint (session->read_op.requested, ==, count);

      if (session->read_op.state == WOCKY_TLS_OP_STATE_DONE)
        {
          session->read_op.state = WOCKY_TLS_OP_STATE_IDLE;

          if (session->read_op.result < 0)
            {
              g_free (session->read_op.buffer);
              session->read_op.buffer = NULL;
              active_job->error = session->read_op.error;
              gnutls_transport_set_errno (session->session, EIO);

              return -1;
            }
          else
            {
              g_assert_cmpint (session->read_op.result, <=, count);

              memcpy (buffer,
                      session->read_op.buffer,
                      session->read_op.result);
              g_free (session->read_op.buffer);
              session->read_op.buffer = NULL;

              return session->read_op.result;
            }
        }

      gnutls_transport_set_errno (session->session, EAGAIN);

      return -1;
    }
  else
    {
      gssize result;

      result = g_input_stream_read (stream, buffer, count,
                                    session->cancellable,
                                    &session->error);
      if (result < 0)
        gnutls_transport_set_errno (session->session, EIO);

      return result;
    }
}

static void
tls_debug (int level,
    const char *msg)
{
  DEBUG ("[%d] [%02d] %s", getpid(), level, msg);
}

static void
wocky_tls_session_init (WockyTLSSession *session)
{
  const char *level;
  guint64 lvl = 0;
  static gsize initialised;

  if G_UNLIKELY (g_once_init_enter (&initialised))
    {
      gnutls_global_init ();
      gnutls_global_set_log_function (tls_debug);
      g_once_init_leave (&initialised, 1);
    }

  if ((level = g_getenv ("WOCKY_TLS_DEBUG_LEVEL")) != NULL)
    lvl = g_ascii_strtoull (level, NULL, 10);

  tls_debug_level = lvl;
  gnutls_global_set_log_level (lvl);
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

static const char *
tls_options (void)
{
  const char *options = g_getenv ("WOCKY_GNUTLS_OPTIONS");
  return (options != NULL && *options != '\0') ? options : DEFAULT_TLS_OPTIONS;
}

static void
wocky_tls_session_constructed (GObject *object)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  gboolean server = session->server;
  gint code;
  const gchar *opt = tls_options ();
  const gchar *pos = NULL;

  /* gnutls_handshake_set_private_extensions (session->session, 1); */
  gnutls_certificate_allocate_credentials (&(session->gnutls_cert_cred));

  /* I think this all needs to be done per connection: conceivably
     the DH parameters could be moved to the global section above,
     but IANA cryptographer */
  if (server)
    {
      gnutls_dh_params_t *dhp;

      if ((session->key_file != NULL) && (session->cert_file != NULL))
        {
          DEBUG ("cert/key pair: %s/%s", session->cert_file, session->key_file);
          gnutls_certificate_set_x509_key_file (session->gnutls_cert_cred,
                                                session->cert_file,
                                                session->key_file,
                                                GNUTLS_X509_FMT_PEM);
        }

      switch (session->dh_bits)
        {
        case 768:
          dhp = &dh_0768;
          break;
        case 1024:
          dhp = &dh_1024;
          break;
        case 2048:
          dhp = &dh_2048;
          break;
        case 3072:
          dhp = &dh_3072;
          break;
        case 4096:
          dhp = &dh_4096;
          break;
        default:
          dhp = &dh_1024;
          break;
        }

      if (*dhp == NULL)
        {
          DEBUG ("Initialising DH parameters (%d bits)", session->dh_bits);
          gnutls_dh_params_init (dhp);
          gnutls_dh_params_generate2 (*dhp, session->dh_bits);
        }

      session->dh_params = *dhp;
      gnutls_certificate_set_dh_params (session->gnutls_cert_cred, *dhp);
      gnutls_init (&session->session, GNUTLS_SERVER);
    }
  else
    gnutls_init (&session->session, GNUTLS_CLIENT);

  code = gnutls_priority_set_direct (session->session, opt, &pos);
  if (code != GNUTLS_E_SUCCESS)
    {
      DEBUG ("could not set priority string: %s", error_to_string (code));
      DEBUG ("    '%s'", opt);
      if (pos >= opt)
        DEBUG ("     %*s^", (int) (pos - opt), "");
    }
  else
    {
      DEBUG ("priority set to: '%s'", opt);
    }

  code = gnutls_credentials_set (session->session,
                                 GNUTLS_CRD_CERTIFICATE,
                                 session->gnutls_cert_cred);
  if (code != GNUTLS_E_SUCCESS)
    DEBUG ("could not set credentials: %s", error_to_string (code));

  gnutls_transport_set_push_function (session->session,
                                      wocky_tls_session_push_func);
  gnutls_transport_set_pull_function (session->session,
                                      wocky_tls_session_pull_func);
  gnutls_transport_set_ptr (session->session, session);

  g_assert (session->stream);
}

static void
wocky_tls_session_finalize (GObject *object)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  gnutls_deinit (session->session);
  gnutls_certificate_free_credentials (session->gnutls_cert_cred);
  g_object_unref (session->stream);

  G_OBJECT_CLASS (wocky_tls_session_parent_class)
    ->finalize (object);
}

static void
wocky_tls_session_dispose (GObject *object)
{
  WockyTLSSession *session = WOCKY_TLS_SESSION (object);

  g_free (session->key_file);
  session->key_file = NULL;

  g_free (session->cert_file);
  session->cert_file = NULL;

  g_free (session->read_op.buffer);
  session->read_op.buffer = NULL;

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
                       "Diffie-Hellmann bits: 768, 1024, 2048, 3072 0r 4096",
                       768, 4096, 1024, G_PARAM_WRITABLE |
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
wocky_tls_connection_close (GIOStream *stream,
  GCancellable *cancellable,
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
