/*
 * wocky-pubsub-service.h - Header of WockyPubsubService
 * Copyright (C) 2009 Collabora Ltd.
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

#ifndef __WOCKY_PUBSUB_SERVICE_H__
#define __WOCKY_PUBSUB_SERVICE_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "wocky-enumtypes.h"
#include "wocky-stanza.h"
#include "wocky-session.h"
#include "wocky-types.h"
#include "wocky-data-form.h"

G_BEGIN_DECLS

typedef struct _WockyPubsubService WockyPubsubService;
typedef struct _WockyPubsubServiceClass WockyPubsubServiceClass;
typedef struct _WockyPubsubServicePrivate WockyPubsubServicePrivate;

/**
 * WockyPubsubServiceError:
 * @WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY: A wrong reply was received
 *
 * #WockyPubsubService specific errors.
 */
typedef enum {
  WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
} WockyPubsubServiceError;

GQuark wocky_pubsub_service_error_quark (void);

#define WOCKY_PUBSUB_SERVICE_ERROR (wocky_pubsub_service_error_quark ())

struct _WockyPubsubServiceClass {
  GObjectClass parent_class;
  GType node_object_type;
};

struct _WockyPubsubService {
  GObject parent;

  WockyPubsubServicePrivate *priv;
};

GType wocky_pubsub_service_get_type (void);

#define WOCKY_TYPE_PUBSUB_SERVICE \
  (wocky_pubsub_service_get_type ())
#define WOCKY_PUBSUB_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PUBSUB_SERVICE, \
   WockyPubsubService))
#define WOCKY_PUBSUB_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PUBSUB_SERVICE, \
   WockyPubsubServiceClass))
#define WOCKY_IS_PUBSUB_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PUBSUB_SERVICE))
#define WOCKY_IS_PUBSUB_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PUBSUB_SERVICE))
#define WOCKY_PUBSUB_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PUBSUB_SERVICE, \
   WockyPubsubServiceClass))

WockyPubsubService * wocky_pubsub_service_new (WockySession *session,
    const gchar *jid);

WockyPubsubNode * wocky_pubsub_service_ensure_node (WockyPubsubService *self,
    const gchar *name);

WockyPubsubNode * wocky_pubsub_service_lookup_node (WockyPubsubService *self,
    const gchar *name);

void wocky_pubsub_service_get_default_node_configuration_async (
    WockyPubsubService *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyDataForm * wocky_pubsub_service_get_default_node_configuration_finish (
    WockyPubsubService *self,
    GAsyncResult *result,
    GError **error);

void wocky_pubsub_service_retrieve_subscriptions_async (
    WockyPubsubService *self,
    WockyPubsubNode *node,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_pubsub_service_retrieve_subscriptions_finish (
    WockyPubsubService *self,
    GAsyncResult *result,
    GList **subscriptions,
    GError **error);

void wocky_pubsub_service_create_node_async (WockyPubsubService *self,
    const gchar *name,
    WockyDataForm *config,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyPubsubNode * wocky_pubsub_service_create_node_finish (
    WockyPubsubService *self,
    GAsyncResult *result,
    GError **error);

/*< prefix=WOCKY_PUBSUB_SUBSCRIPTION >*/
typedef enum {
    WOCKY_PUBSUB_SUBSCRIPTION_NONE,
    WOCKY_PUBSUB_SUBSCRIPTION_PENDING,
    WOCKY_PUBSUB_SUBSCRIPTION_SUBSCRIBED,
    WOCKY_PUBSUB_SUBSCRIPTION_UNCONFIGURED
} WockyPubsubSubscriptionState;

typedef struct {
    /*< public >*/
    WockyPubsubNode *node;
    gchar *jid;
    WockyPubsubSubscriptionState state;
    gchar *subid;
} WockyPubsubSubscription;

#define WOCKY_TYPE_PUBSUB_SUBSCRIPTION \
  (wocky_pubsub_subscription_get_type ())
GType wocky_pubsub_subscription_get_type (void);

WockyPubsubSubscription *wocky_pubsub_subscription_new (
    WockyPubsubNode *node,
    const gchar *jid,
    WockyPubsubSubscriptionState state,
    const gchar *subid);
WockyPubsubSubscription *wocky_pubsub_subscription_copy (
    WockyPubsubSubscription *sub);
void wocky_pubsub_subscription_free (WockyPubsubSubscription *sub);

GList *wocky_pubsub_subscription_list_copy (GList *subs);
void wocky_pubsub_subscription_list_free (GList *subs);

G_END_DECLS

#endif /* __WOCKY_PUBSUB_SERVICE_H__ */
