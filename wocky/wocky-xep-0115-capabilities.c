/*
 * wocky-xep-0115-capabilities.c - interface for holding capabilities
 * of contacts
 *
 * Copyright (C) 2011-2012 Collabora Ltd.
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

#include "config.h"

#include "wocky-xep-0115-capabilities.h"

#include "wocky-contact.h"

G_DEFINE_INTERFACE (WockyXep0115Capabilities, wocky_xep_0115_capabilities,
    G_TYPE_OBJECT);

static void
wocky_xep_0115_capabilities_default_init (
    WockyXep0115CapabilitiesInterface *interface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (interface);
  static gsize initialization_value = 0;

  if (g_once_init_enter (&initialization_value))
    {
      g_signal_new ("capabilities-changed", iface_type,
          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
      g_once_init_leave (&initialization_value, 1);
    }
}

const GPtrArray *
wocky_xep_0115_capabilities_get_data_forms (
    WockyXep0115Capabilities *contact)
{
  WockyXep0115CapabilitiesInterface *iface =
    WOCKY_XEP_0115_CAPABILITIES_GET_INTERFACE (contact);
  WockyXep0115CapabilitiesGetDataFormsFunc method = iface->get_data_forms;

  if (method != NULL)
    return method (contact);

  return NULL;
}

gboolean
wocky_xep_0115_capabilities_has_feature (
    WockyXep0115Capabilities *contact,
    const gchar *feature)
{
  WockyXep0115CapabilitiesInterface *iface =
    WOCKY_XEP_0115_CAPABILITIES_GET_INTERFACE (contact);
  WockyXep0115CapabilitiesHasFeatureFunc method = iface->has_feature;

  if (method != NULL)
    return method (contact, feature);

  return FALSE;
}
