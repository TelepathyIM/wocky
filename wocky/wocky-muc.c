/*
 * wocky-muc.c - Source for WockyMuc
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

#include <string.h>
#include <time.h>

#define DEBUG_FLAG DEBUG_MUC_CONNECTION
#include "wocky-debug.h"

#include "wocky-muc.h"
#include "wocky-muc-enumtypes.h"
#include "wocky-xmpp-error-enumtypes.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"
#include "wocky-signals-marshal.h"
#include "wocky-xmpp-error.h"

static struct { const gchar *name; WockyMucMsgState state; } msg_state[] =
 { { "active",    WOCKY_MUC_MSG_STATE_ACTIVE   },
   { "composing", WOCKY_MUC_MSG_STATE_TYPING   },
   { "inactive",  WOCKY_MUC_MSG_STATE_INACTIVE },
   { "paused",    WOCKY_MUC_MSG_STATE_PAUSED   },
   { "gone",      WOCKY_MUC_MSG_STATE_GONE     },
   { NULL,        WOCKY_MUC_MSG_STATE_NONE     } };

typedef enum {
  SIG_NICK_CHANGE,
  SIG_PERM_CHANGE,
  SIG_PRESENCE,
  SIG_OWN_PRESENCE,
  SIG_PRESENCE_ERROR,
  SIG_JOINED,
  SIG_PARTED,
  SIG_LEFT,
  SIG_MSG,
  SIG_MSG_ERR,
  SIG_FILL_PRESENCE,
  SIG_NULL
} WockyMucSig;

static guint signals[SIG_NULL] = { 0 };

typedef struct { const gchar *ns; WockyMucFeature flag; } feature;
static const feature feature_map[] =
  { { WOCKY_NS_MUC,               WOCKY_MUC_MODERN            },
    { WOCKY_NS_MUC "#register",   WOCKY_MUC_FORM_REGISTER     },
    { WOCKY_NS_MUC "#roomconfig", WOCKY_MUC_FORM_ROOMCONFIG   },
    { WOCKY_NS_MUC "#roominfo",   WOCKY_MUC_FORM_ROOMINFO     },
    { "muc_hidden",               WOCKY_MUC_HIDDEN            },
    { "muc_membersonly",          WOCKY_MUC_MEMBERSONLY       },
    { "muc_moderated",            WOCKY_MUC_MODERATED         },
    { "muc_nonanonymous",         WOCKY_MUC_NONANONYMOUS      },
    { "muc_open",                 WOCKY_MUC_OPEN              },
    { "muc_passwordprotected",    WOCKY_MUC_PASSWORDPROTECTED },
    { "muc_persistent",           WOCKY_MUC_PERSISTENT        },
    { "muc_public",               WOCKY_MUC_PUBLIC            },
    { "muc_rooms",                WOCKY_MUC_ROOMS             },
    { "muc_semianonymous",        WOCKY_MUC_SEMIANONYMOUS     },
    { "muc_temporary",            WOCKY_MUC_TEMPORARY         },
    { "muc_unmoderated",          WOCKY_MUC_UNMODERATED       },
    { "muc_unsecured",            WOCKY_MUC_UNSECURED         },
    { "gc-1.0",                   WOCKY_MUC_OBSOLETE          },
    { NULL,                       0                           } };

static void wocky_muc_class_init (WockyMucClass *klass);

/* create MUC object */
G_DEFINE_TYPE (WockyMuc, wocky_muc, G_TYPE_OBJECT);

/* private methods */
static void wocky_muc_dispose (GObject *object);
static void wocky_muc_finalize (GObject *object);

static void wocky_muc_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);

static void wocky_muc_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

/* private functions */
static gboolean handle_presence (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer data);

static gboolean handle_message (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer data);

enum
{
  PROP_JID = 1,
  PROP_USER,
  PROP_PORTER,
  PROP_SERVICE,
  PROP_ROOM,
  PROP_DESC,
  PROP_NICK,
  PROP_RNICK,
  PROP_PASS,
  PROP_STATUS,
  PROP_ROOM_TYPE,
  PROP_ID_CATEGORY,
  PROP_ID_TYPE,
  PROP_ID_NAME,
  PROP_ROLE,
  PROP_AFFILIATION,
};

typedef struct _WockyMucPrivate WockyMucPrivate;

struct _WockyMucPrivate
{
  /* properties */
  WockyPorter *porter;
  gchar *user;         /* full JID of user      */
  gchar *jid;          /* room@service/nick     */
  gchar *service;      /*      service          */
  gchar *room;         /* room                  */
  gchar *rjid;         /* room@service          */
  gchar *nick;         /*              nick     */
  gchar *rnick;        /* reserved nick, if any */
  gchar *id_category;  /* eg "conference"       */
  gchar *id_type;      /* eg "text"             */
  gchar *id_name;
  gchar *desc;         /* long room description */
  gchar *pass;         /* password or NULL      */
  gchar *status;       /* status message        */
  guint  room_type;    /* ORed WockyMucFeature flags */

  /* not props */
  gboolean dispose_has_run;
  GHashTable *members;
  WockyMucState state;
  WockyMucRole role;
  WockyMucAffiliation affiliation;
  guint pres_handler;
  guint mesg_handler;
  GSimpleAsyncResult *join_cb;
};

#define WOCKY_MUC_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE((o),WOCKY_TYPE_MUC,WockyMucPrivate))

static void
free_member (gpointer data)
{
  WockyMucMember *member = data;

  if (member->presence_stanza != NULL)
    g_object_unref (member->presence_stanza);

  g_free (member->from);
  g_free (member->jid);
  g_free (member->nick);
  g_free (member->status);

  g_slice_free (WockyMucMember, member);
}

static gpointer
alloc_member (void)
{
  return g_slice_new0 (WockyMucMember);
}

static void
wocky_muc_init (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  priv->members = g_hash_table_new_full (g_str_hash,
      g_str_equal,
      g_free,
      free_member);
}

