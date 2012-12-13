/*
 * jingle-transport-iceudp.c - Source for WockyJingleTransportIceUdp
 *
 * Copyright (C) 2008 Collabora Ltd.
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

#include "config.h"
#include "jingle-transport-iceudp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define DEBUG_FLAG GABBLE_DEBUG_MEDIA

#include "debug.h"
#include "jingle-content.h"
#include "jingle-factory.h"
#include "jingle-session.h"
#include "namespaces.h"

static void
transport_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (WockyJingleTransportIceUdp,
    wocky_jingle_transport_iceudp, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (WOCKY_TYPE_JINGLE_TRANSPORT_IFACE,
        transport_iface_init));

/* signal enum */
enum
{
  NEW_CANDIDATES,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* properties */
enum
{
  PROP_CONTENT = 1,
  PROP_TRANSPORT_NS,
  PROP_STATE,
  LAST_PROPERTY
};

struct _WockyJingleTransportIceUdpPrivate
{
  WockyJingleContent *content;
  WockyJingleTransportState state;
  gchar *transport_ns;

  GList *local_candidates;

  /* A pointer into "local_candidates" list to mark the
   * candidates that are still not transmitted, or NULL
   * if all of them are transmitted. */

  GList *pending_candidates;
  GList *remote_candidates;

  gchar *ufrag;
  gchar *pwd;

  /* next ID to send with a candidate */
  int id_sequence;

