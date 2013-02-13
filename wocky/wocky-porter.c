/*
 * wocky-porter.c - Source for WockyPorter
 * Copyright (C) 2009-2011 Collabora Ltd.
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

#include "wocky-porter.h"

#include "wocky-signals-marshal.h"
#include "wocky-xmpp-connection.h"

G_DEFINE_INTERFACE (WockyPorter, wocky_porter, G_TYPE_OBJECT)

static void
wocky_porter_default_init (WockyPorterInterface *iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (iface);
  static gsize initialization_value = 0;
  GParamSpec *spec;

  if (g_once_init_enter (&initialization_value))
    {
      /**
       * WockyPorter:connection:
       *
       * The underlying #WockyXmppConnection wrapped by the #WockyPorter
       */
      spec = g_param_spec_object ("connection", "XMPP connection",
          "the XMPP connection used by this porter",
          WOCKY_TYPE_XMPP_CONNECTION,
          G_PARAM_READWRITE |
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
      g_object_interface_install_property (iface, spec);

      /**
       * WockyPorter:full-jid:
       *
       * The user's full JID (node&commat;domain/resource).
       */
      spec = g_param_spec_string ("full-jid", "Full JID",
          "The user's own full JID (node@domain/resource)",
          NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
      g_object_interface_install_property (iface, spec);

      /**
       * WockyPorter:bare-jid:
       *
       * The user's bare JID (node&commat;domain).
       */
      spec = g_param_spec_string ("bare-jid", "Bare JID",
          "The user's own bare JID (node@domain)",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
      g_object_interface_install_property (iface, spec);

      /**
       * WockyPorter:resource:
       *
       * The resource part of the user's full JID, or %NULL if their full JID does
       * not contain a resource at all.
       */
      spec = g_param_spec_string ("resource", "Resource", "The user's resource",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
      g_object_interface_install_property (iface, spec);

      /**
       * WockyPorter::remote-closed:
       * @porter: the object on which the signal is emitted
       *
       * The ::remote-closed signal is emitted when the other side closed the XMPP
       * stream.
       */
      g_signal_new ("remote-closed", iface_type,
          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);

      /**
       * WockyPorter::remote-error:
       * @porter: the object on which the signal is emitted
       * @domain: error domain (a #GQuark)
       * @code: error code
       * @message: human-readable informative error message
       *
       * The ::remote-error signal is emitted when an error has been detected
       * on the XMPP stream.
       */
      g_signal_new ("remote-error", iface_type,
          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
          _wocky_signals_marshal_VOID__UINT_INT_STRING,
          G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);

      /**
       * WockyPorter::closing:
       * @porter: the object on which the signal is emitted
       *
       * The ::closing signal is emitted when the #WockyPorter starts to close its
       * XMPP connection. Once this signal has been emitted, the #WockyPorter
       * can't be used to send stanzas any more.
       */
      g_signal_new ("closing", iface_type,
          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);

      /**
       * WockyPorter::sending:
       * @porter: the object on which the signal is emitted
       * @stanza: the #WockyStanza being sent, or %NULL if @porter is just
       *    sending whitespace
       *
       * The ::sending signal is emitted whenever #WockyPorter sends data
       * on the XMPP connection.
       */
      g_signal_new ("sending", iface_type,
          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1, WOCKY_TYPE_STANZA);

      g_once_init_leave (&initialization_value, 1);
    }
}

/**
 * wocky_porter_error_quark:
 *
 * Get the error quark used by the porter.
 *
 * Returns: the quark for porter errors.
 */
GQuark
wocky_porter_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-porter-error");

  return quark;
}

/**
 * wocky_porter_get_full_jid: (skip)
 * @self: a porter
 *
 * <!-- nothing more to say -->
 *
 * Returns: (transfer none): the value of #WockyPorter:full-jid
 */
const gchar *
wocky_porter_get_full_jid (WockyPorter *self)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), NULL);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->get_full_jid != NULL);

  return iface->get_full_jid (self);
}

/**
 * wocky_porter_get_bare_jid: (skip)
 * @self: a porter
 *
 * <!-- nothing more to say -->
 *
 * Returns: (transfer none): the value of #WockyPorter:bare-jid
 */