static void
wocky_muc_dispose (GObject *object)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->pres_handler != 0)
    wocky_porter_unregister_handler (priv->porter, priv->pres_handler);
  priv->pres_handler = 0;

  if (priv->mesg_handler != 0)
    wocky_porter_unregister_handler (priv->porter, priv->mesg_handler);
  priv->mesg_handler = 0;

  if (priv->porter)
    g_object_unref (priv->porter);
  priv->porter = NULL;

  if (priv->members)
    g_hash_table_unref (priv->members);
  priv->members = NULL;

  if (G_OBJECT_CLASS (wocky_muc_parent_class )->dispose)
    G_OBJECT_CLASS (wocky_muc_parent_class)->dispose (object);
}

#define GFREE_AND_FORGET(x) g_free (x); x = NULL;

static void
wocky_muc_finalize (GObject *object)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  GFREE_AND_FORGET (priv->user);
  GFREE_AND_FORGET (priv->jid);
  GFREE_AND_FORGET (priv->service);
  GFREE_AND_FORGET (priv->room);
  GFREE_AND_FORGET (priv->rjid);
  GFREE_AND_FORGET (priv->nick);
  GFREE_AND_FORGET (priv->rnick);
  GFREE_AND_FORGET (priv->id_category);
  GFREE_AND_FORGET (priv->id_type);
  GFREE_AND_FORGET (priv->id_name);

  G_OBJECT_CLASS (wocky_muc_parent_class)->finalize (object);
}

