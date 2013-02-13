/*
 * wocky-xmpp-writer.c - Source for WockyXmppWriter
 * Copyright (C) 2006-2009 Collabora Ltd.
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
 * SECTION: wocky-xmpp-writer
 * @title: WockyXmppWriter
 * @short_description: Xmpp stanza to XML serializer
 *
 * The #WockyXmppWriter serializes #WockyStanza<!-- -->s and XMPP stream opening
 * and closing to raw XML. The various functions provide a pointer to an
 * internal buffer, which remains valid until the next call to the writer.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlwriter.h>

#include "wocky-xmpp-writer.h"

G_DEFINE_TYPE (WockyXmppWriter, wocky_xmpp_writer, G_TYPE_OBJECT)

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_XMPP_WRITER
#include "wocky-debug-internal.h"

/* properties */
enum {
  PROP_STREAMING_MODE = 1,
};

/* private structure */
struct _WockyXmppWriterPrivate
{
  gboolean dispose_has_run;
  xmlTextWriterPtr xmlwriter;
  GQuark current_ns;
  GQuark stream_ns;
  gboolean stream_mode;
  xmlBufferPtr buffer;
};

static void
wocky_xmpp_writer_init (WockyXmppWriter *self)
{
  WockyXmppWriterPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_XMPP_WRITER,
      WockyXmppWriterPrivate);
  priv = self->priv;

  priv->current_ns = 0;
  priv->stream_ns = 0;
  priv->buffer = xmlBufferCreate ();
  priv->xmlwriter = xmlNewTextWriterMemory (priv->buffer, 0);
  priv->stream_mode = TRUE;
  /* xmlTextWriterSetIndent (priv->xmlwriter, 1); */
}

static void wocky_xmpp_writer_dispose (GObject *object);
static void wocky_xmpp_writer_finalize (GObject *object);
static void wocky_xmpp_write_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void wocky_xmpp_write_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);

static void
wocky_xmpp_writer_class_init (WockyXmppWriterClass *wocky_xmpp_writer_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_writer_class);
  GParamSpec *param_spec;

  g_type_class_add_private (wocky_xmpp_writer_class,
      sizeof (WockyXmppWriterPrivate));

  object_class->dispose = wocky_xmpp_writer_dispose;
  object_class->finalize = wocky_xmpp_writer_finalize;

  object_class->set_property = wocky_xmpp_write_set_property;
  object_class->get_property = wocky_xmpp_write_get_property;

  param_spec = g_param_spec_boolean ("streaming-mode", "streaming-mode",
    "Whether the xml to be written is one big stream or separate documents",
    TRUE,
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STREAMING_MODE,
    param_spec);
}

void
wocky_xmpp_writer_dispose (GObject *object)
{
  WockyXmppWriter *self = WOCKY_XMPP_WRITER (object);
  WockyXmppWriterPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_xmpp_writer_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_writer_parent_class)->dispose (object);
}

void
wocky_xmpp_writer_finalize (GObject *object)
{
  WockyXmppWriter *self = WOCKY_XMPP_WRITER (object);
  WockyXmppWriterPrivate *priv = self->priv;

  /* free any data held directly by the object here */
  xmlFreeTextWriter (priv->xmlwriter);
  xmlBufferFree (priv->buffer);

  G_OBJECT_CLASS (wocky_xmpp_writer_parent_class)->finalize (object);
}

