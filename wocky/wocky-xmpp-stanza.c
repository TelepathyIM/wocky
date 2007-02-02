/*
 * wocky-xmpp-stanza.c - Source for WockyXmppStanza
 * Copyright (C) 2006 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd@luon.net>
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


#include <stdio.h>
#include <stdlib.h>

#include "wocky-xmpp-stanza.h"

G_DEFINE_TYPE(WockyXmppStanza, wocky_xmpp_stanza, G_TYPE_OBJECT)

/* private structure */
typedef struct _WockyXmppStanzaPrivate WockyXmppStanzaPrivate;

struct _WockyXmppStanzaPrivate
{
  gboolean dispose_has_run;
};

#define WOCKY_XMPP_STANZA_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanzaPrivate))

static void
wocky_xmpp_stanza_init (WockyXmppStanza *obj)
{
  /* allocate any data required by the object here */
  obj->node = NULL;
}

static void wocky_xmpp_stanza_dispose (GObject *object);
static void wocky_xmpp_stanza_finalize (GObject *object);

static void
wocky_xmpp_stanza_class_init (WockyXmppStanzaClass *wocky_xmpp_stanza_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_stanza_class);

  g_type_class_add_private (wocky_xmpp_stanza_class, sizeof (WockyXmppStanzaPrivate));

  object_class->dispose = wocky_xmpp_stanza_dispose;
  object_class->finalize = wocky_xmpp_stanza_finalize;

}

void
wocky_xmpp_stanza_dispose (GObject *object)
{
  WockyXmppStanza *self = WOCKY_XMPP_STANZA (object);
  WockyXmppStanzaPrivate *priv = WOCKY_XMPP_STANZA_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (wocky_xmpp_stanza_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_stanza_parent_class)->dispose (object);
}

void
wocky_xmpp_stanza_finalize (GObject *object)
{
  WockyXmppStanza *self = WOCKY_XMPP_STANZA (object);

  /* free any data held directly by the object here */
  wocky_xmpp_node_free(self->node);

  G_OBJECT_CLASS (wocky_xmpp_stanza_parent_class)->finalize (object);
}


WockyXmppStanza *
wocky_xmpp_stanza_new(gchar *name) {
  WockyXmppStanza *result;

  result = WOCKY_XMPP_STANZA(g_object_new(WOCKY_TYPE_XMPP_STANZA, NULL));
  result->node = wocky_xmpp_node_new(name); 

  return result;
}

