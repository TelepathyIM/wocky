/*
 * wocky-contact.h - Header for WockyContact
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

#ifndef __WOCKY_CONTACT_H__
#define __WOCKY_CONTACT_H__

#include <glib-object.h>

#include "wocky-types.h"
#include "wocky-roster.h"

G_BEGIN_DECLS

typedef struct _WockyContactClass WockyContactClass;

struct _WockyContactClass {
  GObjectClass parent_class;
};

struct _WockyContact {
  GObject parent;
};

GType wocky_contact_get_type (void);

#define WOCKY_TYPE_CONTACT \
  (wocky_contact_get_type ())
#define WOCKY_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_CONTACT, \
   WockyContact))
#define WOCKY_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_CONTACT, \
   WockyContactClass))
#define WOCKY_IS_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_CONTACT))
#define WOCKY_IS_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_CONTACT))
#define WOCKY_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_CONTACT, \
   WockyContactClass))

const gchar *wocky_contact_get_jid (WockyContact *contact);

const gchar *wocky_contact_get_name (WockyContact *contact);

void wocky_contact_set_name (WockyContact *contact, const gchar *name);

WockyRosterSubscriptionFlags wocky_contact_get_subscription (
    WockyContact *contact);

void wocky_contact_set_subscription (WockyContact *contact,
    WockyRosterSubscriptionFlags subscription);

const gchar * const *wocky_contact_get_groups (WockyContact *contact);

void wocky_contact_set_groups (WockyContact *contact, gchar **groups);

gboolean wocky_contact_equal (WockyContact *a,
    WockyContact *b);

void wocky_contact_add_group (WockyContact *contact,
    const gchar *group);

G_END_DECLS

#endif /* #ifndef __WOCKY_CONTACT_H__*/
