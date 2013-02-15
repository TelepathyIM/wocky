/*
 * wocky-xmpp-connection.h - Header for WockyMuc
 * Copyright © 2009 Collabora Ltd.
 * @author Vivek Dasmohapatra <vivek@collabora.co.uk>
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

#ifndef __WOCKY_MUC_H__
#define __WOCKY_MUC_H__

#include <glib-object.h>

#include "wocky-enumtypes.h"
#include "wocky-namespaces.h"
#include "wocky-porter.h"

G_BEGIN_DECLS

typedef struct _WockyMuc WockyMuc;

/**
 * WockyMucClass:
 *
 * The class of a #WockyMuc.
 */
typedef struct _WockyMucClass WockyMucClass;
typedef struct _WockyMucPrivate WockyMucPrivate;

/**
 * WockyMucStatusCode:
 * @WOCKY_MUC_CODE_UNKNOWN: Unknown code
 * @WOCKY_MUC_CODE_ONYMOUS: Room entered is not anonymous
 * @WOCKY_MUC_CODE_AF_CHANGE_OOB: Affiliation changed when not present
 * @WOCKY_MUC_CODE_CFG_SHOW_UNAVAILABLE: Unavailable members visible
 * @WOCKY_MUC_CODE_CFG_HIDE_UNAVAILABLE: Unavailable members invisible
 * @WOCKY_MUC_CODE_CFG_NONPRIVACY: Non-privacy config change
 * @WOCKY_MUC_CODE_OWN_PRESENCE: User's own presence
 * @WOCKY_MUC_CODE_CFG_LOGGING_ENABLED: Logging enabled
 * @WOCKY_MUC_CODE_CFG_LOGGING_DISABLED: Logging disabled
 * @WOCKY_MUC_CODE_CFG_ONYMOUS: Room is now non-anonymous
 * @WOCKY_MUC_CODE_CFG_SEMIONYMOUS: Room is now semi-anonymous
 * @WOCKY_MUC_CODE_CFG_ANONYMOUS: Room is now fully-anonymous
 * @WOCKY_MUC_CODE_NEW_ROOM: Room created (eg by joining)
 * @WOCKY_MUC_CODE_NICK_CHANGE_FORCED: Service enforced nick change
 * @WOCKY_MUC_CODE_BANNED: User has been banned
 * @WOCKY_MUC_CODE_NICK_CHANGE_USER: User's nick changed
 * @WOCKY_MUC_CODE_KICKED: Kicked from the room
 * @WOCKY_MUC_CODE_KICKED_AFFILIATION: Kicked (affiliation change)
 * @WOCKY_MUC_CODE_KICKED_ROOM_PRIVATISED: Kicked (room is now
 *   members-only)
 * @WOCKY_MUC_CODE_KICKED_SHUTDOWN: Kicked (shutdown)
 *
 * MUC status codes, as defined by <ulink
 *    url='http://xmpp.org/extensions/xep-0045.html#registrar-statuscodes'>XEP-0045
 * §15.6</ulink>.
 */
typedef enum {
  WOCKY_MUC_CODE_UNKNOWN = 0,
  WOCKY_MUC_CODE_ONYMOUS = 1 << 0,
  WOCKY_MUC_CODE_AF_CHANGE_OOB = 1 << 1,
  WOCKY_MUC_CODE_CFG_SHOW_UNAVAILABLE = 1 << 2,
  WOCKY_MUC_CODE_CFG_HIDE_UNAVAILABLE = 1 << 3,
  WOCKY_MUC_CODE_CFG_NONPRIVACY = 1 << 4,
  WOCKY_MUC_CODE_OWN_PRESENCE = 1 << 5,
  WOCKY_MUC_CODE_CFG_LOGGING_ENABLED = 1 << 6,
  WOCKY_MUC_CODE_CFG_LOGGING_DISABLED = 1 << 7,
  WOCKY_MUC_CODE_CFG_ONYMOUS = 1 << 8,
  WOCKY_MUC_CODE_CFG_SEMIONYMOUS = 1 << 9,
  WOCKY_MUC_CODE_CFG_ANONYMOUS = 1 << 10,
  WOCKY_MUC_CODE_NEW_ROOM = 1 << 11,
  WOCKY_MUC_CODE_NICK_CHANGE_FORCED = 1 << 12,
  WOCKY_MUC_CODE_BANNED = 1 << 13,
  WOCKY_MUC_CODE_NICK_CHANGE_USER = 1 << 14,
  WOCKY_MUC_CODE_KICKED = 1 << 15,
  WOCKY_MUC_CODE_KICKED_AFFILIATION = 1 << 16,
  WOCKY_MUC_CODE_KICKED_ROOM_PRIVATISED = 1 << 17,
  WOCKY_MUC_CODE_KICKED_SHUTDOWN = 1 << 18,
} WockyMucStatusCode;

