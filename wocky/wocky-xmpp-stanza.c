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
#include <string.h>

#include "wocky-xmpp-stanza.h"
#include "wocky-xmpp-error.h"
#include "wocky-namespaces.h"
#include "wocky-debug.h"

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

static const StanzaTypeName type_names[NUM_WOCKY_STANZA_TYPE] =
{
    { WOCKY_STANZA_TYPE_NONE,            NULL,
        WOCKY_XMPP_NS_JABBER_CLIENT },
    { WOCKY_STANZA_TYPE_MESSAGE,         "message",
        WOCKY_XMPP_NS_JABBER_CLIENT },
    { WOCKY_STANZA_TYPE_PRESENCE,        "presence",
        WOCKY_XMPP_NS_JABBER_CLIENT },
    { WOCKY_STANZA_TYPE_IQ,              "iq",
        WOCKY_XMPP_NS_JABBER_CLIENT },
    { WOCKY_STANZA_TYPE_STREAM,          "stream",
        WOCKY_XMPP_NS_STREAM },
    { WOCKY_STANZA_TYPE_STREAM_FEATURES, "features",
        WOCKY_XMPP_NS_STREAM },
    { WOCKY_STANZA_TYPE_AUTH,            "auth",
        WOCKY_XMPP_NS_SASL_AUTH },
    { WOCKY_STANZA_TYPE_CHALLENGE,       "challenge",
        WOCKY_XMPP_NS_SASL_AUTH },
    { WOCKY_STANZA_TYPE_RESPONSE,        "response",
        WOCKY_XMPP_NS_SASL_AUTH },
    { WOCKY_STANZA_TYPE_SUCCESS,         "success",
        WOCKY_XMPP_NS_SASL_AUTH },
    { WOCKY_STANZA_TYPE_FAILURE,         "failure",
        WOCKY_XMPP_NS_SASL_AUTH },
    { WOCKY_STANZA_TYPE_STREAM_ERROR,    "error",
        WOCKY_XMPP_NS_STREAM },
    { WOCKY_STANZA_TYPE_UNKNOWN,         NULL,        NULL },
};

typedef struct
{
  WockyStanzaSubType sub_type;
  const gchar *name;
  WockyStanzaType type;
} StanzaSubTypeName;

static const StanzaSubTypeName sub_type_names[NUM_WOCKY_STANZA_SUB_TYPE] =
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
    { WOCKY_STANZA_SUB_TYPE_UNKNOWN,        NULL,
        WOCKY_STANZA_TYPE_UNKNOWN },
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
  wocky_xmpp_node_free (self->node);

  G_OBJECT_CLASS (wocky_xmpp_stanza_parent_class)->finalize (object);
}


WockyXmppStanza *
wocky_xmpp_stanza_new (const gchar *name)
{
  WockyXmppStanza *result;

  result = WOCKY_XMPP_STANZA (g_object_new (WOCKY_TYPE_XMPP_STANZA, NULL));
  result->node = wocky_xmpp_node_new (name);

  return result;
}

static gboolean
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

            g_assert (key != NULL);
            g_assert (value != NULL);
            wocky_xmpp_node_set_attribute (stack->data, key, value);
          }
          break;

        case WOCKY_NODE:
          {
            gchar *name = va_arg (ap, gchar *);
            WockyXmppNode *child;

            g_assert (name != NULL);
            child = wocky_xmpp_node_add_child (stack->data, name);
            stack = g_slist_prepend (stack, child);
          }
          break;

        case WOCKY_NODE_TEXT:
          {
            gchar *txt = va_arg (ap, gchar *);

            g_assert (txt != NULL);
            wocky_xmpp_node_set_content (stack->data, txt);
          }
          break;

        case WOCKY_NODE_XMLNS:
          {
            gchar *ns = va_arg (ap, gchar *);

            g_assert (ns != NULL);
            wocky_xmpp_node_set_ns (stack->data, ns);
          }
          break;

        case WOCKY_NODE_END:
          {
            /* delete the top of the stack */
            stack = g_slist_delete_link (stack, stack);
          }
          break;

        case WOCKY_NODE_ASSIGN_TO:
          {
            WockyXmppNode **dest = va_arg (ap, WockyXmppNode **);

            g_assert (dest != NULL);
            *dest = stack->data;
          }
          break;

        default:
          g_assert_not_reached ();
        }

      arg = va_arg (ap, WockyBuildTag);
    }

  g_slist_free (stack);
  return TRUE;
}

static const gchar *
get_type_name (WockyStanzaType type)
{
  if (type <= WOCKY_STANZA_TYPE_NONE ||
      type >= NUM_WOCKY_STANZA_TYPE)
    return NULL;

  g_assert (type_names[type].type == type);
  return type_names[type].name;
}

static const gchar *
get_type_ns (WockyStanzaType type)
{
  if (type <= WOCKY_STANZA_TYPE_NONE ||
      type >= NUM_WOCKY_STANZA_TYPE)
    return NULL;

  g_assert (type_names[type].type == type);
  return type_names[type].ns;
}

