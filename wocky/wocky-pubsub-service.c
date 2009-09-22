/*
 * wocky-pubsub-service.c - WockyPubsubService
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

#include "wocky-pubsub-service.h"

#include <wocky/wocky-porter.h>
#include <wocky/wocky-utils.h>
#include <wocky/wocky-pubsub-node.h>
#include <wocky/wocky-namespaces.h>
#include <wocky/wocky-signals-marshal.h>

#define DEBUG_FLAG DEBUG_PUBSUB
#include "wocky-debug.h"

G_DEFINE_TYPE (WockyPubsubService, wocky_pubsub_service, G_TYPE_OBJECT)

/* signal enum */
#if 0
enum
{
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};
#endif

enum
{
  PROP_SESSION = 1,
  PROP_JID,
};

/* private structure */
typedef struct _WockyPubsubServicePrivate WockyPubsubServicePrivate;

struct _WockyPubsubServicePrivate
{
  WockySession *session;
  WockyPorter *porter;

  gchar *jid;
  /* owned (gchar *) => weak reffed (WockyPubsubNode *) */
  GHashTable *nodes;

  gboolean dispose_has_run;
};

#define WOCKY_PUBSUB_SERVICE_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_PUBSUB_SERVICE, \
    WockyPubsubServicePrivate))

static void
wocky_pubsub_service_init (WockyPubsubService *obj)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (obj);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  priv->nodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
wocky_pubsub_service_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_SESSION:
        priv->session = g_value_get_object (value);
        break;
      case PROP_JID:
        priv->jid = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_pubsub_service_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_SESSION:
        g_value_set_object (value, priv->session);
        break;
      case PROP_JID:
        g_value_set_string (value, priv->jid);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
wocky_pubsub_service_dispose (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->porter != NULL)
    g_object_unref (priv->porter);

  if (G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->dispose (object);
}

static void
wocky_pubsub_service_finalize (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  g_free (priv->jid);
  g_hash_table_unref (priv->nodes);

  G_OBJECT_CLASS (wocky_pubsub_service_parent_class)->finalize (object);
}

static void
wocky_pubsub_service_constructed (GObject *object)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (object);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  g_assert (priv->session != NULL);
  g_assert (priv->jid != NULL);
}

static void
wocky_pubsub_service_class_init (
    WockyPubsubServiceClass *wocky_pubsub_service_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_pubsub_service_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_pubsub_service_class,
      sizeof (WockyPubsubServicePrivate));

  object_class->set_property = wocky_pubsub_service_set_property;
  object_class->get_property = wocky_pubsub_service_get_property;
  object_class->dispose = wocky_pubsub_service_dispose;
  object_class->finalize = wocky_pubsub_service_finalize;
  object_class->constructed = wocky_pubsub_service_constructed;

  param_spec = g_param_spec_object ("session", "session",
      "the Wocky Session associated with this pubsub service",
      WOCKY_TYPE_SESSION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SESSION, param_spec);

  param_spec = g_param_spec_string ("jid", "jid",
      "The jid of the pubsub service",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_JID, param_spec);
}

WockyPubsubService *
wocky_pubsub_service_new (WockySession *session,
    const gchar *jid)
{
  return g_object_new (WOCKY_TYPE_PUBSUB_SERVICE,
      "session", session,
      "jid", jid,
      NULL);
}

static gboolean
remove_node (gpointer key,
    gpointer value,
    gpointer node)
{
  return value == node;
}

/* Called when a WockyPubsubNode has been disposed so we can remove it from
 * the hash table. */
static void
node_disposed_cb (gpointer user_data,
    GObject *node)
{
  WockyPubsubService *self = WOCKY_PUBSUB_SERVICE (user_data);
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  g_hash_table_foreach_remove (priv->nodes, remove_node, node);
}

static WockyPubsubNode *
create_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);
  WockyPubsubNode *node;

  node = wocky_pubsub_node_new (self, name);

  g_object_weak_ref (G_OBJECT (node), node_disposed_cb, self);
  g_hash_table_insert (priv->nodes, g_strdup (name), node);

  return node;
}

WockyPubsubNode *
wocky_pubsub_service_ensure_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);
  WockyPubsubNode *node;

  node = g_hash_table_lookup (priv->nodes, name);
  if (node != NULL)
    return g_object_ref (node);

  return create_node (self, name);
}

WockyPubsubNode *
wocky_pubsub_service_lookup_node (WockyPubsubService *self,
    const gchar *name)
{
  WockyPubsubServicePrivate *priv = WOCKY_PUBSUB_SERVICE_GET_PRIVATE (self);

  return g_hash_table_lookup (priv->nodes, name);
}
