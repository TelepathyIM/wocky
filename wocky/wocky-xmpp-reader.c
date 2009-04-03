/*
 * wocky-xmpp-reader.c - Source for WockyXmppReader
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>

#include "wocky-xmpp-reader.h"
#include "wocky-signals-marshal.h"

#include "wocky-namespaces.h"

#include "wocky-xmpp-stanza.h"

#define DEBUG_FLAG DEBUG_XMPP_READER
#include "wocky-debug.h"

/* properties */
enum {
  PROP_STREAMING_MODE = 1,
  PROP_TO,
  PROP_FROM,
  PROP_VERSION,
  PROP_LANG,
};

G_DEFINE_TYPE (WockyXmppReader, wocky_xmpp_reader, G_TYPE_OBJECT)

/* Parser prototypes */
static void _start_element_ns (void *user_data,
    const xmlChar *localname, const xmlChar *prefix, const xmlChar *uri,
    int nb_namespaces, const xmlChar **namespaces, int nb_attributes,
    int nb_defaulted, const xmlChar **attributes);

static void _end_element_ns (void *user_data, const xmlChar *localname,
    const xmlChar *prefix, const xmlChar *URI);

static void _characters (void *user_data, const xmlChar *ch, int len);

static void _error (void *user_data, xmlErrorPtr error);

static xmlSAXHandler parser_handler = {
  .initialized = XML_SAX2_MAGIC,
  .startElementNs = _start_element_ns,
  .endElementNs   = _end_element_ns,
  .characters     = _characters,
  .serror         = _error,
};

/* private structure */
typedef struct _WockyXmppReaderPrivate WockyXmppReaderPrivate;

struct _WockyXmppReaderPrivate
{
  xmlParserCtxtPtr parser;
  guint depth;
  WockyXmppStanza *stanza;
  WockyXmppNode *node;
  GQueue *nodes;
  gchar *to;
  gchar *from;
  gchar *version;
  gchar *lang;
  gboolean dispose_has_run;
  GError *error /* defeat the coding style checker... */;
  gboolean stream_mode;
  GQueue *stanzas;
  WockyXmppReaderState state;
};

GQuark
wocky_xmpp_reader_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-xmpp-reader-error");

  return quark;
}

#define WOCKY_XMPP_READER_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_READER, \
   WockyXmppReaderPrivate))

static void
wocky_init_xml_parser (WockyXmppReader *obj)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (obj);

  if (priv->parser != NULL)
    xmlFreeParserCtxt (priv->parser);

  priv->parser = xmlCreatePushParserCtxt (&parser_handler, obj, NULL, 0, NULL);
  xmlCtxtUseOptions (priv->parser, XML_PARSE_NOENT);
  priv->depth = 0;
  priv->state = priv->stream_mode ? WOCKY_XMPP_READER_STATE_INITIAL :
      WOCKY_XMPP_READER_STATE_OPENED;
  priv->error = FALSE;

  g_free (priv->to);
  priv->to = NULL;

  g_free (priv->from);
  priv->from = NULL;

  g_free (priv->lang);
  priv->lang = NULL;

  g_free (priv->version);
  priv->version = NULL;
}

static void
wocky_xmpp_reader_constructed (GObject *obj)
{
  wocky_init_xml_parser (WOCKY_XMPP_READER (obj));
}

static void
wocky_xmpp_reader_init (WockyXmppReader *obj)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
  wocky_init_xml_parser (obj);

  priv->stanza = NULL;
  priv->nodes = g_queue_new ();
  priv->node = NULL;
  priv->stanzas = g_queue_new ();
}

static void wocky_xmpp_reader_dispose (GObject *object);
static void wocky_xmpp_reader_finalize (GObject *object);
static void wocky_xmpp_reader_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void wocky_xmpp_reader_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);

static void
wocky_xmpp_reader_class_init (WockyXmppReaderClass *wocky_xmpp_reader_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_reader_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_xmpp_reader_class,
      sizeof (WockyXmppReaderPrivate));

  object_class->constructed = wocky_xmpp_reader_constructed;
  object_class->dispose = wocky_xmpp_reader_dispose;
  object_class->finalize = wocky_xmpp_reader_finalize;
  object_class->set_property = wocky_xmpp_reader_set_property;
  object_class->get_property = wocky_xmpp_reader_get_property;

  param_spec = g_param_spec_boolean ("streaming-mode", "streaming-mode",
    "Whether the xml to be read is one big stream or seperate documents",
    TRUE,
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STREAMING_MODE,
    param_spec);

  param_spec = g_param_spec_string ("to", "to",
    "to attribute in the xml stream opening",
    NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_TO, param_spec);

  param_spec = g_param_spec_string ("from", "from",
    "from attribute in the xml stream opening",
    NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FROM, param_spec);

  param_spec = g_param_spec_string ("version", "version",
    "version attribute in the xml stream opening",
    NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VERSION, param_spec);

  param_spec = g_param_spec_string ("lang", "lang",
    "xml:lang attribute in the xml stream opening",
    NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LANG, param_spec);
}

