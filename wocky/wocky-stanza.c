/*
 * wocky-stanza.c - Source for WockyStanza
 * Copyright (C) 2006-2010 Collabora Ltd.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wocky-stanza.h"
#include "wocky-xmpp-error.h"
#include "wocky-namespaces.h"
#include "wocky-debug-internal.h"

#include "wocky-node-private.h"

G_DEFINE_TYPE(WockyStanza, wocky_stanza, WOCKY_TYPE_NODE_TREE)

/* private structure */
struct _WockyStanzaPrivate
{
  WockyContact *from_contact;
  WockyContact *to_contact;

  gboolean dispose_has_run;
};

typedef struct
{
    WockyStanzaType type;
    const gchar *name;
    const gchar *ns;
    GQuark ns_q;
} StanzaTypeName;

static StanzaTypeName type_names[NUM_WOCKY_STANZA_TYPE] =
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

static void
fill_in_namespace_quarks (void)
{
  int i;

  /* We skip the first entry as it's NONE */
  for (i = 1; type_names[i].type != WOCKY_STANZA_TYPE_UNKNOWN; i++)
    type_names[i].ns_q = g_quark_from_static_string (type_names[i].ns);
}

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
wocky_stanza_init (WockyStanza *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_STANZA,
      WockyStanzaPrivate);

  self->priv->from_contact = NULL;
  self->priv->to_contact = NULL;
}

static void wocky_stanza_dispose (GObject *object);
static void wocky_stanza_finalize (GObject *object);

static void
wocky_stanza_class_init (WockyStanzaClass *wocky_stanza_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_stanza_class);

  g_type_class_add_private (wocky_stanza_class, sizeof (WockyStanzaPrivate));

  object_class->dispose = wocky_stanza_dispose;
  object_class->finalize = wocky_stanza_finalize;

  fill_in_namespace_quarks ();
}

static void
wocky_stanza_dispose (GObject *object)
{
  WockyStanza *self = WOCKY_STANZA (object);
  WockyStanzaPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_stanza_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_stanza_parent_class)->dispose (object);
}

static void
wocky_stanza_finalize (GObject *object)
{
  WockyStanza *self = WOCKY_STANZA (object);

  if (self->priv->from_contact != NULL)
    {
      g_object_unref (self->priv->from_contact);
      self->priv->from_contact = NULL;
    }

  if (self->priv->to_contact != NULL)
    {
      g_object_unref (self->priv->to_contact);
      self->priv->to_contact = NULL;
    }

  G_OBJECT_CLASS (wocky_stanza_parent_class)->finalize (object);
}

WockyStanza *
wocky_stanza_new (const gchar *name,
    const gchar *ns)
{
  WockyStanza *result;

  result = WOCKY_STANZA (g_object_new (WOCKY_TYPE_STANZA,
    "top-node", wocky_node_new (name, ns),
    NULL));

  return result;
}

WockyStanza *
wocky_stanza_copy (WockyStanza *old)
{
  WockyNode *top;

  top = _wocky_node_copy (wocky_stanza_get_top_node (old));

  return g_object_new (WOCKY_TYPE_STANZA,
      "top-node", top,
      NULL);
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
  WockyStanzaType expected_type;

  g_return_val_if_fail (type > WOCKY_STANZA_TYPE_NONE &&
      type < NUM_WOCKY_STANZA_TYPE, FALSE);
  g_return_val_if_fail (sub_type < NUM_WOCKY_STANZA_SUB_TYPE, FALSE);

  g_assert (sub_type_names[sub_type].sub_type == sub_type);

  expected_type = sub_type_names[sub_type].type;

  if (expected_type != WOCKY_STANZA_TYPE_NONE && expected_type != type)
    {
      g_critical ("Stanza sub-type '%s' may only be used with stanzas of "
        "type '%s', not of type '%s'", sub_type_names[sub_type].name,
        type_names[expected_type].name, type_names[type].name);
      g_return_val_if_reached (FALSE);
    }

  return TRUE;
}

static WockyStanza *
wocky_stanza_new_with_sub_type (WockyStanzaType type,
    WockyStanzaSubType sub_type)
{
  WockyStanza *stanza = NULL;
  const gchar *sub_type_name;

  if (!check_sub_type (type, sub_type))
    return NULL;

  stanza = wocky_stanza_new (get_type_name (type), get_type_ns (type));

  sub_type_name = get_sub_type_name (sub_type);
  if (sub_type_name != NULL)
    wocky_node_set_attribute (wocky_stanza_get_top_node (stanza),
        "type", sub_type_name);

  return stanza;
}