/**
 * WockyMucRole:
 * @WOCKY_MUC_ROLE_NONE: no role
 * @WOCKY_MUC_ROLE_VISITOR: visitor role
 * @WOCKY_MUC_ROLE_PARTICIPANT: participant role
 * @WOCKY_MUC_ROLE_MODERATOR: moderator role
 *
 * #WockyMuc roles as described in XEP-0045 §5.1.
 */
typedef enum {
  WOCKY_MUC_ROLE_NONE = 0,
  WOCKY_MUC_ROLE_VISITOR,
  WOCKY_MUC_ROLE_PARTICIPANT,
  WOCKY_MUC_ROLE_MODERATOR
} WockyMucRole;

/**
 * WockyMucAffiliation:
 * @WOCKY_MUC_AFFILIATION_OUTCAST: outcast affiliation
 * @WOCKY_MUC_AFFILIATION_NONE: no affiliation
 * @WOCKY_MUC_AFFILIATION_MEMBER: member affiliation
 * @WOCKY_MUC_AFFILIATION_ADMIN: admin affiliation
 * @WOCKY_MUC_AFFILIATION_OWNER: owner affiliation
 *
 * #WockyMuc affiliations as described in XEP-0045 §5.2.
 */
typedef enum {
  WOCKY_MUC_AFFILIATION_OUTCAST = -1,
  WOCKY_MUC_AFFILIATION_NONE = 0,
  WOCKY_MUC_AFFILIATION_MEMBER,
  WOCKY_MUC_AFFILIATION_ADMIN,
  WOCKY_MUC_AFFILIATION_OWNER,
} WockyMucAffiliation;

/**
 * WockyMucFeature:
 * @WOCKY_MUC_MODERN: the MUC is modern, as documented in XEP-0045
 * @WOCKY_MUC_FORM_REGISTER: the MUC has support for the muc#register
     FORM_TYPE
 * @WOCKY_MUC_FORM_ROOMCONFIG: the MUC has support for the
     muc#register FORM_TYPE
 * @WOCKY_MUC_FORM_ROOMINFO: the MUC has support for the muc#register
     FORM_TYPE
 * @WOCKY_MUC_HIDDEN: the MUC is hidden
 * @WOCKY_MUC_MEMBERSONLY: only members can join this MUC
 * @WOCKY_MUC_MODERATED: the MUC is moderated
 * @WOCKY_MUC_NONANONYMOUS: the MUC is non-anonymous
 * @WOCKY_MUC_OPEN: the MUC is open
 * @WOCKY_MUC_PASSWORDPROTECTED: the MUC is password protected
 * @WOCKY_MUC_PERSISTENT: the MUC is persistent
 * @WOCKY_MUC_PUBLIC: the MUC is public
 * @WOCKY_MUC_ROOMS: the MUC has a list of MUC rooms
 * @WOCKY_MUC_SEMIANONYMOUS: the MUC is semi-anonymous
 * @WOCKY_MUC_TEMPORARY: the MUC is temporary
 * @WOCKY_MUC_UNMODERATED: the MUC is unmoderated
 * @WOCKY_MUC_UNSECURED: the MUC is unsecured
 * @WOCKY_MUC_OBSOLETE: the MUC has obsolete groupchat 1.0 features
 *
 * #WockyMuc feature flags.
 */
typedef enum {
  WOCKY_MUC_MODERN = 1,
  WOCKY_MUC_FORM_REGISTER = (1 << 1),
  WOCKY_MUC_FORM_ROOMCONFIG = (1 << 2),
  WOCKY_MUC_FORM_ROOMINFO = (1 << 3),
  WOCKY_MUC_HIDDEN = (1 << 4),
  WOCKY_MUC_MEMBERSONLY = (1 << 5),
  WOCKY_MUC_MODERATED = (1 << 6),
  WOCKY_MUC_NONANONYMOUS = (1 << 7),
  WOCKY_MUC_OPEN = (1 << 8),
  WOCKY_MUC_PASSWORDPROTECTED = (1 << 9),
  WOCKY_MUC_PERSISTENT = (1 << 10),
  WOCKY_MUC_PUBLIC = (1 << 11),
  WOCKY_MUC_ROOMS = (1 << 12),
  WOCKY_MUC_SEMIANONYMOUS = (1 << 13),
  WOCKY_MUC_TEMPORARY = (1 << 14),
  WOCKY_MUC_UNMODERATED = (1 << 15),
  WOCKY_MUC_UNSECURED = (1 << 16),
  WOCKY_MUC_OBSOLETE = (1 << 17),
} WockyMucFeature;

