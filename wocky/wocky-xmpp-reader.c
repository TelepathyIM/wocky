/*
 * wocky-xmpp-reader.c - Source for WockyXmppReader
 * Copyright (C) 2006,2009 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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
 * SECTION: wocky-xmpp-reader
 * @title: WockyXmppReader
 * @short_description: Xmpp XML to stanza deserializer
 *
 * The #WockyXmppReader deserializes XML to #WockyStanza<!-- -->s,
 * misc, other
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>

#include "wocky-xmpp-reader.h"
#include "wocky-signals-marshal.h"
#include "wocky-utils.h"

#include "wocky-namespaces.h"

#include "wocky-stanza.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_XMPP_READER
#include "wocky-debug-internal.h"

/* properties */
enum {
  PROP_STREAMING_MODE = 1,
  PROP_DEFAULT_NAMESPACE,
  PROP_TO,
  PROP_FROM,
  PROP_VERSION,
  PROP_LANG,
  PROP_ID,
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
  /* internalSubset         */ NULL,
  /* isStandalone           */ NULL,
  /* hasInternalSubset      */ NULL,
  /* hasExternalSubset      */ NULL,
  /* resolveEntity          */ NULL,
  /* getEntity              */ NULL,
  /* entityDecl             */ NULL,
  /* notationDecl           */ NULL,
  /* attributeDecl          */ NULL,
  /* elementDecl            */ NULL,
  /* unparsedEntityDecl     */ NULL,
  /* setDocumentLocator     */ NULL,
  /* startDocument          */ NULL,
  /* endDocument            */ NULL,
  /* startElement           */ NULL,
  /* endElement             */ NULL,
  /* reference              */ NULL,
  /* characters             */ _characters,
  /* ignorableWhitespace    */ NULL,
  /* processingInstruction  */ NULL,
  /* comment                */ NULL,
  /* warning                */ NULL,
  /* error                  */ NULL,
  /* fatalError             */ NULL,
  /* getParameterEntity     */ NULL,
  /* cdataBlock             */ NULL,
  /* externalSubset         */ NULL,
  /* initialized            */ XML_SAX2_MAGIC,
  /* _private               */ NULL,
  /* startElementNs         */ _start_element_ns,
  /* endElementNs           */ _end_element_ns,
  /* serror                 */ _error
};

/* private structure */
struct _WockyXmppReaderPrivate
{
  xmlParserCtxtPtr parser;
  guint depth;
  WockyStanza *stanza;
  WockyNode *node;
  GQueue *nodes;
  gchar *to;
  gchar *from;
  gchar *version;
  gchar *lang;
  gchar *id;
  gboolean dispose_has_run;
  GError *error /* defeat the coding style checker... */;
  gboolean stream_mode;
  gchar *default_namespace;
  GQueue *stanzas;
  WockyXmppReaderState state;
};

/**
 * wocky_xmpp_reader_error_quark
 *
 * Get the error quark used by the reader.
 *
 * Returns: the quark for reader errors.
 */
GQuark
wocky_xmpp_reader_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-xmpp-reader-error");

  return quark;
}

/* clear parser state */
static void
wocky_xmpp_reader_clear_parser_state (WockyXmppReader *self)
{
  WockyXmppReaderPrivate *priv = self->priv;

  while (!g_queue_is_empty (priv->stanzas)) {
    gpointer stanza;
    stanza = g_queue_pop_head (priv->stanzas);
    if (stanza != NULL)
      g_object_unref (stanza);
  }

  if (priv->stanza != NULL)
    g_object_unref (priv->stanza);
  priv->stanza = NULL;

  g_queue_clear (priv->nodes);
  priv->node = NULL;
  priv->depth = 0;

  g_free (priv->to);
  priv->to = NULL;

  g_free (priv->from);
  priv->from = NULL;

  g_free (priv->lang);
  priv->lang = NULL;

  g_free (priv->version);
  priv->version = NULL;

  g_free (priv->id);
  priv->id = NULL;

  if (priv->error != NULL)
    g_error_free (priv->error);
  priv->error = NULL;

  if (priv->parser != NULL)
    xmlFreeParserCtxt (priv->parser);
  priv->parser = NULL;

  priv->state = WOCKY_XMPP_READER_STATE_CLOSED;
}

