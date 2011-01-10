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
#ifndef __WOCKY_MUC_H__
#define __WOCKY_MUC_H__

#include <glib-object.h>

#include "wocky-namespaces.h"
#include "wocky-porter.h"

G_BEGIN_DECLS

typedef struct _WockyMuc WockyMuc;
typedef struct _WockyMucClass WockyMucClass;
typedef struct _WockyMucPrivate WockyMucPrivate;

/**
 * WockyMucStatusCode:
 * @WOCKY_MUC_CODE_ONYMOUS                : Room entered is not anonymous
 * @WOCKY_MUC_CODE_AF_CHANGE_OOB          : Affiliation changed when not present
 * @WOCKY_MUC_CODE_CFG_SHOW_UNAVAILABLE   : Unavailable members visible
 * @WOCKY_MUC_CODE_CFG_HIDE_UNAVAILABLE   : Unavailable members invisible
 * @WOCKY_MUC_CODE_CFG_NONPRIVACY         : Non-privacy config change
 * @WOCKY_MUC_CODE_OWN_PRESENCE           : User's own presence
 * @WOCKY_MUC_CODE_CFG_LOGGING_ENABLED    : Logging enabled
 * @WOCKY_MUC_CODE_CFG_LOGGING_DISABLED   : Logging disabled
 * @WOCKY_MUC_CODE_CFG_ONYMOUS            : Room is now non-anonymous
 * @WOCKY_MUC_CODE_CFG_SEMIONYMOUS        : Room is now semi-anonymous
 * @WOCKY_MUC_CODE_CFG_ANONYMOUS          : Room is now fully-anonymous
 * @WOCKY_MUC_CODE_NEW_ROOM               : Room created (eg by joining)
 * @WOCKY_MUC_CODE_NICK_CHANGE_FORCED     : Service enforced nick change
 * @WOCKY_MUC_CODE_BANNED                 : User has been banned
 * @WOCKY_MUC_CODE_NICK_CHANGE_USER       : User's nick changed
 * @WOCKY_MUC_CODE_KICKED                 : Kicked from the room
 * @WOCKY_MUC_CODE_KICKED_AFFILIATION     : Kicked (affiliation change)
 * @WOCKY_MUC_CODE_KICKED_ROOM_PRIVATISED : Kicked (room is now members-only)
 * @WOCKY_MUC_CODE_KICKED_SHUTDOWN        : Kicked (shutdown)
 */
typedef enum {
  WOCKY_MUC_CODE_UNKNOWN = 0,
  WOCKY_MUC_CODE_ONYMOUS,
  WOCKY_MUC_CODE_AF_CHANGE_OOB,
  WOCKY_MUC_CODE_CFG_SHOW_UNAVAILABLE,
  WOCKY_MUC_CODE_CFG_HIDE_UNAVAILABLE,
  WOCKY_MUC_CODE_CFG_NONPRIVACY,
  WOCKY_MUC_CODE_OWN_PRESENCE,
  WOCKY_MUC_CODE_CFG_LOGGING_ENABLED,
  WOCKY_MUC_CODE_CFG_LOGGING_DISABLED,
  WOCKY_MUC_CODE_CFG_ONYMOUS,
  WOCKY_MUC_CODE_CFG_SEMIONYMOUS,
  WOCKY_MUC_CODE_CFG_ANONYMOUS,
  WOCKY_MUC_CODE_NEW_ROOM,
  WOCKY_MUC_CODE_NICK_CHANGE_FORCED,
  WOCKY_MUC_CODE_BANNED,
  WOCKY_MUC_CODE_NICK_CHANGE_USER,
  WOCKY_MUC_CODE_KICKED,
  WOCKY_MUC_CODE_KICKED_AFFILIATION,
  WOCKY_MUC_CODE_KICKED_ROOM_PRIVATISED,
  WOCKY_MUC_CODE_KICKED_SHUTDOWN,
} WockyMucStatusCode;

typedef enum {
  WOCKY_MUC_ROLE_NONE = 0,
  WOCKY_MUC_ROLE_VISITOR,
  WOCKY_MUC_ROLE_PARTICIPANT,
  WOCKY_MUC_ROLE_MODERATOR
} WockyMucRole;

typedef enum {
  WOCKY_MUC_AFFILIATION_OUTCAST = -1,
  WOCKY_MUC_AFFILIATION_NONE = 0,
  WOCKY_MUC_AFFILIATION_MEMBER,
  WOCKY_MUC_AFFILIATION_ADMIN,
  WOCKY_MUC_AFFILIATION_OWNER,
} WockyMucAffiliation;

typedef enum {
  WOCKY_MUC_MODERN            = 1,
  WOCKY_MUC_FORM_REGISTER     = (1 << 1),
  WOCKY_MUC_FORM_ROOMCONFIG   = (1 << 2),
  WOCKY_MUC_FORM_ROOMINFO     = (1 << 3),
  WOCKY_MUC_HIDDEN            = (1 << 4),
  WOCKY_MUC_MEMBERSONLY       = (1 << 5),
  WOCKY_MUC_MODERATED         = (1 << 6),
  WOCKY_MUC_NONANONYMOUS      = (1 << 7),
  WOCKY_MUC_OPEN              = (1 << 8),
  WOCKY_MUC_PASSWORDPROTECTED = (1 << 9),
  WOCKY_MUC_PERSISTENT        = (1 << 10),
  WOCKY_MUC_PUBLIC            = (1 << 11),
  WOCKY_MUC_ROOMS             = (1 << 12),
  WOCKY_MUC_SEMIANONYMOUS     = (1 << 13),
  WOCKY_MUC_TEMPORARY         = (1 << 14),
  WOCKY_MUC_UNMODERATED       = (1 << 15),
  WOCKY_MUC_UNSECURED         = (1 << 16),
  WOCKY_MUC_OBSOLETE          = (1 << 17),
} WockyMucFeature;

typedef enum {
  WOCKY_MUC_MSG_NONE,
  WOCKY_MUC_MSG_NORMAL,
  WOCKY_MUC_MSG_ACTION,
  WOCKY_MUC_MSG_NOTICE,
} WockyMucMsgType;

typedef enum {
  WOCKY_MUC_MSG_STATE_NONE = -1,
  WOCKY_MUC_MSG_STATE_ACTIVE,
  WOCKY_MUC_MSG_STATE_COMPOSING,
  WOCKY_MUC_MSG_STATE_INACTIVE,
  WOCKY_MUC_MSG_STATE_PAUSED,
} WockyMucMsgState;

typedef enum {
  WOCKY_MUC_CREATED = 0,
  WOCKY_MUC_INITIATED,
  WOCKY_MUC_AUTH,
  WOCKY_MUC_JOINED,
  WOCKY_MUC_ENDED,
} WockyMucState;

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

struct _WockyMucClass {
    GObjectClass parent_class;
};

struct _WockyMuc {
    GObject parent;
    WockyMucPrivate *priv;
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

/* initiate */
void wocky_muc_initiate_async (WockyMuc *muc,
    GAsyncReadyCallback callback,
    GCancellable *cancel,
    gpointer data);

gboolean wocky_muc_initiate_finish (GObject *source,
    GAsyncResult *res,
    GError **error);

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
