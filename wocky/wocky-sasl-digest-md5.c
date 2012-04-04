#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-sasl-digest-md5.h"
#include "wocky-auth-registry.h"
#include "wocky-sasl-utils.h"

#include <string.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"

typedef enum {
  WOCKY_SASL_DIGEST_MD5_STATE_STARTED,
  WOCKY_SASL_DIGEST_MD5_STATE_SENT_AUTH_RESPONSE,
  WOCKY_SASL_DIGEST_MD5_STATE_SENT_FINAL_RESPONSE,
} WockySaslDigestMd5State;

static void
auth_handler_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (WockySaslDigestMd5, wocky_sasl_digest_md5,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (
        WOCKY_TYPE_AUTH_HANDLER, auth_handler_iface_init))

enum
{
  PROP_SERVER = 1,
  PROP_USERNAME,
  PROP_PASSWORD
};

struct _WockySaslDigestMd5Private
{
  WockySaslDigestMd5State state;
  gchar *username;
  gchar *password;
  gchar *server;
  gchar *digest_md5_rspauth;
};

static void
wocky_sasl_digest_md5_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  WockySaslDigestMd5 *self = (WockySaslDigestMd5 *) object;
  WockySaslDigestMd5Private *priv = self->priv;

  switch (property_id)
    {
      case PROP_USERNAME:
        g_value_set_string (value, priv->username);
        break;

      case PROP_PASSWORD:
        g_value_set_string (value, priv->password);
        break;

      case PROP_SERVER:
        g_value_set_string (value, priv->server);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_sasl_digest_md5_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  WockySaslDigestMd5 *self = (WockySaslDigestMd5 *) object;
  WockySaslDigestMd5Private *priv = self->priv;

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

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
wocky_sasl_digest_md5_dispose (GObject *object)
{
  WockySaslDigestMd5 *self = (WockySaslDigestMd5 *) object;
  WockySaslDigestMd5Private *priv = self->priv;

  g_free (priv->server);
  g_free (priv->username);
  g_free (priv->password);
  g_free (priv->digest_md5_rspauth);

  G_OBJECT_CLASS (wocky_sasl_digest_md5_parent_class)->dispose (object);
}

static void
wocky_sasl_digest_md5_class_init (
    WockySaslDigestMd5Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WockySaslDigestMd5Private));

  object_class->dispose = wocky_sasl_digest_md5_dispose;
  object_class->set_property = wocky_sasl_digest_md5_set_property;
  object_class->get_property = wocky_sasl_digest_md5_get_property;

  g_object_class_install_property (object_class, PROP_SERVER,
      g_param_spec_string ("server", "server",
          "The name of the server we're authenticating to", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_USERNAME,
      g_param_spec_string ("username", "username",
          "The username to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PASSWORD,
      g_param_spec_string ("password", "password",
          "The password to authenticate with", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static gboolean
digest_md5_handle_auth_data (WockyAuthHandler *handler,
    const GString *data, GString **response, GError **error);

static gboolean
digest_md5_handle_success (WockyAuthHandler *handler,
    GError **error);

static void
auth_handler_iface_init (gpointer g_iface)
{
  WockyAuthHandlerIface *iface = g_iface;

  iface->mechanism = WOCKY_AUTH_MECH_SASL_DIGEST_MD5;
  iface->plain = FALSE;
  iface->auth_data_func = digest_md5_handle_auth_data;
  iface->success_func = digest_md5_handle_success;
}

static void
wocky_sasl_digest_md5_init (WockySaslDigestMd5 *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      WOCKY_TYPE_SASL_DIGEST_MD5, WockySaslDigestMd5Private);
  self->priv->state = WOCKY_SASL_DIGEST_MD5_STATE_STARTED;
}

WockySaslDigestMd5 *
wocky_sasl_digest_md5_new (
    const gchar *server,
    const gchar *username,
    const gchar *password)
{
  return g_object_new (WOCKY_TYPE_SASL_DIGEST_MD5,
      "server", server,
      "username", username,
      "password", password,
      NULL);
}

static gchar *
strndup_unescaped (const gchar *str, gsize len)
{
  const gchar *s;
  gchar *d, *ret;

  ret = g_malloc0 (len + 1);
  for (s = str, d = ret ; s < (str + len) ; s++, d++) {
    if (*s == '\\')
      s++;
    *d = *s;
  }

  return ret;
}

static GHashTable *
digest_md5_challenge_to_hash (const GString * challenge)
{
  GHashTable *result = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_free);
  const gchar *keystart, *keyend, *valstart;
  const gchar *c = challenge->str;
  gchar *key, *val;

  do {
    keystart = c;
    for (; *c != '\0' && *c != '='; c++)
      ;

    if (*c == '\0' || c == keystart)
      goto error;

    keyend = c;
    c++;

    /* eat any whitespace between the '=' and the '"' */
    for (; g_ascii_isspace (*c); c++)
      ;

    if (*c == '"')
      {
        gboolean esc = FALSE;
        c++;
        valstart = c;

        /* " terminates a quoted value _unless_ we are in a \ escape */
        /* \0 always terminates (end of string encountered)          */
        for (; *c != '\0' && (esc || *c != '"'); c++)
          {
            if (esc)
              esc = FALSE;      /* we are in a \ char escape, finish it   */
            else
              esc = *c == '\\'; /* is this char \ (ie starting an escape) */
          }
        if (*c == '\0' || c == valstart)
          goto error;
        val = strndup_unescaped (valstart, c - valstart);
        c++;
      }
    else
      {
        valstart = c;
        for (; *c !=  '\0' && *c != ','; c++)
          ;
        if (c == valstart)
          goto error;
        val = g_strndup (valstart, c - valstart);
      }

    /* the key is unguarded by '"' delimiters so any whitespace *
     * at either end should be discarded as irrelevant          */
    key = g_strndup (keystart, keyend - keystart);
    key = g_strstrip (key);

    DEBUG ("challenge '%s' = '%s'", key, val);
    g_hash_table_insert (result, key, val);

    /* eat any whitespace between the '"' and the next ',' */
    for (; g_ascii_isspace (*c); c++)
      ;

    if (*c == ',')
      c++;
  } while (*c != '\0');

  return result;

error:
  DEBUG ("Failed to parse challenge: %s", challenge->str);
  g_hash_table_unref (result);
  return NULL;
}

static guint8 *
md5_hash (gchar *value)
{
  GChecksum *checksum;
  guint8 *result;
  gsize len;

  len = g_checksum_type_get_length (G_CHECKSUM_MD5);
  g_assert (len == 16);

  result = g_malloc (len);

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) value, -1);
  g_checksum_get_digest (checksum, result, &len);
  g_checksum_free (checksum);

  g_assert (len == 16);

  return result;
}