  gboolean dispose_has_run;
};

static void
wocky_jingle_transport_iceudp_init (WockyJingleTransportIceUdp *obj)
{
  WockyJingleTransportIceUdpPrivate *priv =
     G_TYPE_INSTANCE_GET_PRIVATE (obj, WOCKY_TYPE_JINGLE_TRANSPORT_ICEUDP,
         WockyJingleTransportIceUdpPrivate);
  obj->priv = priv;

  priv->id_sequence = 1;
  priv->dispose_has_run = FALSE;
}

static void
wocky_jingle_transport_iceudp_dispose (GObject *object)
{
  WockyJingleTransportIceUdp *trans = WOCKY_JINGLE_TRANSPORT_ICEUDP (object);
  WockyJingleTransportIceUdpPrivate *priv = trans->priv;

  if (priv->dispose_has_run)
    return;

  DEBUG ("dispose called");
  priv->dispose_has_run = TRUE;

  jingle_transport_free_candidates (priv->remote_candidates);
  priv->remote_candidates = NULL;

  jingle_transport_free_candidates (priv->local_candidates);
  priv->local_candidates = NULL;

  g_free (priv->transport_ns);
  priv->transport_ns = NULL;

  g_free (priv->ufrag);
  priv->ufrag = NULL;

  g_free (priv->pwd);
  priv->pwd = NULL;

  if (G_OBJECT_CLASS (wocky_jingle_transport_iceudp_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_jingle_transport_iceudp_parent_class)->dispose (object);
}

static void
wocky_jingle_transport_iceudp_get_property (GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec)
{
  WockyJingleTransportIceUdp *trans = WOCKY_JINGLE_TRANSPORT_ICEUDP (object);
  WockyJingleTransportIceUdpPrivate *priv = trans->priv;

  switch (property_id) {
    case PROP_CONTENT:
      g_value_set_object (value, priv->content);
      break;
    case PROP_TRANSPORT_NS:
      g_value_set_string (value, priv->transport_ns);
      break;
    case PROP_STATE:
      g_value_set_uint (value, priv->state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
wocky_jingle_transport_iceudp_set_property (GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
  WockyJingleTransportIceUdp *trans = WOCKY_JINGLE_TRANSPORT_ICEUDP (object);
  WockyJingleTransportIceUdpPrivate *priv = trans->priv;

  switch (property_id) {
    case PROP_CONTENT:
      priv->content = g_value_get_object (value);
      break;
    case PROP_TRANSPORT_NS:
      g_free (priv->transport_ns);
      priv->transport_ns = g_value_dup_string (value);
      break;
    case PROP_STATE:
      priv->state = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
wocky_jingle_transport_iceudp_class_init (WockyJingleTransportIceUdpClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (WockyJingleTransportIceUdpPrivate));

  object_class->get_property = wocky_jingle_transport_iceudp_get_property;
  object_class->set_property = wocky_jingle_transport_iceudp_set_property;
  object_class->dispose = wocky_jingle_transport_iceudp_dispose;

  /* property definitions */
  param_spec = g_param_spec_object ("content", "WockyJingleContent object",
                                    "Jingle content object using this transport.",
                                    WOCKY_TYPE_JINGLE_CONTENT,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CONTENT, param_spec);

  param_spec = g_param_spec_string ("transport-ns", "Transport namespace",
                                    "Namespace identifying the transport type.",
                                    NULL,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_TRANSPORT_NS, param_spec);

  param_spec = g_param_spec_uint ("state",
                                  "Connection state for the transport.",
                                  "Enum specifying the connection state of the transport.",
                                  WOCKY_JINGLE_TRANSPORT_STATE_DISCONNECTED,
                                  WOCKY_JINGLE_TRANSPORT_STATE_CONNECTED,
                                  WOCKY_JINGLE_TRANSPORT_STATE_DISCONNECTED,
                                  G_PARAM_READWRITE |
                                  G_PARAM_STATIC_NAME |
                                  G_PARAM_STATIC_NICK |
                                  G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_STATE, param_spec);

  /* signal definitions */
  signals[NEW_CANDIDATES] = g_signal_new (
    "new-candidates",
    G_TYPE_FROM_CLASS (cls),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static void
parse_candidates (WockyJingleTransportIface *obj,
    WockyNode *transport_node, GError **error)
{
  WockyJingleTransportIceUdp *t = WOCKY_JINGLE_TRANSPORT_ICEUDP (obj);
  WockyJingleTransportIceUdpPrivate *priv = t->priv;
  gboolean node_contains_a_candidate = FALSE;
  GList *candidates = NULL;
  WockyNodeIter i;
  WockyNode *node;

  DEBUG ("called");

  wocky_node_iter_init (&i, transport_node, "candidate", NULL);
  while (wocky_node_iter_next (&i, &node))
    {
      const gchar *id, *address, *user, *pass, *str;
      guint port, net, gen, component = 1;
      gdouble pref;
      WockyJingleTransportProtocol proto;
      WockyJingleCandidateType ctype;
      WockyJingleCandidate *c;

      node_contains_a_candidate = TRUE;

      id = wocky_node_get_attribute (node, "foundation");
      if (id == NULL)
        {
          DEBUG ("candidate doesn't contain foundation");
          continue;
        }

      address = wocky_node_get_attribute (node, "ip");
      if (address == NULL)
        {
          DEBUG ("candidate doesn't contain ip");
          continue;
        }

      str = wocky_node_get_attribute (node, "port");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain port");
          continue;
        }
      port = atoi (str);

      str = wocky_node_get_attribute (node, "protocol");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain protocol");
          continue;
        }

      if (!wocky_strdiff (str, "udp"))
        {
          proto = WOCKY_JINGLE_TRANSPORT_PROTOCOL_UDP;
        }
      else
        {
          /* unknown protocol */
          DEBUG ("unknown protocol: %s", str);
          continue;
        }

      str = wocky_node_get_attribute (node, "priority");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain priority");
          continue;
        }
      pref = g_ascii_strtod (str, NULL);

      str = wocky_node_get_attribute (node, "type");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain type");
          continue;
        }

      if (!wocky_strdiff (str, "host"))
        {
          ctype = WOCKY_JINGLE_CANDIDATE_TYPE_LOCAL;
        }
      else if (!wocky_strdiff (str, "srflx") || !wocky_strdiff (str, "prflx"))
        {
          /* FIXME Strictly speaking a prflx candidate should be a different
           * type, but the TP spec has now way to distinguish and it doesn't
           * matter much anyway.. */
          ctype = WOCKY_JINGLE_CANDIDATE_TYPE_STUN;
        }
      else if (!wocky_strdiff (str, "relay"))
        {
          ctype = WOCKY_JINGLE_CANDIDATE_TYPE_RELAY;
        }
      else
        {
          /* unknown candidate type */
          DEBUG ("unknown candidate type: %s", str);
          continue;
        }

      user = wocky_node_get_attribute (transport_node, "ufrag");
      if (user == NULL)
        {
          DEBUG ("transport doesn't contain ufrag");
          continue;
        }

      pass = wocky_node_get_attribute (transport_node, "pwd");
      if (pass == NULL)
        {
          DEBUG ("transport doesn't contain pwd");
          continue;
        }

      str = wocky_node_get_attribute (node, "network");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain network");
          continue;
        }
      net = atoi (str);

      str = wocky_node_get_attribute (node, "generation");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain generation");
          continue;
        }
      gen = atoi (str);

      str = wocky_node_get_attribute (node, "component");
      if (str == NULL)
        {
          DEBUG ("candidate doesn't contain component");
          continue;
        }
      component = atoi (str);

      if (priv->ufrag == NULL || strcmp (priv->ufrag, user))
        {
          g_free (priv->ufrag);
          priv->ufrag = g_strdup (user);
        }

      if (priv->pwd == NULL || strcmp (priv->pwd, pass))
        {
          g_free (priv->pwd);
          priv->pwd = g_strdup (pass);
        }

      c = wocky_jingle_candidate_new (proto, ctype, id, component,
          address, port, gen, pref, user, pass, net);

      candidates = g_list_append (candidates, c);
    }

  if (candidates == NULL)
    {
      if (node_contains_a_candidate)
        {
          NODE_DEBUG (transport_node,
              "couldn't parse any of the given candidates");
          g_set_error (error, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_BAD_REQUEST,
              "could not parse any of the given candidates");
        }
      else
        {
          DEBUG ("no candidates in this stanza");
        }
    }
  else
    {
      DEBUG ("emitting %d new remote candidates", g_list_length (candidates));

      g_signal_emit (obj, signals[NEW_CANDIDATES], 0, candidates);

      priv->remote_candidates = g_list_concat (priv->remote_candidates,
          candidates);
    }
}

static void
inject_candidates (WockyJingleTransportIface *obj,
    WockyNode *transport_node)
{
  WockyJingleTransportIceUdp *self = WOCKY_JINGLE_TRANSPORT_ICEUDP (obj);
  WockyJingleTransportIceUdpPrivate *priv = self->priv;
  const gchar *username = NULL;

  for (; priv->pending_candidates != NULL;
      priv->pending_candidates = priv->pending_candidates->next)
    {
      WockyJingleCandidate *c = (WockyJingleCandidate *) priv->pending_candidates->data;
      gchar port_str[16], pref_str[16], comp_str[16], id_str[16],
          *type_str, *proto_str;
      WockyNode *cnode;

      if (username == NULL)
        {
          username = c->username;
        }
      else if (wocky_strdiff (username, c->username))
        {
          DEBUG ("found a candidate with a different username (%s not %s); "
              "will send in a separate batch", c->username, username);
          break;
        }

      sprintf (pref_str, "%d", c->preference);
      sprintf (port_str, "%d", c->port);
      sprintf (comp_str, "%d", c->component);
      sprintf (id_str, "%d", priv->id_sequence++);

      switch (c->type) {
        case WOCKY_JINGLE_CANDIDATE_TYPE_LOCAL:
          type_str = "host";
          break;
        case WOCKY_JINGLE_CANDIDATE_TYPE_STUN:
          type_str = "srflx";
          break;
        case WOCKY_JINGLE_CANDIDATE_TYPE_RELAY:
          type_str = "relay";
          break;
        default:
          DEBUG ("skipping candidate with unknown type %u", c->type);
          continue;
      }

      switch (c->protocol) {
        case WOCKY_JINGLE_TRANSPORT_PROTOCOL_UDP:
          proto_str = "udp";
          break;
        case WOCKY_JINGLE_TRANSPORT_PROTOCOL_TCP:
          DEBUG ("ignoring TCP candidate");
          continue;
        default:
          DEBUG ("skipping candidate with unknown protocol %u", c->protocol);
          continue;
      }

      wocky_node_set_attributes (transport_node,
          "ufrag", c->username,
          "pwd", c->password,
          NULL);

      cnode = wocky_node_add_child (transport_node, "candidate");
      wocky_node_set_attributes (cnode,
          "ip", c->address,
          "port", port_str,
          "priority", pref_str,
          "protocol", proto_str,
          "type", type_str,
          "component", comp_str,
          "foundation", c->id,
          "id", id_str,
          "network", "0",
          "generation", "0",
          NULL);
    }
}

/* We never have to retransmit candidates we've already sent, so we ignore
 * @all.
 */
static void
send_candidates (WockyJingleTransportIface *iface,
    gboolean all G_GNUC_UNUSED)
{
  WockyJingleTransportIceUdp *self = WOCKY_JINGLE_TRANSPORT_ICEUDP (iface);
  WockyJingleTransportIceUdpPrivate *priv = self->priv;

  while (priv->pending_candidates != NULL)
    {
      WockyNode *trans_node, *sess_node;
      WockyStanza *msg;

      msg = wocky_jingle_session_new_message (priv->content->session,
          WOCKY_JINGLE_ACTION_TRANSPORT_INFO, &sess_node);

      wocky_jingle_content_produce_node (priv->content, sess_node, FALSE,
          TRUE, &trans_node);
      inject_candidates (iface, trans_node);

      wocky_porter_send_iq_async (
          wocky_jingle_session_get_porter (priv->content->session), msg,
          NULL, NULL, NULL);
      g_object_unref (msg);
    }

  DEBUG ("sent all pending candidates");
}

/* Takes in a list of slice-allocated WockyJingleCandidate structs */
static void
new_local_candidates (WockyJingleTransportIface *obj, GList *new_candidates)
{
  WockyJingleTransportIceUdp *transport =
    WOCKY_JINGLE_TRANSPORT_ICEUDP (obj);
  WockyJingleTransportIceUdpPrivate *priv = transport->priv;

  priv->local_candidates = g_list_concat (priv->local_candidates,
      new_candidates);

  /* If all previous candidates have been signalled, set the new
   * ones as pending. If there are existing pending candidates,
   * the new ones will just be appended to that list. */
  if (priv->pending_candidates == NULL)
      priv->pending_candidates = new_candidates;
}

static GList *
get_remote_candidates (WockyJingleTransportIface *iface)
{
  WockyJingleTransportIceUdp *transport =
    WOCKY_JINGLE_TRANSPORT_ICEUDP (iface);
  WockyJingleTransportIceUdpPrivate *priv = transport->priv;

  return priv->remote_candidates;
}

static GList *
get_local_candidates (WockyJingleTransportIface *iface)
{
  WockyJingleTransportIceUdp *transport =
    WOCKY_JINGLE_TRANSPORT_ICEUDP (iface);
  WockyJingleTransportIceUdpPrivate *priv = transport->priv;

  return priv->local_candidates;
}

static WockyJingleTransportType
get_transport_type (void)
{
  return JINGLE_TRANSPORT_ICE_UDP;
}

static gboolean
get_credentials (WockyJingleTransportIface *iface,
      gchar **ufrag, gchar **pwd)
{
  WockyJingleTransportIceUdp *transport =
    WOCKY_JINGLE_TRANSPORT_ICEUDP (iface);
  WockyJingleTransportIceUdpPrivate *priv = transport->priv;

  if (!priv->ufrag || !priv->pwd)
    return FALSE;

  if (ufrag)
    *ufrag = priv->ufrag;
  if (pwd)
    *pwd = priv->pwd;

  return TRUE;
}


static void
transport_iface_init (gpointer g_iface, gpointer iface_data)
{
  WockyJingleTransportIfaceClass *klass = (WockyJingleTransportIfaceClass *) g_iface;

  klass->parse_candidates = parse_candidates;

  klass->new_local_candidates = new_local_candidates;
  klass->inject_candidates = inject_candidates;
  klass->send_candidates = send_candidates;

  klass->get_remote_candidates = get_remote_candidates;
  klass->get_local_candidates = get_local_candidates;
  klass->get_transport_type = get_transport_type;
  klass->get_credentials = get_credentials;
}

void
jingle_transport_iceudp_register (WockyJingleFactory *factory)
{
  wocky_jingle_factory_register_transport (factory,
      NS_JINGLE_TRANSPORT_ICEUDP,
      WOCKY_TYPE_JINGLE_TRANSPORT_ICEUDP);
}

