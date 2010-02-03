/*
 * wocky-xmpp-stanza.h - Header for WockyXmppStanza
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

#ifndef __WOCKY_XMPP_STANZA_H__
#define __WOCKY_XMPP_STANZA_H__

#include <stdarg.h>

#include <glib-object.h>
#include "wocky-xmpp-node.h"
#include "wocky-xmpp-error.h"

G_BEGIN_DECLS

typedef struct _WockyXmppStanza WockyXmppStanza;
typedef struct _WockyXmppStanzaClass WockyXmppStanzaClass;

struct _WockyXmppStanzaClass {
    GObjectClass parent_class;
};

struct _WockyXmppStanza {
    GObject parent;
    WockyXmppNode *node;
};

GType wocky_xmpp_stanza_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_STANZA \
  (wocky_xmpp_stanza_get_type ())
#define WOCKY_XMPP_STANZA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_STANZA, \
   WockyXmppStanza))
#define WOCKY_XMPP_STANZA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanzaClass))
#define WOCKY_IS_XMPP_STANZA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_STANZA))
#define WOCKY_IS_XMPP_STANZA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_STANZA))
#define WOCKY_XMPP_STANZA_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_STANZA, WockyXmppStanzaClass))

typedef enum
{
  WOCKY_STANZA_TYPE_NONE,
  WOCKY_STANZA_TYPE_MESSAGE,
  WOCKY_STANZA_TYPE_PRESENCE,
  WOCKY_STANZA_TYPE_IQ,
  WOCKY_STANZA_TYPE_STREAM,
  WOCKY_STANZA_TYPE_STREAM_FEATURES,
  WOCKY_STANZA_TYPE_AUTH,
  WOCKY_STANZA_TYPE_CHALLENGE,
  WOCKY_STANZA_TYPE_RESPONSE,
  WOCKY_STANZA_TYPE_SUCCESS,
  WOCKY_STANZA_TYPE_FAILURE,
  WOCKY_STANZA_TYPE_STREAM_ERROR,
  WOCKY_STANZA_TYPE_UNKNOWN,
  NUM_WOCKY_STANZA_TYPE
} WockyStanzaType;

typedef enum
{
  WOCKY_STANZA_SUB_TYPE_NONE,
  WOCKY_STANZA_SUB_TYPE_AVAILABLE,
  WOCKY_STANZA_SUB_TYPE_NORMAL,
  WOCKY_STANZA_SUB_TYPE_CHAT,
  WOCKY_STANZA_SUB_TYPE_GROUPCHAT,
  WOCKY_STANZA_SUB_TYPE_HEADLINE,
  WOCKY_STANZA_SUB_TYPE_UNAVAILABLE,
  WOCKY_STANZA_SUB_TYPE_PROBE,
  WOCKY_STANZA_SUB_TYPE_SUBSCRIBE,
  WOCKY_STANZA_SUB_TYPE_UNSUBSCRIBE,
  WOCKY_STANZA_SUB_TYPE_SUBSCRIBED,
  WOCKY_STANZA_SUB_TYPE_UNSUBSCRIBED,
  WOCKY_STANZA_SUB_TYPE_GET,
  WOCKY_STANZA_SUB_TYPE_SET,
  WOCKY_STANZA_SUB_TYPE_RESULT,
  WOCKY_STANZA_SUB_TYPE_ERROR,
  WOCKY_STANZA_SUB_TYPE_UNKNOWN,
  NUM_WOCKY_STANZA_SUB_TYPE
} WockyStanzaSubType;

typedef enum
{
  WOCKY_NODE,
  WOCKY_NODE_TEXT,
  WOCKY_NODE_END,
  WOCKY_NODE_ATTRIBUTE,
  WOCKY_NODE_XMLNS,
  WOCKY_NODE_ASSIGN_TO,
  WOCKY_STANZA_END
} WockyBuildTag;

WockyXmppStanza * wocky_xmpp_stanza_new (const gchar *name);

WockyXmppStanza * wocky_xmpp_stanza_build (WockyStanzaType type,
    WockyStanzaSubType sub_type, const gchar *from, const gchar *to,
    WockyBuildTag spec, ...);

void wocky_xmpp_stanza_get_type_info (WockyXmppStanza *stanza,
    WockyStanzaType *type, WockyStanzaSubType *sub_type);

WockyXmppStanza * wocky_xmpp_stanza_build_va (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    const gchar *to,
    WockyBuildTag spec,
    va_list ap);

WockyXmppStanza * wocky_xmpp_stanza_build_iq_result (WockyXmppStanza *iq,
    WockyBuildTag spec, ...);

WockyXmppStanza * wocky_xmpp_stanza_build_iq_error (WockyXmppStanza *iq,
    WockyBuildTag spec, ...);

gboolean wocky_xmpp_stanza_extract_errors (WockyXmppStanza *stanza,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyXmppNode **specialized_node);

gboolean wocky_xmpp_stanza_extract_stream_error (WockyXmppStanza *stanza,
    GError **stream_error);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_STANZA_H__*/
