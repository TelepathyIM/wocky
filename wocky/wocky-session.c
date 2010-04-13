/*
 * wocky-session.c - Source for WockySession
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
 * SECTION: wocky-session
 * @title: WockySession
 * @short_description:
 * @include: wocky/wocky-session.h
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

G_DEFINE_TYPE (WockySession, wocky_session, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_CONNECTION = 1,
  PROP_PORTER,
  PROP_CONTACT_FACTORY,
};

/* signal enum */
enum
{
  LAST_SIGNAL,
};

/*
static guint signals[LAST_SIGNAL] = {0};
*/

/* private structure */
struct _WockySessionPrivate
{
  gboolean dispose_has_run;

  WockyXmppConnection *connection;
  WockyPorter *porter;
  WockyContactFactory *contact_factory;
};

static void
wocky_session_init (WockySession *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_SESSION,
      WockySessionPrivate);

  self->priv->contact_factory = wocky_contact_factory_new ();
}

static void
wocky_session_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockySession *self = WOCKY_SESSION (object);
  WockySessionPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_CONNECTION:
        priv->connection = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_session_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockySession *self = WOCKY_SESSION (object);
  WockySessionPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;
      case PROP_PORTER:
        g_value_set_object (value, priv->porter);
        break;
      case PROP_CONTACT_FACTORY:
        g_value_set_object (value, priv->contact_factory);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_session_constructed (GObject *object)
{
  WockySession *self = WOCKY_SESSION (object);
  WockySessionPrivate *priv = self->priv;

  g_assert (priv->connection != NULL);

  priv->porter = wocky_porter_new (priv->connection);
}

static void
wocky_session_dispose (GObject *object)
{
  WockySession *self = WOCKY_SESSION (object);
  WockySessionPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_object_unref (priv->connection);
  g_object_unref (priv->porter);
  g_object_unref (priv->contact_factory);

  if (G_OBJECT_CLASS (wocky_session_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_session_parent_class)->dispose (object);
}

static void
wocky_session_finalize (GObject *object)
{
  /*
  WockySession *self = WOCKY_SESSION (object);
  WockySessionPrivate *priv = self->priv;
  */

  G_OBJECT_CLASS (wocky_session_parent_class)->finalize (object);
}

static void
wocky_session_class_init (WockySessionClass *wocky_session_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_session_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_session_class,
      sizeof (WockySessionPrivate));

  object_class->constructed = wocky_session_constructed;
  object_class->set_property = wocky_session_set_property;
  object_class->get_property = wocky_session_get_property;
  object_class->dispose = wocky_session_dispose;
  object_class->finalize = wocky_session_finalize;

  spec = g_param_spec_object ("connection", "Connection",
      "The WockyXmppConnection associated with this session",
      WOCKY_TYPE_XMPP_CONNECTION,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, spec);

  spec = g_param_spec_object ("porter", "Porter",
      "The WockyPorter associated with this session",
      WOCKY_TYPE_PORTER,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PORTER, spec);

  spec = g_param_spec_object ("contact-factory", "Contact factory",
      "The WockyContactFactory associated with this session",
      WOCKY_TYPE_CONTACT_FACTORY,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT_FACTORY, spec);
}

WockySession *
wocky_session_new (WockyXmppConnection *conn)
{
  return g_object_new (WOCKY_TYPE_SESSION,
      "connection", conn,
      NULL);
}

void wocky_session_start (WockySession *self)
{
  WockySessionPrivate *priv = self->priv;

  wocky_porter_start (priv->porter);
}

WockyPorter *
wocky_session_get_porter (WockySession *self)
{
  WockySessionPrivate *priv = self->priv;

  return priv->porter;
}

WockyContactFactory *
wocky_session_get_contact_factory (WockySession *self)
{
  WockySessionPrivate *priv = self->priv;

  return priv->contact_factory;
}