static gchar *
md5_hex_hash (gchar *value, gsize length)
{
  return g_compute_checksum_for_string (G_CHECKSUM_MD5, value, length);
}

static GString *
md5_prepare_response (WockySaslDigestMd5Private *priv, GHashTable *challenge,
    GError **error)
{
  GString *response = g_string_new ("");
  const gchar *realm, *nonce;
  gchar *a1, *a1h, *a2, *a2h, *kd, *kdh;
  gchar *cnonce = NULL;
  gchar *tmp;
  guint8 *digest_md5;
  gsize len;

  if (priv->username == NULL || priv->password == NULL)
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_NO_CREDENTIALS,
          "No username or password provided");
      goto error;
    }

  DEBUG ("Got username and password");

  nonce = g_hash_table_lookup (challenge, "nonce");
  if (nonce == NULL || nonce == '\0')
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server didn't provide a nonce in the challenge");
      goto error;
    }

  cnonce = sasl_generate_base64_nonce ();

  /* FIXME challenge can contain multiple realms */
  realm = g_hash_table_lookup (challenge, "realm");
  if (realm == NULL)
    {
      realm = priv->server;
    }

  /* FIXME properly escape values */
  g_string_append_printf (response, "username=\"%s\"", priv->username);
  g_string_append_printf (response, ",realm=\"%s\"", realm);
  g_string_append_printf (response, ",digest-uri=\"xmpp/%s\"", priv->server);
  g_string_append_printf (response, ",nonce=\"%s\",nc=00000001", nonce);
  g_string_append_printf (response, ",cnonce=\"%s\"", cnonce);
  /* FIXME should check if auth is in the cop challenge val */
  g_string_append_printf (response, ",qop=auth,charset=utf-8");

  tmp = g_strdup_printf ("%s:%s:%s", priv->username, realm, priv->password);
  digest_md5 = md5_hash (tmp);
  g_free (tmp);

  a1 = g_strdup_printf ("0123456789012345:%s:%s", nonce, cnonce);
  len = strlen (a1);
  /* MD5 hash is 16 bytes */
  memcpy (a1, digest_md5, 16);
  a1h = md5_hex_hash (a1, len);

  g_free (digest_md5);

  a2 = g_strdup_printf ("AUTHENTICATE:xmpp/%s", priv->server);
  a2h = md5_hex_hash (a2, -1);

  kd = g_strdup_printf ("%s:%s:00000001:%s:auth:%s", a1h, nonce, cnonce, a2h);
  kdh = md5_hex_hash (kd, -1);
  g_string_append_printf (response, ",response=%s", kdh);

  g_free (kd);
  g_free (kdh);
  g_free (a2);
  g_free (a2h);

  /* Calculate the response we expect from the server */
  a2 = g_strdup_printf (":xmpp/%s", priv->server);
  a2h = md5_hex_hash (a2, -1);

  kd =  g_strdup_printf ("%s:%s:00000001:%s:auth:%s", a1h, nonce, cnonce, a2h);
  g_free (priv->digest_md5_rspauth);
  priv->digest_md5_rspauth = md5_hex_hash (kd, -1);

  g_free (a1);
  g_free (a1h);
  g_free (a2);
  g_free (a2h);
  g_free (kd);

