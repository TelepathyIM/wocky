/*
 * wocky-sasl-scram.c - SCRAM-SHA1 implementation (to be RFC 5802)
 * Copyright (C) 2010 Sjoerd Simons <sjoerd@luon.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-sasl-scram.h"
#include "wocky-sasl-auth.h"
#include "wocky-sasl-utils.h"
#include "wocky-utils.h"

#include <string.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_AUTH
#include "wocky-debug-internal.h"

typedef enum {
  WOCKY_SASL_SCRAM_STATE_STARTED,
  WOCKY_SASL_SCRAM_STATE_SERVER_FIRST_MESSAGE,
  WOCKY_SASL_SCRAM_STATE_SERVER_FINAL_MESSAGE,
  WOCKY_SASL_SCRAM_STATE_FINISHED,
} WockySaslScramState;

static void
sasl_handler_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (WockySaslScram, wocky_sasl_scram,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (
        WOCKY_TYPE_AUTH_HANDLER, sasl_handler_iface_init))

enum
{
  PROP_SERVER = 1,
  PROP_CB_TYPE,
  PROP_CB_DATA,
  PROP_HASH_ALGO,
  PROP_USERNAME,
  PROP_PASSWORD
};

struct _WockySaslScramPrivate
{
  WockySaslScramState state;
  WockyTLSBindingType cb_type;
  GChecksumType hash_algo;
  const gchar *gs2_flag;
  gchar *cb_data;
  gchar *username;
  gchar *password;
  gchar *server;

  gchar *client_nonce;
  gchar *nonce;
  gchar *salt;

  gchar *client_first_bare;
  gchar *server_first_bare;

  gchar *auth_message;

  guint64 iterations;

  GByteArray *salted_password;
};

static void
wocky_sasl_scram_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  WockySaslScram *self = (WockySaslScram *) object;
  WockySaslScramPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_HASH_ALGO:
        g_value_set_int (value, priv->hash_algo);
        break;

      case PROP_CB_TYPE:
        g_value_set_enum (value, priv->cb_type);
        break;

      case PROP_CB_DATA:
        g_value_set_string (value, priv->cb_data);
        break;

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
wocky_sasl_scram_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  WockySaslScram *self = (WockySaslScram *) object;
  WockySaslScramPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_HASH_ALGO:
        priv->hash_algo = g_value_get_int (value);
        break;

      case PROP_CB_TYPE:
        priv->cb_type = g_value_get_enum (value);
        break;

      case PROP_CB_DATA:
        g_free (priv->cb_data);
        priv->cb_data = g_value_dup_string (value);
        break;

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
wocky_sasl_scram_dispose (GObject *object)
{
  WockySaslScram *self = (WockySaslScram *) object;
  WockySaslScramPrivate *priv = self->priv;

  g_free (priv->server);
  g_free (priv->username);
  g_free (priv->password);
  g_free (priv->cb_data);

  g_free (priv->client_nonce);
  g_free (priv->nonce);
  g_free (priv->salt);

  g_free (priv->client_first_bare);
  g_free (priv->server_first_bare);

  g_free (priv->auth_message);

  if (priv->salted_password != NULL)
    g_byte_array_unref (priv->salted_password);

  G_OBJECT_CLASS (wocky_sasl_scram_parent_class)->dispose (object);
}

static void
wocky_sasl_scram_class_init (
    WockySaslScramClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WockySaslScramPrivate));

  object_class->dispose = wocky_sasl_scram_dispose;
  object_class->set_property = wocky_sasl_scram_set_property;
  object_class->get_property = wocky_sasl_scram_get_property;

  g_object_class_install_property (object_class, PROP_HASH_ALGO,
      g_param_spec_int ("hash-algo", "hash algorithm",
          "The type of the Hash Algorithm to use for HMAC from GChecksumType",
          G_CHECKSUM_SHA1, G_CHECKSUM_SHA512, G_CHECKSUM_SHA1,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_CB_TYPE,
      g_param_spec_enum ("cb-type", "binding type",
          "The type of the TLS Channel Binding to use in SASL negotiation",
          WOCKY_TYPE_TLS_BINDING_TYPE, WOCKY_TLS_BINDING_DISABLED,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_CB_DATA,
      g_param_spec_string ("cb-data", "binding data",
          "Base64 encoded TLS Channel binding data for the set type", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

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
scram_initial_response (WockyAuthHandler *handler,
    GString **response,
    GError **error);

static gboolean
scram_handle_auth_data (WockyAuthHandler *handler,
    const GString *data, GString **response, GError **error);

static gboolean
scram_handle_success (WockyAuthHandler *handler,
    GError **error);

static void
sasl_handler_iface_init (gpointer g_iface)
{
  WockyAuthHandlerIface *iface = g_iface;

  iface->mechanism = "SCRAM-SHA-1";
  iface->plain = FALSE;
  iface->initial_response_func = scram_initial_response;
  iface->auth_data_func = scram_handle_auth_data;
  iface->success_func = scram_handle_success;
}

static void
wocky_sasl_scram_init (WockySaslScram *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      WOCKY_TYPE_SASL_SCRAM, WockySaslScramPrivate);
  self->priv->state = WOCKY_SASL_SCRAM_STATE_STARTED;
}

WockySaslScram *
wocky_sasl_scram_new (
    const gchar *server,
    const gchar *username,
    const gchar *password)
{
  return g_object_new (WOCKY_TYPE_SASL_SCRAM,
      "server", server,
      "username", username,
      "password", password,
      NULL);
}

static gboolean
scram_initial_response (WockyAuthHandler *handler,
    GString **response,
    GError **error)
{
  WockySaslScram *self = WOCKY_SASL_SCRAM (handler);
  WockySaslScramPrivate *priv = self->priv;

  if (priv->username == NULL || priv->password == NULL)
    {
      g_set_error (error, WOCKY_AUTH_ERROR,
        WOCKY_AUTH_ERROR_NO_CREDENTIALS,
        "No username or password");

      return FALSE;
    }

  switch (priv->cb_type)
    {
      /* no client cb support, make sure we don't stuff cb_data in */
      case WOCKY_TLS_BINDING_DISABLED:
        priv->gs2_flag = "n,,";
        g_free (priv->cb_data);
        priv->cb_data = NULL;
        break;
      /* we support channel binding, let's inform the other side */
      case WOCKY_TLS_BINDING_NONE:
        /* no server support, wipe cb data, just in case */
        priv->gs2_flag = "y,,";
        g_free (priv->cb_data);
        priv->cb_data = NULL;
        break;
      case WOCKY_TLS_BINDING_TLS_UNIQUE:
        priv->gs2_flag = "p=tls-unique,,";
        g_assert (priv->cb_data != NULL);
        break;
      case WOCKY_TLS_BINDING_TLS_SERVER_END_POINT:
        priv->gs2_flag = "p=tls-server-end-point,,";
        g_assert (priv->cb_data != NULL);
        break;
      default:
        g_assert_not_reached ();
    }

  g_assert (priv->client_nonce == NULL);
  priv->client_nonce = sasl_generate_base64_nonce ();

  priv->client_first_bare = g_strdup_printf ("n=%s,r=%s",
    priv->username,
    priv->client_nonce);

  *response = g_string_new (priv->client_first_bare);
  g_string_prepend (*response, priv->gs2_flag);

  priv->state = WOCKY_SASL_SCRAM_STATE_SERVER_FIRST_MESSAGE;

  return TRUE;
}