void
wocky_xmpp_reader_dispose (GObject *object)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (object);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  while (!g_queue_is_empty (priv->stanzas)) {
    gpointer stanza;
    stanza = g_queue_pop_head (priv->stanzas);
    if (stanza != NULL)
      g_object_unref (stanza);
  }

  if (G_OBJECT_CLASS (wocky_xmpp_reader_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_reader_parent_class)->dispose (object);
}

void
wocky_xmpp_reader_finalize (GObject *object)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (object);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (self);

  /* free any data held directly by the object here */
  if (priv->parser != NULL) {
    xmlFreeParserCtxt (priv->parser);
    priv->parser = NULL;
  }

  g_queue_free (priv->stanzas);
  g_queue_free (priv->nodes);
  g_free (priv->to);
  g_free (priv->from);
  g_free (priv->version);
  g_free (priv->lang);
  if (priv->error != NULL)
    g_error_free (priv->error);

  G_OBJECT_CLASS (wocky_xmpp_reader_parent_class)->finalize (object);
}

static void
wocky_xmpp_reader_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyXmppReader *reader = WOCKY_XMPP_READER (object);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);

  switch (property_id)
    {
      case PROP_STREAMING_MODE:
        priv->stream_mode = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_reader_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyXmppReader *reader = WOCKY_XMPP_READER (object);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);

  switch (property_id)
    {
      case PROP_STREAMING_MODE:
        g_value_set_boolean (value, priv->stream_mode);
        break;
      case PROP_FROM:
        g_value_set_string (value, priv->from);
        break;
      case PROP_TO:
        g_value_set_string (value, priv->to);
        break;
      case PROP_LANG:
        g_value_set_string (value, priv->lang);
        break;
      case PROP_VERSION:
        g_value_set_string (value, priv->version);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

WockyXmppReader *
wocky_xmpp_reader_new (void)
{
  return g_object_new (WOCKY_TYPE_XMPP_READER, NULL);
}

WockyXmppReader *
wocky_xmpp_reader_new_no_stream (void)
{
  return g_object_new (WOCKY_TYPE_XMPP_READER,
      "streaming-mode", FALSE,
      NULL);
}

static void
_start_element_ns (void *user_data, const xmlChar *localname,
    const xmlChar *prefix, const xmlChar *uri, int nb_namespaces,
    const xmlChar **namespaces, int nb_attributes, int nb_defaulted,
    const xmlChar **attributes)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (self);
  int i;

  if (prefix)
    {
       DEBUG ("Element %s:%s started, depth %d", prefix, localname,
           priv->depth);
    }
  else
    {
      DEBUG ("Element %s started, depth %d", localname, priv->depth);
    }

  if (priv->stream_mode && G_UNLIKELY (priv->depth == 0))
    {
      if (strcmp ("stream", (gchar *) localname)
          || strcmp (WOCKY_XMPP_NS_STREAM, (gchar *) uri))
        {
          priv->error = g_error_new_literal (WOCKY_XMPP_READER_ERROR,
            WOCKY_XMPP_READER_ERROR_INVALID_STREAM_START,
            "Invalid start of the XMPP stream");
          g_queue_push_tail (priv->stanzas, NULL);
          return;
        }

      priv->state = WOCKY_XMPP_READER_STATE_OPENED;

      for (i = 0; i < nb_attributes * 5; i+=5)
        {
          if (!strcmp ((gchar *) attributes[i], "to"))
            {
              g_free (priv->to);
              priv->to = g_strndup ((gchar *) attributes[i+3],
                           (gsize) (attributes[i+4] - attributes[i+3]));
            }

          if (!strcmp ((gchar *) attributes[i], "from"))
            {
              g_free (priv->from);
              priv->from = g_strndup ((gchar *) attributes[i+3],
                         (gsize) (attributes[i+4] - attributes[i+3]));
            }

          if (!strcmp ((gchar *) attributes[i], "version"))
            {
              g_free (priv->version);
              priv->version = g_strndup ((gchar *) attributes[i+3],
                  (gsize) (attributes[i+4] - attributes[i+3]));
            }
          if (!strcmp ((gchar *) attributes[i], "lang") &&
              !strcmp ((gchar *) attributes[i + 1], "xml"))
            {
              g_free (priv->lang);
              priv->lang = g_strndup ((gchar *) attributes[i+3],
                  (gsize) (attributes[i+4] - attributes[i+3]));
            }
        }
      priv->depth++;
      return;
    }

  if (priv->stanza == NULL)
    {
      priv->stanza = wocky_xmpp_stanza_new ((gchar *) localname);
      priv->node = priv->stanza->node;
    }
  else
    {
      g_queue_push_tail (priv->nodes, priv->node);
      priv->node = wocky_xmpp_node_add_child (priv->node, (gchar *) localname);
    }
  wocky_xmpp_node_set_ns (priv->node, (gchar *) uri);

  for (i = 0; i < nb_attributes * 5; i+=5)
    {
      /* Node is localname, prefix, uri, valuestart, valueend */
      if (attributes[i+1] != NULL && !strcmp ((gchar *) attributes[i+1], "xml")
          && !strcmp ((gchar *) attributes[i], "lang"))
        {
          wocky_xmpp_node_set_language_n (priv->node,
              (gchar *) attributes[i+3],
              (gsize) (attributes[i+4] - attributes[i+3]));
        }
      else
        {
          wocky_xmpp_node_set_attribute_n_ns (priv->node,
              (gchar *) attributes[i], (gchar *) attributes[i+3],
              (gsize)(attributes[i+4] - attributes[i+3]),
              (gchar *) attributes[i+2]);
        }
     }
  priv->depth++;
}

