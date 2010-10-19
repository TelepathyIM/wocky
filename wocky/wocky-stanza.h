/*
 * wocky-stanza.h - Header for WockyStanza
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

#ifndef __WOCKY_STANZA_H__
#define __WOCKY_STANZA_H__

#include <stdarg.h>

#include <glib-object.h>
#include "wocky-node-tree.h"
#include "wocky-xmpp-error.h"

G_BEGIN_DECLS

typedef struct _WockyStanzaPrivate WockyStanzaPrivate;
typedef struct _WockyStanza WockyStanza;
typedef struct _WockyStanzaClass WockyStanzaClass;

struct _WockyStanzaClass {
    WockyNodeTreeClass parent_class;
};

struct _WockyStanza {
    WockyNodeTree parent;

    WockyStanzaPrivate *priv;
};

GType wocky_stanza_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_STANZA \
  (wocky_stanza_get_type ())
#define WOCKY_STANZA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_STANZA, \
   WockyStanza))
#define WOCKY_STANZA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_STANZA, WockyStanzaClass))
#define WOCKY_IS_STANZA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_STANZA))
#define WOCKY_IS_STANZA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_STANZA))
#define WOCKY_STANZA_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_STANZA, WockyStanzaClass))

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

WockyStanza * wocky_stanza_new (const gchar *name, const gchar *ns);

WockyNode *wocky_stanza_get_top_node (WockyStanza *self);

WockyStanza * wocky_stanza_build (WockyStanzaType type,
    WockyStanzaSubType sub_type, const gchar *from, const gchar *to,
    ...) G_GNUC_NULL_TERMINATED;

void wocky_stanza_get_type_info (WockyStanza *stanza,
    WockyStanzaType *type, WockyStanzaSubType *sub_type);

const gchar *wocky_stanza_get_from (WockyStanza *self);
const gchar *wocky_stanza_get_to (WockyStanza *self);

WockyStanza * wocky_stanza_build_va (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    const gchar *to,
    va_list ap);

WockyStanza * wocky_stanza_build_iq_result (WockyStanza *iq,
    ...) G_GNUC_NULL_TERMINATED;

WockyStanza * wocky_stanza_build_iq_error (WockyStanza *iq,
    ...) G_GNUC_NULL_TERMINATED;

gboolean wocky_stanza_extract_errors (WockyStanza *stanza,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyNode **specialized_node);

gboolean wocky_stanza_extract_stream_error (WockyStanza *stanza,
    GError **stream_error);

G_END_DECLS

#endif /* #ifndef __WOCKY_STANZA_H__*/
