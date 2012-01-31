/*
 * wocky-pep-service.h - Header of WockyPepService
 * Copyright © 2009, 2012 Collabora Ltd.
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

#ifndef __WOCKY_PEP_SERVICE_H__
#define __WOCKY_PEP_SERVICE_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "wocky-stanza.h"
#include "wocky-session.h"

G_BEGIN_DECLS

/**
 * WockyPepService:
 *
 * Object to aid with looking up PEP nodes and listening for changes.
 */
typedef struct _WockyPepService WockyPepService;

/**
 * WockyPepServiceClass:
 *
 * The class of a #WockyPepService.
 */
typedef struct _WockyPepServiceClass WockyPepServiceClass;
typedef struct _WockyPepServicePrivate WockyPepServicePrivate;


struct _WockyPepServiceClass {
  /*<private>*/
  GObjectClass parent_class;
};

struct _WockyPepService {
  /*<private>*/
  GObject parent;

  WockyPepServicePrivate *priv;
};

GType wocky_pep_service_get_type (void);

#define WOCKY_TYPE_PEP_SERVICE \
  (wocky_pep_service_get_type ())
#define WOCKY_PEP_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PEP_SERVICE, \
   WockyPepService))
#define WOCKY_PEP_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PEP_SERVICE, \
   WockyPepServiceClass))
#define WOCKY_IS_PEP_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PEP_SERVICE))
#define WOCKY_IS_PEP_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PEP_SERVICE))
#define WOCKY_PEP_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PEP_SERVICE, \
   WockyPepServiceClass))

WockyPepService * wocky_pep_service_new (const gchar *node,
    gboolean subscribe) G_GNUC_WARN_UNUSED_RESULT;

void wocky_pep_service_start (WockyPepService *self,
    WockySession *session);

void wocky_pep_service_get_async (WockyPepService *self,
    WockyBareContact *contact,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyStanza * wocky_pep_service_get_finish (WockyPepService *self,
    GAsyncResult *result,
    WockyNode **item,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

WockyStanza * wocky_pep_service_make_publish_stanza (WockyPepService *self,
    WockyNode **item) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __WOCKY_PEP_SERVICE_H__ */
