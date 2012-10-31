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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_STANZA_H__
#define __WOCKY_STANZA_H__

#include <stdarg.h>

#include <glib-object.h>
#include "wocky-node-tree.h"
#include "wocky-xmpp-error.h"
#include "wocky-contact.h"

G_BEGIN_DECLS

typedef struct _WockyStanza WockyStanza;

/**
 * WockyStanzaClass:
 *
 * The class of a #WockyStanza.
 */
typedef struct _WockyStanzaClass WockyStanzaClass;
typedef struct _WockyStanzaPrivate WockyStanzaPrivate;

struct _WockyStanzaClass {
    /*<private>*/
    WockyNodeTreeClass parent_class;
};

struct _WockyStanza {
    /*<private>*/
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

/**
 * WockyStanzaType:
 * @WOCKY_STANZA_TYPE_NONE: no stanza type
 * @WOCKY_STANZA_TYPE_MESSAGE: <code>&lt;message/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_PRESENCE: <code>&lt;presence/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_IQ: <code>&lt;iq/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_STREAM: <code>&lt;stream/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_STREAM_FEATURES: <code>&lt;stream:features/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_AUTH: <code>&lt;auth/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_CHALLENGE: <code>&lt;challenge/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_RESPONSE: <code>&lt;response/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_SUCCESS: <code>&lt;success/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_FAILURE: <code>&lt;failure/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_STREAM_ERROR: <code>&lt;stream:error/&gt;</code> stanza
 * @WOCKY_STANZA_TYPE_UNKNOWN: unknown stanza type
 *
 * XMPP stanza types.
 */
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
  /*< private >*/
  NUM_WOCKY_STANZA_TYPE
} WockyStanzaType;

/**
 * WockyStanzaSubType:
 * @WOCKY_STANZA_SUB_TYPE_NONE: no sub type
 * @WOCKY_STANZA_SUB_TYPE_AVAILABLE: "available" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_NORMAL: "normal" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_CHAT: "chat" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_GROUPCHAT: "groupchat" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_HEADLINE: "headline" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_UNAVAILABLE: "unavailable" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_PROBE: "probe" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_SUBSCRIBE: "subscribe" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_UNSUBSCRIBE: "unsubscribe" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_SUBSCRIBED: "subscribed" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_UNSUBSCRIBED: "unsubscribed" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_GET: "get" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_SET: "set" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_RESULT: "result" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_ERROR: "error" stanza sub type
 * @WOCKY_STANZA_SUB_TYPE_UNKNOWN: unknown stanza sub type
 *
 * XMPP stanza sub types.
 */
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
  /*< private >*/
  NUM_WOCKY_STANZA_SUB_TYPE
} WockyStanzaSubType;

WockyStanza * wocky_stanza_new (const gchar *name, const gchar *ns);

WockyStanza * wocky_stanza_copy (WockyStanza *old);

WockyNode *wocky_stanza_get_top_node (WockyStanza *self);

WockyStanza * wocky_stanza_build (WockyStanzaType type,
    WockyStanzaSubType sub_type, const gchar *from, const gchar *to,
    ...) G_GNUC_NULL_TERMINATED;

WockyStanza * wocky_stanza_build_to_contact (WockyStanzaType type,
    WockyStanzaSubType sub_type, const gchar *from,
    WockyContact *to, ...) G_GNUC_NULL_TERMINATED;

void wocky_stanza_get_type_info (WockyStanza *stanza,
    WockyStanzaType *type, WockyStanzaSubType *sub_type);
gboolean wocky_stanza_has_type (WockyStanza *stanza,
    WockyStanzaType expected_type);

const gchar *wocky_stanza_get_from (WockyStanza *self);
const gchar *wocky_stanza_get_to (WockyStanza *self);

WockyStanza * wocky_stanza_build_va (WockyStanzaType type,
    WockyStanzaSubType sub_type,
    const gchar *from,
    const gchar *to,
    va_list ap);

WockyStanza * wocky_stanza_build_iq_result (WockyStanza *iq,
    ...) G_GNUC_NULL_TERMINATED;
WockyStanza *wocky_stanza_build_iq_result_va (
    WockyStanza *iq,
    va_list ap);

WockyStanza * wocky_stanza_build_iq_error (WockyStanza *iq,
    ...) G_GNUC_NULL_TERMINATED;
WockyStanza *wocky_stanza_build_iq_error_va (
    WockyStanza *iq,
    va_list ap);

gboolean wocky_stanza_extract_errors (WockyStanza *stanza,
    WockyXmppErrorType *type,
    GError **core,
    GError **specialized,
    WockyNode **specialized_node);

gboolean wocky_stanza_extract_stream_error (WockyStanza *stanza,
    GError **stream_error);

WockyContact * wocky_stanza_get_to_contact (WockyStanza *self);
WockyContact * wocky_stanza_get_from_contact (WockyStanza *self);

void wocky_stanza_set_to_contact (WockyStanza *self,
    WockyContact *contact);
void wocky_stanza_set_from_contact (WockyStanza *self,
    WockyContact *contact);

G_END_DECLS

#endif /* #ifndef __WOCKY_STANZA_H__*/