const gchar *
wocky_porter_get_bare_jid (WockyPorter *self)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), NULL);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->get_bare_jid != NULL);

  return iface->get_bare_jid (self);
}

/**
 * wocky_porter_get_resource: (skip)
 * @self: a porter
 *
 * <!-- nothing more to say -->
 *
 * Returns: (transfer none): the value of #WockyPorter:resource
 */
const gchar *
wocky_porter_get_resource (WockyPorter *self)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), NULL);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->get_resource != NULL);

  return iface->get_resource (self);
}

/**
 * wocky_porter_start:
 * @porter: a #WockyPorter
 *
 * Start a #WockyPorter to make it read and dispatch incoming stanzas.
 */
void
wocky_porter_start (WockyPorter *self)
{
  WockyPorterInterface *iface;

  g_return_if_fail (WOCKY_IS_PORTER (self));

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->start != NULL);

  iface->start (self);
}

/**
 * wocky_porter_send_async:
 * @porter: a #WockyPorter
 * @stanza: the #WockyStanza to send
 * @cancellable: optional #GCancellable object, %NULL <!-- --> to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request asynchronous sending of a #WockyStanza.
 * When the stanza has been sent callback will be called.
 * You can then call wocky_porter_send_finish() to get the result
 * of the operation.
 */
void
wocky_porter_send_async (WockyPorter *self,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterInterface *iface;

  g_return_if_fail (WOCKY_IS_PORTER (self));

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->send_async != NULL);

  iface->send_async (self, stanza, cancellable, callback, user_data);
}

/**
 * wocky_porter_send_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL <!-- -->to
 * ignore.
 *
 * Finishes sending a #WockyStanza.
 *
 * Returns: %TRUE on success or %FALSE on error.
 */
gboolean
wocky_porter_send_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), FALSE);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->send_finish != NULL);

  return iface->send_finish (self, result, error);
}

/**
 * wocky_porter_send:
 * @porter: a #WockyPorter
 * @stanza: the #WockyStanza to send
 *
 * Send a #WockyStanza.  This is a convenient function to not have to
 * call wocky_porter_send_async() with lot of %NULL arguments if you
 * don't care to know when the stanza has been actually sent.
 */
void
wocky_porter_send (WockyPorter *porter,
    WockyStanza *stanza)
{
  wocky_porter_send_async (porter, stanza, NULL, NULL, NULL);
}


