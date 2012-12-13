/*
 * jingle-transport-google.c - Source for WockyJingleTransportGoogle
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
#include "jingle-transport-google.h"

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

G_DEFINE_TYPE_WITH_CODE (WockyJingleTransportGoogle,
    wocky_jingle_transport_google, G_TYPE_OBJECT,
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

struct _WockyJingleTransportGooglePrivate
{
  WockyJingleContent *content;
  WockyJingleTransportState state;
  gchar *transport_ns;

  /* Component names or jingle-share transport 'channels'
     g_strdup'd component name => GINT_TO_POINTER (component id) */
  GHashTable *component_names;

  GList *local_candidates;

  /* A pointer into "local_candidates" list to mark the
   * candidates that are still not transmitted, or NULL
   * if all of them are transmitted. */

  GList *pending_candidates;
  GList *remote_candidates;
  gboolean dispose_has_run;
};

static void
wocky_jingle_transport_google_init (WockyJingleTransportGoogle *obj)
{
  WockyJingleTransportGooglePrivate *priv =
     G_TYPE_INSTANCE_GET_PRIVATE (obj, WOCKY_TYPE_JINGLE_TRANSPORT_GOOGLE,
         WockyJingleTransportGooglePrivate);
  obj->priv = priv;

  priv->component_names = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  priv->dispose_has_run = FALSE;
}