static gboolean
scram_get_next_attr_value (gchar **message,
    gchar *attr,
    gchar **value)
{
  gchar *end;
  end = *message;

  /* Need to not be at the end of the string, have a = as the first character
   * after the attribute and a valid value character as the next one */
  if (end[0] == '\0' || end[1] != '=' || end[2] == '\0')
    return FALSE;

  *attr = end[0];
  end += 2;

  *value = end;

  for (; *end != '\0' && *end != ','; end++)
      /* pass */ ;

  if (*end != '\0')
    *message = end + 1;

  /* Set the end to \0 so we can just point value to the start of it */
  *end = '\0';

  return TRUE;
}

/* xor the result array with the in array */
static void
scram_xor_array (GByteArray *result, GByteArray *in)
{
  gsize i;
  g_assert (result->len == in->len);

  for (i = 0; i < result->len ; i++)
    result->data[i] ^=  in->data[i];
}

static void
scram_calculate_salted_password (WockySaslScram *self)
{
  WockySaslScramPrivate *priv = self->priv;
  guint64 i;
  GByteArray *result, *prev, *salt;
  guint8 one[] = {0,0,0,1};
  gint state = 0;
  guint save = 0;
  gsize len;
  gsize pass_len = strlen (priv->password);

  /* salt for U1 */
  salt = g_byte_array_new ();
  /* Make sure we have enough data for the decoding base 64 and add 4 extra
   * bytes for the calculation */
  g_byte_array_set_size (salt, (strlen (priv->salt)/4 * 3) + 3 + 4 );
  len = g_base64_decode_step (priv->salt, strlen (priv->salt),
    salt->data, &state, &save);
  g_byte_array_set_size (salt, len);
  g_byte_array_append (salt, one, sizeof (one));

  /* Calculate U1 */
  result = sasl_calculate_hmac (priv->hash_algo,
    (guint8 *) priv->password, pass_len,
    salt->data, salt->len);

  prev = g_byte_array_sized_new (result->len);
  g_byte_array_append (prev, result->data, result->len);

  /* Calculate U2 and onwards, while keeping a rolling result */
  for (i = 1; i < priv->iterations; i++)
    {
      GByteArray *U = sasl_calculate_hmac (priv->hash_algo,
        (guint8 *) priv->password,
        pass_len, prev->data, prev->len);

      g_byte_array_unref (prev);
      prev = U;

      scram_xor_array (result, U);
    }

  g_byte_array_unref (prev);
  g_byte_array_unref (salt);
  priv->salted_password = result;
}