/**
 * wocky_stanza_build:
 * @type: The type of stanza to build
 * @sub_type: The stanza's subtype; valid values depend on @type. (For instance,
 *           #WOCKY_STANZA_TYPE_IQ can use #WOCKY_STANZA_SUB_TYPE_GET, but not
 *           #WOCKY_STANZA_SUB_TYPE_SUBSCRIBED.)
 * @from: The sender's JID, or %NULL to leave it unspecified.
 * @to: The target's JID, or %NULL to leave it unspecified.
 * @...: the description of the stanza to build,
 *  terminated with %NULL
 *
 * Build a XMPP stanza from a list of arguments. For example, the following invocation:
 *
 * |[
 * wocky_stanza_build (
 *    WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
 *    "alice@<!-- -->collabora.co.uk", "bob@<!-- -->collabora.co.uk",
 *    WOCKY_NODE_START, "html",
 *      WOCKY_NODE_XMLNS, "http://www.w3.org/1999/xhtml",
 *      WOCKY_NODE, "body",
 *        WOCKY_NODE_ATTRIBUTE, "textcolor", "red",
 *        WOCKY_NODE_TEXT, "Telepathy rocks!",
 *      WOCKY_NODE_END,
 *    WOCKY_NODE_END,
 *   NULL);
 * ]|
 *
 * produces this stanza:
 *
 * |[
 * &lt;message from='alice@<!-- -->collabora.co.uk' to='bob@<!-- -->collabora.co.uk'&gt;
 *   &lt;html xmlns='http://www.w3.org/1999/xhtml'&gt;
 *     &lt;body textcolor='red'&gt;
 *       Telepathy rocks!
 *     &lt;/body&gt;
 *   &lt;/html&gt;
 * &lt;/message&gt;
 * ]|
 *
 * You may optionally use mnemonic ASCII characters in place of the build tags,
 * to better reflect the structure of the stanza in C source. For example, the
 * above stanza could be written as:
 *
 * |[
 * wocky_stanza_build (
 *    WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
 *    "alice@<!-- -->collabora.co.uk", "bob@<!-- -->collabora.co.uk",
 *    '(', "html", ':', "http://www.w3.org/1999/xhtml",
 *      '(', "body", '@', "textcolor", "red",
 *        '$', "Telepathy rocks!",
 *      ')',
 *    ')'
 *   NULL);
 * ]|
 *
 * Returns: a new stanza object
 */
WockyStanza *
wocky_stanza_build (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    const gchar *to,
    ...)

{
  WockyStanza *stanza;
  va_list ap;

  va_start (ap, to);
  stanza = wocky_stanza_build_va (type, sub_type, from, to, ap);
  va_end (ap);

  return stanza;
}

WockyStanza *
wocky_stanza_build_to_contact (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    WockyContact *to,
    ...)

{
  WockyStanza *stanza;
  va_list ap;
  gchar *to_jid = NULL;

  if (to != NULL)
    to_jid = wocky_contact_dup_jid (to);

  va_start (ap, to);
  stanza = wocky_stanza_build_va (type, sub_type, from, to_jid, ap);
  va_end (ap);

  g_free (to_jid);

  stanza->priv->to_contact = g_object_ref (to);

  return stanza;
}

WockyStanza *
wocky_stanza_build_va (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    const gchar *to,
    va_list ap)
{
  WockyStanza *stanza;

  g_return_val_if_fail (type < NUM_WOCKY_STANZA_TYPE, NULL);
  g_return_val_if_fail (sub_type < NUM_WOCKY_STANZA_SUB_TYPE, NULL);

  stanza = wocky_stanza_new_with_sub_type (type, sub_type);
  if (stanza == NULL)
    return NULL;

  if (from != NULL)
    wocky_node_set_attribute (wocky_stanza_get_top_node (stanza),
        "from", from);

  if (to != NULL)
    wocky_node_set_attribute (wocky_stanza_get_top_node (stanza),
        "to", to);

  wocky_node_add_build_va (wocky_stanza_get_top_node (stanza), ap);

  return stanza;
}

