/*
 * wocky-pubsub-node.h - Header of WockyPubsubNode
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

#ifndef __WOCKY_PUBSUB_NODE_H__
#define __WOCKY_PUBSUB_NODE_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "wocky-enumtypes.h"
#include "wocky-types.h"
#include "wocky-session.h"
#include "wocky-pubsub-service.h"

G_BEGIN_DECLS

/**
 * WockyPubsubNodeClass:
 *
 * The class of a #WockyPubsubNode.
 */
typedef struct _WockyPubsubNodeClass WockyPubsubNodeClass;
typedef struct _WockyPubsubNodePrivate WockyPubsubNodePrivate;


struct _WockyPubsubNodeClass {
  /*<private>*/
  GObjectClass parent_class;
};

struct _WockyPubsubNode {
  /*<private>*/
  GObject parent;

  WockyPubsubNodePrivate *priv;
};

GType wocky_pubsub_node_get_type (void);

#define WOCKY_TYPE_PUBSUB_NODE \
  (wocky_pubsub_node_get_type ())
#define WOCKY_PUBSUB_NODE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_PUBSUB_NODE, \
   WockyPubsubNode))
#define WOCKY_PUBSUB_NODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_PUBSUB_NODE, \
   WockyPubsubNodeClass))
#define WOCKY_IS_PUBSUB_NODE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_PUBSUB_NODE))
#define WOCKY_IS_PUBSUB_NODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_PUBSUB_NODE))
#define WOCKY_PUBSUB_NODE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_PUBSUB_NODE, \
   WockyPubsubNodeClass))

const gchar * wocky_pubsub_node_get_name (WockyPubsubNode *self);

WockyStanza *wocky_pubsub_node_make_publish_stanza (WockyPubsubNode *self,
    WockyNode **pubsub_out,
    WockyNode **publish_out,
    WockyNode **item_out);

void wocky_pubsub_node_subscribe_async (WockyPubsubNode *self,
    const gchar *jid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyPubsubSubscription *wocky_pubsub_node_subscribe_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error);

void wocky_pubsub_node_unsubscribe_async (WockyPubsubNode *self,
    const gchar *jid,
    const gchar *subid,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_pubsub_node_unsubscribe_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error);

void wocky_pubsub_node_delete_async (WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_pubsub_node_delete_finish (WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error);

void wocky_pubsub_node_list_subscribers_async (
    WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_pubsub_node_list_subscribers_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GList **subscribers,
    GError **error);

/*< prefix=WOCKY_PUBSUB_AFFILIATION >*/
typedef enum {
    WOCKY_PUBSUB_AFFILIATION_OWNER,
    WOCKY_PUBSUB_AFFILIATION_PUBLISHER,
    WOCKY_PUBSUB_AFFILIATION_PUBLISH_ONLY,
    WOCKY_PUBSUB_AFFILIATION_MEMBER,
    WOCKY_PUBSUB_AFFILIATION_NONE,
    WOCKY_PUBSUB_AFFILIATION_OUTCAST
} WockyPubsubAffiliationState;

typedef struct _WockyPubsubAffiliation WockyPubsubAffiliation;
struct _WockyPubsubAffiliation {
    /*< public >*/
    WockyPubsubNode *node;
    gchar *jid;
    WockyPubsubAffiliationState state;
};

#define WOCKY_TYPE_PUBSUB_AFFILIATION \
  (wocky_pubsub_affiliation_get_type ())
GType wocky_pubsub_affiliation_get_type (void);

WockyPubsubAffiliation *wocky_pubsub_affiliation_new (
    WockyPubsubNode *node,
    const gchar *jid,
    WockyPubsubAffiliationState state);
WockyPubsubAffiliation *wocky_pubsub_affiliation_copy (
    WockyPubsubAffiliation *aff);
void wocky_pubsub_affiliation_free (WockyPubsubAffiliation *aff);

GList *wocky_pubsub_affiliation_list_copy (GList *affs);
void wocky_pubsub_affiliation_list_free (GList *affs);

void wocky_pubsub_node_list_affiliates_async (
    WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_pubsub_node_list_affiliates_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GList **affiliates,
    GError **error);

void wocky_pubsub_node_modify_affiliates_async (
    WockyPubsubNode *self,
    GList *affiliates,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean wocky_pubsub_node_modify_affiliates_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error);

void wocky_pubsub_node_get_configuration_async (
    WockyPubsubNode *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

WockyDataForm *wocky_pubsub_node_get_configuration_finish (
    WockyPubsubNode *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif /* __WOCKY_PUBSUB_NODE_H__ */