static void
_characters (void *user_data, const xmlChar *ch, int len)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (self);

  if (priv->node != NULL)
    {
      wocky_xmpp_node_append_content_n (priv->node, (const gchar *)ch,
          (gsize)len);
    }
}

static void
_end_element_ns (void *user_data, const xmlChar *localname,
    const xmlChar *prefix, const xmlChar *uri)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (self);

  priv->depth--;

  if (prefix)
    {
      DEBUG ("Element %s:%s ended, depth %d", prefix, localname, priv->depth);
    }
  else
    {
      DEBUG ("Element %s ended, depth %d", localname, priv->depth);
    }

  if (priv->node && priv->node->content)
    {
      /* Remove content if it's purely whitespace */
      const char *c;
      for (c = priv->node->content; *c != '\0' && g_ascii_isspace (*c); c++)
        ;
      if (*c == '\0')
        wocky_xmpp_node_set_content (priv->node, NULL);
    }

  if (priv->stream_mode && priv->depth == 0)
    {
      DEBUG ("Stream ended");
      g_queue_push_tail (priv->stanzas, NULL);
    }
  else if (priv->depth == (priv->stream_mode ? 1 : 0))
    {
      g_assert (g_queue_get_length (priv->nodes) == 0);
      DEBUG ("Received stanza");
      g_queue_push_tail (priv->stanzas, priv->stanza);
      priv->stanza = NULL;
      priv->node = NULL;
    }
  else
    {
      priv->node = (WockyXmppNode *) g_queue_pop_tail (priv->nodes);
    }
}

static void
_error (void *user_data, xmlErrorPtr error)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (self);

  priv->error = g_error_new_literal (WOCKY_XMPP_READER_ERROR,
    WOCKY_XMPP_READER_ERROR_PARSE_ERROR, error->message);

  DEBUG ("Parsing failed %s", error->message);
  g_queue_push_tail (priv->stanzas, NULL);
}

WockyXmppReaderState
wocky_xmpp_reader_get_state (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);

  return priv->state;
}

static void
wocky_xmpp_reader_check_eos (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);

  if (!g_queue_is_empty (priv->stanzas)
      && g_queue_peek_head (priv->stanzas) == NULL)
    {
      priv->state = priv->error ? WOCKY_XMPP_READER_STATE_ERROR :
        WOCKY_XMPP_READER_STATE_CLOSED;
    }
}

void
wocky_xmpp_reader_push (WockyXmppReader *reader, const guint8 *data,
    gsize length)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);
  xmlParserCtxtPtr parser;

  g_return_if_fail (priv->state < WOCKY_XMPP_READER_STATE_CLOSED);

  DEBUG ("Parsing chunk: %.*s", (int)length, data);

  parser = priv->parser;
  xmlParseChunk (parser, (const char*)data, length, FALSE);

  wocky_xmpp_reader_check_eos (reader);

 /* When in non-streaming mode, prepare the parser for the next hunk */
 if (!priv->stream_mode)
   {
     wocky_init_xml_parser (reader);
   }
}

WockyXmppStanza *
wocky_xmpp_reader_pop_stanza (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);
  WockyXmppStanza *s;

  if (g_queue_is_empty (priv->stanzas))
    return NULL;

  s = g_queue_pop_head (priv->stanzas);

  wocky_xmpp_reader_check_eos (reader);

  if (!priv->stream_mode)
    {
      priv->state = WOCKY_XMPP_READER_STATE_CLOSED;
    }

  return s;
}

GError *
wocky_xmpp_reader_get_error (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);

  return priv->error == NULL ? NULL : g_error_copy (priv->error);
}

void
wocky_xmpp_reader_reset (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = WOCKY_XMPP_READER_GET_PRIVATE (reader);

  DEBUG ("Resetting xmpp reader");

  if (priv->error != NULL)
    g_error_free (priv->error);
  priv->error = NULL;

  wocky_init_xml_parser (reader);
}