static void
wocky_init_xml_parser (WockyXmppReader *obj)
{
  WockyXmppReaderPrivate *priv = obj->priv;

  priv->parser = xmlCreatePushParserCtxt (&parser_handler, obj, NULL, 0, NULL);
  xmlCtxtUseOptions (priv->parser, XML_PARSE_NOENT);
  priv->state = priv->stream_mode ? WOCKY_XMPP_READER_STATE_INITIAL :
      WOCKY_XMPP_READER_STATE_OPENED;
}

static void
wocky_xmpp_reader_constructed (GObject *obj)
{
  wocky_init_xml_parser (WOCKY_XMPP_READER (obj));
}

static void
wocky_xmpp_reader_init (WockyXmppReader *self)
{
  WockyXmppReaderPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_XMPP_READER,
      WockyXmppReaderPrivate);
  priv = self->priv;

  priv->nodes = g_queue_new ();
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
  wocky_xmpp_reader_class->stream_element_name = "stream";
  wocky_xmpp_reader_class->stream_element_ns = WOCKY_XMPP_NS_STREAM;

  object_class->constructed = wocky_xmpp_reader_constructed;
  object_class->dispose = wocky_xmpp_reader_dispose;
  object_class->finalize = wocky_xmpp_reader_finalize;
  object_class->set_property = wocky_xmpp_reader_set_property;
  object_class->get_property = wocky_xmpp_reader_get_property;

  param_spec = g_param_spec_boolean ("streaming-mode", "streaming-mode",
    "Whether the xml to be read is one big stream or separate documents",
    TRUE,
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STREAMING_MODE,
    param_spec);

  param_spec = g_param_spec_string ("default-namespace", "default namespace",
      "The default namespace for the root element of the document. "
      "Only meaningful if streaming-mode is FALSE.",
      "",
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DEFAULT_NAMESPACE,
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

  param_spec = g_param_spec_string ("id", "ID",
    "id attribute in the xml stream opening",
    NULL,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ID, param_spec);
}

void
wocky_xmpp_reader_dispose (GObject *object)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (object);
  WockyXmppReaderPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  wocky_xmpp_reader_clear_parser_state (self);

  if (G_OBJECT_CLASS (wocky_xmpp_reader_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_reader_parent_class)->dispose (object);
}

