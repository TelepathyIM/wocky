/*
 * wocky-disco-identity.h —  utility API representing a Disco Identity
 * Copyright © 2010 Collabora Ltd.
 * Copyright © 2010 Nokia Corporation
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

#ifndef __WOCKY_PLUGINS_DISCO_IDENTITY_H__
#define __WOCKY_PLUGINS_DISCO_IDENTITY_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _WockyDiscoIdentity WockyDiscoIdentity;

struct _WockyDiscoIdentity
{
  gchar *category;
  gchar *type;
  gchar *lang;
  gchar *name;
};

WockyDiscoIdentity *wocky_disco_identity_new (const gchar *category,
    const gchar *type, const gchar *lang, const gchar *name);

WockyDiscoIdentity *wocky_disco_identity_copy (
    const WockyDiscoIdentity *source);

const gchar * wocky_disco_identity_get_category (WockyDiscoIdentity *identity);
const gchar * wocky_disco_identity_get_type (WockyDiscoIdentity *identity);
const gchar * wocky_disco_identity_get_lang (WockyDiscoIdentity *identity);
const gchar * wocky_disco_identity_get_name (WockyDiscoIdentity *identity);

void wocky_disco_identity_free (WockyDiscoIdentity *identity);

/* array of WockyDiscoIdentity helper methods */
GPtrArray * wocky_disco_identity_array_new (void);
GPtrArray * wocky_disco_identity_array_copy (const GPtrArray *source);
void wocky_disco_identity_array_free (GPtrArray *arr);

G_END_DECLS

#endif