static void
wocky_xmpp_write_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyXmppWriter *writer = WOCKY_XMPP_WRITER (object);
  WockyXmppWriterPrivate *priv = writer->priv;

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
wocky_xmpp_write_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyXmppWriter *writer = WOCKY_XMPP_WRITER (object);
  WockyXmppWriterPrivate *priv = writer->priv;

  switch (property_id)
    {
      case PROP_STREAMING_MODE:
        g_value_set_boolean (value, priv->stream_mode);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

/**
 * wocky_xmpp_writer_new
 *
 * Convenience function to create a new #WockyXmppWriter.
 *
 * Returns: a new #WockyXmppWriter
 */
WockyXmppWriter *
wocky_xmpp_writer_new (void)
{
  return g_object_new (WOCKY_TYPE_XMPP_WRITER, NULL);
}

/**
 * wocky_xmpp_writer_new_no_stream
 *
 * Convenience function to create a new #WockyXmppWriter that has streaming
 * mode disabled.
 *
 * Returns: a new #WockyXmppWriter in non-streaming mode
 */
WockyXmppWriter *
wocky_xmpp_writer_new_no_stream (void)
{
  return g_object_new (WOCKY_TYPE_XMPP_WRITER, "streaming-mode", FALSE, NULL);
}

/**
 * wocky_xmpp_writer_stream_open:
 * @writer: a WockyXmppWriter
 * @to: the target of the stream opening (usually the xmpp server name)
 * @from: the sender of the stream opening (usually the jid of the client)
 * @version: XMPP version
 * @lang: default XMPP stream language
 * @id: XMPP Stream ID, if any, or NULL
 * @data: location to store a pointer to the data buffer
 * @length: length of the data buffer
 *
 * Create the XML opening header of an XMPP stream. The result is available in
 * the @data buffer. The buffer is only valid until the next call to a function
 * the writer.
 *
 * This function can only be called in streaming mode.
 */
void
wocky_xmpp_writer_stream_open (WockyXmppWriter *writer,
    const gchar *to,
    const gchar *from,
    const gchar *version,
    const gchar *lang,
    const gchar *id,
    const guint8 **data,
    gsize *length)
{
  WockyXmppWriterPrivate *priv = writer->priv;

  g_assert (priv->stream_mode);

  xmlBufferEmpty (priv->buffer);
  xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)
      "<?xml version='1.0' encoding='UTF-8'?>\n"            \
      "<stream:stream"                                      \
      " xmlns='jabber:client'"                              \
      " xmlns:stream='http://etherx.jabber.org/streams'");

  if (to != NULL)
    {
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)" to=\"");
      xmlTextWriterFlush (priv->xmlwriter);
      xmlAttrSerializeTxtContent (priv->buffer, NULL, NULL, (xmlChar *) to);
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)"\"");
    }

  if (from != NULL)
    {
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)" from=\"");
      xmlTextWriterFlush (priv->xmlwriter);
      xmlAttrSerializeTxtContent (priv->buffer, NULL, NULL, (xmlChar *) from);
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)"\"");
    }

  if (version != NULL)
    {
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)" version=\"");
      xmlTextWriterFlush (priv->xmlwriter);
      xmlAttrSerializeTxtContent (priv->buffer, NULL,
        NULL, (xmlChar *) version);
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)"\"");
    }

  if (lang != NULL)
    {
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)" xml:lang=\"");
      xmlTextWriterFlush (priv->xmlwriter);
      xmlAttrSerializeTxtContent (priv->buffer, NULL,
        NULL, (xmlChar *) lang);
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)"\"");
    }

  if (id != NULL)
    {
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)" id=\"");
      xmlTextWriterFlush (priv->xmlwriter);
      xmlAttrSerializeTxtContent (priv->buffer, NULL, NULL, (xmlChar *) id);
      xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *)"\"");
    }

  xmlTextWriterWriteString (priv->xmlwriter, (xmlChar *) ">\n");
  xmlTextWriterFlush (priv->xmlwriter);

  *data = (const guint8 *)priv->buffer->content;
  *length  = priv->buffer->use;

  /* Set the magic known namespaces */
  priv->current_ns = g_quark_from_string ("jabber:client");
  priv->stream_ns = g_quark_from_string ("http://etherx.jabber.org/streams");

  DEBUG ("Writing stream opening: %.*s", (int) *length, *data);
}

/**
 * wocky_xmpp_writer_stream_close:
 * @writer: a WockyXmppWriter
 * @data: location to store a pointer to the data buffer
 * @length: length of the data buffer
 *
 * Create the XML closing footer of an XMPP stream . The result is available
 * in the @data buffer. The buffer is only valid until the next call to a
 * function
 *
 * This function can only be called in streaming mode.
 */
void
wocky_xmpp_writer_stream_close (WockyXmppWriter *writer,
    const guint8 **data, gsize *length)
{
  WockyXmppWriterPrivate *priv = writer->priv;
  static const guint8 *close = (const guint8 *)"</stream:stream>\n";

  g_assert (priv->stream_mode);

  *data = close;
  *length = strlen ((gchar *) close);

  DEBUG ("Writing stream close: %.*s", (int) *length, *data);
}

static void
_xml_write_node (WockyXmppWriter *writer, WockyNode *node);

static gboolean
_write_attr (const gchar *key, const gchar *value,
    const gchar *prefix, const gchar *ns,
    gpointer user_data)
{
  WockyXmppWriter *self = WOCKY_XMPP_WRITER (user_data);
  WockyXmppWriterPrivate *priv = self->priv;
  GQuark attrns = 0;

  if (ns != NULL)
    {
      attrns = g_quark_from_string (ns);
    }

  if (attrns == 0 || attrns == priv->current_ns)
    {
      xmlTextWriterWriteAttribute (priv->xmlwriter,
          (const xmlChar *)key,
          (const xmlChar *)value);
    }
  else if (attrns == priv->stream_ns)
    {
      xmlTextWriterWriteAttributeNS (priv->xmlwriter,
          (const xmlChar *)"stream", (const xmlChar *)key,
          (const xmlChar *)NULL, (const xmlChar *)value);
    }
  else
    {
      xmlTextWriterWriteAttributeNS (priv->xmlwriter,
          (const xmlChar *)prefix, (const xmlChar *)key, (const xmlChar *)ns,
          (const xmlChar *)value);
    }
  return TRUE;
}