static gchar *
scram_make_client_proof (WockySaslScram *self)
{
/* As per RFC
 * ClientProof     := ClientKey XOR ClientSignature
 * ClientSignature := HMAC(StoredKey, AuthMessage)
 * StoredKey       := H(ClientKey)
 * ClientKey       := HMAC(SaltedPassword, "Client Key")
 * SaltedPassword  := Hi(Normalize(password), salt, i)
 */
#define CLIENT_KEY_STR "Client Key"
  WockySaslScramPrivate *priv = self->priv;
  gchar *proof = NULL;
  GByteArray *client_key, *client_signature;
  gsize len = g_checksum_type_get_length (priv->hash_algo);
  guint8 *stored_key;
  GChecksum *checksum;

  stored_key = g_new (guint8, len);
  /* Calculate the salted password and save it for later as we need it to
   * verify the servers reply */
  scram_calculate_salted_password (self);

  client_key = sasl_calculate_hmac (priv->hash_algo,
      priv->salted_password->data,
      priv->salted_password->len, (guint8 *) CLIENT_KEY_STR,
      strlen (CLIENT_KEY_STR));

  checksum = g_checksum_new (priv->hash_algo);
  g_checksum_update (checksum, client_key->data, client_key->len);
  g_checksum_get_digest (checksum, stored_key, &len);
  g_checksum_free (checksum);

  DEBUG ("auth message: %s", priv->auth_message);

  client_signature = sasl_calculate_hmac (priv->hash_algo,
      stored_key, len,
      (guint8 *) priv->auth_message, strlen (priv->auth_message));

  /* xor signature and key, overwriting key */
  scram_xor_array (client_key, client_signature);

  proof = g_base64_encode (client_key->data, client_key->len);

  g_free (stored_key);
  g_byte_array_unref (client_key);
  g_byte_array_unref (client_signature);

  return proof;
#undef CLIENT_KEY_STR
}

static gboolean
scram_handle_server_first_message (WockySaslScram *self,
    gchar *message,
    GString **reply,
    GError **error)
{
  WockySaslScramPrivate *priv = self->priv;
  gchar attr, *value = NULL;
  gchar *proof = NULL;
  GByteArray *cb = NULL;
  gchar *cb_b64 = NULL;
  GString *client_reply;

  if (!scram_get_next_attr_value (&message, &attr, &value))
    goto invalid;

  /* Fail when getting an unknown mandatory extension (we don't know any) */
  if (attr == 'm')
    goto unknown_extension;

  /* get the nonce */
  if (attr != 'r')
    goto invalid;

  priv->nonce = g_strdup (value);
  if (strncmp (priv->client_nonce, priv->nonce,
      strlen (priv->client_nonce)) != 0)
    goto invalid_nonce;

  /* get the salt */
  if (!scram_get_next_attr_value (&message, &attr, &value) || attr != 's')
    goto invalid;

  priv->salt = g_strdup (value);

  /* get the number of iterations */
  if (!scram_get_next_attr_value (&message, &attr, &value) || attr != 'i')
    goto invalid;

  priv->iterations = g_ascii_strtoull (value, NULL, 10);
  if (priv->iterations == 0)
    goto invalid_iterations;

  /* We got everything we needed for our response without proof
   * base64("n,,") => biws */
  client_reply = g_string_new (NULL);
  if (priv->cb_data)
    {
      gsize len = 0;
      guchar *buf = g_base64_decode (priv->cb_data, &len);
      cb = g_byte_array_new_take (buf, len);
    }
  else
    cb = g_byte_array_new ();
  cb = g_byte_array_prepend (cb, (const guint8 *)priv->gs2_flag, strlen (priv->gs2_flag));
  cb_b64 = g_base64_encode (cb->data, cb->len);
  g_byte_array_unref (cb);
  g_string_append_printf (client_reply, "c=%s,r=%s", cb_b64, priv->nonce);
  g_free (cb_b64);

  /* So we can make the auth message */
  priv->auth_message = g_strdup_printf ("%s,%s,%s",
    priv->client_first_bare,
    priv->server_first_bare,
    client_reply->str);

  /* and prepare our proof */
  proof = scram_make_client_proof (self);
  g_string_append_printf (client_reply, ",p=%s", proof);
  g_free (proof);

  DEBUG ("Client reply: %s", client_reply->str);

  *reply = client_reply;

  return TRUE;

invalid_iterations:
  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent an invalid interation count");
  return FALSE;

invalid_nonce:
  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent an invalid invalid nonce value");
  return FALSE;

invalid:
  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent an invalid first reply");
  return FALSE;

unknown_extension:
  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent an unknown mandatory extension");
  return FALSE;
}