static WockyStanzaType
get_type_from_node (WockyNode *node)
{
  const gchar *name = node->name;
  GQuark ns = node->ns;
  guint i;

  if (name == NULL)
    return WOCKY_STANZA_TYPE_NONE;

  /* We skip the first entry as it's NONE */
  for (i = 1; i < WOCKY_STANZA_TYPE_UNKNOWN; i++)
    {
       if (type_names[i].name != NULL &&
           ns == type_names[i].ns_q &&
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
wocky_stanza_get_type_info (WockyStanza *stanza,
    WockyStanzaType *type,
    WockyStanzaSubType *sub_type)
{
  WockyNode *top_node;

  g_return_if_fail (stanza != NULL);

  top_node = wocky_stanza_get_top_node (stanza);
  g_assert (top_node != NULL);

  if (type != NULL)
    *type = get_type_from_node (top_node);

  if (sub_type != NULL)
    *sub_type = get_sub_type_from_name (
        wocky_node_get_attribute (top_node, "type"));
}

gboolean
wocky_stanza_has_type (WockyStanza *stanza,
    WockyStanzaType expected_type)
{
  WockyStanzaType actual_type;

  g_return_val_if_fail (WOCKY_IS_STANZA (stanza), FALSE);

  wocky_stanza_get_type_info (stanza, &actual_type, NULL);

  return expected_type == actual_type;
}

static WockyStanza *
create_iq_reply (WockyStanza *iq,
    WockyStanzaSubType sub_type_reply,
    va_list ap)
{
  WockyStanza *reply;
  WockyStanzaType type;
  WockyNode *node;
  WockyStanzaSubType sub_type;
  const gchar *from, *to, *id;
  WockyContact *contact;

  g_return_val_if_fail (iq != NULL, NULL);

  wocky_stanza_get_type_info (iq, &type, &sub_type);
  g_return_val_if_fail (type == WOCKY_STANZA_TYPE_IQ, NULL);
  g_return_val_if_fail (sub_type == WOCKY_STANZA_SUB_TYPE_GET ||
      sub_type == WOCKY_STANZA_SUB_TYPE_SET, NULL);

  node = wocky_stanza_get_top_node (iq);
  from = wocky_node_get_attribute (node, "from");
  to = wocky_node_get_attribute (node, "to");
  id = wocky_node_get_attribute (node, "id");

  if (id == NULL)
    return NULL;

  reply = wocky_stanza_build_va (WOCKY_STANZA_TYPE_IQ,
      sub_type_reply, to, from, ap);

  wocky_node_set_attribute (wocky_stanza_get_top_node (reply), "id", id);

  contact = wocky_stanza_get_from_contact (iq);
  if (contact != NULL)
    wocky_stanza_set_to_contact (reply, contact);

  return reply;
}

WockyStanza *
wocky_stanza_build_iq_result (WockyStanza *iq,
    ...)
{
  WockyStanza *reply;
  va_list ap;

  va_start (ap, iq);
  reply = create_iq_reply (iq, WOCKY_STANZA_SUB_TYPE_RESULT, ap);
  va_end (ap);

  return reply;
}

WockyStanza *
wocky_stanza_build_iq_result_va (
    WockyStanza *iq,
    va_list ap)
{
  return create_iq_reply (iq, WOCKY_STANZA_SUB_TYPE_RESULT, ap);
}

/**
 * wocky_stanza_build_iq_error:
 * @iq: a stanza of type #WOCKY_STANZA_TYPE_IQ and sub-type either
 *      #WOCKY_STANZA_SUB_TYPE_SET or #WOCKY_STANZA_SUB_TYPE_GET
 * @...: a wocky_stanza_build() specification
 *
 * Builds an error reply to @iq containing the given body. This function also
 * adds the child element of @iq to the reply, as recommended by <ulink
 * url='http://xmpp.org/rfcs/rfc3920.html#stanzas-semantics-iq'>RFC3920 §9.2.3
 * ‘IQ Semantics’</ulink>.
 *
 * No <code>&lt;error/&gt;</code> element is added to the reply. To add a
 * standard stanza error, plus message, consider using
 * wocky_stanza_error_to_node(). To add a more complicated error with an
 * application-specific condition, specify it when calling this function. For
 * example:
 *
 * |[
 * WockyStanza *reply = wocky_stanza_build_iq_error (iq,
 *    '(', "error",
 *      '@', "type", "cancel",
 *      '(', "feature-not-implemented", ':', WOCKY_XMPP_NS_STANZAS, ')',
 *      '(', "unsupported", ':', WOCKY_XMPP_NS_PUBSUB_ERRORS,
 *        '@', "feature", "subscribe",
 *      ')',
 *    ')', NULL);
 * ]|
 *
 * Returns: an error reply for @iq
 */
WockyStanza *
wocky_stanza_build_iq_error (WockyStanza *iq,
    ...)
{
  WockyStanza *reply;
  WockyNode *query;
  va_list ap;

  va_start (ap, iq);
  reply = create_iq_reply (iq, WOCKY_STANZA_SUB_TYPE_ERROR, ap);
  va_end (ap);

  /* RFC3920 §9.2.3 dictates:
   *    5. An IQ stanza of type "get" or "set" MUST contain one and only one
   *       child element that specifies the semantics of the particular request
   *       or response.
   *    …
   *    7. An IQ stanza of type "error" SHOULD include the child element
   *       contained in the associated "get" or "set" and MUST include an
   *       <error/> child; for details, see Stanza Errors.
   *
   * So here we take the first child out of the stanza we're replying to, and
   * include it in the error reply. If @iq has more than one child, it
   * was illegal, so we're within our rights to ignore everything after the
   * first. If @iq has no children, it was also illegal, so there's no way we
   * can comply with the SHOULD.
   */
  query = wocky_node_get_first_child (wocky_stanza_get_top_node (iq));

  if (reply != NULL && query != NULL)
    {
      WockyNodeTree *query_tree = wocky_node_tree_new_from_node (query);

      wocky_node_prepend_node_tree (wocky_stanza_get_top_node (reply),
          query_tree);
      g_object_unref (query_tree);
    }

  return reply;
}

WockyStanza *
wocky_stanza_build_iq_error_va (
    WockyStanza *iq,
    va_list ap)
{
  return create_iq_reply (iq, WOCKY_STANZA_SUB_TYPE_ERROR, ap);
}

/**
 * wocky_stanza_extract_errors:
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
wocky_stanza_extract_errors (WockyStanza *stanza,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyNode **specialized_node)
{
  WockyStanzaSubType sub_type;
  WockyNode *error;

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);

  if (sub_type != WOCKY_STANZA_SUB_TYPE_ERROR)
    return FALSE;

  error = wocky_node_get_child (wocky_stanza_get_top_node (stanza),
    "error");

  if (error == NULL)
    {
      if (type != NULL)
        *type = WOCKY_XMPP_ERROR_TYPE_CANCEL;

      g_set_error (core, WOCKY_XMPP_ERROR,
          WOCKY_XMPP_ERROR_UNDEFINED_CONDITION,
          "stanza had type='error' but no <error/> node");

      if (specialized_node != NULL)
        *specialized_node = NULL;
    }
  else
    {
      wocky_xmpp_error_extract (error, type, core, specialized,
          specialized_node);
    }

  return TRUE;
}

/**
 * wocky_stanza_extract_stream_error:
 * @stanza: a stanza
 * @stream_error: location at which to store an error in domain
 *                #WOCKY_XMPP_STREAM_ERROR, if one is found.
 *
 * Returns: %TRUE and sets @stream_error if the stanza was indeed a stream
 *          error.
 */
gboolean
wocky_stanza_extract_stream_error (WockyStanza *stanza,
    GError **stream_error)
{
  if (!wocky_stanza_has_type (stanza, WOCKY_STANZA_TYPE_STREAM_ERROR))
    return FALSE;

  g_propagate_error (stream_error,
      wocky_xmpp_stream_error_from_node (wocky_stanza_get_top_node (stanza)));
  return TRUE;
}

/**
 * wocky_stanza_get_top_node:
 * @self: a stanza
 *
 * Returns: A pointer to the topmost node of the stanza
 */
WockyNode *
wocky_stanza_get_top_node (WockyStanza *self)
{
  return wocky_node_tree_get_top_node (WOCKY_NODE_TREE (self));
}

/**
 * wocky_stanza_get_from:
 * @self: a stanza
 *
 * <!-- moo -->
 *
 * Returns: The sender of @self, or %NULL if no sender was specified.
 */
const gchar *
wocky_stanza_get_from (WockyStanza *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (WOCKY_IS_STANZA (self), NULL);

  return wocky_node_get_attribute (wocky_stanza_get_top_node (self), "from");
}

/**
 * wocky_stanza_get_to:
 * @self: a stanza
 *
 * <!-- moo -->
 *
 * Returns: The recipient of @self, or %NULL if no recipient was specified.
 */
const gchar *
wocky_stanza_get_to (WockyStanza *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (WOCKY_IS_STANZA (self), NULL);

  return wocky_node_get_attribute (wocky_stanza_get_top_node (self), "to");
}

WockyContact *
wocky_stanza_get_to_contact (WockyStanza *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (WOCKY_IS_STANZA (self), NULL);

  return self->priv->to_contact;
}

WockyContact *
wocky_stanza_get_from_contact (WockyStanza *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (WOCKY_IS_STANZA (self), NULL);

  return self->priv->from_contact;
}

void
wocky_stanza_set_to_contact (WockyStanza *self,
    WockyContact *contact)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (WOCKY_IS_STANZA (self));
  g_return_if_fail (WOCKY_IS_CONTACT (contact));

  if (self->priv->to_contact != NULL)
    g_object_unref (self->priv->to_contact);

  self->priv->to_contact = g_object_ref (contact);
}

void
wocky_stanza_set_from_contact (WockyStanza *self,
    WockyContact *contact)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (WOCKY_IS_STANZA (self));
  g_return_if_fail (WOCKY_IS_CONTACT (contact));

  if (self->priv->from_contact != NULL)
    g_object_unref (self->priv->from_contact);

  self->priv->from_contact = g_object_ref (contact);
}
