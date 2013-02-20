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

/**
 * SECTION: wocky-muc
 * @title: WockyMuc
 * @short_description: multi-user chat rooms
 * @include: wocky/wocky.h
 *
 * Represents a multi-user chat room. Because the MUC protocol is so terrible,
 * you will find yourself consulting <ulink
 * url='http://xmpp.org/extensions/xep-0045.html'>XEP-0045</ulink> and shedding
 * more than a few tears while using this class.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_MUC_CONNECTION
#include "wocky-debug-internal.h"

#include "wocky-muc.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"
#include "wocky-signals-marshal.h"
#include "wocky-xmpp-error.h"

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
    WockyStanza *stanza,
    gpointer data);

static gboolean handle_message (WockyPorter *porter,
    WockyStanza *stanza,
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
  muc->priv = G_TYPE_INSTANCE_GET_PRIVATE (muc, WOCKY_TYPE_MUC,
      WockyMucPrivate);

  muc->priv->members = g_hash_table_new_full (g_str_hash,
      g_str_equal,
      g_free,
      free_member);
}

static void
wocky_muc_dispose (GObject *object)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = muc->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->pres_handler != 0)
    wocky_porter_unregister_handler (priv->porter, priv->pres_handler);
  priv->pres_handler = 0;

  if (priv->mesg_handler != 0)
    wocky_porter_unregister_handler (priv->porter, priv->mesg_handler);
  priv->mesg_handler = 0;

  if (priv->porter != NULL)
    g_object_unref (priv->porter);
  priv->porter = NULL;

  if (priv->members != NULL)
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
  WockyMucPrivate *priv = muc->priv;

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
      _wocky_signals_marshal_VOID__POINTER_UINT,
      G_TYPE_NONE, 2,
      WOCKY_TYPE_STANZA, G_TYPE_UINT);

  signals[SIG_PRESENCE] = g_signal_new ("presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_UINT_POINTER,
      G_TYPE_NONE, 3,
      WOCKY_TYPE_STANZA, G_TYPE_UINT, G_TYPE_POINTER);

  signals[SIG_OWN_PRESENCE] = g_signal_new ("own-presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_UINT,
      G_TYPE_NONE, 2,
      WOCKY_TYPE_STANZA, G_TYPE_UINT);

  /**
   * WockyMuc::joined:
   * @muc: the MUC
   * @stanza: the presence stanza
   * @codes: bitwise OR of %WockyMucStatusCode flags with miscellaneous
   *  information about the MUC
   *
   * Emitted when the local user successfully joins @muc.
   */
  signals[SIG_JOINED] = g_signal_new ("joined", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_UINT,
      G_TYPE_NONE, 2,
      WOCKY_TYPE_STANZA,
      G_TYPE_UINT);

  /**
   * WockyMuc::error:
   * @muc: the MUC
   * @stanza: the presence stanza
   * @error_type: the type of error
   * @error: an error in domain #WOCKY_XMPP_ERROR, whose message (if not %NULL)
   *  is a human-readable message from the server
   *
   * Emitted when a presence error is received from the MUC, which is generally
   * in response to trying to join the MUC.
   */
  signals[SIG_PRESENCE_ERROR] = g_signal_new ("error", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_ENUM_BOXED,
      G_TYPE_NONE, 3,
      WOCKY_TYPE_STANZA,
      WOCKY_TYPE_XMPP_ERROR_TYPE,
      G_TYPE_ERROR);

  /**
   * WockyMuc::permissions:
   * @muc: the muc
   * @stanza: the presence stanza heralding the change
   * @codes: bitwise OR of %WockyMucStatusCode flags
   * @actor_jid: the JID of the user who changed our permissions, or %NULL
   * @reason: a human-readable reason for the change, or %NULL
   *
   * Emitted when our permissions within the MUC are changed.
   */
  signals[SIG_PERM_CHANGE] = g_signal_new ("permissions", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_UINT_POINTER_POINTER,
      G_TYPE_NONE, 4,
      WOCKY_TYPE_STANZA,
      G_TYPE_UINT,
      G_TYPE_STRING,
      G_TYPE_STRING);

  /**
   * WockyMuc::parted:
   * @muc: the MUC
   * @stanza: the presence stanza
   * @codes: bitwise OR of %WockyMucStatusCode flags describing why the user
   *  left the MUC
   * @actor: if the user was removed from the MUC by another participant, that
   *  participant's JID
   * @reason: if the user was removed from the MUC by another participant, a
   *  human-readable reason given by that participant
   * @message: a parting message we provided to other participants, or %NULL
   *
   * Emitted when the local user leaves the MUC, whether by choice or by force.
   */
  signals[SIG_PARTED] = g_signal_new ("parted", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_UINT_STRING_STRING_STRING,
      G_TYPE_NONE, 5,
      WOCKY_TYPE_STANZA,
      G_TYPE_UINT,
      G_TYPE_STRING,  /* actor jid */
      G_TYPE_STRING,  /* reason    */
      G_TYPE_STRING); /* message: usually none, but allowed by spec */

  /**
   * WockyMuc::left:
   * @muc: the MUC
   * @stanza: the presence stanza
   * @codes: bitwise OR of %WockyMucStatusCode flags describing why @member
   *  left the MUC
   * @member: the (now ex-)member of the MUC who left
   * @actor: if @member was removed from the MUC by another participant, that
   *  participant's JID
   * @reason: if @member was removed from the MUC by another participant, a
   *  human-readable reason given by that participant
   * @message: a parting message provided by @member, or %NULL
   *
   * Emitted when another participant leaves, or is kicked from, the MUC
   */
  signals[SIG_LEFT] = g_signal_new ("left", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_UINT_POINTER_STRING_STRING_STRING,
      G_TYPE_NONE, 6,
      WOCKY_TYPE_STANZA,
      G_TYPE_UINT,
      G_TYPE_POINTER,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_STRING);

  /**
   * WockyMuc::message:
   * @muc: the MUC
   * @stanza: the incoming message stanza
   * @message_type: the message's type
   * @id: the stanza's identifier (which may be %NULL if neither the sender nor
   *  the MUC specified one)
   * @timestamp: for messages received as scrollback when joining the MUC, the
   *  time the message was sent; %NULL for messages received while in the MUC
   * @sender: a %WockyMucMember struct describing the sender of the message
   * @body: the body of the message, or %NULL
   * @subject: the new subject for the MUC, or %NULL
   * @state: whether @sender is currently typing.
   *
   * Emitted when a non-error message stanza is received. This may indicate:
   *
   * <itemizedlist>
   * <listitem>if @body is not %NULL, a message sent by @sender to the
   *  MUC;</listitem>
   * <listitem>or, if @subject is not %NULL, @sender changed the subject of the
   *  MUC;</listitem>
   * <listitem>additionally, that @sender is typing, or maybe stopped typing,
   *  depending on @state.</listitem>
   * </itemizedlist>
   */
  signals[SIG_MSG] = g_signal_new ("message", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_ENUM_STRING_LONG_POINTER_STRING_STRING_ENUM,
      G_TYPE_NONE, 8,
      WOCKY_TYPE_STANZA,
      WOCKY_TYPE_MUC_MSG_TYPE,
      G_TYPE_STRING,
      G_TYPE_DATE_TIME,
      G_TYPE_POINTER,
      G_TYPE_STRING,
      G_TYPE_STRING,
      WOCKY_TYPE_MUC_MSG_STATE);

  /**
   * WockyMuc::message-error:
   * @muc: the MUC
   * @stanza: the incoming %WOCKY_STANZA_SUB_TYPE_ERROR message
   * @message_type: the type of the message which was rejected
   * @id: the identifier for the original message and this error (which may be
   *  %NULL)
   * @timestamp: the timestamp attached to the original message, which is
   *  probably %NULL because timestamps are only attached to scrollback messages
   * @member: a %WockyMucMember struct describing the sender of the original
   *  message (which is, we presume, us)
   * @body: the body of the message which failed to send
   * @error_type: the type of error
   * @error: an error in domain %WOCKY_XMPP_ERROR, whose message (if not %NULL)
   *  is a human-readable message from the server
   *
   * Emitted when we receive an error from the MUC in response to sending a
   * message stanza to the MUC.
   */
  signals[SIG_MSG_ERR] = g_signal_new ("message-error", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__OBJECT_ENUM_STRING_LONG_POINTER_STRING_ENUM_BOXED,
      G_TYPE_NONE, 8,
      WOCKY_TYPE_STANZA,
      WOCKY_TYPE_MUC_MSG_TYPE,
      G_TYPE_STRING,
      G_TYPE_DATE_TIME,
      G_TYPE_POINTER,
      G_TYPE_STRING,
      WOCKY_TYPE_XMPP_ERROR_TYPE,
      G_TYPE_ERROR);

  signals[SIG_FILL_PRESENCE] = g_signal_new ("fill-presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        _wocky_signals_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        WOCKY_TYPE_STANZA);
}

