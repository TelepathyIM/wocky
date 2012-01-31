/*
 * wocky-disco-identity.h — utility API representing a Disco Identity
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_DISCO_IDENTITY_H__
#define __WOCKY_DISCO_IDENTITY_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _WockyDiscoIdentity WockyDiscoIdentity;

/**
 * WockyDiscoIdentity:
 * @category: the identity category
 * @type: the identity type
 * @lang: the identity language
 * @name: the identity name
 *
 * A structure used to hold information regarding an identity from a
 * disco reply as described in XEP-0030.
 */
struct _WockyDiscoIdentity
{
  gchar *category;
  gchar *type;
  gchar *lang;
  gchar *name;
};

#define WOCKY_TYPE_DISCO_IDENTITY (wocky_disco_identity_get_type ())
GType wocky_disco_identity_get_type (void);

WockyDiscoIdentity *wocky_disco_identity_new (const gchar *category,
    const gchar *type, const gchar *lang, const gchar *name)
    G_GNUC_WARN_UNUSED_RESULT;

WockyDiscoIdentity *wocky_disco_identity_copy (
    const WockyDiscoIdentity *source) G_GNUC_WARN_UNUSED_RESULT;

void wocky_disco_identity_free (WockyDiscoIdentity *identity);

gint wocky_disco_identity_cmp (WockyDiscoIdentity *left, WockyDiscoIdentity *right);

/* array of WockyDiscoIdentity helper methods */
GPtrArray * wocky_disco_identity_array_new (void) G_GNUC_WARN_UNUSED_RESULT;
GPtrArray * wocky_disco_identity_array_copy (const GPtrArray *source)
    G_GNUC_WARN_UNUSED_RESULT;
void wocky_disco_identity_array_free (GPtrArray *arr);

G_END_DECLS

#endif /* #ifndef __WOCKY_DISCO_IDENTITY_H__ */
