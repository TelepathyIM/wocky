/*
 * Wocky TLS integration - common stuff for GNUTLS and OpenSSL backends
 * (This would be called wocky-tls.c, but that's the GNUTLS backend for
 * historical reasons.)
 *
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 * Copyright © 2008-2009 Codethink Limited
 * Copyright © 2009-2012 Collabora Limited
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
 * Based on an unmerged gnio feature. See wocky-tls.c for details.
 */

#include "config.h"

#include "wocky-tls.h"

GQuark
wocky_tls_cert_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-tls-cert-error");

  return quark;
}

GQuark
wocky_tls_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-tls-error");

  return quark;
}

/* this file is "borrowed" from an unmerged gnio feature: */
/* Local Variables:                                       */
/* c-file-style: "gnu"                                    */
/* End:                                                   */