static gboolean
scram_check_server_verification (WockySaslScram *self,
  gchar *verification)
{
/*
 *    ServerSignature := HMAC(ServerKey, AuthMessage)
 *    ServerKey       := HMAC(SaltedPassword, "Server Key")
 */
#define SERVER_KEY_STR "Server Key"
  WockySaslScramPrivate *priv = self->priv;
  GByteArray *server_key;
  GByteArray *server_signature;
  gchar *v;
  gboolean ret;

  server_key = sasl_calculate_hmac (priv->hash_algo,
    priv->salted_password->data, priv->salted_password->len,
    (guint8 *) SERVER_KEY_STR, strlen (SERVER_KEY_STR));

  server_signature = sasl_calculate_hmac (priv->hash_algo, server_key->data,
    server_key->len, (guint8 *) priv->auth_message,
    strlen (priv->auth_message));

  v = g_base64_encode (server_signature->data, server_signature->len);

  ret = !wocky_strdiff (v, verification);

  if (!ret)
    DEBUG ("Unexpected verification: got %s, expected %s",
      verification,  v);


  g_byte_array_unref (server_key);
  g_byte_array_unref (server_signature);
  g_free (v);

  return ret;
#undef SERVER_KEY_STR
}


static gboolean
scram_handle_server_final_message (WockySaslScram *self,
    gchar *message,
    GError **error)
{
  gchar attr, *value = NULL;

  if (!scram_get_next_attr_value (&message, &attr, &value) || attr != 'v')
    goto invalid;

  if (!scram_check_server_verification (self, value))
    goto server_not_verified;

  return TRUE;

server_not_verified:
  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent an incorrect final reply");
  return FALSE;

invalid:
  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent an invalid final reply");
  return FALSE;
}

static gboolean
scram_handle_auth_data (WockyAuthHandler *handler,
    const GString *data,
    GString **response,
    GError **error)
{
  WockySaslScram *self = WOCKY_SASL_SCRAM (handler);
  WockySaslScramPrivate *priv = self->priv;
  gboolean ret = FALSE;

  DEBUG ("Got server message: %s", data->str);

  switch (priv->state)
    {
      case WOCKY_SASL_SCRAM_STATE_SERVER_FIRST_MESSAGE:
        priv->server_first_bare = g_strdup (data->str);

        if (!(ret = scram_handle_server_first_message (self, data->str,
            response, error)))
          goto out;

        priv->state = WOCKY_SASL_SCRAM_STATE_SERVER_FINAL_MESSAGE;
        break;
      case WOCKY_SASL_SCRAM_STATE_SERVER_FINAL_MESSAGE:
        if (!(ret = scram_handle_server_final_message (self, data->str,
            error)))
          goto out;
        priv->state = WOCKY_SASL_SCRAM_STATE_FINISHED;
        break;
      default:
        g_set_error (error, WOCKY_AUTH_ERROR,
        WOCKY_AUTH_ERROR_INVALID_REPLY,
        "Server sent an unexpected reply");
        goto out;
    }

out:
  return ret;
}

static gboolean
scram_handle_success (WockyAuthHandler *handler,
    GError **error)
{
  WockySaslScram *self = WOCKY_SASL_SCRAM (handler);
  WockySaslScramPrivate *priv = self->priv;

  if (priv->state == WOCKY_SASL_SCRAM_STATE_FINISHED)
    return TRUE;

  g_set_error (error, WOCKY_AUTH_ERROR,
    WOCKY_AUTH_ERROR_INVALID_REPLY,
    "Server sent success before finishing authentication");
  return FALSE;
}