static void
wocky_muc_class_init (WockyMucClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GType ctype = G_OBJECT_CLASS_TYPE (klass);
  GParamSpec *spec;

  g_type_class_add_private (klass, sizeof (WockyMucPrivate));

  oclass->get_property = wocky_muc_get_property;
  oclass->set_property = wocky_muc_set_property;
  oclass->dispose      = wocky_muc_dispose;
  oclass->finalize     = wocky_muc_finalize;

  spec = g_param_spec_string ("jid", "jid",
      "Full room@service/nick JID of the MUC room",
      NULL,
      G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_JID, spec);

  spec = g_param_spec_string ("user", "user",
      "Full JID of the user (node@domain/resource) who is connecting",
      NULL,
      (G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_USER, spec);

  spec = g_param_spec_object ("porter", "porter",
      "The WockyPorter instance doing all the actual XMPP interaction",
      WOCKY_TYPE_PORTER,
      (G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_PORTER, spec);

  spec = g_param_spec_string ("service", "service",
      "The service (domain) part of the MUC JID",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_SERVICE, spec);

  spec = g_param_spec_string ("room", "room",
      "The node part of the MUC room JID",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ROOM, spec);

  spec = g_param_spec_string ("description", "desc",
      "The long description oof the room",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_DESC, spec);

  spec = g_param_spec_string ("nickname", "nick",
      "The user's in-room nickname",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_NICK, spec);

  spec = g_param_spec_string ("reserved-nick", "reserved-nick",
      "The user's reserved in-room nickname, if any",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_RNICK, spec);

  spec = g_param_spec_string ("password", "password",
      "User's MUC room password",
      NULL,
      (G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_PASS, spec);

  spec = g_param_spec_string ("status-message", "status",
      "User's MUC status message",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_STATUS, spec);

  spec = g_param_spec_ulong ("muc-flags", "muc-flags",
      "ORed set of WockyMucFeature MUC property flags",
      0, G_MAXULONG, 0,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ROOM_TYPE, spec);

  spec = g_param_spec_string ("category", "category",
      "Category of the MUC, usually \"conference\"",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ID_CATEGORY, spec);

  spec = g_param_spec_string ("type", "type",
      "Type of the MUC, eg \"text\"",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ID_TYPE, spec);

  spec = g_param_spec_string ("name", "name",
      "The human-readable name of the room (usually a short label)",
      NULL,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ID_NAME, spec);

  spec = g_param_spec_uint ("role", "role",
      "The role (WockyMucRole) of the user in the MUC room",
      WOCKY_MUC_ROLE_NONE, WOCKY_MUC_ROLE_MODERATOR, WOCKY_MUC_ROLE_NONE,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_ROLE, spec);

  spec = g_param_spec_enum ("affiliation", "affiliation",
      "The affiliation of the user with the MUC room",
      WOCKY_TYPE_MUC_AFFILIATION,
      WOCKY_MUC_AFFILIATION_NONE,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_AFFILIATION, spec);

  signals[SIG_NICK_CHANGE] = g_signal_new ("nick-change", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_POINTER,
      G_TYPE_NONE, 2,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_HASH_TABLE);

  signals[SIG_PRESENCE] = g_signal_new ("presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_BOXED_POINTER,
      G_TYPE_NONE, 3,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

  signals[SIG_OWN_PRESENCE] = g_signal_new ("own-presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_BOXED,
      G_TYPE_NONE, 2,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_HASH_TABLE);

  signals[SIG_JOINED] = g_signal_new ("joined", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_POINTER,
      G_TYPE_NONE, 2,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_HASH_TABLE);

  signals[SIG_PRESENCE_ERROR] = g_signal_new ("error", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_ENUM_STRING,
      G_TYPE_NONE, 3,
      WOCKY_TYPE_XMPP_STANZA, WOCKY_TYPE_XMPP_ERROR, G_TYPE_STRING);

  /* These signals convey actor(jid) + reason */
  signals[SIG_PERM_CHANGE] = g_signal_new ("permissions", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_POINTER_POINTER_POINTER,
      G_TYPE_NONE, 4,
      WOCKY_TYPE_XMPP_STANZA, G_TYPE_HASH_TABLE, G_TYPE_STRING, G_TYPE_STRING);

  /* and these two pass on any message as well: */
  signals[SIG_PARTED] = g_signal_new ("parted", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_BOXED_STRING_STRING_STRING,
      G_TYPE_NONE, 5,
      WOCKY_TYPE_XMPP_STANZA,
      G_TYPE_HASH_TABLE,
      G_TYPE_STRING,  /* actor jid */
      G_TYPE_STRING,  /* reason    */
      G_TYPE_STRING); /* message: usually none, but allowed by spec */

  signals[SIG_LEFT] = g_signal_new ("left", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_BOXED_POINTER_STRING_STRING_STRING,
      G_TYPE_NONE, 6,
      WOCKY_TYPE_XMPP_STANZA,
      G_TYPE_HASH_TABLE,
      G_TYPE_POINTER,  /* member struct   */
      G_TYPE_STRING,   /* actor jid       */
      G_TYPE_STRING,   /* reason          */
      G_TYPE_STRING);  /* message, if any */

  signals[SIG_MSG] = g_signal_new ("message", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_ENUM_STRING_LONG_POINTER_STRING_STRING_ENUM,
      G_TYPE_NONE, 8,
      WOCKY_TYPE_XMPP_STANZA,
      WOCKY_TYPE_MUC_MSG_TYPE,    /* WockyMucMsgType  */
      G_TYPE_STRING,  /* XMPP msg ID      */
      G_TYPE_LONG,    /* time_t           */
      G_TYPE_POINTER, /* WockyMucMember * */
      G_TYPE_STRING,  /* content          */
      G_TYPE_STRING,  /* subject          */
      WOCKY_TYPE_MUC_MSG_STATE);   /* WockyMucMsgState */

  signals[SIG_MSG_ERR] = g_signal_new ("message-error", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_ENUM_STRING_LONG_POINTER_STRING_ENUM_ENUM,
      G_TYPE_NONE, 8,
      WOCKY_TYPE_XMPP_STANZA,
      WOCKY_TYPE_MUC_MSG_TYPE,    /* WockyMucMsgType  */
      G_TYPE_STRING,  /* XMPP msg ID      */
      G_TYPE_LONG,    /* time_t           */
      G_TYPE_POINTER, /* WockyMucMember * */
      G_TYPE_STRING,  /* content          */
      WOCKY_TYPE_XMPP_ERROR,    /* WockyXmppError   */
      WOCKY_TYPE_XMPP_ERROR_TYPE); /* error type       */

  signals[SIG_FILL_PRESENCE] = g_signal_new ("fill-presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        _wocky_signals_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        WOCKY_TYPE_XMPP_STANZA);
}

static void
wocky_muc_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  switch (property_id)
    {
      case PROP_PORTER:
        priv->porter = g_value_dup_object (value);
        break;
      case PROP_JID:
        g_free (priv->jid);
        g_free (priv->service);
        g_free (priv->room);
        g_free (priv->nick);
        g_free (priv->rjid);
        priv->jid = g_value_dup_string (value);
        wocky_decode_jid (priv->jid,
            &(priv->room),
            &(priv->service),
            &(priv->nick));
        priv->rjid = g_strdup_printf ("%s@%s", priv->room, priv->service);
        break;
      case PROP_NICK:
        g_free (priv->nick);
        priv->nick = g_value_dup_string (value);
        if (priv->jid != NULL && priv->nick != NULL)
          {
            g_free (priv->jid);
            priv->jid =
              g_strdup_printf ("%s@%s/%s",
                  priv->room,
                  priv->service,
                  priv->nick);
          }
      case PROP_RNICK:
        g_free (priv->rnick);
        priv->rnick = g_value_dup_string (value);
      case PROP_PASS:
        g_free (priv->pass);
        priv->pass = g_value_dup_string (value);
        break;
      case PROP_USER:
        g_free (priv->user);
        priv->user = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_muc_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  switch (property_id)
    {
      case PROP_PORTER:
        g_value_set_object (value, priv->porter);
        break;
      case PROP_JID:
        g_value_set_string (value, priv->jid);
        break;
      case PROP_SERVICE:
        g_value_set_string (value, priv->service);
        break;
      case PROP_ROOM:
        g_value_set_string (value, priv->room);
        break;
      case PROP_DESC:
        g_value_set_string (value, priv->desc);
        break;
      case PROP_NICK:
        g_value_set_string (value, priv->nick);
        break;
      case PROP_PASS:
        g_value_set_string (value, priv->pass);
        break;
      case PROP_STATUS:
        g_value_set_string (value, priv->status);
        break;
      case PROP_RNICK:
        g_value_set_string (value, priv->rnick);
        break;
      case PROP_USER:
        g_value_set_string (value, priv->user);
        break;
      case PROP_ROOM_TYPE:
        g_value_set_uint (value, priv->room_type);
        break;
      case PROP_ID_CATEGORY:
        g_value_set_string (value, priv->id_category);
        break;
      case PROP_ID_TYPE:
        g_value_set_string (value, priv->id_type);
        break;
      case PROP_ID_NAME:
        g_value_set_string (value, priv->id_name);
        break;
      case PROP_ROLE:
        g_value_set_uint (value, priv->role);
        break;
      case PROP_AFFILIATION:
        g_value_set_enum (value, priv->affiliation);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

/* ************************************************************************ */
/* status code mapping */
static guint
status_code_to_muc_flag (guint code)
{
  switch (code)
    {
      case 100: return WOCKY_MUC_CODE_ONYMOUS;
      case 101: return WOCKY_MUC_CODE_AF_CHANGE_OOB;
      case 102: return WOCKY_MUC_CODE_CFG_SHOW_UNAVAILABLE;
      case 103: return WOCKY_MUC_CODE_CFG_HIDE_UNAVAILABLE;
      case 104: return WOCKY_MUC_CODE_CFG_NONPRIVACY;
      case 110: return WOCKY_MUC_CODE_OWN_PRESENCE;
      case 170: return WOCKY_MUC_CODE_CFG_LOGGING_ENABLED;
      case 171: return WOCKY_MUC_CODE_CFG_LOGGING_DISABLED;
      case 172: return WOCKY_MUC_CODE_CFG_ONYMOUS;
      case 173: return WOCKY_MUC_CODE_CFG_SEMIONYMOUS;
      case 174: return WOCKY_MUC_CODE_CFG_ANONYMOUS;
      case 201: return WOCKY_MUC_CODE_NEW_ROOM;
      case 210: return WOCKY_MUC_CODE_NICK_CHANGE_FORCED;
      case 301: return WOCKY_MUC_CODE_BANNED;
      case 303: return WOCKY_MUC_CODE_NICK_CHANGE_USER;
      case 307: return WOCKY_MUC_CODE_KICKED;
      case 321: return WOCKY_MUC_CODE_KICKED_AFFILIATION;
      case 322: return WOCKY_MUC_CODE_KICKED_ROOM_PRIVATISED;
      case 332: return WOCKY_MUC_CODE_KICKED_SHUTDOWN;
    }
  return WOCKY_MUC_CODE_UNKNOWN;
}

/* ************************************************************************ */
/* check MUC exists/disco MUC info */
static gboolean
store_muc_disco_info_x (WockyXmppNode *field, gpointer data)
{
  WockyMucPrivate *priv = data;
  const gchar *var = NULL;

  if (wocky_strdiff (field->name, "field"))
    return TRUE;

  var = wocky_xmpp_node_get_attribute (field, "var");

  if (wocky_strdiff (var, "muc#roominfo_description"))
    return TRUE;

  priv->desc = g_strdup (
      wocky_xmpp_node_get_content_from_child (field, "value"));

  return TRUE;
}

static gboolean
store_muc_disco_info (WockyXmppNode *feat, gpointer data)
{
  WockyMucPrivate *priv = data;

  if (!wocky_strdiff (feat->name, "feature"))
    {
      guint i;
      const gchar *thing = wocky_xmpp_node_get_attribute (feat, "var");

      if (thing == NULL)
        return TRUE;

      for (i = 0; feature_map[i].ns != NULL; i++)
        if (!wocky_strdiff (thing, feature_map[i].ns))
          {
            priv->room_type |= feature_map[i].flag;
            break;
          }
      return TRUE;
    }

  if (!wocky_strdiff (feat->name, "x"))
    wocky_xmpp_node_each_child (feat, store_muc_disco_info_x, priv);

  return TRUE;
}

static void
muc_disco_info (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  WockyMuc *muc;
  WockyMucPrivate *priv;
  GError *error = NULL;
  WockyXmppStanza *iq;
  WockyStanzaType type;
  WockyStanzaSubType sub;
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (data);

  muc = WOCKY_MUC (g_async_result_get_source_object (G_ASYNC_RESULT (result)));
  priv = WOCKY_MUC_GET_PRIVATE (muc);

  iq = wocky_porter_send_iq_finish (priv->porter, res, &error);

  priv->room_type = 0;
  g_free (priv->id_name);
  g_free (priv->id_type);
  g_free (priv->id_category);
  priv->id_name = NULL;
  priv->id_type = NULL;
  priv->id_category = NULL;

  if (error != NULL)
    goto out;

  if (iq == NULL)
    goto out;

  wocky_xmpp_stanza_get_type_info (iq, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      error = g_error_new (WOCKY_XMPP_ERROR,
          WOCKY_XMPP_ERROR_UNDEFINED_CONDITION, "Bizarre response: Not an IQ");
      goto out;
    }

  switch (sub)
    {
      WockyXmppNode *query;
      WockyXmppNode *node;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        query = wocky_xmpp_node_get_child_ns (iq->node, "query", NS_DISCO_INFO);

        if (!query)
          {
            error = g_error_new (WOCKY_XMPP_ERROR,
                WOCKY_XMPP_ERROR_UNDEFINED_CONDITION,
                "Malformed IQ reply");
            goto out;
          }

        node = wocky_xmpp_node_get_child (query, "identity");

        if (!node)
          {
            error = g_error_new (WOCKY_XMPP_ERROR,
                WOCKY_XMPP_ERROR_UNDEFINED_CONDITION,
                "Malformed IQ reply: No Identity");
            goto out;
          }
        else
          {
            const gchar *attr;

            attr = wocky_xmpp_node_get_attribute (node, "category");
            g_free (priv->id_category);
            priv->id_category = g_strdup (attr);

            attr = wocky_xmpp_node_get_attribute (node, "name");
            g_free (priv->id_name);
            priv->id_name = g_strdup (attr);

            attr = wocky_xmpp_node_get_attribute (node, "type");
            g_free (priv->id_type);
            priv->id_type = g_strdup (attr);
          }

        wocky_xmpp_node_each_child (query, store_muc_disco_info, priv);
        if (priv->state < WOCKY_MUC_INITIATED)
          priv->state = WOCKY_MUC_INITIATED;
        break;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_xmpp_stanza_extract_errors (iq, NULL, &error, NULL, NULL);
        break;

      default:
        break;
    }

out:
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
  g_object_unref (muc);

  if (iq != NULL)
    g_object_unref (iq);
}

gboolean
wocky_muc_disco_info_finish (WockyMuc *muc,
    GAsyncResult *res,
    GError **error)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (result, error))
    return FALSE;

  return TRUE;
}

void
wocky_muc_disco_info_async (WockyMuc *muc,
    GAsyncReadyCallback callback,
    GCancellable *cancel,
    gpointer data)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  GSimpleAsyncResult *result;
  WockyXmppStanza *iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET,
      priv->user,
      priv->jid,
      WOCKY_NODE, "query",
      WOCKY_NODE_XMLNS, NS_DISCO_INFO,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  result = g_simple_async_result_new (G_OBJECT (muc), callback, data,
    wocky_muc_disco_info_finish);

  wocky_porter_send_iq_async (priv->porter, iq, cancel, muc_disco_info,
      result);
}

/* ask for MUC member list */

/* ************************************************************************ */
/* send presence to MUC */
WockyXmppStanza *
wocky_muc_create_presence (WockyMuc *muc,
    WockyStanzaSubType type,
    const gchar *status,
    const gchar *password)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyXmppStanza *stanza =
    wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_PRESENCE,
        type,
        priv->user,
        priv->jid,
        WOCKY_STANZA_END);
  WockyXmppNode *presence = stanza->node;
  WockyXmppNode *x = wocky_xmpp_node_add_child_ns (presence, "x", WOCKY_NS_MUC);


  /* There should be separate API to leave a room, but atm there isn't... so
   * only allow the status to be set directly when making a presence to leave
   * the muc */
  g_assert (status == NULL || type == WOCKY_STANZA_SUB_TYPE_UNAVAILABLE);

  if (status != NULL)
    {
      wocky_xmpp_node_add_child_with_content (presence, "status", status);
    }
  else
    {
      g_signal_emit (muc, signals[SIG_FILL_PRESENCE], 0, stanza);
    }

  if (password != NULL)
    wocky_xmpp_node_add_child_with_content (x, "password", password);

  return stanza;
}

static void
wocky_muc_send_presence (WockyMuc *muc,
    WockyStanzaSubType type,
    const gchar *status)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyXmppStanza *pres = wocky_muc_create_presence (muc, type, status,
      priv->pass);

  wocky_porter_send (priv->porter, pres);
}

/* ************************************************************************ */
/* register presence handler for MUC */
static guint
register_presence_handler (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  if (priv->pres_handler == 0)
    priv->pres_handler = wocky_porter_register_handler (priv->porter,
        WOCKY_STANZA_TYPE_PRESENCE,
        WOCKY_STANZA_SUB_TYPE_NONE,
        priv->rjid,
        WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
        handle_presence, muc,
        WOCKY_STANZA_END);

  return priv->pres_handler;
}

/* ************************************************************************ */
/* register message handler for MUC */
static guint
register_message_handler (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  if (priv->mesg_handler == 0)
    priv->mesg_handler = wocky_porter_register_handler (priv->porter,
        WOCKY_STANZA_TYPE_MESSAGE,
        WOCKY_STANZA_SUB_TYPE_NONE,
        priv->rjid,
        WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
        handle_message, muc,
        WOCKY_STANZA_END);

    return priv->mesg_handler;
}

/* ************************************************************************ */
/* handle presence from MUC */
static gboolean
presence_code (WockyXmppNode *node, gpointer data)
{
  const gchar *code = NULL;
  GHashTable *status = data;
  gulong cnum = 0;

  if (wocky_strdiff (node->name, "status"))
    return TRUE;

  code = wocky_xmpp_node_get_attribute (node, "code");

  if (code == NULL)    return TRUE;

  cnum = (gulong) g_ascii_strtoull (code, NULL, 10);

  if (cnum == 0)
    return TRUE;

  cnum = status_code_to_muc_flag ((guint) cnum);

  g_hash_table_insert (status, (gpointer)cnum, (gpointer)cnum);

  /* OWN_PRESENCE  is a SHOULD       *
   * CHANGE_FORCED is a MUST   which *
   * implies OWN_PRESENCE            */
  /* 201 (NEW_ROOM) also implies OWN_PRESENCE */
  if (cnum == WOCKY_MUC_CODE_NICK_CHANGE_FORCED)
    g_hash_table_insert (status,
        (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE,
        (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE);

  if (cnum == WOCKY_MUC_CODE_NEW_ROOM)
    g_hash_table_insert (status,
        (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE,
        (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE);

  return TRUE;
}

static gboolean
presence_status (WockyXmppNode *node, gpointer data)
{
  GString **status = data;

  if (wocky_strdiff (node->name, "status"))
    return TRUE;

  if (node->content != NULL)
    {
      if (*status == NULL)
        *status = g_string_new (node->content);
      else
        g_string_append (*status, node->content);
    }

  return TRUE;
}

static void
presence_features (gpointer key,
    gpointer val,
    gpointer data)
{
  WockyMucStatusCode code = (WockyMucStatusCode) key;
  WockyMucPrivate *priv = data;

  switch (code)
    {
      case WOCKY_MUC_CODE_CFG_SHOW_UNAVAILABLE:
      case WOCKY_MUC_CODE_CFG_HIDE_UNAVAILABLE:
      case WOCKY_MUC_CODE_CFG_NONPRIVACY:
      case WOCKY_MUC_CODE_CFG_LOGGING_ENABLED:
      case WOCKY_MUC_CODE_CFG_LOGGING_DISABLED:
        /* unhandled room config change: */
        break;
      case WOCKY_MUC_CODE_CFG_ONYMOUS:
        priv->room_type |= WOCKY_MUC_NONANONYMOUS;
        priv->room_type &= ~WOCKY_MUC_SEMIANONYMOUS;
        break;
      case WOCKY_MUC_CODE_CFG_SEMIONYMOUS:
        priv->room_type |= WOCKY_MUC_SEMIANONYMOUS;
        priv->room_type &= ~WOCKY_MUC_NONANONYMOUS;
        break;
      case WOCKY_MUC_CODE_CFG_ANONYMOUS:
        priv->room_type &= ~(WOCKY_MUC_NONANONYMOUS|WOCKY_MUC_SEMIANONYMOUS);
        break;
      default:
        break;
        /* non config change, don't care */
    }
}

#define REPLACE_STR(place,val)                  \
  if (wocky_strdiff (place, val))               \
    {                                           \
      g_free (place);                           \
      place = g_strdup (val);                   \
    }

static gboolean
handle_self_presence (WockyMuc *muc,
    WockyXmppStanza *stanza,
    const gchar *nick,
    WockyMucRole role,
    WockyMucAffiliation aff,
    const gchar *actor,
    const gchar *why,
    const gchar *status,
    GHashTable *code)
{
  gboolean nick_update = FALSE;
  gboolean permission_update = FALSE;
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  DEBUG ("Received our own presence");

  /* we already know if we changed our own status, so no signal for that */
  nick_update = wocky_strdiff (priv->nick, nick);
  REPLACE_STR (priv->nick, nick);
  REPLACE_STR (priv->status, status);

  permission_update = ((priv->role != role) || (priv->affiliation != aff));
  priv->role = role;
  priv->affiliation = aff;

  g_hash_table_foreach (code, presence_features, priv);

  if (nick_update)
    {
      gchar *new_jid = g_strdup_printf ("%s@%s/%s",
          priv->room, priv->service, priv->nick);
      REPLACE_STR (priv->jid, new_jid);
      g_signal_emit (muc, signals[SIG_NICK_CHANGE], 0, stanza, code);
    }

  if (permission_update)
    g_signal_emit (muc, signals[SIG_PERM_CHANGE], 0, stanza, code, actor, why);

  return TRUE;
}

static gboolean
handle_user_presence (WockyMuc *muc,
    WockyXmppStanza *stanza,
    const gchar *from,
    const gchar *jid,
    const gchar *nick,
    WockyMucRole role,
    WockyMucAffiliation aff,
    const gchar *actor,
    const gchar *why,
    const gchar *status,
    GHashTable *code)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyMucMember *member = NULL;

  if (nick == NULL)
    return FALSE;

  member = g_hash_table_lookup (priv->members, from);

  if (member == NULL)
    {
      DEBUG ("New presence from %s, %s (state: %d)", from, nick, priv->state);

      member = alloc_member ();
      g_hash_table_insert (priv->members, g_strdup (from), member);
    }
  else
    {
    }

  REPLACE_STR (member->from, from);
  REPLACE_STR (member->jid, jid);
  REPLACE_STR (member->nick, nick);
  REPLACE_STR (member->status, status);

  member->role = role;
  member->affiliation = aff;

  if (member->presence_stanza != NULL)
    g_object_unref (member->presence_stanza);

  member->presence_stanza = g_object_ref (stanza);

  if (priv->state >= WOCKY_MUC_JOINED)
    g_signal_emit (muc, signals[SIG_PRESENCE], 0, stanza, code, member);

  return TRUE;
}

static WockyMucRole
string_to_role (const gchar *role)
{
  if (!wocky_strdiff (role, "visitor"))
    return WOCKY_MUC_ROLE_VISITOR;

  if (!wocky_strdiff (role, "participant"))
    return WOCKY_MUC_ROLE_PARTICIPANT;

  if (!wocky_strdiff (role, "moderator"))
    return WOCKY_MUC_ROLE_MODERATOR;

  return WOCKY_MUC_ROLE_NONE;
}

static WockyMucAffiliation
string_to_aff (const gchar *aff)
{
  if (!wocky_strdiff (aff, "outcast"))
    return WOCKY_MUC_AFFILIATION_OUTCAST;

  if (!wocky_strdiff (aff, "member"))
    return WOCKY_MUC_AFFILIATION_MEMBER;

  if (!wocky_strdiff (aff, "admin"))
    return WOCKY_MUC_AFFILIATION_ADMIN;

  if (!wocky_strdiff (aff, "owner"))
    return WOCKY_MUC_AFFILIATION_OWNER;

  return WOCKY_MUC_AFFILIATION_NONE;
}

static gboolean
handle_presence_standard (WockyMuc *muc,
    WockyXmppStanza *stanza,
    WockyStanzaSubType type)
{
  gchar *room = NULL;
  gchar *serv = NULL;
  gchar *nick = NULL;
  gboolean ok = FALSE;
  WockyXmppNode *node = stanza->node;
  WockyXmppNode *x = wocky_xmpp_node_get_child_ns (node, "x", WOCKY_NS_MUC_USR);
  WockyXmppNode *item = NULL;
  const gchar *from = wocky_xmpp_node_get_attribute (node, "from");
  const gchar *pjid = NULL;
  const gchar *pnic = NULL;
  const gchar *role = NULL;
  const gchar *aff = NULL;
  GHashTable *code = NULL;
  const gchar *ajid = NULL;
  const gchar *why = NULL;
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyMucRole r = WOCKY_MUC_ROLE_NONE;
  WockyMucAffiliation a = WOCKY_MUC_AFFILIATION_NONE;
  gboolean self_presence = FALSE;
  GString *status_msg = NULL;
  gchar *msg = NULL;

  if (from == NULL)
    {
      DEBUG ("presence stanza without from attribute, ignoring");
      return FALSE;
    }

  if (!wocky_decode_jid (from, &room, &serv, &nick))
    {
      g_free (room);
      g_free (serv);
      g_free (nick);
      return FALSE;
    }

  wocky_xmpp_node_each_child (node, presence_status, &status_msg);
  if (status_msg != NULL)
    msg = g_string_free (status_msg, FALSE);

  if (x != NULL)
    {
      item = wocky_xmpp_node_get_child (x, "item");

      if (item != NULL)
        {
          WockyXmppNode *actor = NULL;
          WockyXmppNode *cause = NULL;

          pjid = wocky_xmpp_node_get_attribute (item, "jid");
          pnic = wocky_xmpp_node_get_attribute (item, "nick");
          role = wocky_xmpp_node_get_attribute (item, "role");
          aff = wocky_xmpp_node_get_attribute (item, "affiliation");
          actor = wocky_xmpp_node_get_child (item, "actor");
          cause = wocky_xmpp_node_get_child (item, "reason");

          r = string_to_role (role);
          a = string_to_aff (aff);

          if (actor != NULL)
            ajid = wocky_xmpp_node_get_attribute (actor, "jid");

          if (cause != NULL)
            why = cause->content;
        }

      /* if this was not in the item, set it from the envelope: */
      if (pnic == NULL)
        pnic = nick;

      code = g_hash_table_new (g_direct_hash, NULL);
      wocky_xmpp_node_each_child (x, presence_code, code);
      /* belt and braces: it is possible OWN_PRESENCE is not set, as it is   *
       * only a SHOULD in the RFC: check the 'from' stanza attribute and the *
       * jid item node attribute against the MUC jid and the users full jid  *
       * respectively to see if this is our own presence                     */
      if (!wocky_strdiff (priv->jid,  from) ||
          !wocky_strdiff (priv->user, pjid) )
        g_hash_table_insert (code,
            (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE,
            (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE);

      self_presence = g_hash_table_lookup (code,
          (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE) != NULL;

      /* ok, we've extracted all the presence stanza data we should need: *
       * if this was a presence notification, deal with it:               */
      if (type == WOCKY_STANZA_SUB_TYPE_NONE)
        {
          /* if this was the first time we got our own presence it also means *
           * we successfully joined the channel, so update our internal state *
           * and emit the channel-joined signal                               */
          if (self_presence)
            {
              ok = handle_self_presence (muc, stanza,
                  pnic, r, a, ajid, why, msg, code);

              if (priv->state < WOCKY_MUC_JOINED)
                {
                  priv->state = WOCKY_MUC_JOINED;
                  if (priv->join_cb != NULL)
                    {
                      g_simple_async_result_complete (priv->join_cb);
                      g_object_unref (priv->join_cb);
                      priv->join_cb = NULL;
                    }
                  g_signal_emit (muc, signals[SIG_JOINED], 0, stanza, code);
                }
              else
                g_signal_emit (muc, signals[SIG_OWN_PRESENCE], 0,
                  stanza, code);
            }
          /* if this is someone else's presence, update internal member list */
          else
            {
              ok =
                handle_user_presence (muc,
                    stanza,
                    from, /* room@service/nick */
                    pjid, /* jid attr from item */
                    pnic, /* nick attr from item or /res from envelope 'from' */
                    r, a, ajid, why, msg, code);
            }
        }
      else if (type == WOCKY_STANZA_SUB_TYPE_UNAVAILABLE)
        {
          if (self_presence)
            {
              priv->state = WOCKY_MUC_ENDED;
              priv->role = WOCKY_MUC_ROLE_NONE;
              g_signal_emit (muc, signals[SIG_PARTED], 0,
                  stanza, code, ajid, why, msg);
              ok = TRUE;
            }
          else
            {
              WockyMucMember *member =
                g_hash_table_lookup (priv->members, from);

              if (member == NULL)
                {
                  DEBUG ("Someone not in the muc left!?");
                  goto out;
                }

              g_signal_emit (muc, signals[SIG_LEFT], 0,
                  stanza, code, member, ajid, why, msg);

              g_hash_table_remove (priv->members, from);
              ok = TRUE;
            }
          goto out;
        }
    }

 out:
  g_free (msg);
  if (code != NULL)
    g_hash_table_unref (code);
  return ok;
}

static gboolean
handle_presence_error (WockyMuc *muc,
    WockyXmppStanza *stanza,
    WockyStanzaSubType type)
{
  gboolean ok = FALSE;
  gchar *room = NULL;
  gchar *serv = NULL;
  gchar *nick = NULL;
  const gchar *from = wocky_xmpp_node_get_attribute (stanza->node, "from");
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  GError *error = NULL;

  if (!wocky_decode_jid (from, &room, &serv, &nick))
    {
      DEBUG ("malformed 'from' attribute in presence error stanza");
      goto out;
    }

  if (wocky_strdiff (room, priv->room) || wocky_strdiff (serv, priv->service))
    {
      DEBUG ("presence error is not from MUC - not handled");
      goto out;
    }

  wocky_xmpp_stanza_extract_errors (stanza, NULL, &error, NULL, NULL);

  if (priv->state >= WOCKY_MUC_JOINED)
    {
      DEBUG ("presence error after joining; not handled");
      DEBUG ("    %s: %s",
          wocky_xmpp_error_string (error->code),
          error->message);
    }

  g_signal_emit (muc, signals[SIG_PRESENCE_ERROR], 0, stanza, error->code,
      error->message);
  g_clear_error (&error);

 out:
  g_free (room);
  g_free (serv);
  g_free (nick);
  return ok;
}

static gboolean
handle_presence (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer data)
{
  WockyMuc *muc = WOCKY_MUC (data);
  WockyStanzaType type;
  WockyStanzaSubType subtype;
  gboolean handled = FALSE;

  wocky_xmpp_stanza_get_type_info (stanza, &type, &subtype);

  if (type != WOCKY_STANZA_TYPE_PRESENCE)
    {
      g_warning ("presence handler received '%s' stanza", stanza->node->name);
      return FALSE;
    }

  switch (subtype)
    {
      case WOCKY_STANZA_SUB_TYPE_NONE:
      case WOCKY_STANZA_SUB_TYPE_UNAVAILABLE:
        handled = handle_presence_standard (muc, stanza, subtype);
        break;
      case WOCKY_STANZA_SUB_TYPE_ERROR:
        handled = handle_presence_error (muc, stanza, subtype);
        break;
      default:
        DEBUG ("unexpected stanza sub-type: %d", subtype);
        break;
    }

  return handled;
}

/* ************************************************************************ */
/* handle message from MUC */
static gboolean
handle_message (WockyPorter *porter,
    WockyXmppStanza *stanza,
    gpointer data)
{
  WockyMuc *muc = WOCKY_MUC (data);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyStanzaSubType stype;
  WockyXmppNode *msg = stanza->node;
  const gchar *from = NULL;
  WockyXmppNode *child = NULL;
  gboolean from_self = FALSE;
  int x;

  time_t stamp = 0;
  const gchar *id = NULL;
  const gchar *body = NULL;
  const gchar *subj = NULL;
  WockyMucMember *who = NULL;
  WockyMucMsgType mtype = WOCKY_MUC_MSG_NOTICE;
  WockyMucMsgState mstate = WOCKY_MUC_MSG_STATE_NONE;

  wocky_xmpp_stanza_get_type_info (stanza, NULL, &stype);

  /* ***************************************************************** *
   * HACK: we interrupt this function for a HACK:                      *
   * google servers send offline messags w/o a type; kludge it:        */
  if (stype == WOCKY_STANZA_SUB_TYPE_NONE &&
      wocky_xmpp_node_get_child_ns (msg, "time", "google:timestamp") != NULL &&
      wocky_xmpp_node_get_child_ns (msg, "x", WOCKY_XMPP_NS_DELAY) != NULL)
    stype = WOCKY_STANZA_SUB_TYPE_GROUPCHAT;

  /* ***************************************************************** *
   * we now return you to your regular logic                           */
  id = wocky_xmpp_node_get_attribute (msg, "id");
  from = wocky_xmpp_node_get_attribute (msg, "from");

  /* if the message purports to be from a MUC member, treat as such: */
  if (strchr (from, '/') != NULL)
    {
      who = g_hash_table_lookup (priv->members, from);
      if (who == NULL)
        {
          /* not another member, is it from 'ourselves'? */
          gchar *from_jid = wocky_normalise_jid (from);

          /* is it from us? fake up a member struct */
          if (g_str_equal (from_jid, priv->jid))
            {
              from_self = TRUE;
              g_free (from_jid);
            }
          else
            {
              DEBUG ("Message received from unknown MUC member %s.", from_jid);
              g_free (from_jid);
              return FALSE;
            }
        }

      /* type must be groupchat, or it is simply a non-MUC message relayed *
       * by the MUC, and therefore not our responsibility:                 */
      if (stype != WOCKY_STANZA_SUB_TYPE_GROUPCHAT)
        {
          DEBUG ("Non groupchat message from MUC member %s: ignored.", from);
          return FALSE;
        }

      if (from_self)
        {
          who = alloc_member ();
          who->from = g_strdup (priv->jid);
          who->jid  = g_strdup (priv->user);
          who->nick = g_strdup (priv->nick);
          who->role = priv->role;
          who->affiliation = priv->affiliation;
        }
    }

  body = wocky_xmpp_node_get_content_from_child (msg, "body");
  subj = wocky_xmpp_node_get_content_from_child (msg, "subject");

  /* ********************************************************************** */
  /* parse timestap, if any */
  child = wocky_xmpp_node_get_child_ns (msg, "x", WOCKY_XMPP_NS_DELAY);

  if (child != NULL)
    {
      const gchar *tm = wocky_xmpp_node_get_attribute (child, "stamp");

      /* These timestamps do not contain a timezone, but are understood to be
       * in GMT. They're in the format yyyymmddThhmmss, so if we append 'Z'
       * we'll get (one of the many valid syntaxes for) an ISO-8601 timestamp.
       */
      if (tm != NULL)
        {
          GTimeVal timeval = { 0, 0 };
          gchar *tm_dup = g_strdup_printf ("%sZ", tm);

          if (!g_time_val_from_iso8601 (tm_dup, &timeval))
            DEBUG ("Malformed date string '%s' for " WOCKY_XMPP_NS_DELAY, tm);
          else
            stamp = timeval.tv_sec;

          g_free (tm_dup);
        }
    }

  /* ********************************************************************** */
  /* work out the message type: */
  if (body != NULL)
    {
      if (g_str_has_prefix (body, "/me "))
        {
          mtype = WOCKY_MUC_MSG_ACTION;
          body += 4;
        }
      else if (g_str_equal (body, "/me"))
        {
          mtype = WOCKY_MUC_MSG_ACTION;
          body = "";
        }
      else if ((stype == WOCKY_STANZA_SUB_TYPE_GROUPCHAT) ||
          (stype == WOCKY_STANZA_SUB_TYPE_CHAT))
        {
          mtype = WOCKY_MUC_MSG_NORMAL;
        }
    }

  if (stype == WOCKY_STANZA_SUB_TYPE_ERROR)
    {
      WockyXmppErrorType etype;
      GError *error = NULL;

      wocky_xmpp_stanza_extract_errors (stanza, &etype, &error, NULL, NULL);
      g_signal_emit (muc, signals[SIG_MSG_ERR], 0,
          stanza, mtype, id, stamp, who, body, error->code, etype);
      g_clear_error (&error);
      goto out;
    }

  for (x = 0; msg_state[x].name != NULL; x++)
    {
      const gchar *item = msg_state[x].name;
      child = wocky_xmpp_node_get_child_ns (msg, item, WOCKY_NS_CHATSTATE);
      if (child != NULL)
        break;
    }
  mstate = msg_state[x].state;

  g_signal_emit (muc, signals[SIG_MSG], 0,
      stanza, mtype, id, stamp, who, body, subj, mstate);

 out:
  if (from_self)
    free_member (who);
  return TRUE;
}

/* ************************************************************************ */
/* join MUC */
void
wocky_muc_join (WockyMuc *muc,
    GCancellable *cancel)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  if (priv->state < WOCKY_MUC_INITIATED)
    {
      register_presence_handler (muc);
      register_message_handler (muc);
    }

  priv->state = WOCKY_MUC_INITIATED;


  wocky_muc_send_presence (muc, WOCKY_STANZA_SUB_TYPE_NONE, NULL);
}

/* misc meta data */
const gchar *
wocky_muc_jid (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  return priv->jid;
}

WockyMucRole
wocky_muc_role (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  return priv->role;
}

WockyMucAffiliation
wocky_muc_affiliation (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  return priv->affiliation;
}

const gchar *
wocky_muc_user (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  return priv->user;
}

GHashTable *
wocky_muc_members (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  if (priv->members != NULL)
    return g_hash_table_ref (priv->members);

  return NULL;
}

WockyMucState
wocky_muc_get_state (WockyMuc *muc)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  return priv->state;
}

/* send message to muc */

/* send message to participant */