/**
 * WockyMucMsgType:
 * @WOCKY_MUC_MSG_NONE: no message type
 * @WOCKY_MUC_MSG_NORMAL: a normal message
 * @WOCKY_MUC_MSG_ACTION: an action message
 * @WOCKY_MUC_MSG_NOTICE: a notice message
 *
 * XMPP MUC message types.
 */
typedef enum {
  WOCKY_MUC_MSG_NONE,
  WOCKY_MUC_MSG_NORMAL,
  WOCKY_MUC_MSG_ACTION,
  WOCKY_MUC_MSG_NOTICE,
} WockyMucMsgType;

/**
 * WockyMucMsgState:
 * @WOCKY_MUC_MSG_STATE_NONE: no message state applies
 * @WOCKY_MUC_MSG_STATE_ACTIVE: the contact in the MUC is active
 * @WOCKY_MUC_MSG_STATE_COMPOSING: the contact in the MUC is composing
 *   a message
 * @WOCKY_MUC_MSG_STATE_INACTIVE: the contact in the MUC is inactive
 * @WOCKY_MUC_MSG_STATE_PAUSED: the contact in the MUC has paused
 *   composing a message
 *
 * XMPP MUC message states as documeted in XEP-0085.
 */
typedef enum {
  WOCKY_MUC_MSG_STATE_NONE = -1,
  WOCKY_MUC_MSG_STATE_ACTIVE,
  WOCKY_MUC_MSG_STATE_COMPOSING,
  WOCKY_MUC_MSG_STATE_INACTIVE,
  WOCKY_MUC_MSG_STATE_PAUSED,
} WockyMucMsgState;

/**
 * WockyMucState:
 * @WOCKY_MUC_CREATED: the #WockyMuc has been created
 * @WOCKY_MUC_INITIATED: the MUC has been initiated on the server
 * @WOCKY_MUC_AUTH: the user is authenticating with the MUC
 * @WOCKY_MUC_JOINED: the user has joined the MUC and can chat
 * @WOCKY_MUC_ENDED: the MUC has ended
 *
 * #WockyMuc states.
 */
typedef enum {
  WOCKY_MUC_CREATED = 0,
  WOCKY_MUC_INITIATED,
  WOCKY_MUC_AUTH,
  WOCKY_MUC_JOINED,
  WOCKY_MUC_ENDED,
} WockyMucState;

/**
 * WockyMucMember:
 * @from: the JID of the member (room&commat;server/nick)
 * @jid: the JID of the owner (owner&commat;domain/resource)
 * @nick: the nickname of the member
 * @role: the #WockyMucRole of the member
 * @affiliation: the #WockyMucAffiliation of the member
 * @status: the user set status string
 * @presence_stanza: the #WockyStanza that was received regarding the
 *   member's presence
 */
typedef struct {
  gchar *from;   /* room@service/nick     */
  gchar *jid;    /* owner@domain/resource */
  gchar *nick;   /* nick */
  WockyMucRole role;
  WockyMucAffiliation affiliation;
  gchar *status; /* user set status string */
  WockyStanza *presence_stanza;
} WockyMucMember;

GType wocky_muc_get_type (void);

struct _WockyMuc {
    /*<private>*/
    GObject parent;
    WockyMucPrivate *priv;
};

struct _WockyMucClass {
    /*<private>*/
    GObjectClass parent_class;
};


/* TYPE MACROS */
#define WOCKY_TYPE_MUC (wocky_muc_get_type ())

#define WOCKY_MUC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_MUC, WockyMuc))

#define WOCKY_MUC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_MUC, WockyXmppMuc))
#define WOCKY_IS_MUC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_MUC))

#define WOCKY_IS_MUC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_MUC))

#define WOCKY_MUC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_MUC, WockyMucClass))

/* disco info */
void wocky_muc_disco_info_async (WockyMuc *muc,
    GAsyncReadyCallback callback,
    GCancellable *cancel,
    gpointer data);

gboolean wocky_muc_disco_info_finish (WockyMuc *muc,
    GAsyncResult *res,
    GError **error);

/* presence */
WockyStanza *wocky_muc_create_presence (WockyMuc *muc,
    WockyStanzaSubType type,
    const gchar *status);

/* join */
void wocky_muc_join (WockyMuc *muc,
    GCancellable *cancel);

/* meta data */
const gchar * wocky_muc_jid (WockyMuc *muc);
const gchar * wocky_muc_user (WockyMuc *muc);
WockyMucRole wocky_muc_role (WockyMuc *muc);
WockyMucAffiliation wocky_muc_affiliation (WockyMuc *muc);
GHashTable * wocky_muc_members (WockyMuc *muc);

WockyMucState wocky_muc_get_state (WockyMuc *muc);

G_END_DECLS

#endif /* __WOCKY_MUC_H__ */