static void
wocky_muc_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = muc->priv;

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
        break;
      case PROP_RNICK:
        g_free (priv->rnick);
        priv->rnick = g_value_dup_string (value);
        break;
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
  WockyMucPrivate *priv = muc->priv;

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

static guint
status_code_to_muc_flag (guint64 code)
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

static gboolean
store_muc_disco_info_x (WockyNode *field, gpointer data)
{
  WockyMucPrivate *priv = data;
  const gchar *var = NULL;

  if (wocky_strdiff (field->name, "field"))
    return TRUE;

  var = wocky_node_get_attribute (field, "var");

  if (wocky_strdiff (var, "muc#roominfo_description"))
    return TRUE;

  priv->desc = g_strdup (
      wocky_node_get_content_from_child (field, "value"));

  return TRUE;
}

static gboolean
store_muc_disco_info (WockyNode *feat, gpointer data)
{
  WockyMucPrivate *priv = data;

  if (!wocky_strdiff (feat->name, "feature"))
    {
      guint i;
      const gchar *thing = wocky_node_get_attribute (feat, "var");

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
    wocky_node_each_child (feat, store_muc_disco_info_x, priv);

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
  WockyStanza *iq;
  WockyStanzaType type;
  WockyStanzaSubType sub;
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (data);

  muc = WOCKY_MUC (g_async_result_get_source_object (G_ASYNC_RESULT (result)));
  priv = muc->priv;

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

  wocky_stanza_get_type_info (iq, &type, &sub);

  if (type != WOCKY_STANZA_TYPE_IQ)
    {
      error = g_error_new (WOCKY_XMPP_ERROR,
          WOCKY_XMPP_ERROR_UNDEFINED_CONDITION, "Bizarre response: Not an IQ");
      goto out;
    }

  switch (sub)
    {
      WockyNode *query;
      WockyNode *node;

      case WOCKY_STANZA_SUB_TYPE_RESULT:
        query = wocky_node_get_child_ns (
          wocky_stanza_get_top_node (iq), "query", WOCKY_NS_DISCO_INFO);

        if (query == NULL)
          {
            error = g_error_new (WOCKY_XMPP_ERROR,
                WOCKY_XMPP_ERROR_UNDEFINED_CONDITION,
                "Malformed IQ reply");
            goto out;
          }

        node = wocky_node_get_child (query, "identity");

        if (node == NULL)
          {
            error = g_error_new (WOCKY_XMPP_ERROR,
                WOCKY_XMPP_ERROR_UNDEFINED_CONDITION,
                "Malformed IQ reply: No Identity");
            goto out;
          }
        else
          {
            const gchar *attr;

            attr = wocky_node_get_attribute (node, "category");
            g_free (priv->id_category);
            priv->id_category = g_strdup (attr);

            attr = wocky_node_get_attribute (node, "name");
            g_free (priv->id_name);
            priv->id_name = g_strdup (attr);

            attr = wocky_node_get_attribute (node, "type");
            g_free (priv->id_type);
            priv->id_type = g_strdup (attr);
          }

        wocky_node_each_child (query, store_muc_disco_info, priv);
        if (priv->state < WOCKY_MUC_INITIATED)
          priv->state = WOCKY_MUC_INITIATED;
        break;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        wocky_stanza_extract_errors (iq, NULL, &error, NULL, NULL);
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
  WockyMucPrivate *priv = muc->priv;
  GSimpleAsyncResult *result;
  WockyStanza *iq = wocky_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET,
      priv->user,
      priv->jid,
      '(', "query",
      ':', WOCKY_NS_DISCO_INFO,
      ')',
      NULL);

  result = g_simple_async_result_new (G_OBJECT (muc), callback, data,
    wocky_muc_disco_info_async);

  wocky_porter_send_iq_async (priv->porter, iq, cancel, muc_disco_info,
      result);
}

/* ask for MUC member list */

WockyStanza *
wocky_muc_create_presence (WockyMuc *muc,
    WockyStanzaSubType type,
    const gchar *status)
{
  WockyMucPrivate *priv = muc->priv;
  WockyStanza *stanza =
    wocky_stanza_build (WOCKY_STANZA_TYPE_PRESENCE,
        type,
        priv->user,
        priv->jid,
        NULL);
  WockyNode *presence = wocky_stanza_get_top_node (stanza);


  /* There should be separate API to leave a room, but atm there isn't... so
   * only allow the status to be set directly when making a presence to leave
   * the muc */
  g_assert (status == NULL || type == WOCKY_STANZA_SUB_TYPE_UNAVAILABLE);

  if (status != NULL)
    {
      wocky_node_add_child_with_content (presence, "status", status);
    }
  else
    {
      g_signal_emit (muc, signals[SIG_FILL_PRESENCE], 0, stanza);
    }

  return stanza;
}

static void
register_presence_handler (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;

  if (priv->pres_handler == 0)
    priv->pres_handler = wocky_porter_register_handler_from (priv->porter,
        WOCKY_STANZA_TYPE_PRESENCE,
        WOCKY_STANZA_SUB_TYPE_NONE,
        priv->rjid,
        WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
        handle_presence, muc,
        NULL);
}

static void
register_message_handler (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;

  if (priv->mesg_handler == 0)
    priv->mesg_handler = wocky_porter_register_handler_from (priv->porter,
        WOCKY_STANZA_TYPE_MESSAGE,
        WOCKY_STANZA_SUB_TYPE_NONE,
        priv->rjid,
        WOCKY_PORTER_HANDLER_PRIORITY_NORMAL,
        handle_message, muc,
        NULL);
}

static guint
extract_status_codes (WockyNode *x)
{
  guint codes = 0;
  WockyNodeIter iter;
  WockyNode *node;

  wocky_node_iter_init (&iter, x, "status", NULL);
  while (wocky_node_iter_next (&iter, &node))
    {
      const gchar *code;
      WockyMucStatusCode cnum;

      code = wocky_node_get_attribute (node, "code");

      if (code == NULL)
        continue;

      cnum = status_code_to_muc_flag (g_ascii_strtoull (code, NULL, 10));
      codes |= cnum;

      /* OWN_PRESENCE  is a SHOULD       *
       * CHANGE_FORCED is a MUST   which *
       * implies OWN_PRESENCE            */
      /* 201 (NEW_ROOM) also implies OWN_PRESENCE */
      if (cnum == WOCKY_MUC_CODE_NICK_CHANGE_FORCED)
        codes |= WOCKY_MUC_CODE_OWN_PRESENCE;

      if (cnum == WOCKY_MUC_CODE_NEW_ROOM)
        codes |= WOCKY_MUC_CODE_OWN_PRESENCE;
    }

  return codes;
}

static void
presence_features (
    WockyMucPrivate *priv,
    guint codes)
{
  if ((codes & WOCKY_MUC_CODE_CFG_ONYMOUS) != 0)
    {
      priv->room_type |= WOCKY_MUC_NONANONYMOUS;
      priv->room_type &= ~WOCKY_MUC_SEMIANONYMOUS;
    }
  else if ((codes & WOCKY_MUC_CODE_CFG_SEMIONYMOUS) != 0)
    {
      priv->room_type |= WOCKY_MUC_SEMIANONYMOUS;
      priv->room_type &= ~WOCKY_MUC_NONANONYMOUS;
    }
  else if ((codes & WOCKY_MUC_CODE_CFG_ANONYMOUS) != 0)
    {
      priv->room_type &= ~(WOCKY_MUC_NONANONYMOUS|WOCKY_MUC_SEMIANONYMOUS);
    }
}

#define REPLACE_STR(place,val)                  \
  if (wocky_strdiff (place, val))               \
    {                                           \
      g_free (place);                           \
      place = g_strdup (val);                   \
    }

static void
handle_self_presence (WockyMuc *muc,
    WockyStanza *stanza,
    const gchar *nick,
    WockyMucRole role,
    WockyMucAffiliation aff,
    const gchar *actor,
    const gchar *why,
    const gchar *status,
    guint codes)
{
  gboolean nick_update = FALSE;
  gboolean permission_update = FALSE;
  WockyMucPrivate *priv = muc->priv;

  DEBUG ("Received our own presence");

  if (wocky_strdiff (priv->nick, nick))
    {
      nick_update = TRUE;
      g_free (priv->nick);
      priv->nick = g_strdup (nick);
    }

  /* we already know if we changed our own status, so no signal for that */
  REPLACE_STR (priv->status, status);

  permission_update = ((priv->role != role) || (priv->affiliation != aff));
  priv->role = role;
  priv->affiliation = aff;

  presence_features (priv, codes);

  if (nick_update)
    {
      gchar *new_jid = g_strdup_printf ("%s@%s/%s",
          priv->room, priv->service, priv->nick);

      g_free (priv->jid);
      priv->jid = new_jid;
      g_signal_emit (muc, signals[SIG_NICK_CHANGE], 0, stanza, codes);
    }

  if (permission_update)
    g_signal_emit (muc, signals[SIG_PERM_CHANGE], 0, stanza, codes, actor, why);
}

static gboolean
handle_user_presence (WockyMuc *muc,
    WockyStanza *stanza,
    const gchar *from,
    const gchar *jid,
    const gchar *nick,
    WockyMucRole role,
    WockyMucAffiliation aff,
    const gchar *actor,
    const gchar *why,
    const gchar *status,
    guint codes)
{
  WockyMucPrivate *priv = muc->priv;
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
    g_signal_emit (muc, signals[SIG_PRESENCE], 0, stanza, codes, member);

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
    WockyStanza *stanza,
    WockyStanzaSubType type,
    const gchar *resource)
{
  WockyNode *node = wocky_stanza_get_top_node (stanza);
  WockyNode *x = wocky_node_get_child_ns (node,
      "x", WOCKY_NS_MUC_USER);
  WockyNode *item = NULL;
  const gchar *from = wocky_stanza_get_from (stanza);
  const gchar *pjid = NULL;
  const gchar *pnic = NULL;
  const gchar *role = NULL;
  const gchar *aff = NULL;
  guint codes = 0;
  const gchar *ajid = NULL;
  const gchar *why = NULL;
  WockyMucPrivate *priv = muc->priv;
  WockyMucRole r = WOCKY_MUC_ROLE_NONE;
  WockyMucAffiliation a = WOCKY_MUC_AFFILIATION_NONE;
  gboolean self_presence = FALSE;
  const gchar *msg = NULL;

  msg = wocky_node_get_content_from_child (node, "status");

  if (x == NULL)
    return FALSE;

  item = wocky_node_get_child (x, "item");

  if (item != NULL)
    {
      WockyNode *actor = NULL;
      WockyNode *cause = NULL;

      pjid = wocky_node_get_attribute (item, "jid");
      pnic = wocky_node_get_attribute (item, "nick");
      role = wocky_node_get_attribute (item, "role");
      aff = wocky_node_get_attribute (item, "affiliation");
      actor = wocky_node_get_child (item, "actor");
      cause = wocky_node_get_child (item, "reason");

      r = string_to_role (role);
      a = string_to_aff (aff);

      if (actor != NULL)
        ajid = wocky_node_get_attribute (actor, "jid");

      if (cause != NULL)
        why = cause->content;
    }

  /* if this was not in the item, set it from the envelope: */
  if (pnic == NULL)
    pnic = resource;

  codes = extract_status_codes (x);
  /* belt and braces: it is possible OWN_PRESENCE is not set, as it is   *
   * only a SHOULD in the RFC: check the 'from' stanza attribute and the *
   * jid item node attribute against the MUC jid and the users full jid  *
   * respectively to see if this is our own presence                     */
  if (!wocky_strdiff (priv->jid,  from) ||
      !wocky_strdiff (priv->user, pjid) )
    codes |= WOCKY_MUC_CODE_OWN_PRESENCE;

  self_presence = (codes & WOCKY_MUC_CODE_OWN_PRESENCE) != 0;

  /* ok, we've extracted all the presence stanza data we should need: *
   * if this was a presence notification, deal with it:               */
  if (type == WOCKY_STANZA_SUB_TYPE_NONE)
    {
      /* if this was the first time we got our own presence it also means *
       * we successfully joined the channel, so update our internal state *
       * and emit the channel-joined signal                               */
      if (self_presence)
        {
          handle_self_presence (muc, stanza,
              pnic, r, a, ajid, why, msg, codes);

          if (priv->state < WOCKY_MUC_JOINED)
            {
              priv->state = WOCKY_MUC_JOINED;
              if (priv->join_cb != NULL)
                {
                  g_simple_async_result_complete (priv->join_cb);
                  g_object_unref (priv->join_cb);
                  priv->join_cb = NULL;
                }
              g_signal_emit (muc, signals[SIG_JOINED], 0, stanza, codes);
            }
          else
            g_signal_emit (muc, signals[SIG_OWN_PRESENCE], 0,
              stanza, codes);

          /* Allow other handlers to run for this stanza. */
          return FALSE;
        }
      /* if this is someone else's presence, update internal member list */
      else
        {
          return
            handle_user_presence (muc,
                stanza,
                from, /* room@service/nick */
                pjid, /* jid attr from item */
                pnic, /* nick attr from item or /res from envelope 'from' */
                r, a, ajid, why, msg, codes);
        }
    }
  else if (type == WOCKY_STANZA_SUB_TYPE_UNAVAILABLE)
    {
      if (self_presence)
        {
          priv->state = WOCKY_MUC_ENDED;
          priv->role = WOCKY_MUC_ROLE_NONE;
          g_signal_emit (muc, signals[SIG_PARTED], 0,
              stanza, codes, ajid, why, msg);
          return TRUE;
        }
      else
        {
          WockyMucMember *member =
            g_hash_table_lookup (priv->members, from);

          if (member == NULL)
            {
              DEBUG ("Someone not in the muc left!?");
              return FALSE;
            }

          g_signal_emit (muc, signals[SIG_LEFT], 0,
              stanza, codes, member, ajid, why, msg);

          g_hash_table_remove (priv->members, from);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
handle_presence_error (WockyMuc *muc,
    WockyStanza *stanza)
{
  gboolean ok = FALSE;
  WockyMucPrivate *priv = muc->priv;
  WockyXmppErrorType type;
  GError *error = NULL;

  wocky_stanza_extract_errors (stanza, &type, &error, NULL, NULL);

  if (priv->state >= WOCKY_MUC_JOINED)
    {
      DEBUG ("presence error after joining; not handled");
      DEBUG ("    %s: %s",
          wocky_xmpp_error_string (error->code),
          error->message);
    }

  g_signal_emit (muc, signals[SIG_PRESENCE_ERROR], 0, stanza, type, error);
  g_clear_error (&error);

  return ok;
}

static gboolean
handle_presence (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer data)
{
  WockyMuc *muc = WOCKY_MUC (data);
  WockyStanzaSubType subtype;
  gboolean handled = FALSE;

  wocky_stanza_get_type_info (stanza, NULL, &subtype);

  switch (subtype)
    {
      case WOCKY_STANZA_SUB_TYPE_NONE:
      case WOCKY_STANZA_SUB_TYPE_UNAVAILABLE:
      {
        gchar *resource;

        /* If the JID is unparseable, discard the stanza. The porter shouldn't
         * even give us such stanzas. */
        if (!wocky_decode_jid (wocky_stanza_get_from (stanza), NULL, NULL,
              &resource))
          return TRUE;

        handled = handle_presence_standard (muc, stanza, subtype, resource);
        g_free (resource);
        break;
      }
      case WOCKY_STANZA_SUB_TYPE_ERROR:
        handled = handle_presence_error (muc, stanza);
        break;
      default:
        DEBUG ("unexpected stanza sub-type: %d", subtype);
        break;
    }

  return handled;
}

/* Looks up the sender of a message. If they're not currently a MUC member,
 * then a temporary structure is created, and @member_is_temporary is set to
 * %TRUE; the caller needs to free the returned value when they're done with
 * it.
 */
static WockyMucMember *
get_message_sender (WockyMuc *muc,
    const gchar *from,
    gboolean *member_is_temporary)
{
  WockyMucPrivate *priv = muc->priv;
  WockyMucMember *who = g_hash_table_lookup (priv->members, from);

  if (who != NULL)
    {
      *member_is_temporary = FALSE;
      return who;
    }

  /* Okay, it's from someone not currently in the MUC. We'll have to
   * fake up a structure. */
  *member_is_temporary = TRUE;

  who = alloc_member ();
  who->from = wocky_normalise_jid (from);

  if (!wocky_strdiff (who->from, priv->jid))
  {
    /* It's from us! */
    who->jid  = g_strdup (priv->user);
    who->nick = g_strdup (priv->nick);
    who->role = priv->role;
    who->affiliation = priv->affiliation;
  }
  /* else, we don't know anything more about the sender.
   *
   * FIXME: actually, if the server uses XEP-0203 Delayed Delivery
   * rather than XEP-0091 Legacy Delayed Delivery, the from=''
   * attribute of the <delay/> element says who the original JID
   * actually was. Unfortunately, XEP-0091 said that from='' should be
   * the bare JID of the MUC, so it's completely useless.
   *
   * FIXME: also: we assume here that a delayed message from resource
   * /blah was sent by the user currently called /blah, but that ain't
   * necessarily so.
   */

  return who;
}

/*
 * Parse timestamp of delayed messages. For non-delayed, it's 0.
 */
static GDateTime *
extract_timestamp (WockyNode *msg)
{
  WockyNode *x = wocky_node_get_child_ns (msg, "x", WOCKY_XMPP_NS_DELAY);
  GDateTime *stamp = NULL;

  if (x != NULL)
    {
      const gchar *tm = wocky_node_get_attribute (x, "stamp");

      /* These timestamps do not contain a timezone, but are understood to be
       * in GMT. They're in the format yyyymmddThhmmss, so if we append 'Z'
       * we'll get (one of the many valid syntaxes for) an ISO-8601 timestamp.
       */
      if (tm != NULL)
        {
          GTimeVal timeval = { 0, 0 };
          gchar *tm_dup = g_strdup_printf ("%sZ", tm);

          /* FIXME: GTimeVal should go away */
          if (!g_time_val_from_iso8601 (tm_dup, &timeval))
            DEBUG ("Malformed date string '%s' for " WOCKY_XMPP_NS_DELAY, tm);
          else
            stamp = g_date_time_new_from_timeval_local (&timeval);

          g_free (tm_dup);
        }
    }

  return stamp;
}

/* Messages starting with /me are ACTION messages, and the /me should be
 * removed. type="chat" messages are NORMAL.  Everything else is
 * something that doesn't necessarily expect a reply or ongoing
 * conversation ("normal") or has been auto-sent, so we make it NOTICE in
 * all other cases. */
static WockyMucMsgType
determine_message_type (const gchar **body,
    WockyStanzaSubType sub_type)
{
  WockyMucMsgType mtype = WOCKY_MUC_MSG_NOTICE;

  if (*body != NULL)
    {
      if (g_str_has_prefix (*body, "/me "))
        {
          mtype = WOCKY_MUC_MSG_ACTION;
          *body += 4;
        }
      else if (g_str_equal (body, "/me"))
        {
          mtype = WOCKY_MUC_MSG_ACTION;
          *body = "";
        }
      else if ((sub_type == WOCKY_STANZA_SUB_TYPE_GROUPCHAT) ||
               (sub_type == WOCKY_STANZA_SUB_TYPE_CHAT))
        {
          mtype = WOCKY_MUC_MSG_NORMAL;
        }
    }

  return mtype;
}

static WockyMucMsgState
extract_chat_state (WockyNode *msg)
{
  WockyNode *child = wocky_node_get_first_child_ns (msg, WOCKY_NS_CHATSTATE);
  WockyMucMsgState mstate;

  if (child == NULL ||
      !wocky_enum_from_nick (WOCKY_TYPE_MUC_MSG_STATE, child->name, &mstate))
    mstate = WOCKY_MUC_MSG_NONE;

  return mstate;
}

static gboolean
handle_message (WockyPorter *porter,
    WockyStanza *stanza,
    gpointer data)
{
  WockyMuc *muc = WOCKY_MUC (data);
  WockyNode *msg = wocky_stanza_get_top_node (stanza);
  const gchar *id = wocky_node_get_attribute (msg, "id");
  const gchar *from = wocky_node_get_attribute (msg, "from");
  const gchar *body = wocky_node_get_content_from_child (msg, "body");
  const gchar *subj = wocky_node_get_content_from_child (msg, "subject");
  GDateTime *datetime = extract_timestamp (msg);
  WockyStanzaSubType sub_type;
  WockyMucMsgType mtype;
  WockyMucMember *who = NULL;
  gboolean member_is_temporary = FALSE;

  wocky_stanza_get_type_info (stanza, NULL, &sub_type);

  /* if the message purports to be from a MUC member, treat as such: */
  if (strchr (from, '/') != NULL)
    {
      who = get_message_sender (muc, from, &member_is_temporary);

      /* If it's a message from a member (as opposed to the MUC itself), and
       * it's not type='groupchat', then it's a non-MUC message relayed by the
       * MUC and therefore not our responsibility.
       */
      if (sub_type != WOCKY_STANZA_SUB_TYPE_GROUPCHAT)
        {
          DEBUG ("Non groupchat message from MUC member %s: ignored.", from);
          return FALSE;
        }
    }

  mtype = determine_message_type (&body, sub_type);

  if (sub_type == WOCKY_STANZA_SUB_TYPE_ERROR)
    {
      WockyXmppErrorType etype;
      GError *error = NULL;

      wocky_stanza_extract_errors (stanza, &etype, &error, NULL, NULL);
      g_signal_emit (muc, signals[SIG_MSG_ERR], 0,
          stanza, mtype, id, datetime, who, body, etype, error);
      g_clear_error (&error);
    }
  else
    {
      WockyMucMsgState mstate = extract_chat_state (msg);

      g_signal_emit (muc, signals[SIG_MSG], 0,
          stanza, mtype, id, datetime, who, body, subj, mstate);
    }

  if (member_is_temporary)
    free_member (who);

  if (datetime != NULL)
    g_date_time_unref (datetime);

  return TRUE;
}

void
wocky_muc_join (WockyMuc *muc,
    GCancellable *cancel)
{
  WockyMucPrivate *priv = muc->priv;
  WockyStanza *presence = wocky_muc_create_presence (muc,
      WOCKY_STANZA_SUB_TYPE_NONE, NULL);
  WockyNode *x = wocky_node_add_child_ns (wocky_stanza_get_top_node (presence),
      "x", WOCKY_NS_MUC);

  if (priv->pass != NULL)
    wocky_node_add_child_with_content (x, "password", priv->pass);

  if (priv->state < WOCKY_MUC_INITIATED)
    {
      register_presence_handler (muc);
      register_message_handler (muc);
    }

  priv->state = WOCKY_MUC_INITIATED;

  wocky_porter_send (priv->porter, presence);
  g_object_unref (presence);
}

/* misc meta data */
const gchar *
wocky_muc_jid (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;
  return priv->jid;
}

WockyMucRole
wocky_muc_role (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;
  return priv->role;
}

WockyMucAffiliation
wocky_muc_affiliation (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;
  return priv->affiliation;
}

const gchar *
wocky_muc_user (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;
  return priv->user;
}

GHashTable *
wocky_muc_members (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;

  if (priv->members != NULL)
    return g_hash_table_ref (priv->members);

  return NULL;
}

WockyMucState
wocky_muc_get_state (WockyMuc *muc)
{
  WockyMucPrivate *priv = muc->priv;

  return priv->state;
}

/* send message to muc */

/* send message to participant */
