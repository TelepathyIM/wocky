/*
 * wocky-bare-contact.h - Header for WockyBareContact
 * Copyright (C) 2009 Collabora Ltd.
 * @author Jonny Lamb <jonny.lamb@collabora.co.uk>
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

#ifndef __WOCKY_BARE_CONTACT_H__
#define __WOCKY_BARE_CONTACT_H__

#include <glib-object.h>

#include "wocky-types.h"
#include "wocky-contact.h"
#include "wocky-roster.h"

G_BEGIN_DECLS

/**
 * WockyBareContactClass:
 *
 * The class of a #WockyBareContact.
 */
typedef struct _WockyBareContactClass WockyBareContactClass;
typedef struct _WockyBareContactPrivate WockyBareContactPrivate;

struct _WockyBareContactClass {
  /*<private>*/
  WockyContactClass parent_class;
};

struct _WockyBareContact {
  /*<private>*/
  WockyContact parent;

  WockyBareContactPrivate *priv;
};

GType wocky_bare_contact_get_type (void);

#define WOCKY_TYPE_BARE_CONTACT \
  (wocky_bare_contact_get_type ())
#define WOCKY_BARE_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_BARE_CONTACT, \
   WockyBareContact))
#define WOCKY_BARE_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_BARE_CONTACT, \
   WockyBareContactClass))
#define WOCKY_IS_BARE_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_BARE_CONTACT))
#define WOCKY_IS_BARE_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_BARE_CONTACT))
#define WOCKY_BARE_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_BARE_CONTACT, \
   WockyBareContactClass))

WockyBareContact * wocky_bare_contact_new (const gchar *jid);

const gchar *wocky_bare_contact_get_jid (WockyBareContact *contact);

const gchar *wocky_bare_contact_get_name (WockyBareContact *contact);

void wocky_bare_contact_set_name (WockyBareContact *contact, const gchar *name);

WockyRosterSubscriptionFlags wocky_bare_contact_get_subscription (
    WockyBareContact *contact);

void wocky_bare_contact_set_subscription (WockyBareContact *contact,
    WockyRosterSubscriptionFlags subscription);

const gchar * const *wocky_bare_contact_get_groups (WockyBareContact *contact);

void wocky_bare_contact_set_groups (WockyBareContact *contact, gchar **groups);

gboolean wocky_bare_contact_equal (WockyBareContact *a,
    WockyBareContact *b);

void wocky_bare_contact_add_group (WockyBareContact *contact,
    const gchar *group);

gboolean wocky_bare_contact_in_group (WockyBareContact *contact,
    const gchar *group);

void wocky_bare_contact_remove_group (WockyBareContact *contact,
    const gchar *group);

WockyBareContact * wocky_bare_contact_copy (WockyBareContact *contact);

void wocky_bare_contact_debug_print (WockyBareContact *contact);


void wocky_bare_contact_add_resource (WockyBareContact *contact,
    WockyResourceContact *resource);


GSList * wocky_bare_contact_get_resources (WockyBareContact *contact);

G_END_DECLS

#endif /* #ifndef __WOCKY_BARE_CONTACT_H__*/
