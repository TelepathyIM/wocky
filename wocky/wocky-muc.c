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

#define DEBUG_FLAG DEBUG_MUC_CONNECTION
#include "wocky-debug.h"

#include "wocky-muc.h"
#include "wocky-namespaces.h"
#include "wocky-utils.h"
#include "wocky-signals-marshal.h"
#include "wocky-xmpp-error.h"

typedef enum {
  SIG_NICK_CHANGE,
  SIG_PERM_CHANGE,
  SIG_PRESENCE,
  SIG_PRESENCE_ERROR,
  SIG_JOINED,
  SIG_PARTED,
  SIG_LEFT,
  SIG_NULL
} WockyMucSig;

static guint signals[SIG_NULL] = { 0 };

typedef enum {
  MUC_CREATED = 0,
  MUC_INITIATED,
  MUC_AUTH,
  MUC_JOINED,
  MUC_ENDED,
} WockyMucState;

typedef struct { GAsyncReadyCallback func; gpointer data; } Callback;

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
  g_free (member->jid);
  g_free (member->nick);
  g_free (member->status);
  g_free (member);
}

static gpointer
alloc_member (void)
{
  return g_new0 (WockyMucMember, 1);
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
      (G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
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

  spec = g_param_spec_string ("pass", "pass",
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

  spec = g_param_spec_int ("affiliation", "affiliation",
      "The affiliation of the user with the MUC room",
      WOCKY_MUC_AFFILIATION_OUTCAST,
      WOCKY_MUC_AFFILIATION_NONE,
      WOCKY_MUC_AFFILIATION_OWNER,
      (G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (oclass, PROP_AFFILIATION, spec);

  signals[SIG_NICK_CHANGE] = g_signal_new ("nick-change", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_HASH_TABLE);

  signals[SIG_PERM_CHANGE] = g_signal_new ("perm-change", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_HASH_TABLE);

  signals[SIG_PARTED] = g_signal_new ("parted", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_HASH_TABLE);

  signals[SIG_PRESENCE] = g_signal_new ("presence", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_POINTER,
      G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

  signals[SIG_PRESENCE_ERROR] = g_signal_new ("presence-error", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__ENUM_POINTER,
      G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_ENUM, G_TYPE_STRING);

  signals[SIG_JOINED] = g_signal_new ("joined", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_POINTER,
      G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);

  signals[SIG_LEFT] = g_signal_new ("left", ctype,
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _wocky_signals_marshal_VOID__POINTER_POINTER,
      G_TYPE_NONE, 2, G_TYPE_HASH_TABLE, G_TYPE_POINTER);
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
        priv->porter = g_value_get_object (value);
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
        g_value_set_uint (value, priv->affiliation);
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
  WockyXmppNode *val = NULL;
  const gchar *var = NULL;

  if (wocky_strdiff (field->name, "field"))
    return TRUE;

  var = wocky_xmpp_node_get_attribute (field, "var");

  if (wocky_strdiff (var, "muc#roominfo_description"))
    return TRUE;

  val = wocky_xmpp_node_get_child (field, "value");

  if (val == NULL)
    return TRUE;

  priv->desc = g_strdup (val->content);

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
  WockyMuc *muc = WOCKY_MUC (source);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  Callback *cb = data;
  GAsyncReadyCallback func = cb->func;
  gpointer user_data = cb->data;
  GError *error = NULL;
  WockyXmppStanza *iq = wocky_porter_send_iq_finish (priv->porter, res, &error);
  WockyStanzaType type;
  WockyStanzaSubType sub;
  GSimpleAsyncResult *result = NULL;

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
        if (priv->state < MUC_INITIATED)
          priv->state = MUC_INITIATED;
        break;

      case WOCKY_STANZA_SUB_TYPE_ERROR:
        error = wocky_xmpp_stanza_to_gerror (iq);
        break;

      default:
        break;
    }

 out:
  if (error != NULL)
    {
      result =
        g_simple_async_result_new_from_error (source, func, user_data, error);
      g_error_free (error);
    }
  else
    {
      result = g_simple_async_result_new (source, func, user_data,
          wocky_muc_disco_info_finish);
    }

  cb->func (source, (GAsyncResult *)result, cb->data);
  g_free (cb);
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
  Callback *cb = g_new0 (Callback, 1);
  WockyXmppStanza *iq = wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
      WOCKY_STANZA_SUB_TYPE_GET,
      priv->user,
      priv->jid,
      WOCKY_NODE, "query",
      WOCKY_NODE_XMLNS, NS_DISCO_INFO,
      WOCKY_NODE_END,
      WOCKY_STANZA_END);

  cb->func = callback;
  cb->data = data;

  wocky_porter_send_iq_async (priv->porter, iq, cancel, muc_disco_info, cb);
}

/* ask for MUC member list */

/* ************************************************************************ */
/* send presence to MUC */
void
wocky_muc_send_presence (WockyMuc *muc,
    WockyStanzaSubType presence_type,
    const gchar *status)
{
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyXmppStanza *stanza =
    wocky_xmpp_stanza_build (WOCKY_STANZA_TYPE_IQ,
        presence_type,
        priv->user,
        priv->jid,
        WOCKY_STANZA_END);
  WockyXmppNode *presence = stanza->node;
  WockyXmppNode *x = wocky_xmpp_node_add_child_ns (presence, "x", WOCKY_NS_MUC);

  if (status != NULL)
    {
      wocky_xmpp_node_add_child_with_content (presence, "show", "xa");
      wocky_xmpp_node_add_child_with_content (presence, "status", status);
    }

  if (priv->pass != NULL)
    wocky_xmpp_node_add_child_with_content(x, "password", priv->pass);

  wocky_porter_send (priv->porter, stanza);
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

  cnum = g_ascii_strtoull (code, NULL, 10);

  if (cnum == 0)
    return TRUE;

  cnum = status_code_to_muc_flag ((guint) cnum);

  g_hash_table_insert (status, (gpointer)cnum, (gpointer)cnum);

  /* OWN_PRESENCE  is a SHOULD       *
   * CHANGE_FORCED is a MUST   which *
   * implies OWN_PRESENCE            */
  if (cnum == WOCKY_MUC_CODE_NICK_CHANGE_FORCED)
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

  /* we already know if we changed our own status, so no signal for that */
  nick_update = wocky_strdiff (priv->nick, nick);
  REPLACE_STR (priv->nick, nick);
  REPLACE_STR (priv->status, status);

  permission_update = ((priv->role != role) || (priv->affiliation != aff));
  priv->role = role;
  priv->affiliation = aff;

  if (nick_update)
    g_signal_emit (muc, SIG_NICK_CHANGE, 0, code);

  if (permission_update)
    g_signal_emit (muc, SIG_PERM_CHANGE, 0, code);

  g_hash_table_foreach (code, presence_features, priv);

  return TRUE;
}

static gboolean
handle_user_presence (WockyMuc *muc,
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

  member = g_hash_table_lookup (priv->members, nick);

  if (member == NULL)
    {
      member = alloc_member();
      member->jid = g_strdup (jid);
      member->nick = g_strdup (nick);
      member->role = role;
      member->affiliation = aff;
      member->status = g_strdup (status);
      g_hash_table_insert (priv->members, g_strdup (nick), member);
    }
  else
    {
      REPLACE_STR (member->jid, jid);
      REPLACE_STR (member->nick, nick);
      REPLACE_STR (member->status, status);
      member->role = role;
      member->affiliation = aff;
    }

  if (priv->state >= MUC_JOINED)
    g_signal_emit (muc, SIG_PRESENCE, 0, code, member);

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
    WockyXmppNode *node,
    WockyStanzaSubType type)
{
  gchar *room = NULL;
  gchar *serv = NULL;
  gchar *nick = NULL;
  gboolean ok = FALSE;
  WockyXmppNode *x = wocky_xmpp_node_get_child_ns (node, "x", WOCKY_NS_MUC_USR);
  WockyXmppNode *item = wocky_xmpp_node_get_child (x, "item");
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

  if (x == NULL)
    return FALSE;

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

  self_presence =
    g_hash_table_lookup (code, (gpointer)WOCKY_MUC_CODE_OWN_PRESENCE) != NULL;

  /* ok, we've extracted all the presence stanza meta data we should need:  *
   * if this was a presence notification, deal with it:                     */
  if (type == WOCKY_STANZA_SUB_TYPE_NONE)
    {
      /* if this was the first time we got our own presence, it also means *
       * we successfully joined the channel, so update our internal state  *
       * and emit the channel-joined signal                                */
      if (self_presence)
        {
          ok = handle_self_presence (muc, pnic, r, a, ajid, why, msg, code);
          if (priv->state < MUC_JOINED)
            {
              priv->state = MUC_JOINED;
              if (priv->join_cb != NULL)
                {
                  g_simple_async_result_complete (priv->join_cb);
                  g_free (priv->join_cb);
                  priv->join_cb = NULL;
                }
              g_signal_emit (muc, SIG_JOINED, 0, code);
            }
        }
      /* if this is someone else's presence, update our internal member list */
      else
        {
          ok =
            handle_user_presence (muc, pjid, pnic, r, a, ajid, why, msg, code);
        }
    }
  else if (type == WOCKY_STANZA_SUB_TYPE_UNAVAILABLE)
    {
      if (self_presence)
        {
          g_signal_emit (muc, SIG_PARTED, 0, code);
        }
      else
        {
          WockyMucMember *member = g_hash_table_lookup (priv->members, pnic);
          g_signal_emit (muc, SIG_LEFT, 0, code, member);
          g_hash_table_remove (priv->members, pnic);
        }
    }

  g_free (msg);
  g_hash_table_unref (code);
  return ok;
}

static gboolean
handle_presence_error (WockyMuc *muc,
    WockyXmppNode *node,
    WockyStanzaSubType type)
{
  gboolean ok = FALSE;
  gchar *room = NULL;
  gchar *serv = NULL;
  gchar *nick = NULL;
  const gchar *err = NULL;
  WockyXmppNode *text = NULL;
  const gchar *from = wocky_xmpp_node_get_attribute (node, "from");
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  WockyXmppError errnum = WOCKY_XMPP_ERROR_UNDEFINED_CONDITION;
  const gchar *message = NULL;

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

  err = wocky_xmpp_error_unpack_node (node, NULL, &text, NULL, NULL, &errnum);

  if (err == NULL)
    {
      DEBUG ("malformed error stanza");
      goto out;
    }

  if (priv->state >= MUC_JOINED)
    {
      DEBUG ("presence error after joining: not handled");
      if (text != NULL)
        DEBUG ("    %s: %s", err, text->content);
      goto out;
    }

  message = text ? text->content : err;
  g_signal_emit (muc, SIG_PRESENCE_ERROR, 0, errnum, message);

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
        handled = handle_presence_standard (muc, stanza->node, subtype);
        break;
      case WOCKY_STANZA_SUB_TYPE_ERROR:
        handled = handle_presence_error (muc, stanza->node, subtype);
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
  /* WockyMuc *muc = WOCKY_MUC (data); */
  /* WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc); */

  return TRUE;
}

/* ************************************************************************ */
/* initiate MUC */
static void
muc_initiate (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  GError *error = NULL;
  WockyMuc *muc = WOCKY_MUC (source);
  Callback *cb = data;
  GSimpleAsyncResult *result = NULL;

  if (wocky_muc_disco_info_finish (muc, res, &error))
    {
      result = g_simple_async_result_new (source, cb->func, cb->data,
          wocky_muc_initiate_finish);
      register_presence_handler (muc);
      register_message_handler (muc);
    }
  else
    {
      result = g_simple_async_result_new_from_error (source, cb->func, cb->data,
          error);
    }

  g_simple_async_result_complete (result);
  g_free (result);
  g_free (cb);
}

gboolean
wocky_muc_initiate_finish (GObject *source,
    GAsyncResult *res,
    GError **error)
{
  WockyMuc *muc = WOCKY_MUC (source);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (result, error))
    return FALSE;

  return (priv->state >= MUC_INITIATED);
}

void
wocky_muc_initiate_async (WockyMuc *muc,
    GAsyncReadyCallback callback,
    GCancellable *cancel,
    gpointer data)
{
  Callback *cb = NULL;
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);

  if (priv->state >= MUC_INITIATED)
    {
      GSimpleAsyncResult *res =
        g_simple_async_result_new (G_OBJECT (muc), callback, data,
            wocky_muc_initiate_finish);
      g_simple_async_result_complete (res);
      g_free (res);
      return;
    }

  cb = g_new0 (Callback, 1);
  cb->func = callback;
  cb->data = data;
  wocky_muc_disco_info_async (muc, muc_initiate, cancel, cb);
}

/* ************************************************************************ */
/* join MUC */
static void
muc_join_init_cb (GObject *source,
    GAsyncResult *res,
    gpointer data)
{
  GError *error = NULL;
  WockyMuc *muc = WOCKY_MUC (source);
  GSimpleAsyncResult *result = NULL;
  Callback *cb = data;

  if (wocky_muc_initiate_finish (source, res, &error))
    {
      WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
      wocky_muc_send_presence (muc, WOCKY_STANZA_SUB_TYPE_NONE, NULL);
      priv->join_cb = g_simple_async_result_new (source, cb->func, cb->data,
          wocky_muc_join_finish);
      return;
    }

  result =
    g_simple_async_result_new_from_error (source, cb->func, cb->data, error);
  g_simple_async_result_complete (result);
  g_free (res);
  g_free (cb);
}

gboolean
wocky_muc_join_finish (GObject *object,
    GAsyncResult *res,
    GError **error)
{
  WockyMuc *muc = WOCKY_MUC (object);
  WockyMucPrivate *priv = WOCKY_MUC_GET_PRIVATE (muc);
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (result, error))
    return FALSE;

  return (priv->state >= MUC_JOINED);
}

void
wocky_muc_join_async (WockyMuc *muc,
    GAsyncReadyCallback callback,
    GCancellable *cancel,
    gpointer data)
{
  Callback *cb = g_new0 (Callback, 1);
  cb->func = callback;
  cb->data = data;

  wocky_muc_initiate_async (muc, muc_join_init_cb, cancel, cb);
}

/* process status code from MUC */

/* send message to muc */

/* send message to participant */
