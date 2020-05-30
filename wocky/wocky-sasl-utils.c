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
sasl_calculate_hmac (GChecksumType digest_type,
    guint8 *key,
    gsize key_len,
    guint8 *text,
    gsize text_len)
{
  GHmac *hmac = g_hmac_new (digest_type, key, key_len);
  gsize len = g_checksum_type_get_length (digest_type);
  guint8 *digest = g_new (guint8, len);

  g_hmac_update (hmac, text, text_len);
  g_hmac_get_digest (hmac, digest, &len);

  g_hmac_unref (hmac);
  return g_byte_array_new_take (digest, len);
}

