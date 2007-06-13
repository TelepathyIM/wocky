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
#include "wocky-namespaces.h"

G_DEFINE_TYPE(WockyXmppStanza, wocky_xmpp_stanza, G_TYPE_OBJECT)

/* private structure */
typedef struct _WockyXmppStanzaPrivate WockyXmppStanzaPrivate;

struct _WockyXmppStanzaPrivate
{
  gboolean dispose_has_run;
};

#define WOCKY_XMPP_STANZA_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanzaPrivate))

typedef struct
{
    WockyStanzaType type;
    const gchar *name;
    const gchar *ns;
} StanzaTypeName;

static const StanzaTypeName type_names[LAST_WOCKY_STANZA_TYPE] =
{
    { WOCKY_STANZA_TYPE_NONE,            NULL,        NULL },
    { WOCKY_STANZA_TYPE_MESSAGE,         "message",   NULL },
    { WOCKY_STANZA_TYPE_PRESENCE,        "presence",  NULL },
    { WOCKY_STANZA_TYPE_IQ,              "iq",        NULL },
    { WOCKY_STANZA_TYPE_STREAM,          "stream",    WOCKY_XMPP_NS_STREAM },
    { WOCKY_STANZA_TYPE_STREAM_FEATURES, "features",  WOCKY_XMPP_NS_STREAM },
    { WOCKY_STANZA_TYPE_AUTH,            "auth",      NULL },
    { WOCKY_STANZA_TYPE_CHALLENGE,       "challenge", NULL },
    { WOCKY_STANZA_TYPE_RESPONSE,        "response",  NULL },
    { WOCKY_STANZA_TYPE_SUCCESS,         "success",   NULL },
    { WOCKY_STANZA_TYPE_FAILURE,         "failure",   NULL },
    { WOCKY_STANZA_TYPE_STREAM_ERROR,    "error",     WOCKY_XMPP_NS_STREAM },
};

typedef struct
{
  WockyStanzaSubType sub_type;
  const gchar *name;
  WockyStanzaType type;
} StanzaSubTypeName;

static const StanzaSubTypeName sub_type_names[LAST_WOCKY_STANZA_SUB_TYPE] =
{
    { WOCKY_STANZA_SUB_TYPE_NONE,           NULL,
        WOCKY_STANZA_TYPE_NONE },
    { WOCKY_STANZA_SUB_TYPE_AVAILABLE,
        NULL, WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_NORMAL,         "normal",
        WOCKY_STANZA_TYPE_NONE },
    { WOCKY_STANZA_SUB_TYPE_CHAT,           "chat",
        WOCKY_STANZA_TYPE_MESSAGE },
    { WOCKY_STANZA_SUB_TYPE_GROUPCHAT,      "groupchat",
        WOCKY_STANZA_TYPE_MESSAGE },
    { WOCKY_STANZA_SUB_TYPE_HEADLINE,       "headline",
        WOCKY_STANZA_TYPE_MESSAGE },
    { WOCKY_STANZA_SUB_TYPE_UNAVAILABLE,    "unavailable",
        WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_PROBE,          "probe",
        WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_SUBSCRIBE,      "subscribe",
        WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_UNSUBSCRIBE,    "unsubscribe",
        WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_SUBSCRIBED,     "subscribed",
        WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_UNSUBSCRIBED,   "unsubscribed",
        WOCKY_STANZA_TYPE_PRESENCE },
    { WOCKY_STANZA_SUB_TYPE_GET,            "get",
        WOCKY_STANZA_TYPE_IQ },
    { WOCKY_STANZA_SUB_TYPE_SET,            "set",
        WOCKY_STANZA_TYPE_IQ },
    { WOCKY_STANZA_SUB_TYPE_RESULT,         "result",
        WOCKY_STANZA_TYPE_IQ },
    { WOCKY_STANZA_SUB_TYPE_ERROR,          "error",
        WOCKY_STANZA_TYPE_NONE },
};

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
wocky_xmpp_stanza_new (const gchar *name)
{
  WockyXmppStanza *result;

  result = WOCKY_XMPP_STANZA(g_object_new(WOCKY_TYPE_XMPP_STANZA, NULL));
  result->node = wocky_xmpp_node_new(name); 

  return result;
}

static void
wocky_xmpp_stanza_add_build_va (WockyXmppNode *node,
                                 WockyBuildTag arg,
                                 va_list ap)
{
  GSList *stack = NULL;

  stack = g_slist_prepend (stack, node);

  while (arg != WOCKY_STANZA_END)
    {
      switch (arg)
        {
        case WOCKY_NODE_ATTRIBUTE:
          {
            gchar *key = va_arg (ap, gchar *);
            gchar *value = va_arg (ap, gchar *);

            g_return_if_fail (key != NULL);
            g_return_if_fail (value != NULL);
            wocky_xmpp_node_set_attribute (stack->data, key, value);
          }
          break;

        case WOCKY_NODE:
          {
            gchar *name = va_arg (ap, gchar *);
            WockyXmppNode *child;

            g_return_if_fail (name != NULL);
            child = wocky_xmpp_node_add_child (stack->data, name);
            stack = g_slist_prepend (stack, child);
          }
          break;

        case WOCKY_NODE_TEXT:
          {
            gchar *txt = va_arg (ap, gchar *);

            g_return_if_fail (txt != NULL);
            wocky_xmpp_node_set_content (stack->data, txt);
          }
          break;

        case WOCKY_NODE_XMLNS:
          {
            gchar *ns = va_arg (ap, gchar *);

            g_return_if_fail (ns != NULL);
            wocky_xmpp_node_set_ns (stack->data, ns);
          }
          break;

        case WOCKY_NODE_END:
          {
            /* delete the top of the stack */
            stack = g_slist_delete_link (stack, stack);
          }
          break;

        default:
          g_assert_not_reached ();
        }

      arg = va_arg (ap, WockyBuildTag);
    }

  g_slist_free (stack);
}

