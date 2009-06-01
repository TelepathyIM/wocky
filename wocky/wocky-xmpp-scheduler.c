/*
 * wocky-xmpp-scheduler.c - Source for WockyXmppScheduler
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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

/**
 * SECTION: wocky-xmpp-scheduler
 * @title: WockyXmppScheduler
 * @short_description: Wrapper around a #WockyXmppConnection providing a
 * higher level API.
 *
 * Sends and receives #WockyXmppStanza from an underlying
 * #WockyXmppConnection.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "wocky-xmpp-scheduler.h"
#include "wocky-signals-marshal.h"

G_DEFINE_TYPE(WockyXmppScheduler, wocky_xmpp_scheduler, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_CONNECTION = 1,
};

/* private structure */
typedef struct _WockyXmppSchedulerPrivate WockyXmppSchedulerPrivate;

struct _WockyXmppSchedulerPrivate
{
  gboolean dispose_has_run;

  WockyXmppConnection *connection;
};

#define WOCKY_XMPP_SCHEDULER_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_SCHEDULER, \
    WockyXmppSchedulerPrivate))

static void
wocky_xmpp_scheduler_init (WockyXmppScheduler *obj)
{
}

static void wocky_xmpp_scheduler_dispose (GObject *object);
static void wocky_xmpp_scheduler_finalize (GObject *object);

static void
wocky_xmpp_scheduler_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyXmppScheduler *connection = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (connection);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_assert (priv->connection == NULL);
        priv->connection = g_value_dup_object (value);
        g_assert (priv->connection != NULL);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_scheduler_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyXmppScheduler *connection = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (connection);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_scheduler_class_init (
    WockyXmppSchedulerClass *wocky_xmpp_scheduler_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_scheduler_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_xmpp_scheduler_class,
      sizeof (WockyXmppSchedulerPrivate));

  object_class->set_property = wocky_xmpp_scheduler_set_property;
  object_class->get_property = wocky_xmpp_scheduler_get_property;
  object_class->dispose = wocky_xmpp_scheduler_dispose;
  object_class->finalize = wocky_xmpp_scheduler_finalize;

  spec = g_param_spec_object ("connection", "XMPP connection",
    "the XMPP connection used by this scheduler",
    WOCKY_TYPE_XMPP_CONNECTION,
    G_PARAM_READWRITE |
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_CONNECTION, spec);
}

void
wocky_xmpp_scheduler_dispose (GObject *object)
{
  WockyXmppScheduler *self = WOCKY_XMPP_SCHEDULER (object);
  WockyXmppSchedulerPrivate *priv =
      WOCKY_XMPP_SCHEDULER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->connection != NULL)
    {
      g_object_unref (priv->connection);
      priv->connection = NULL;
    }

  if (G_OBJECT_CLASS (wocky_xmpp_scheduler_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_scheduler_parent_class)->dispose (object);
}

void
wocky_xmpp_scheduler_finalize (GObject *object)
{
  G_OBJECT_CLASS (wocky_xmpp_scheduler_parent_class)->finalize (object);
}

/**
 * wocky_xmpp_scheduler_new:
 * @connection: #WockyXmppConnection which will be used to receive and send
 * #WockyXmppStanza
 *
 * Convenience function to create a new #WockyXmppScheduler.
 *
 * Returns: a new #WockyXmppScheduler.
 */
WockyXmppScheduler *
wocky_xmpp_scheduler_new (WockyXmppConnection *connection)
{
  WockyXmppScheduler *result;

  result = g_object_new (WOCKY_TYPE_XMPP_SCHEDULER,
    "connection", connection,
    NULL);

  return result;
}