/**
 * wocky_porter_register_handler_from_va:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @from: the JID whose messages this handler is intended for (may not be
 *  %NULL)
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @ap: a wocky_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  passed to this function, as per wocky_node_is_superset().
 *
 * A <type>va_list</type> version of wocky_porter_register_handler_from(); see
 * that function for more details.
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler_from_va (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    va_list ap)
{
  guint ret;
  WockyStanza *stanza;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);
  g_return_val_if_fail (from != NULL, 0);

  if (type == WOCKY_STANZA_TYPE_NONE)
    {
      stanza = NULL;
      g_return_val_if_fail (
          (va_arg (ap, WockyNodeBuildTag) == 0) &&
          "Pattern-matching is not supported when matching stanzas "
          "of any type", 0);
    }
  else
    {
      stanza = wocky_stanza_build_va (type, WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL, ap);
      g_assert (stanza != NULL);
    }

  ret = wocky_porter_register_handler_from_by_stanza (self, type, sub_type,
      from,
      priority, callback, user_data, stanza);

  if (stanza != NULL)
    g_object_unref (stanza);

  return ret;
}

/**
 * wocky_porter_register_handler_from_by_stanza:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @from: the JID whose messages this handler is intended for (may not be
 *  %NULL)
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @stanza: a #WockyStanza. The handler will match a stanza only if
 *  the stanza received is a superset of the one passed to this
 *  function, as per wocky_node_is_superset().
 *
 * A #WockyStanza version of wocky_porter_register_handler_from(); see
 * that function for more details.
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler_from_by_stanza (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);
  g_return_val_if_fail (from != NULL, 0);

  if (type == WOCKY_STANZA_TYPE_NONE)
    g_return_val_if_fail (stanza == NULL, 0);
  else
    g_return_val_if_fail (WOCKY_IS_STANZA (stanza), 0);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->register_handler_from_by_stanza != NULL);

  return iface->register_handler_from_by_stanza (self, type, sub_type,
      from, priority, callback, user_data, stanza);
}

/**
 * wocky_porter_register_handler_from:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @from: the JID whose messages this handler is intended for (may not be
 *  %NULL)
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @...: a wocky_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  passed to this function, as per wocky_node_is_superset().
 *
 * Register a new stanza handler.
 * Stanza handlers are called when the Porter receives a new stanza matching
 * the rules of the handler. Matching handlers are sorted by priority and are
 * called until one claims to have handled the stanza (by returning %TRUE).
 *
 * If @from is a bare JID, then the resource of the JID in the from attribute
 * will be ignored: In other words, a handler registered against a bare JID
 * will match <emphasis>all</emphasis> stanzas from a JID with the same node
 * and domain:
 * <code>"foo&commat;bar.org"</code> will match
 * <code>"foo&commat;bar.org"</code>,
 * <code>"foo&commat;bar.org/moose"</code> and so forth.
 *
 * To register an IQ handler from Juliet for all the Jingle stanzas related
 * to one Jingle session:
 *
 * |[
 * id = wocky_porter_register_handler_from (porter,
 *   WOCKY_STANZA_TYPE_IQ, WOCKY_STANZA_SUB_TYPE_NONE,
 *   "juliet@example.com/Balcony",
 *   WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
 *   jingle_cb,
 *   '(', "jingle",
 *     ':', "urn:xmpp:jingle:1",
 *     '@', "sid", "my_sid",
 *   ')', NULL);
 * ]|
 *
 * To match stanzas from any sender, see
 * wocky_porter_register_handler_from_anyone(). If the porter is a
 * #WockyC2SPorter, one can match stanzas sent by the server; see
 * wocky_c2s_porter_register_handler_from_server().
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler_from (WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    ...)
{
  va_list ap;
  guint ret;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);
  g_return_val_if_fail (from != NULL, 0);

  va_start (ap, user_data);
  ret = wocky_porter_register_handler_from_va (self, type, sub_type, from,
      priority, callback, user_data, ap);
  va_end (ap);

  return ret;
}

/**
 * wocky_porter_register_handler_from_anyone_va:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @ap: a wocky_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  passed to this function, as per wocky_node_is_superset().
 *
 * A <type>va_list</type> version of
 * wocky_porter_register_handler_from_anyone(); see that function for more
 * details.
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler_from_anyone_va (
    WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    va_list ap)
{
  guint ret;
  WockyStanza *stanza;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);

  if (type == WOCKY_STANZA_TYPE_NONE)
    {
      stanza = NULL;
      g_return_val_if_fail (
          (va_arg (ap, WockyNodeBuildTag) == 0) &&
          "Pattern-matching is not supported when matching stanzas "
          "of any type", 0);
    }
  else
    {
      stanza = wocky_stanza_build_va (type, WOCKY_STANZA_SUB_TYPE_NONE,
          NULL, NULL, ap);
      g_assert (stanza != NULL);
    }

  ret = wocky_porter_register_handler_from_anyone_by_stanza (self, type,
      sub_type, priority, callback, user_data, stanza);

  if (stanza != NULL)
    g_object_unref (stanza);

  return ret;
}

/**
 * wocky_porter_register_handler_from_anyone_by_stanza:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @stanza: a #WockyStanza. The handler will match a stanza only if
 *  the stanza received is a superset of the one passed to this
 *  function, as per wocky_node_is_superset().
 *
 * A #WockyStanza version of
 * wocky_porter_register_handler_from_anyone(); see that function for
 * more details.
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler_from_anyone_by_stanza (
    WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    WockyStanza *stanza)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);

  if (type == WOCKY_STANZA_TYPE_NONE)
    g_return_val_if_fail (stanza == NULL, 0);
  else
    g_return_val_if_fail (WOCKY_IS_STANZA (stanza), 0);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->register_handler_from_anyone_by_stanza != NULL);

  return iface->register_handler_from_anyone_by_stanza (self, type, sub_type,
      priority, callback, user_data, stanza);
}

/**
 * wocky_porter_register_handler_from_anyone:
 * @self: A #WockyPorter instance (passed to @callback).
 * @type: The type of stanza to be handled, or WOCKY_STANZA_TYPE_NONE to match
 *  any type of stanza.
 * @sub_type: The subtype of stanza to be handled, or
 *  WOCKY_STANZA_SUB_TYPE_NONE to match any type of stanza.
 * @priority: a priority between %WOCKY_PORTER_HANDLER_PRIORITY_MIN and
 *  %WOCKY_PORTER_HANDLER_PRIORITY_MAX (often
 *  %WOCKY_PORTER_HANDLER_PRIORITY_NORMAL). Handlers with a higher priority
 *  (larger number) are called first.
 * @callback: A #WockyPorterHandlerFunc, which should return %FALSE to decline
 *  the stanza (Wocky will continue to the next handler, if any), or %TRUE to
 *  stop further processing.
 * @user_data: Passed to @callback.
 * @...: a wocky_stanza_build() specification. The handler
 *  will match a stanza only if the stanza received is a superset of the one
 *  passed to this function, as per wocky_node_is_superset().
 *
 * Registers a handler for incoming stanzas from anyone, including those where
 * the from attribute is missing.
 *
 * For example, to register a handler matching all message stanzas received
 * from anyone, call:
 *
 * |[
 * id = wocky_porter_register_handler (porter,
 *   WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE, NULL,
 *   WOCKY_PORTER_HANDLER_PRIORITY_NORMAL, message_received_cb, NULL,
 *   NULL);
 * ]|
 *
 * As a more interesting example, the following matches incoming PEP
 * notifications for contacts' geolocation information:
 *
 * |[
 * id = wocky_porter_register_handler_from_anyone (porter,
 *    WOCKY_STANZA_TYPE_MESSAGE, WOCKY_STANZA_SUB_TYPE_NONE,
 *    WOCKY_PORTER_HANDLER_PRIORITY_MAX,
 *    msg_event_cb, self,
 *    '(', "event",
 *      ':', WOCKY_XMPP_NS_PUBSUB_EVENT,
 *      '(', "items",
 *        '@', "node", "http://jabber.org/protocol/geoloc",
 *      ')',
 *    ')',
 *    NULL);
 * ]|
 *
 * Returns: a non-zero ID for use with wocky_porter_unregister_handler().
 */