static const gchar *
get_type_name (WockyStanzaType type)
{
  if (type < WOCKY_STANZA_TYPE_NONE ||
      type >= LAST_WOCKY_STANZA_TYPE)
    return NULL;

  g_assert (type_names[type].type == type);
  return type_names[type].name;
}

static const gchar *
get_type_ns (WockyStanzaType type)
{
  if (type < WOCKY_STANZA_TYPE_NONE ||
      type >= LAST_WOCKY_STANZA_TYPE)
    return NULL;

  g_assert (type_names[type].type == type);
  return type_names[type].ns;
}

static const gchar *
get_sub_type_name (WockyStanzaSubType sub_type)
{
  if (sub_type < WOCKY_STANZA_SUB_TYPE_NONE ||
      sub_type >= LAST_WOCKY_STANZA_SUB_TYPE)
    return NULL;

  g_assert (sub_type_names[sub_type].sub_type == sub_type);
  return sub_type_names[sub_type].name;
}

static gboolean
check_sub_type (WockyStanzaType type,
                WockyStanzaSubType sub_type)
{
  g_return_val_if_fail (type >= WOCKY_STANZA_TYPE_NONE &&
      type < LAST_WOCKY_STANZA_TYPE, FALSE);
  g_return_val_if_fail (sub_type >= WOCKY_STANZA_SUB_TYPE_NONE &&
      sub_type < LAST_WOCKY_STANZA_SUB_TYPE, FALSE);

  g_assert (sub_type_names[sub_type].sub_type == sub_type);
  g_return_val_if_fail (
      sub_type_names[sub_type].type == WOCKY_STANZA_TYPE_NONE ||
      sub_type_names[sub_type].type == type, FALSE);

  return TRUE;
}

static WockyXmppStanza *
wocky_xmpp_stanza_new_with_sub_type (WockyStanzaType type,
                                      WockyStanzaSubType sub_type)
{
  WockyXmppStanza *stanza = NULL;
  const gchar *sub_type_name;

  if (!check_sub_type (type, sub_type))
    return NULL;

  stanza = wocky_xmpp_stanza_new (get_type_name (type));
  wocky_xmpp_node_set_ns (stanza->node, get_type_ns (type));

  sub_type_name = get_sub_type_name (sub_type);
  if (sub_type_name != NULL)
    wocky_xmpp_node_set_attribute (stanza->node, "type", sub_type_name);

  return stanza;
}

/**
 * wocky_xmpp_stanza_build
 *
 * Build a XMPP stanza from a list of arguments.
 * Example:
 *
 * wocky_xmpp_stanza_build (
 *    WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
 *    "alice@collabora.co.uk", "bob@collabora.co.uk",
 *    WOCKY_NODE, "html", "http://www.w3.org/1999/xhtml",
 *      WOCKY_NODE_XMLNS, 
 *      WOCKY_NODE, "body",
 *        WOCKY_NODE_ATTRIBUTE, "textcolor", "red",
 *        WOCKY_NODE_TEXT, "Telepathy rocks!",
 *      WOCKY_NODE_END,
 *    WOCKY_NODE_END,
 *   WOCKY_STANZA_END);
 *
 * -->
 *
 * <message from='alice@collabora.co.uk' to='bob@collabora.co.uk'>
 *   <html xmlns='http://www.w3.org/1999/xhtml'>
 *     <body textcolor='red'>
 *       Telepathy rocks!
 *     </body>
 *   </html>
 * </message>
 **/
WockyXmppStanza *
wocky_xmpp_stanza_build (WockyStanzaType type,
                          WockyStanzaSubType sub_type,
                          const gchar *from,
                          const gchar *to,
                          WockyBuildTag spec,
                          ...)

{
  WockyXmppStanza *stanza;
  va_list ap;

  g_return_val_if_fail (type < LAST_WOCKY_STANZA_TYPE, NULL);
  g_return_val_if_fail (sub_type < LAST_WOCKY_STANZA_SUB_TYPE, NULL);

  stanza = wocky_xmpp_stanza_new_with_sub_type (type, sub_type);
  if (stanza == NULL)
    return NULL;

  if (from != NULL)
    wocky_xmpp_node_set_attribute (stanza->node, "from", from);

  if (to != NULL)
    wocky_xmpp_node_set_attribute (stanza->node, "to", to);

  va_start (ap, spec);
  wocky_xmpp_stanza_add_build_va (stanza->node, spec, ap);
  va_end (ap);

  return stanza;
}