void
wocky_xmpp_reader_finalize (GObject *object)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (object);
  WockyXmppReaderPrivate *priv = self->priv;

  /* free any data held directly by the object here */
  g_queue_free (priv->stanzas);
  g_queue_free (priv->nodes);

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
  WockyXmppReaderPrivate *priv = reader->priv;

  switch (property_id)
    {
      case PROP_STREAMING_MODE:
        priv->stream_mode = g_value_get_boolean (value);
        break;
      case PROP_DEFAULT_NAMESPACE:
        g_free (priv->default_namespace);
        priv->default_namespace = g_value_dup_string (value);

        if (priv->default_namespace == NULL)
          priv->default_namespace = g_strdup ("");

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
  WockyXmppReaderPrivate *priv = reader->priv;

  switch (property_id)
    {
      case PROP_STREAMING_MODE:
        g_value_set_boolean (value, priv->stream_mode);
        break;
      case PROP_DEFAULT_NAMESPACE:
        g_value_set_string (value, priv->default_namespace);
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
      case PROP_ID:
        g_value_set_string (value, priv->id);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

/**
 * wocky_xmpp_reader_new:
 *
 * Convenience function to create a new #WockyXmppReader.
 *
 * Returns: a new #WockyXmppReader
 */
WockyXmppReader *
wocky_xmpp_reader_new (void)
{
  return g_object_new (WOCKY_TYPE_XMPP_READER, NULL);
}

/**
 * wocky_xmpp_reader_new_no_stream:
 *
 * Convenience function to create a new #WockyXmppReader that has streaming
 * mode disabled.
 *
 * Returns: a new #WockyXmppReader in non-streaming mode
 */
WockyXmppReader *
wocky_xmpp_reader_new_no_stream (void)
{
  return g_object_new (WOCKY_TYPE_XMPP_READER,
      "streaming-mode", FALSE,
      NULL);
}

/**
 * wocky_xmpp_reader_new_no_stream_ns:
 * @default_namespace: default XML namespace to apply to the top-level element
 *
 * Create a new #WockyXmppReader, with #WockyXmppReader:streaming-mode disabled
 * and the specified #WockyXmppReader:default-namespace.
 *
 * Returns: (transfer full): a new #WockyXmppReader in non-streaming mode.
 */
WockyXmppReader *
wocky_xmpp_reader_new_no_stream_ns (
    const gchar *default_namespace)
{
  return g_object_new (WOCKY_TYPE_XMPP_READER,
      "streaming-mode", FALSE,
      "default-namespace", default_namespace,
      NULL);
}

static void
handle_stream_open (
    WockyXmppReader *self,
    const gchar *localname,
    const gchar *uri,
    const gchar *prefix,
    int nb_attributes,
    const xmlChar **attributes)
{
  WockyXmppReaderClass *klass = WOCKY_XMPP_READER_GET_CLASS (self);
  WockyXmppReaderPrivate *priv = self->priv;
  int i;

  if (wocky_strdiff (klass->stream_element_name, localname)
      || wocky_strdiff (klass->stream_element_ns, uri))
    {
      priv->error = g_error_new (WOCKY_XMPP_READER_ERROR,
        WOCKY_XMPP_READER_ERROR_INVALID_STREAM_START,
        "Invalid start of the XMPP stream "
        "(expected <%s xmlns=%s>, got <%s xmlns=%s>)",
        klass->stream_element_name, klass->stream_element_ns,
        localname, uri);
      g_queue_push_tail (priv->stanzas, NULL);
      return;
    }

  DEBUG ("Received stream opening: %s, prefix: %s, uri: %s",
    localname,
    prefix != NULL ? prefix : "<no prefix>",
    uri != NULL ? uri : "<no uri>");

  priv->state = WOCKY_XMPP_READER_STATE_OPENED;

  for (i = 0; i < nb_attributes * 5; i+=5)
    {
      /* attr_name and attr_value are guaranteed non-NULL; attr_prefix and
       * attr_uri may be NULL.
       */
      const gchar *attr_name = (const gchar *) attributes[i];
      const gchar *attr_prefix = (const gchar *) attributes[i+1];
      const gchar *attr_uri = (const gchar *) attributes[i+2];
      gsize value_len = attributes[i+4] - attributes[i+3];
      gchar *attr_value = g_strndup (
          (const gchar *) attributes[i+3], value_len);

      DEBUG ("Stream opening attribute: %s = '%s' (prefix: %s, uri: %s)",
          attr_name, attr_value,
          attr_prefix != NULL ? attr_prefix : "<no prefix>",
          attr_uri != NULL ? attr_uri : "<no uri>");

      if (!strcmp (attr_name, "to"))
        {
          g_free (priv->to);
          priv->to = attr_value;
        }
      else if (!strcmp (attr_name, "from"))
        {
          g_free (priv->from);
          priv->from = attr_value;
        }
      else if (!strcmp (attr_name, "version"))
        {
          g_free (priv->version);
          priv->version = attr_value;
        }
      else if (!strcmp (attr_name, "lang") &&
          !wocky_strdiff (attr_prefix, "xml"))
        {
          g_free (priv->lang);
          priv->lang = attr_value;
        }
      else if (!strcmp (attr_name, "id"))
        {
          g_free (priv->id);
          priv->id = attr_value;
        }
      else
        {
          g_free (attr_value);
        }
    }

  priv->depth++;
}

static void
handle_regular_element (
    WockyXmppReader *self,
    const gchar *localname,
    const gchar *uri,
    int nb_attributes,
    const xmlChar **attributes)
{
  WockyXmppReaderPrivate *priv = self->priv;
  int i;

  if (priv->stanza == NULL)
    {
      if (uri != NULL)
        {
          priv->stanza = wocky_stanza_new (localname, uri);
        }
      else
        {
          /* This can only happy in non-streaming mode when the top node
           * of the document doesn't have a namespace. */
          DEBUG ("Stanza without a namespace, using default namespace '%s'",
              priv->default_namespace);
          priv->stanza = wocky_stanza_new (localname, priv->default_namespace);
        }

      priv->node = wocky_stanza_get_top_node (priv->stanza);
    }
  else
    {
      g_queue_push_tail (priv->nodes, priv->node);
      priv->node = wocky_node_add_child_ns (priv->node,
        localname, uri);
    }

  for (i = 0; i < nb_attributes * 5; i+=5)
    {
      /* attr_name and attr_value are guaranteed non-NULL; attr_prefix and
       * attr_uri may be NULL.
       */
      const gchar *attr_name = (const gchar *) attributes[i];
      const gchar *attr_prefix = (const gchar *) attributes[i+1];
      const gchar *attr_uri = (const gchar *) attributes[i+2];
      /* Not NULL-terminated! */
      const gchar *attr_value = (const gchar *) attributes[i+3];
      gsize value_len = attributes[i+4] - attributes[i+3];

      if (!wocky_strdiff (attr_prefix, "xml") &&
          !wocky_strdiff (attr_name, "lang"))
        {
          wocky_node_set_language_n (priv->node, attr_value, value_len);
        }
      else
        {
          /* preserve the prefix, if any was received */
          if (attr_prefix != NULL)
            {
              GQuark ns = g_quark_from_string (attr_uri);
              wocky_node_attribute_ns_set_prefix (ns, attr_prefix);
            }

          wocky_node_set_attribute_n_ns (priv->node, attr_name,
              attr_value, value_len, attr_uri);
        }
     }

  priv->depth++;
}

static void
_start_element_ns (void *user_data, const xmlChar *localname,
    const xmlChar *prefix, const xmlChar *ns_uri, int nb_namespaces,
    const xmlChar **namespaces, int nb_attributes, int nb_defaulted,
    const xmlChar **attributes)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = self->priv;
  gchar *uri = NULL;

  if (ns_uri != NULL)
    uri = g_strstrip (g_strdup ((const gchar *) ns_uri));

  if (priv->stream_mode && G_UNLIKELY (priv->depth == 0))
    handle_stream_open (self, (const gchar *) localname, uri,
        (const gchar *) prefix, nb_attributes, attributes);
  else
    handle_regular_element (self, (const gchar *) localname, uri,
        nb_attributes, attributes);

  g_free (uri);
}

static void
_characters (void *user_data, const xmlChar *ch, int len)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = self->priv;

  if (priv->node != NULL)
    {
      wocky_node_append_content_n (priv->node, (const gchar *)ch,
          (gsize)len);
    }
}