static void
wocky_jingle_transport_google_dispose (GObject *object)
{
  WockyJingleTransportGoogle *trans = WOCKY_JINGLE_TRANSPORT_GOOGLE (object);
  WockyJingleTransportGooglePrivate *priv = trans->priv;

  if (priv->dispose_has_run)
    return;

  DEBUG ("dispose called");
  priv->dispose_has_run = TRUE;

  g_hash_table_unref (priv->component_names);
  priv->component_names = NULL;

  jingle_transport_free_candidates (priv->remote_candidates);
  priv->remote_candidates = NULL;

  jingle_transport_free_candidates (priv->local_candidates);
  priv->local_candidates = NULL;

  g_free (priv->transport_ns);
  priv->transport_ns = NULL;

  if (G_OBJECT_CLASS (wocky_jingle_transport_google_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_jingle_transport_google_parent_class)->dispose (object);
}

static void
wocky_jingle_transport_google_get_property (GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec)
{
  WockyJingleTransportGoogle *trans = WOCKY_JINGLE_TRANSPORT_GOOGLE (object);
  WockyJingleTransportGooglePrivate *priv = trans->priv;

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
wocky_jingle_transport_google_set_property (GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
  WockyJingleTransportGoogle *trans = WOCKY_JINGLE_TRANSPORT_GOOGLE (object);
  WockyJingleTransportGooglePrivate *priv = trans->priv;

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
wocky_jingle_transport_google_class_init (WockyJingleTransportGoogleClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (WockyJingleTransportGooglePrivate));

  object_class->get_property = wocky_jingle_transport_google_get_property;
  object_class->set_property = wocky_jingle_transport_google_set_property;
  object_class->dispose = wocky_jingle_transport_google_dispose;

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
  WockyJingleTransportGoogle *t = WOCKY_JINGLE_TRANSPORT_GOOGLE (obj);
  WockyJingleTransportGooglePrivate *priv = t->priv;
  GList *candidates = NULL;
  WockyNodeIter i;
  WockyNode *node;

  wocky_node_iter_init (&i, transport_node, "candidate", NULL);
  while (wocky_node_iter_next (&i, &node))
    {
      const gchar *name, *address, *user, *pass, *str;
      guint port, net, gen, component;
      int pref;
      WockyJingleTransportProtocol proto;
      WockyJingleCandidateType ctype;
      WockyJingleCandidate *c;

      name = wocky_node_get_attribute (node, "name");
      if (name == NULL)
          break;

      if (!g_hash_table_lookup_extended (priv->component_names, name,
              NULL, NULL))
        {
          DEBUG ("component name %s unknown to this transport", name);
          continue;
        }

      component = GPOINTER_TO_INT (g_hash_table_lookup (priv->component_names,
              name));
      address = wocky_node_get_attribute (node, "address");
      if (address == NULL)
          break;

      str = wocky_node_get_attribute (node, "port");
      if (str == NULL)
          break;
      port = atoi (str);

      str = wocky_node_get_attribute (node, "protocol");
      if (str == NULL)
          break;

      if (!wocky_strdiff (str, "udp"))
        {
          proto = WOCKY_JINGLE_TRANSPORT_PROTOCOL_UDP;
        }
      else if (!wocky_strdiff (str, "tcp"))
        {
          /* candiates on port 443 must be "ssltcp" */
          if (port == 443)
              break;

          proto = WOCKY_JINGLE_TRANSPORT_PROTOCOL_TCP;
        }
      else if (!wocky_strdiff (str, "ssltcp"))
        {
          /* "ssltcp" must use port 443 */
          if (port != 443)
              break;

          /* we really don't care about "ssltcp" otherwise */
          proto = WOCKY_JINGLE_TRANSPORT_PROTOCOL_TCP;
        }
      else
        {
          /* unknown protocol */
          DEBUG ("unknown protocol: %s", str);
          break;
        }

      str = wocky_node_get_attribute (node, "preference");
      if (str == NULL)
          break;

      pref = g_ascii_strtod (str, NULL) * 65536;

      str = wocky_node_get_attribute (node, "type");
      if (str == NULL)
          break;

      if (!wocky_strdiff (str, "local"))
        {
          ctype = WOCKY_JINGLE_CANDIDATE_TYPE_LOCAL;
        }
      else if (!wocky_strdiff (str, "stun"))
        {
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
          break;
        }

      user = wocky_node_get_attribute (node, "username");
      if (user == NULL)
          break;

      pass = wocky_node_get_attribute (node, "password");
      if (pass == NULL)
          break;

      str = wocky_node_get_attribute (node, "network");
      if (str == NULL)
          break;
      net = atoi (str);

      str = wocky_node_get_attribute (node, "generation");
      if (str == NULL)
          break;
      gen = atoi (str);

      str = wocky_node_get_attribute (node, "component");
      if (str != NULL)
          component = atoi (str);

      c = wocky_jingle_candidate_new (proto, ctype, NULL, component,
          address, port, gen, pref, user, pass, net);

      candidates = g_list_append (candidates, c);
    }

  if (wocky_node_iter_next (&i, NULL))
    {
      DEBUG ("not all nodes were processed, reporting error");
      /* rollback these */
      jingle_transport_free_candidates (candidates);
      g_set_error (error, WOCKY_XMPP_ERROR, WOCKY_XMPP_ERROR_BAD_REQUEST,
          "invalid candidate");
      return;
    }

  DEBUG ("emitting %d new remote candidates", g_list_length (candidates));

  g_signal_emit (obj, signals[NEW_CANDIDATES], 0, candidates);

  /* append them to the known remote candidates */
  priv->remote_candidates = g_list_concat (priv->remote_candidates, candidates);
}

static void
transmit_candidates (WockyJingleTransportGoogle *transport,
    const gchar *name,
    GList *candidates)
{
  WockyJingleTransportGooglePrivate *priv = transport->priv;
  GList *li;
  WockyStanza *msg;
  WockyNode *trans_node, *sess_node;

  if (candidates == NULL)
    return;

  msg = wocky_jingle_session_new_message (priv->content->session,
    WOCKY_JINGLE_ACTION_TRANSPORT_INFO, &sess_node);

  wocky_jingle_content_produce_node (priv->content, sess_node, FALSE, TRUE,
      &trans_node);

  for (li = candidates; li; li = li->next)
    {
      WockyJingleCandidate *c = (WockyJingleCandidate *) li->data;
      gchar port_str[16], pref_str[16], comp_str[16], *type_str, *proto_str;
      WockyNode *cnode;

      sprintf (port_str, "%d", c->port);
      sprintf (pref_str, "%lf", c->preference / 65536.0);
      sprintf (comp_str, "%d", c->component);

      switch (c->type) {
        case WOCKY_JINGLE_CANDIDATE_TYPE_LOCAL:
          type_str = "local";
          break;
        case WOCKY_JINGLE_CANDIDATE_TYPE_STUN:
          type_str = "stun";
          break;
        case WOCKY_JINGLE_CANDIDATE_TYPE_RELAY:
          type_str = "relay";
          break;
        default:
          g_assert_not_reached ();
      }

      switch (c->protocol) {
        case WOCKY_JINGLE_TRANSPORT_PROTOCOL_UDP:
          proto_str = "udp";
          break;
        case WOCKY_JINGLE_TRANSPORT_PROTOCOL_TCP:
          if ((c->port == 443) && (c->type == WOCKY_JINGLE_CANDIDATE_TYPE_RELAY))
            proto_str = "ssltcp";
          else
            proto_str = "tcp";
          break;
        default:
          g_assert_not_reached ();
      }

      cnode = wocky_node_add_child (trans_node, "candidate");
      wocky_node_set_attributes (cnode,
          "address", c->address,
          "port", port_str,
          "username", c->username,
          "password", c->password != NULL ? c->password : "",
          "preference", pref_str,
          "protocol", proto_str,
          "type", type_str,
          "component", comp_str,
          "network", "0",
          "generation", "0",
          NULL);

      wocky_node_set_attribute (cnode, "name", name);
    }

  wocky_porter_send_iq_async (
      wocky_jingle_session_get_porter (priv->content->session), msg,
      NULL, NULL, NULL);
  g_object_unref (msg);
}

/* Groups @candidates into rtp and rtcp and sends each group in its own
 * transport-info. This works around old Gabble, which rejected transport-info
 * stanzas containing non-rtp candidates.
 */
static void
group_and_transmit_candidates (WockyJingleTransportGoogle *transport,
    GList *candidates)
{
  WockyJingleTransportGooglePrivate *priv = transport->priv;
  GList *all_candidates = NULL;
  WockyJingleMediaType media;
  GList *li;
  GList *cands;

  for (li = candidates; li != NULL; li = g_list_next (li))
    {
      WockyJingleCandidate *c = li->data;

      for (cands = all_candidates; cands != NULL;  cands = g_list_next (cands))
        {
          WockyJingleCandidate *c2 = ((GList *) cands->data)->data;

          if (c->component == c2->component)
            {
              break;
            }
        }
      if (cands == NULL)
        {
          all_candidates = g_list_prepend (all_candidates, NULL);
          cands = all_candidates;
        }

      cands->data = g_list_prepend (cands->data, c);
    }

  g_object_get (priv->content, "media-type", &media, NULL);

  for (cands = all_candidates; cands != NULL;  cands = g_list_next (cands))
    {
      GHashTableIter iter;
      gpointer key, value;
      gchar *name = NULL;
      WockyJingleCandidate *c = ((GList *) cands->data)->data;

      g_hash_table_iter_init (&iter, priv->component_names);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (GPOINTER_TO_INT (value) == c->component)
            {
              name = key;
              break;
            }
        }
      if (name)
        {
          transmit_candidates (transport, name, cands->data);
        }
      else
        {
          DEBUG ("Ignoring unknown component %d", c->component);
        }
      g_list_free (cands->data);
    }

  g_list_free (all_candidates);
}

/* Takes in a list of slice-allocated WockyJingleCandidate structs */
static void
new_local_candidates (WockyJingleTransportIface *obj, GList *new_candidates)
{
  WockyJingleTransportGoogle *transport =
    WOCKY_JINGLE_TRANSPORT_GOOGLE (obj);
  WockyJingleTransportGooglePrivate *priv = transport->priv;

  priv->local_candidates = g_list_concat (priv->local_candidates,
      new_candidates);

  /* If all previous candidates have been signalled, set the new
   * ones as pending. If there are existing pending candidates,
   * the new ones will just be appended to that list. */
  if (priv->pending_candidates == NULL)
    priv->pending_candidates = new_candidates;
}

static void
send_candidates (WockyJingleTransportIface *obj, gboolean all)
{
  WockyJingleTransportGoogle *transport =
    WOCKY_JINGLE_TRANSPORT_GOOGLE (obj);
  WockyJingleTransportGooglePrivate *priv = transport->priv;

  if (all)
    {
      /* for gtalk3, we might have to retransmit everything */
      group_and_transmit_candidates (transport, priv->local_candidates);
      priv->pending_candidates = NULL;
    }
  else
    {
      /* If the content became ready after we wanted to transmit
       * these originally, we are called to transmit when it them */
      if (priv->pending_candidates != NULL)
        {
          group_and_transmit_candidates (transport, priv->pending_candidates);
          priv->pending_candidates = NULL;
      }
    }
}

static GList *
get_local_candidates (WockyJingleTransportIface *iface)
{
  WockyJingleTransportGoogle *transport =
    WOCKY_JINGLE_TRANSPORT_GOOGLE (iface);
  WockyJingleTransportGooglePrivate *priv = transport->priv;

  return priv->local_candidates;
}

static GList *
get_remote_candidates (WockyJingleTransportIface *iface)
{
  WockyJingleTransportGoogle *transport =
    WOCKY_JINGLE_TRANSPORT_GOOGLE (iface);
  WockyJingleTransportGooglePrivate *priv = transport->priv;

  return priv->remote_candidates;
}

static WockyJingleTransportType
get_transport_type (void)
{
  return JINGLE_TRANSPORT_GOOGLE_P2P;
}

static void
transport_iface_init (gpointer g_iface, gpointer iface_data)
{
  WockyJingleTransportIfaceClass *klass = (WockyJingleTransportIfaceClass *) g_iface;

  klass->parse_candidates = parse_candidates;

  klass->new_local_candidates = new_local_candidates;
  /* Not implementing inject_candidates: gtalk-p2p candidates are always sent
   * in transport-info or equivalent.
   */
  klass->send_candidates = send_candidates;

  klass->get_remote_candidates = get_remote_candidates;
  klass->get_local_candidates = get_local_candidates;
  klass->get_transport_type = get_transport_type;
}

/* Returns FALSE if the component name already exists */
gboolean
jingle_transport_google_set_component_name (
    WockyJingleTransportGoogle *transport,
    const gchar *name, guint component_id)
{
  WockyJingleTransportGooglePrivate *priv = transport->priv;

  if (g_hash_table_lookup_extended (priv->component_names, name, NULL, NULL))
      return FALSE;

  g_hash_table_insert (priv->component_names, g_strdup (name),
      GINT_TO_POINTER (component_id));

  return TRUE;
}

void
jingle_transport_google_register (WockyJingleFactory *factory)
{
  /* GTalk libjingle0.3 dialect */
  wocky_jingle_factory_register_transport (factory, "",
      WOCKY_TYPE_JINGLE_TRANSPORT_GOOGLE);

  /* GTalk libjingle0.4 dialect */
  wocky_jingle_factory_register_transport (factory,
      NS_GOOGLE_TRANSPORT_P2P,
      WOCKY_TYPE_JINGLE_TRANSPORT_GOOGLE);
}