guint
wocky_porter_register_handler_from_anyone (
    WockyPorter *self,
    WockyStanzaType type,
    WockyStanzaSubType sub_type,
    guint priority,
    WockyPorterHandlerFunc callback,
    gpointer user_data,
    ...)
{
  va_list ap;
  guint ret;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), 0);

  va_start (ap, user_data);
  ret = wocky_porter_register_handler_from_anyone_va (self, type, sub_type,
      priority, callback, user_data, ap);
  va_end (ap);

  return ret;
}

/**
 * wocky_porter_unregister_handler:
 * @porter: a #WockyPorter
 * @id: the id of the handler to unregister
 *
 * Unregister a registered handler. This handler won't be called when
 * receiving stanzas anymore.
 */
void
wocky_porter_unregister_handler (WockyPorter *self,
    guint id)
{
  WockyPorterInterface *iface;

  g_return_if_fail (WOCKY_IS_PORTER (self));

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->unregister_handler != NULL);

  iface->unregister_handler (self, id);
}

/**
 * wocky_porter_close_async:
 * @porter: a #WockyPorter
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request asynchronous closing of a #WockyPorter. This fires the
 * WockyPorter::closing signal, flushes the sending queue, closes the XMPP
 * stream and waits that the other side closes the XMPP stream as well.
 * When this is done, @callback is called.
 * You can then call wocky_porter_close_finish() to get the result of
 * the operation.
 */
