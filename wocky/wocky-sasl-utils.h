/*
 * wocky-sasl-utils.h - Some sasl helper functions
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

#ifndef __WOCKY_SASL_UTILS_H__
#define __WOCKY_SASL_UTILS_H__

#include <glib.h>

#define WOCKY_SHA1_BLOCK_SIZE 64
#define WOCKY_SHA1_DIGEST_SIZE 20

gchar * sasl_generate_base64_nonce (void);
GByteArray * sasl_calculate_hmac_sha1 (guint8 *key,
    gsize key_len,
    guint8 *text,
    gsize text_len);

#endif /* __WOCKY_SASL_UTILS_H__ */
