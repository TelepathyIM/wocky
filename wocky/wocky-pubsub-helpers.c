/*
 * wocky-pubsub-helpers.c — PubSub helper functions
 * Copyright © 2009–2012 Collabora Ltd.
 * Copyright © 2010 Nokia Corporation
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

#include "wocky-pubsub-helpers.h"

#include "wocky-namespaces.h"
#include "wocky-pubsub-service.h"
#include "wocky-session.h"
#include "wocky-xep-0115-capabilities.h"


/**
 * wocky_pubsub_make_event_stanza:
 * @node: the the name of the pubsub node; may not be %NULL
 * @from: a JID to use as the 'from' attribute, or %NULL
 * @item_out: a location to store the <code>item</code> #WockyNode, or %NULL
 *
 * Generates a new message stanza to send to other contacts about an
 * updated PEP node.
 *
 * Note that this should only be used in link-local
 * connections. Regular pubsub consists of making a publish stanza
 * with wocky_pubsub_make_publish_stanza() and sending it to your own
 * server. The server will then send the event stanza on to your
 * contacts who have the appropriate capability.
 *
 * Returns: a new #WockyStanza pubsub event stanza; free with g_object_unref()
 */
WockyStanza *
wocky_pubsub_make_event_stanza (const gchar *node,
    const gchar *from,
    WockyNode **item_out)
{
  WockyStanza *stanza;
  WockyNode *message, *event, *items, *item;

  g_return_val_if_fail (node != NULL, NULL);

  stanza = wocky_stanza_build (WOCKY_STANZA_TYPE_MESSAGE,
      WOCKY_STANZA_SUB_TYPE_HEADLINE, from, NULL,
      '(', "event",
        ':', WOCKY_XMPP_NS_PUBSUB_EVENT,
        '(', "items",
          '@', "node", node,
            '(', "item",
            ')',
        ')',
      ')', NULL);

  message = wocky_stanza_get_top_node (stanza);
  event = wocky_node_get_first_child (message);
  items = wocky_node_get_first_child (event);
  item = wocky_node_get_first_child (items);

  if (item_out != NULL)
    *item_out = item;

  return stanza;
}

/**
 * wocky_pubsub_make_publish_stanza:
 * @service: the JID of a PubSub service, or %NULL
 * @node: the name of a node on @service; may not be %NULL
 * @pubsub_out: address at which to store a pointer to the &lt;pubsub/&gt; node
 * @publish_out: address at which to store a pointer to the &lt;publish/&gt;
 *               node
 * @item_out: address at which to store a pointer to the &lt;item/&gt; node
 *
 * <!-- -->
 *
 * Returns: a new iq[type='set']/pubsub/publish/item stanza
 */
WockyStanza *
wocky_pubsub_make_publish_stanza (
    const gchar *service,
    const gchar *node,
    WockyNode **pubsub_out,
    WockyNode **publish_out,
    WockyNode **item_out)
{
  WockyStanza *stanza;
  WockyNode *publish, *item;

  g_return_val_if_fail (node != NULL, NULL);

  stanza = wocky_pubsub_make_stanza (service, WOCKY_STANZA_SUB_TYPE_SET,
      WOCKY_XMPP_NS_PUBSUB, "publish", pubsub_out, &publish);

  wocky_node_set_attribute (publish, "node", node);
  item = wocky_node_add_child (publish, "item");

  if (publish_out != NULL)
    *publish_out = publish;

  if (item_out != NULL)
    *item_out = item;

  return stanza;
}

/**
 * wocky_pubsub_make_stanza:
 * @service: the JID of a PubSub service, or %NULL
 * @sub_type: #WOCKY_STANZA_SUB_TYPE_SET or #WOCKY_STANZA_SUB_TYPE_GET, as you wish
 * @pubsub_ns: the namespace for the &lt;pubsub/&gt; node of the stanza
 * @action_name: the action node to add to &lt;pubsub/&gt;
 * @pubsub_node: address at which to store a pointer to the &lt;pubsub/&gt; node
 * @action_node: address at wihch to store a pointer to the &lt;@action/&gt;
 *               node
 *
 * <!-- -->
 *
 * Returns: a new iq[type=@sub_type]/pubsub/@action stanza
 */
