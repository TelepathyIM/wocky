/*
 * wocky-xep-0115-capabilities.h - interface for holding capabilities
 * of contacts
 *
 * Copyright (C) 2011 Collabora Ltd.
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

#ifndef __WOCKY_XEP_0115_CAPABILITIES_H__
#define __WOCKY_XEP_0115_CAPABILITIES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define WOCKY_TYPE_XEP_0115_CAPABILITIES \
  (wocky_xep_0115_capabilities_get_type ())

#define WOCKY_XEP_0115_CAPABILITIES(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  WOCKY_TYPE_XEP_0115_CAPABILITIES, WockyXep0115Capabilities))

#define WOCKY_IS_XEP_0115_CAPABILITIES(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  WOCKY_TYPE_XEP_0115_CAPABILITIES))

#define WOCKY_XEP_0115_CAPABILITIES_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  WOCKY_TYPE_XEP_0115_CAPABILITIES, WockyXep0115CapabilitiesInterface))

typedef struct _WockyXep0115Capabilities WockyXep0115Capabilities;
typedef struct _WockyXep0115CapabilitiesInterface WockyXep0115CapabilitiesInterface;

/* virtual methods */

typedef const GPtrArray * (*WockyXep0115CapabilitiesGetDataFormsFunc) (
    WockyXep0115Capabilities *contact);

const GPtrArray * wocky_xep_0115_capabilities_get_data_forms (
    WockyXep0115Capabilities *contact);

struct _WockyXep0115CapabilitiesInterface {
    GTypeInterface parent;

    WockyXep0115CapabilitiesGetDataFormsFunc get_data_forms;
};

GType wocky_xep_0115_capabilities_get_type (void);

G_END_DECLS

#endif /* WOCKY_XEP_0115_CAPABILITIES_H */
