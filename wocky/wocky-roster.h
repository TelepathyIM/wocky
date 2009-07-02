/*
 * wocky-roster.h - Header for WockyRoster
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

#ifndef __WOCKY_ROSTER_H__
#define __WOCKY_ROSTER_H__

#include <glib-object.h>

#include "wocky-xmpp-connection.h"
#include "wocky-porter.h"

G_BEGIN_DECLS

typedef struct _WockyRoster WockyRoster;
typedef struct _WockyRosterClass WockyRosterClass;

GQuark wocky_roster_error_quark (void);

struct _WockyRosterClass {
  GObjectClass parent_class;
};

struct _WockyRoster {
  GObject parent;
};

GType wocky_roster_get_type (void);

#define WOCKY_TYPE_ROSTER \
  (wocky_roster_get_type ())
#define WOCKY_ROSTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_ROSTER, \
   WockyRoster))
#define WOCKY_ROSTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_ROSTER, \
   WockyRosterClass))
#define WOCKY_IS_ROSTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_ROSTER))
#define WOCKY_IS_ROSTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_ROSTER))
#define WOCKY_ROSTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_ROSTER, \
   WockyRosterClass))

WockyRoster * wocky_roster_new (WockyXmppConnection *conn,
    WockyPorter *porter);

void wocky_roster_fetch_roster (WockyRoster *self);

G_END_DECLS

#endif /* #ifndef __WOCKY_ROSTER_H__*/