WockyStanza *
wocky_pubsub_make_stanza (
    const gchar *service,
    WockyStanzaSubType sub_type,
    const gchar *pubsub_ns,
    const gchar *action_name,
    WockyNode **pubsub_node,
    WockyNode **action_node)
{
  WockyStanza *stanza;
  WockyNode *pubsub, *action;

  g_assert (pubsub_ns != NULL);
  g_assert (action_name != NULL);

  stanza = wocky_stanza_build (
      WOCKY_STANZA_TYPE_IQ, sub_type,
      NULL, service,
        '(', "pubsub",
          ':', pubsub_ns,
          '*', &pubsub,
          '(', action_name,
            '*', &action,
          ')',
        ')',
      NULL);

  if (pubsub_node != NULL)
    *pubsub_node = pubsub;

  if (action_node != NULL)
    *action_node = action;

  return stanza;
}

static void
send_stanza_to_contact (WockyPorter *porter,
    WockyContact *contact,
    WockyStanza *stanza)
{
  WockyStanza *to_send = wocky_stanza_copy (stanza);

  wocky_stanza_set_to_contact (to_send, contact);
  wocky_porter_send (porter, to_send);
  g_object_unref (to_send);
}

/**
 * wocky_send_ll_pep_event:
 * @session: the WockySession to send on
 * @stanza: the PEP event stanza to send
 *
 * Send a PEP event to all link-local contacts interested in receiving it.
 */
void
wocky_send_ll_pep_event (WockySession *session,
    WockyStanza *stanza)
{
  WockyContactFactory *contact_factory;
  WockyPorter *porter;
  WockyLLContact *self_contact;
  GList *contacts, *l;
  WockyNode *message, *event, *items;
  const gchar *pep_node;
  gchar *node;

  g_return_if_fail (WOCKY_IS_SESSION (session));
  g_return_if_fail (WOCKY_IS_STANZA (stanza));

  message = wocky_stanza_get_top_node (stanza);
  event = wocky_node_get_first_child (message);
  items = wocky_node_get_first_child (event);

  pep_node = wocky_node_get_attribute (items, "node");

  if (pep_node == NULL)
    return;

  node = g_strdup_printf ("%s+notify", pep_node);

  contact_factory = wocky_session_get_contact_factory (session);
  porter = wocky_session_get_porter (session);

  contacts = wocky_contact_factory_get_ll_contacts (contact_factory);

  for (l = contacts; l != NULL; l = l->next)
    {
      WockyXep0115Capabilities *contact;

      if (!WOCKY_IS_XEP_0115_CAPABILITIES (l->data))
        continue;

      contact = l->data;

      if (wocky_xep_0115_capabilities_has_feature (contact, node))
        send_stanza_to_contact (porter, WOCKY_CONTACT (contact), stanza);
    }

  /* now send to self */
  self_contact = wocky_contact_factory_ensure_ll_contact (contact_factory,
      wocky_porter_get_full_jid (porter));

  send_stanza_to_contact (porter, WOCKY_CONTACT (self_contact), stanza);

  g_object_unref (self_contact);
  g_list_free (contacts);
  g_free (node);
}

static gboolean
get_pubsub_child_node (WockyStanza *reply,
    const gchar *pubsub_ns,
    const gchar *child_name,
    WockyNodeTree **child_out,
    GError **error)
{
  WockyNode *n;

  g_return_val_if_fail (reply != NULL, FALSE);

  n = wocky_node_get_child_ns (
    wocky_stanza_get_top_node (reply), "pubsub", pubsub_ns);

  if (n == NULL)
    {
      g_set_error (error, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "Reply doesn't contain &lt;pubsub/&gt; node");
      return FALSE;
    }

  n = wocky_node_get_child (n, child_name);

  if (n == NULL)
    {
      g_set_error (error, WOCKY_PUBSUB_SERVICE_ERROR,
          WOCKY_PUBSUB_SERVICE_ERROR_WRONG_REPLY,
          "Reply doesn't contain <%s/> node", child_name);
      return FALSE;
    }

  if (child_out != NULL)
    *child_out = wocky_node_tree_new_from_node (n);

  return TRUE;
}

