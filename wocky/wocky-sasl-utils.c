/*
 * wocky-sasl-utils.c - Some sasl helper functions
 * Copyright (C) 2006-2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#include <string.h>
#include "wocky-sasl-utils.h"

/* Generate a good random nonce encoded with base64 such that it falls in the
 * allowable alphabet of various crypto mechanism. */
gchar *
sasl_generate_base64_nonce (void)
{
  /* RFC 2831 recommends the the nonce to be either hexadecimal or base64 with
   * at least 64 bits of entropy */
#define NR 8
  guint32 n[NR];
  int i;

  for (i = 0; i < NR; i++)
    n[i] = g_random_int ();

  return g_base64_encode ((guchar *) n, sizeof (n));
}

GByteArray *
sasl_calculate_hmac_sha1 (guint8 *key,
    gsize key_len,
    guint8 *text,
    gsize text_len)
{
/* Calculate the HMAC keyed hash algorithm as defined in RFC2104, using
 * SHA-1 as the hash algorithm */
  GChecksum *checksum;
  guint8 k_ipad[WOCKY_SHA1_BLOCK_SIZE];
  guint8 k_opad[WOCKY_SHA1_BLOCK_SIZE];
  guint8 inner_checksum[WOCKY_SHA1_DIGEST_SIZE];
  GByteArray *result;
  gsize len = WOCKY_SHA1_DIGEST_SIZE, i;

  memset (k_ipad, 0x36, WOCKY_SHA1_BLOCK_SIZE);
  memset (k_opad, 0x5c, WOCKY_SHA1_BLOCK_SIZE);

  if (key_len > WOCKY_SHA1_BLOCK_SIZE)
    {
      guchar k[WOCKY_SHA1_DIGEST_SIZE];

      checksum = g_checksum_new (G_CHECKSUM_SHA1);
      g_checksum_update (checksum, key, key_len);
      g_checksum_get_digest (checksum, k, &len);
      g_checksum_free (checksum);

      for (i = 0; i < WOCKY_SHA1_DIGEST_SIZE; i++)
        {
          k_ipad[i] ^= k[i];
          k_opad[i] ^= k[i];
        }
    }
  else
    {
      for (i = 0; i < key_len; i++)
        {
          k_ipad[i] ^= key[i];
          k_opad[i] ^= key[i];
        }
    }

  /* inner checksum */
  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  g_checksum_update (checksum, k_ipad, WOCKY_SHA1_BLOCK_SIZE);
  g_checksum_update (checksum, text, text_len);
  g_checksum_get_digest (checksum, inner_checksum, &len);
  g_checksum_free (checksum);

  /* outer checksum */
  result = g_byte_array_new ();
  g_byte_array_set_size (result, WOCKY_SHA1_DIGEST_SIZE);

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  g_checksum_update (checksum, k_opad, WOCKY_SHA1_BLOCK_SIZE);
  g_checksum_update (checksum, inner_checksum, WOCKY_SHA1_DIGEST_SIZE);
  g_checksum_get_digest (checksum, result->data, &len);
  g_checksum_free (checksum);

  return result;
}