void
wocky_porter_close_async (WockyPorter *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterInterface *iface;

  g_return_if_fail (WOCKY_IS_PORTER (self));

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->close_async != NULL);

  iface->close_async (self, cancellable, callback, user_data);
}

/**
 * wocky_porter_close_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore.
 *
 * Finishes a close operation.
 *
 * Returns: %TRUE on success or %FALSE on error.
 */
gboolean
wocky_porter_close_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), FALSE);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->close_finish != NULL);

  return iface->close_finish (self, result, error);
}

/**
 * wocky_porter_send_iq_async:
 * @porter: a #WockyPorter
 * @stanza: the #WockyStanza to send
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Request asynchronous sending of a #WockyStanza of type
 * %WOCKY_STANZA_TYPE_IQ and sub-type %WOCKY_STANZA_SUB_TYPE_GET or
 * %WOCKY_STANZA_SUB_TYPE_SET.
 * When the reply to this IQ has been received callback will be called.
 * You can then call #wocky_porter_send_iq_finish to get the reply stanza.
 */
void
wocky_porter_send_iq_async (WockyPorter *self,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterInterface *iface;

  g_return_if_fail (WOCKY_IS_PORTER (self));

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->send_iq_async != NULL);

  iface->send_iq_async (self, stanza, cancellable, callback, user_data);
}

/**
 * wocky_porter_send_iq_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore.
 *
 * Get the reply of an IQ query.
 *
 * Returns: a reffed #WockyStanza on success, %NULL on error
 */
WockyStanza *
wocky_porter_send_iq_finish (WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), FALSE);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->send_iq_finish != NULL);

  return iface->send_iq_finish (self, result, error);
}

/**
 * wocky_porter_acknowledge_iq:
 * @porter: a #WockyPorter
 * @stanza: a stanza of type #WOCKY_STANZA_TYPE_IQ and sub-type either
 *          #WOCKY_STANZA_SUB_TYPE_SET or #WOCKY_STANZA_SUB_TYPE_GET
 * @...: a wocky_stanza_build() specification; pass %NULL to include no
 *           body in the reply.
 *
 * Sends an acknowledgement for @stanza back to the sender, as a shorthand for
 * calling wocky_stanza_build_iq_result() and wocky_porter_send().
 */
