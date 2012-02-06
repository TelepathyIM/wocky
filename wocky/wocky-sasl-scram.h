/*
 * wocky-sasl-scram.h - SCRAM-SHA1 implementation (to be RFC 5802)
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef _WOCKY_SASL_SCRAM_H
#define _WOCKY_SASL_SCRAM_H

#include <glib-object.h>

#include "wocky-auth-handler.h"

G_BEGIN_DECLS

#define WOCKY_TYPE_SASL_SCRAM \
    wocky_sasl_scram_get_type ()

#define WOCKY_SASL_SCRAM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_SASL_SCRAM, \
        WockySaslScram))

#define WOCKY_SASL_SCRAM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_SASL_SCRAM, \
        WockySaslScramClass))

#define WOCKY_IS_SASL_SCRAM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_SASL_SCRAM))

#define WOCKY_IS_SASL_SCRAM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_SASL_SCRAM))

#define WOCKY_SASL_SCRAM_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_SASL_SCRAM, \
        WockySaslScramClass))

typedef struct _WockySaslScramPrivate WockySaslScramPrivate;

typedef struct
{
  GObject parent;
  WockySaslScramPrivate *priv;
} WockySaslScram;

typedef struct
{
  GObjectClass parent_class;
} WockySaslScramClass;

GType
wocky_sasl_scram_get_type (void);

WockySaslScram *
wocky_sasl_scram_new (
    const gchar *server, const gchar *username, const gchar *password);

G_END_DECLS

#endif /* _WOCKY_SASL_SCRAM_H */