static gboolean
wocky_pubsub_distill_iq_reply_internal (GObject *source,
    GAsyncResult *res,
    const gchar *pubsub_ns,
    const gchar *child_name,
    gboolean body_optional,
    WockyNodeTree **child_out,
    GError **error)
{
  WockyStanza *reply;
  gboolean ret = FALSE;

  if (child_out != NULL)
    *child_out = NULL;

  /* Superlative news out of the 00:04 to Cambridge: earlier today, an
   * asynchronous method call announced its plans to bring a node to The
   * People's Republic of Wocky.
   */
  reply = wocky_porter_send_iq_finish (WOCKY_PORTER (source), res, error);

  if (reply != NULL)
    {
      if (!wocky_stanza_extract_errors (reply, NULL, error, NULL, NULL))
        {
          if (pubsub_ns == NULL)
            ret = TRUE;
          else
            ret = wocky_pubsub_distill_stanza (reply, pubsub_ns, child_name,
                body_optional, child_out, error);
        }

      g_object_unref (reply);
    }

  return ret;
}

/**
 * wocky_pubsub_distill_stanza:
 * @result: an iq type='result'
 * @pubsub_ns: the namespace of the &lt;pubsub/&gt; node expected in this reply
 *             (such as #WOCKY_XMPP_NS_PUBSUB)
 * @child_name: the name of the child of &lt;pubsub/&gt; expected in this reply
 *              (such as "subscriptions")
 * @body_optional: If %TRUE, the child being absent is not considered an error
 * @child_out: location at which to store a reference to the node tree at
 *             @child_name, if it is found, or to be set to %NULL if it is not.
 * @error: location at which to store an error if the child node is not found
 *         and @body_optional is %FALSE
 *
 * Helper function to extract a particular pubsub child node from a reply, if
 * it is present. If @body_optional is %FALSE, the
 * &lt;pubsub&gt;&lt;@child_name/&gt;&lt;/pubsub&gt; tree being absent is not
 * considered an error: @child_out is set to %NULL and the function returns
 * %TRUE.
 *
 * If you are happy to delegate calling wocky_porter_send_iq_finish() and
 * extracting stanza errors, you would probably be better served by one of
 * wocky_pubsub_distill_iq_reply() or
 * wocky_pubsub_distill_ambivalent_iq_reply().
 *
 * Returns: %TRUE if the child was found or was optional; %FALSE with @error
 *          set otherwise.
 */
gboolean
wocky_pubsub_distill_stanza (WockyStanza *result,
    const gchar *pubsub_ns,
    const gchar *child_name,
    gboolean body_optional,
    WockyNodeTree **child_out,
    GError **error)
{
  g_return_val_if_fail (pubsub_ns != NULL, FALSE);
  g_return_val_if_fail (child_name != NULL, FALSE);

  if (child_out != NULL)
    *child_out = NULL;

  /* A force of a thousand function calls will anchor the node to
   * a resplendent out parameter modeled on the Dear Leader's hand.
   */
  if (get_pubsub_child_node (result, pubsub_ns, child_name, child_out, error))
    {
      /* The People's Great and Harmonious Node Pointer of Peter
       * Saint-Andre will conclude the most astonishing stanza breakdown
       * ever witnessed by man.
       */
      return TRUE;
    }
  else if (body_optional)
    {
      /* “The stanza is perfect. We have already succeeded.” */
      g_clear_error (error);
      return TRUE;
    }
  else
    {
      /* Meanwhile, the American president today revealed himself to be a
       * lizard.
       */
      return FALSE;
    }
}