void
wocky_porter_acknowledge_iq (
    WockyPorter *porter,
    WockyStanza *stanza,
    ...)
{
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyStanza *result;
  va_list ap;

  g_return_if_fail (WOCKY_IS_PORTER (porter));
  g_return_if_fail (WOCKY_IS_STANZA (stanza));

  wocky_stanza_get_type_info (stanza, &type, &sub_type);
  g_return_if_fail (type == WOCKY_STANZA_TYPE_IQ);
  g_return_if_fail (sub_type == WOCKY_STANZA_SUB_TYPE_GET ||
      sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  va_start (ap, stanza);
  result = wocky_stanza_build_iq_result_va (stanza, ap);
  va_end (ap);

  if (result != NULL)
    {
      wocky_porter_send (porter, result);
      g_object_unref (result);
    }
}

/**
 * wocky_porter_send_iq_error:
 * @porter: the porter whence @stanza came
 * @stanza: a stanza of type %WOCKY_STANZA_TYPE_IQ and sub-type either
 *          #WOCKY_STANZA_SUB_TYPE_SET or #WOCKY_STANZA_SUB_TYPE_GET
 * @error_code: an XMPP Core stanza error code
 * @message: (allow-none): an optional error message to include with the reply.
 *
 * Sends an error reply for @stanza back to its sender, with the given
 * @error_code and @message, and including the child element from the original
 * stanza.
 *
 * To send error replies with more detailed error elements, see
 * wocky_porter_send_iq_gerror(), or use wocky_stanza_build_iq_error() and
 * wocky_porter_send() directly, possibly using wocky_stanza_error_to_node() to
 * construct the error element.
 */
void
wocky_porter_send_iq_error (
    WockyPorter *porter,
    WockyStanza *stanza,
    WockyXmppError error_code,
    const gchar *message)
{
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  GError *error = NULL;

  g_return_if_fail (WOCKY_IS_PORTER (porter));
  g_return_if_fail (WOCKY_IS_STANZA (stanza));

  wocky_stanza_get_type_info (stanza, &type, &sub_type);
  g_return_if_fail (type == WOCKY_STANZA_TYPE_IQ);
  g_return_if_fail (sub_type == WOCKY_STANZA_SUB_TYPE_GET ||
      sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  g_return_if_fail (error_code < NUM_WOCKY_XMPP_ERRORS);

  error = g_error_new_literal (WOCKY_XMPP_ERROR, error_code,
      message != NULL ? message : "");
  wocky_porter_send_iq_gerror (porter, stanza, error);
  g_clear_error (&error);
}

/**
 * wocky_porter_send_iq_gerror:
 * @porter: the porter whence @stanza came
 * @stanza: a stanza of type %WOCKY_STANZA_TYPE_IQ and sub-type either
 *          #WOCKY_STANZA_SUB_TYPE_SET or #WOCKY_STANZA_SUB_TYPE_GET
 * @error: an error whose domain is either %WOCKY_XMPP_ERROR, some other stanza
 *         error domain supplied with Wocky (such as %WOCKY_JINGLE_ERROR or
 *         %WOCKY_SI_ERROR), or a custom domain registered with
 *         wocky_xmpp_error_register_domain()
 *
 * Sends an error reply for @stanza back to its sender, building the
 * <code>&lt;error/&gt;</code> element from the given @error. To send error
 * replies with simple XMPP Core stanza errors in the %WOCKY_XMPP_ERROR domain,
 * wocky_porter_send_iq_error() may be more convenient to use.
 */
void
wocky_porter_send_iq_gerror (
    WockyPorter *porter,
    WockyStanza *stanza,
    const GError *error)
{
  WockyStanzaType type;
  WockyStanzaSubType sub_type;
  WockyStanza *result;
  WockyNode *result_node;

  g_return_if_fail (WOCKY_IS_PORTER (porter));
  g_return_if_fail (WOCKY_IS_STANZA (stanza));
  g_return_if_fail (error != NULL);

  wocky_stanza_get_type_info (stanza, &type, &sub_type);
  g_return_if_fail (type == WOCKY_STANZA_TYPE_IQ);
  g_return_if_fail (sub_type == WOCKY_STANZA_SUB_TYPE_GET ||
      sub_type == WOCKY_STANZA_SUB_TYPE_SET);

  result = wocky_stanza_build_iq_error (stanza, '*', &result_node, NULL);

  if (result != NULL)
    {
      /* RFC3920 §9.2.3 dictates:
       *    An IQ stanza of type "error" … MUST include an <error/> child.
       */
      wocky_stanza_error_to_node (error, result_node);

      wocky_porter_send (porter, result);
      g_object_unref (result);
    }
}

/**
 * wocky_porter_force_close_async:
 * @porter: a #WockyPorter
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Force the #WockyPorter to close the TCP connection of the underlying
 * #WockyXmppConnection.
 * If a close operation is pending, it will be completed with the
 * %WOCKY_PORTER_ERROR_FORCIBLY_CLOSED error.
 * When the connection has been closed, @callback will be called.
 * You can then call wocky_porter_force_close_finish() to get the result of
 * the operation.
 */
void
wocky_porter_force_close_async (WockyPorter *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyPorterInterface *iface;

  g_return_if_fail (WOCKY_IS_PORTER (self));

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->force_close_async != NULL);

  iface->force_close_async (self, cancellable, callback, user_data);
}

/**
 * wocky_porter_force_close_finish:
 * @porter: a #WockyPorter
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occuring, or %NULL to ignore.
 *
 * Finishes a force close operation.
 *
 * Returns: %TRUE on success or %FALSE on error.
 */
gboolean
wocky_porter_force_close_finish (
    WockyPorter *self,
    GAsyncResult *result,
    GError **error)
{
  WockyPorterInterface *iface;

  g_return_val_if_fail (WOCKY_IS_PORTER (self), FALSE);

  iface = WOCKY_PORTER_GET_INTERFACE (self);

  g_assert (iface->force_close_finish != NULL);

  return iface->force_close_finish (self, result, error);
}