static void
_end_element_ns (void *user_data, const xmlChar *localname,
    const xmlChar *prefix, const xmlChar *uri)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = self->priv;

  priv->depth--;

  if (priv->stream_mode && priv->depth == 0)
    {
      DEBUG ("Stream ended");
      g_queue_push_tail (priv->stanzas, NULL);
    }
  else if (priv->depth == (priv->stream_mode ? 1 : 0))
    {
      g_assert (g_queue_get_length (priv->nodes) == 0);
      DEBUG_STANZA (priv->stanza, "Received stanza");
      g_queue_push_tail (priv->stanzas, priv->stanza);
      priv->stanza = NULL;
      priv->node = NULL;
    }
  else
    {
      priv->node = (WockyNode *) g_queue_pop_tail (priv->nodes);
    }
}

static void
_error (void *user_data, xmlErrorPtr error)
{
  WockyXmppReader *self = WOCKY_XMPP_READER (user_data);
  WockyXmppReaderPrivate *priv = self->priv;

  if (error->level < XML_ERR_FATAL)
    {
      DEBUG ("Ignoring parser %s: %s",
       error->level == XML_ERR_WARNING ? "warning" : "recoverable error",
       error->message);
      return;
    }

  priv->error = g_error_new_literal (WOCKY_XMPP_READER_ERROR,
    WOCKY_XMPP_READER_ERROR_PARSE_ERROR, error->message);

  DEBUG ("Parsing failed %s", error->message);
  g_queue_push_tail (priv->stanzas, NULL);
}