static const gchar *
get_sub_type_name (WockyStanzaSubType sub_type)
{
  if (sub_type <= WOCKY_STANZA_SUB_TYPE_NONE ||
      sub_type >= NUM_WOCKY_STANZA_SUB_TYPE)
    return NULL;

  g_assert (sub_type_names[sub_type].sub_type == sub_type);
  return sub_type_names[sub_type].name;
}

static gboolean
check_sub_type (WockyStanzaType type,
                WockyStanzaSubType sub_type)
{
  g_return_val_if_fail (type > WOCKY_STANZA_TYPE_NONE &&
      type < NUM_WOCKY_STANZA_TYPE, FALSE);
  g_return_val_if_fail (sub_type < NUM_WOCKY_STANZA_SUB_TYPE, FALSE);

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
 * wocky_xmpp_stanza_build:
 * @type: The type of stanza to build
 * @sub_type: The stanza's subtype; valid values depend on @type. (For instance,
 *           #WOCKY_STANZA_TYPE_IQ can use #WOCKY_STANZA_SUB_TYPE_GET, but not
 *           #WOCKY_STANZA_SUB_TYPE_SUBSCRIBED.)
 * @from: The sender's JID, or %NULL to leave it unspecified.
 * @to: The target's JID, or %NULL to leave it unspecified.
 * @spec: The beginning of a description of the stanza to build,
 *  or %WOCKY_STANZA_END to build an empty stanza
 * @Varargs: The rest of the description of the stanza to build,
 *  terminated with %WOCKY_STANZA_END
 *
 * Build a XMPP stanza from a list of arguments.
 * Example:
 *
 * <example><programlisting>
 * wocky_xmpp_stanza_build (
 *    WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
 *    "alice@<!-- -->collabora.co.uk", "bob@<!-- -->collabora.co.uk",
 *    WOCKY_NODE, "html",
 *      WOCKY_NODE_XMLNS, "http://www.w3.org/1999/xhtml",
 *      WOCKY_NODE, "body",
 *        WOCKY_NODE_ATTRIBUTE, "textcolor", "red",
 *        WOCKY_NODE_TEXT, "Telepathy rocks!",
 *      WOCKY_NODE_END,
 *    WOCKY_NODE_END,
 *   WOCKY_STANZA_END);
 * <!-- -->
 * /<!-- -->* produces
 * &lt;message from='alice@<!-- -->collabora.co.uk' to='bob@<!-- -->collabora.co.uk'&gt;
 *   &lt;html xmlns='http://www.w3.org/1999/xhtml'&gt;
 *     &lt;body textcolor='red'&gt;
 *       Telepathy rocks!
 *     &lt;/body&gt;
 *   &lt;/html&gt;
 * &lt;/message&gt;
 * *<!-- -->/
 * </programlisting></example>
 *
 * Returns: a new stanza object
 */
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

  va_start (ap, spec);
  stanza = wocky_xmpp_stanza_build_va (type, sub_type, from, to, spec, ap);
  va_end (ap);

  return stanza;
}

WockyXmppStanza *
wocky_xmpp_stanza_build_va (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    const gchar *to,
    WockyBuildTag spec,
    va_list ap)
{
  WockyXmppStanza *stanza;

  g_return_val_if_fail (type < NUM_WOCKY_STANZA_TYPE, NULL);
  g_return_val_if_fail (sub_type < NUM_WOCKY_STANZA_SUB_TYPE, NULL);

  stanza = wocky_xmpp_stanza_new_with_sub_type (type, sub_type);
  if (stanza == NULL)
    return NULL;

  if (from != NULL)
    wocky_xmpp_node_set_attribute (stanza->node, "from", from);

  if (to != NULL)
    wocky_xmpp_node_set_attribute (stanza->node, "to", to);

  if (!wocky_xmpp_stanza_add_build_va (stanza->node, spec, ap))
    {
      g_object_unref (stanza);
      stanza = NULL;
    }

  return stanza;
}

static WockyStanzaType
get_type_from_name (const gchar *name)
{
  guint i;

  if (name == NULL)
    return WOCKY_STANZA_TYPE_NONE;

  /* We skip the first entry as it's NONE */
  for (i = 1; i < WOCKY_STANZA_TYPE_UNKNOWN; i++)
    {
       if (type_names[i].name != NULL &&
           strcmp (name, type_names[i].name) == 0)
         {
           return type_names[i].type;
         }
    }

  return WOCKY_STANZA_TYPE_UNKNOWN;
}

static WockyStanzaSubType
get_sub_type_from_name (const gchar *name)
{
  guint i;

  if (name == NULL)
    return WOCKY_STANZA_SUB_TYPE_NONE;

  /* We skip the first entry as it's NONE */
  for (i = 1; i < WOCKY_STANZA_SUB_TYPE_UNKNOWN; i++)
    {
      if (sub_type_names[i].name != NULL &&
          strcmp (name, sub_type_names[i].name) == 0)
        {
          return sub_type_names[i].sub_type;
        }
    }

  return WOCKY_STANZA_SUB_TYPE_UNKNOWN;
}