/**
 * wocky_pubsub_distill_iq_reply:
 * @source: a #WockyPorter instance
 * @res: a result passed to the callback for wocky_porter_send_iq_async()
 * @pubsub_ns: the namespace of the &lt;pubsub/&gt; node expected in this reply
 *             (such as #WOCKY_XMPP_NS_PUBSUB), or %NULL if one is not expected
 * @child_name: the name of the child of &lt;pubsub/&gt; expected in this reply
 *              (such as "subscriptions"); ignored if @pubsub_ns is %NULL
 * @child_out: location at which to store a reference to the node tree at
 *             @child_name, or %NULL if you don't need it.
 * @error: location at which to store an error if the call to
 *         wocky_porter_send_iq_async() returned an error, or if the reply was
 *         an error
 *
 * Helper function to finish a wocky_porter_send_iq_async() operation
 * and extract a particular pubsub child from the resulting reply, if needed.
 *
 * Returns: %TRUE if the desired pubsub child was found; %FALSE if
 *          sending the IQ failed, the reply had type='error', or the
 *          pubsub child was not found, with @error set appropriately.
 */
gboolean
wocky_pubsub_distill_iq_reply (GObject *source,
    GAsyncResult *res,
    const gchar *pubsub_ns,
    const gchar *child_name,
    WockyNodeTree **child_out,
    GError **error)
{
  return wocky_pubsub_distill_iq_reply_internal (source, res, pubsub_ns,
      child_name, FALSE, child_out, error);
}

/**
 * wocky_pubsub_distill_void_iq_reply:
 * @source: a #WockyPorter instance
 * @res: a result passed to the callback for wocky_porter_send_iq_async()
 * @error: location at which to store an error if the call to
 *         wocky_porter_send_iq_async() returned an error, or if the reply was
 *         an error
 *
 * Helper function to finish a wocky_porter_send_iq_async() operation where no
 * pubsub child is expected in the resulting reply.
 *
 * Returns: %TRUE if the IQ was a success; %FALSE if
 *          sending the IQ failed or the reply had type='error',
 *          with @error set appropriately.
 */
gboolean
wocky_pubsub_distill_void_iq_reply (GObject *source,
    GAsyncResult *res,
    GError **error)
{
  return wocky_pubsub_distill_iq_reply_internal (source, res, NULL, NULL, TRUE,
      NULL, error);
}

/**
 * wocky_pubsub_distill_ambivalent_iq_reply:
 * @source: a #WockyPorter instance
 * @res: a result passed to the callback for wocky_porter_send_iq_async()
 * @pubsub_ns: the namespace of the &lt;pubsub/&gt; node accepted in this reply
 *             (such as #WOCKY_XMPP_NS_PUBSUB)
 * @child_name: the name of the child of &lt;pubsub/&gt; accepted in this reply
 *              (such as "subscriptions")
 * @child_out: location at which to store a reference to the node tree at
 *             @child_name, if it is found, or to be set to %NULL if it is not
 *             found
 * @error: location at which to store an error if the call to
 *         wocky_porter_send_iq_async() returned an error, or if the reply was
 *         an error
 *
 * Helper function to finish a wocky_porter_send_iq_async() operation
 * and extract a particular pubsub child from the resulting reply, if it is
 * present. This is like wocky_pubsub_distill_iq_reply(), but is ambivalent as
 * to whether the &lt;pubsub/&gt; structure has to be included.
 *
 * Returns: %TRUE if the IQ was a success; %FALSE if
 *          sending the IQ failed or the reply had type='error',
 *          with @error set appropriately.
 */
gboolean
wocky_pubsub_distill_ambivalent_iq_reply (GObject *source,
    GAsyncResult *res,
    const gchar *pubsub_ns,
    const gchar *child_name,
    WockyNodeTree **child_out,
    GError **error)
{
  return wocky_pubsub_distill_iq_reply_internal (source, res, pubsub_ns,
      child_name, TRUE, child_out, error);
}