out:
  g_free (cnonce);

  return response;

error:
  g_string_free (response, TRUE);
  response = NULL;
  goto out;
}


static gboolean
digest_md5_make_initial_response (
    WockySaslDigestMd5Private *priv,
    GHashTable *challenge,
    GString **response,
    GError **error)
{
  g_return_val_if_fail (response != NULL, FALSE);

  *response = md5_prepare_response (priv, challenge, error);

  if (*response == NULL)
    return FALSE;

  DEBUG ("Prepared response: %s", (*response)->str);

  priv->state = WOCKY_SASL_DIGEST_MD5_STATE_SENT_AUTH_RESPONSE;

  return TRUE;
}

static gboolean
digest_md5_check_server_response (
    WockySaslDigestMd5Private *priv,
    GHashTable *challenge,
    GError **error)
{
  const gchar *rspauth;

  rspauth = g_hash_table_lookup (challenge, "rspauth");
  if (rspauth == NULL)
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server sent an invalid reply (no rspauth)");
      return FALSE;
    }

  if (strcmp (priv->digest_md5_rspauth, rspauth))
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server sent an invalid reply (rspauth not matching)");
      return FALSE;
    }

  priv->state = WOCKY_SASL_DIGEST_MD5_STATE_SENT_FINAL_RESPONSE;
  return TRUE;
}

static GHashTable *
auth_data_to_hash (const GString *challenge, GError **error)
{
  GHashTable *h = NULL;

  DEBUG ("Got digest-md5 challenge: %s", challenge->str);
  h = digest_md5_challenge_to_hash (challenge);

  if (h == NULL)
    g_set_error (error, WOCKY_AUTH_ERROR,
      WOCKY_AUTH_ERROR_INVALID_REPLY,
      "Server sent invalid auth data");

  return h;
}

static gboolean
digest_md5_handle_auth_data (WockyAuthHandler *handler,
    const GString *data,
    GString **response,
    GError **error)
{
  WockySaslDigestMd5 *self = WOCKY_SASL_DIGEST_MD5 (handler);
  WockySaslDigestMd5Private *priv = self->priv;
  GHashTable *h;
  gboolean ret = FALSE;

  if (data == NULL)
    {
      DEBUG ("Expected auth data but didn't get any!");
      g_set_error (error, WOCKY_AUTH_ERROR,
        WOCKY_AUTH_ERROR_INVALID_REPLY,
        "Expected auth data from the server, but didn't get any");
      return FALSE;
    }

  h = auth_data_to_hash (data, error);

  if (h == NULL)
    return FALSE;

  switch (priv->state) {
    case WOCKY_SASL_DIGEST_MD5_STATE_STARTED:
      ret = digest_md5_make_initial_response (priv, h, response, error);
      break;
    case WOCKY_SASL_DIGEST_MD5_STATE_SENT_AUTH_RESPONSE:
      ret = digest_md5_check_server_response (priv, h, error);
      break;
    default:
      g_set_error (error, WOCKY_AUTH_ERROR,
          WOCKY_AUTH_ERROR_INVALID_REPLY,
          "Server sent unexpected auth data");
  }
  g_hash_table_unref (h);

  return ret;
}

static gboolean
digest_md5_handle_success (WockyAuthHandler *handler,
    GError **error)
{
  WockySaslDigestMd5 *self = WOCKY_SASL_DIGEST_MD5 (handler);
  WockySaslDigestMd5Private *priv = self->priv;

  if (priv->state == WOCKY_SASL_DIGEST_MD5_STATE_SENT_FINAL_RESPONSE)
    return TRUE;

  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent success before finishing authentication");
  return FALSE;
}