void
wocky_xmpp_stanza_get_type_info (WockyXmppStanza *stanza,
                                  WockyStanzaType *type,
                                  WockyStanzaSubType *sub_type)
{
  g_return_if_fail (stanza != NULL);
  g_assert (stanza->node != NULL);

  if (type != NULL)
    *type = get_type_from_name (stanza->node->name);

  if (sub_type != NULL)
    *sub_type = get_sub_type_from_name (wocky_xmpp_node_get_attribute (
          stanza->node, "type"));
}

static WockyXmppStanza *
create_iq_reply (WockyXmppStanza *iq,
    WockyStanzaSubType sub_type_reply,
    WockyBuildTag spec,
    va_list ap)
{
  WockyXmppStanza *reply;
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  const gchar *from, *to, *id;

  g_return_val_if_fail (iq != NULL, NULL);

  wocky_xmpp_stanza_get_type_info (iq, &type, &sub_type);
  g_return_val_if_fail (type == WOCKY_STANZA_TYPE_IQ, NULL);
  g_return_val_if_fail (sub_type == WOCKY_STANZA_SUB_TYPE_GET ||
      sub_type == WOCKY_STANZA_SUB_TYPE_SET, NULL);

  from = wocky_xmpp_node_get_attribute (iq->node, "from");
  to = wocky_xmpp_node_get_attribute (iq->node, "to");
  id = wocky_xmpp_node_get_attribute (iq->node, "id");
  g_return_val_if_fail (id != NULL, NULL);

  reply = wocky_xmpp_stanza_build_va (WOCKY_STANZA_TYPE_IQ,
      sub_type_reply, to, from, spec, ap);

  wocky_xmpp_node_set_attribute (reply->node, "id", id);
  return reply;
}

WockyXmppStanza *
wocky_xmpp_stanza_build_iq_result (WockyXmppStanza *iq,
    WockyBuildTag spec,
    ...)
{
  WockyXmppStanza *reply;
  va_list ap;

  va_start (ap, spec);
  reply = create_iq_reply (iq, WOCKY_STANZA_SUB_TYPE_RESULT, spec, ap);
  va_end (ap);

  return reply;
}

WockyXmppStanza *
wocky_xmpp_stanza_build_iq_error (WockyXmppStanza *iq,
    WockyBuildTag spec,
    ...)
{
  WockyXmppStanza *reply;
  va_list ap;

  va_start (ap, spec);
  reply = create_iq_reply (iq, WOCKY_STANZA_SUB_TYPE_ERROR, spec, ap);
  va_end (ap);

  return reply;
}

/**
 * wocky_xmpp_stanza_extract_errors:
 * @stanza: a message/iq/presence stanza
 * @type: location at which to store the error type
 * @core: location at which to store an error in the domain #WOCKY_XMPP_ERROR
 * @specialized: location at which to store an error in an application-specific
 *               domain, if one is found
 * @specialized_node: location at which to store the node representing an
 *                    application-specific error, if one is found
 *
 * Given a message, iq or presence stanza with type='error', breaks it down
 * into values describing the error. @type and @core are guaranteed to be set;
 * @specialized and @specialized_node will be set if a recognised
 * application-specific error is found, and the latter will be set to %NULL if
 * no application-specific error is found.
 *
 * Any or all of the out parameters may be %NULL to ignore the value.  The
 * value stored in @specialized_node is borrowed from @stanza, and is only
 * valid as long as the latter is alive.
 *
 * Returns: %TRUE if the stanza had type='error'; %FALSE otherwise
 */
gboolean
wocky_xmpp_stanza_extract_errors (WockyXmppStanza *stanza,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyXmppNode **specialized_node)
{
  WockyStanzaSubType sub_type;
  WockyXmppNode *error;

  wocky_xmpp_stanza_get_type_info (stanza, NULL, &sub_type);

  if (sub_type != WOCKY_STANZA_SUB_TYPE_ERROR)
    return FALSE;

  error = wocky_xmpp_node_get_child (stanza->node, "error");
  wocky_xmpp_error_extract (error, type, core, specialized, specialized_node);
  return TRUE;
}

/**
 * wocky_xmpp_stanza_extract_stream_error:
 * @stanza: a stanza
 * @stream_error: location at which to store an error in domain
 *                #WOCKY_XMPP_STREAM_ERROR, if one is found.
 *
 * Returns: %TRUE and sets @stream_error if the stanza was indeed a stream
 *          error.
 */
gboolean
wocky_xmpp_stanza_extract_stream_error (WockyXmppStanza *stanza,
    GError **stream_error)
{
  WockyStanzaType type;

  wocky_xmpp_stanza_get_type_info (stanza, &type, NULL);

  if (type != WOCKY_STANZA_TYPE_STREAM_ERROR)
    return FALSE;

  g_propagate_error (stream_error,
      wocky_xmpp_stream_error_from_node (stanza->node));
  return TRUE;
}