static gboolean
_write_child (WockyNode *node, gpointer user_data)
{
  _xml_write_node (WOCKY_XMPP_WRITER (user_data), node);
  return TRUE;
}

static void
_xml_write_node (WockyXmppWriter *writer, WockyNode *node)
{
  const gchar *l;
  GQuark oldns;
  WockyXmppWriterPrivate *priv = writer->priv;

  oldns = priv->current_ns;

  if (node->ns == 0 || oldns == node->ns)
    {
      /* Another element in the current namespace */
      xmlTextWriterStartElement (priv->xmlwriter, (const xmlChar*) node->name);
    }
  else if (node->ns == priv->stream_ns)
    {
      xmlTextWriterStartElementNS(priv->xmlwriter,
          (const xmlChar *) "stream", (const xmlChar *) node->name, NULL);

    }
  else
    {
      priv->current_ns = node->ns;
      xmlTextWriterStartElementNS (priv->xmlwriter,
          NULL, (const xmlChar *) node->name,
          (const xmlChar *) wocky_node_get_ns (node));
    }

  wocky_node_each_attribute (node, _write_attr, writer);

  l = wocky_node_get_language (node);

  if (l != NULL)
    {
      xmlTextWriterWriteAttributeNS(priv->xmlwriter,
          (const xmlChar *)"xml", (const xmlChar *)"lang", NULL,
          (const xmlChar *)l);
    }

  wocky_node_each_child (node, _write_child, writer);

  if (node->content != NULL)
    {
      xmlTextWriterWriteString (priv->xmlwriter,
        (const xmlChar*)node->content);
    }

  xmlTextWriterEndElement (priv->xmlwriter);
  priv->current_ns = oldns;
}

static void
_write_node_tree (WockyXmppWriter *writer,
    WockyNodeTree *tree,
    const guint8 **data,
    gsize *length)
{
  WockyXmppWriterPrivate *priv = writer->priv;

  xmlBufferEmpty (priv->buffer);

  DEBUG_NODE_TREE (tree, "Serializing tree:");

  if (!priv->stream_mode)
    {
      xmlTextWriterStartDocument (priv->xmlwriter, "1.0", "utf-8", NULL);
    }

  _xml_write_node (writer, wocky_node_tree_get_top_node (tree));

  if (!priv->stream_mode)
    {
      xmlTextWriterEndDocument (priv->xmlwriter);
    }
  xmlTextWriterFlush (priv->xmlwriter);

  *data = (const guint8 *)priv->buffer->content;
  *length  = priv->buffer->use;

#ifdef ENABLE_DEBUG
  wocky_debug (WOCKY_DEBUG_NET, "Writing xml: %.*s", (int)*length, *data);
#endif
}

/**
 * wocky_xmpp_writer_write_stanza:
 * @writer: a WockyXmppWriter
 * @stanza: the stanza to serialize
 * @data: location to store a pointer to the data buffer
 * @length: length of the data buffer
 *
 * Serialize the @stanza to XML. The result is available in the
 * @data buffer. The buffer is only valid until the next call to a function
 */
void
wocky_xmpp_writer_write_stanza (WockyXmppWriter *writer,
    WockyStanza *stanza,
    const guint8 **data,
    gsize *length)
{
  _write_node_tree (writer, WOCKY_NODE_TREE (stanza), data, length);
}

/**
 * wocky_xmpp_writer_write_node_tree:
 * @writer: a WockyXmppWriter
 * @tree: the node tree to serialize
 * @data: location to store a pointer to the data buffer
 * @length: length of the data buffer
 *
 * Serialize the @tree to XML. The result is available in the
 * @data buffer. The buffer is only valid until the next call to a function.
 * This function may only be called in non-streaming mode.
 */
void
wocky_xmpp_writer_write_node_tree (WockyXmppWriter *writer,
    WockyNodeTree *tree,
    const guint8 **data,
    gsize *length)
{
  *data = NULL;
  *length = 0;

  g_return_if_fail (!writer->priv->stream_mode);

  _write_node_tree (writer, tree, data, length);
}

/**
 * wocky_xmpp_writer_flush:
 * @writer: a WockyXmppWriter
 *
 * Flushes and frees the internal data buffer
 */
void
wocky_xmpp_writer_flush (WockyXmppWriter *writer)
{
  WockyXmppWriterPrivate *priv = writer->priv;

  xmlBufferFree (priv->buffer);
  priv->buffer = xmlBufferCreate ();
}