/**
 * wocky_xmpp_reader_get_state:
 * @reader: a #WockyXmppReader
 *
 * Returns: The current state of the reader
 */
WockyXmppReaderState
wocky_xmpp_reader_get_state (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = reader->priv;

  return priv->state;
}

/* When the end of stream is reached the parser puts a NULL entry on the
 * queue. When that's the only entry left, go into either closed or ready state
 * (depending if an error was hit)
 */
static void
wocky_xmpp_reader_check_eos (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = reader->priv;

  if (!g_queue_is_empty (priv->stanzas)
      && g_queue_peek_head (priv->stanzas) == NULL)
    {
      priv->state = priv->error ? WOCKY_XMPP_READER_STATE_ERROR :
        WOCKY_XMPP_READER_STATE_CLOSED;
    }
}

/**
 * wocky_xmpp_reader_push:
 * @reader: a WockyXmppReader
 * @data: Data to read
 * @length: Size of @data
 *
 * Push an amount of data to parse.
 */
void
wocky_xmpp_reader_push (WockyXmppReader *reader, const guint8 *data,
    gsize length)
{
  WockyXmppReaderPrivate *priv = reader->priv;
  xmlParserCtxtPtr parser;

  g_return_if_fail (priv->state < WOCKY_XMPP_READER_STATE_CLOSED);

#ifdef ENABLE_DEBUG
  wocky_debug (WOCKY_DEBUG_NET, "Parsing chunk: %.*s", (int)length, data);
#endif

  parser = priv->parser;
  xmlParseChunk (parser, (const char*)data, length, FALSE);

  wocky_xmpp_reader_check_eos (reader);
}

/**
 * wocky_xmpp_reader_peek_stanza:
 * @reader: a #WockyXmppReader
 *
 * Returns the first #WockyStanza available from reader or NULL
 * if there are no available stanzas. The stanza is not removed from the
 * readers queue
 *
 * Returns: One #WockyStanza or NULL if there are no available stanzas. The
 * stanza is owned by the #WockyXmppReader
 */
WockyStanza *
wocky_xmpp_reader_peek_stanza (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = reader->priv;

  return g_queue_peek_head (priv->stanzas);
}

/**
 * wocky_xmpp_reader_pop_stanza:
 * @reader: a #WockyXmppReader
 *
 * Gets one #WockyStanza out of the reader or NULL if there are no
 * available stanzas.
 *
 * Returns: One #WockyStanza or NULL if there are no available stanzas.
 * Caller owns the returned stanza.
 */
WockyStanza *
wocky_xmpp_reader_pop_stanza (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = reader->priv;
  WockyStanza *s;

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

/**
 * wocky_xmpp_reader_get_error:
 * @reader: a #WockyXmppReader
 *
 * Get the error from the reader
 *
 * Returns: A copy of the error as encountered by the reader or NULL if there
 * was no error. Free after use.
 */
GError *
wocky_xmpp_reader_get_error (WockyXmppReader *reader)
{
  WockyXmppReaderPrivate *priv = reader->priv;

  return priv->error == NULL ? NULL : g_error_copy (priv->error);
}

/**
 * wocky_xmpp_reader_reset:
 * @reader: a #WockyXmppReader
 *
 * Reset the xml parser.
 *
 */
void
wocky_xmpp_reader_reset (WockyXmppReader *reader)
{
  DEBUG ("Resetting the xmpp reader");

  wocky_xmpp_reader_clear_parser_state (reader);
  wocky_init_xml_parser (reader);
}
