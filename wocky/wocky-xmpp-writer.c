/*
 * wocky-xmpp-writer.c - Source for WockyXmppWriter
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

#include <libxml/xmlwriter.h>

#include "wocky-xmpp-writer.h"

G_DEFINE_TYPE(WockyXmppWriter, wocky_xmpp_writer, G_TYPE_OBJECT)

/* private structure */
typedef struct _WockyXmppWriterPrivate WockyXmppWriterPrivate;

struct _WockyXmppWriterPrivate
{
  gboolean dispose_has_run;
  xmlTextWriterPtr xmlwriter;
  GQuark current_ns;
  GQuark stream_ns;
  xmlBufferPtr buffer;
};

#define WOCKY_XMPP_WRITER_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_WRITER, WockyXmppWriterPrivate))

static void
wocky_xmpp_writer_init (WockyXmppWriter *obj)
{
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
  priv->current_ns = 0;
  priv->stream_ns = 0;
  priv->buffer = xmlBufferCreate();
  priv->xmlwriter = xmlNewTextWriterMemory(priv->buffer, 0);
  xmlTextWriterSetIndent(priv->xmlwriter, 1);
}

static void wocky_xmpp_writer_dispose (GObject *object);
static void wocky_xmpp_writer_finalize (GObject *object);

static void
wocky_xmpp_writer_class_init (WockyXmppWriterClass *wocky_xmpp_writer_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_writer_class);

  g_type_class_add_private (wocky_xmpp_writer_class, sizeof (WockyXmppWriterPrivate));

  object_class->dispose = wocky_xmpp_writer_dispose;
  object_class->finalize = wocky_xmpp_writer_finalize;

}

void
wocky_xmpp_writer_dispose (GObject *object)
{
  WockyXmppWriter *self = WOCKY_XMPP_WRITER (object);
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (self);

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
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (self);

  /* free any data held directly by the object here */
  xmlFreeTextWriter(priv->xmlwriter);
  xmlBufferFree(priv->buffer);

  G_OBJECT_CLASS (wocky_xmpp_writer_parent_class)->finalize (object);
}

WockyXmppWriter *
wocky_xmpp_writer_new(void) {
  return g_object_new(WOCKY_TYPE_XMPP_WRITER, NULL);
}

void 
wocky_xmpp_writer_stream_open(WockyXmppWriter *writer,
                              const gchar *to, const gchar *from,
                              const gchar *version,
                              const guint8 **data, gsize *length) {
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (writer);

  xmlBufferEmpty(priv->buffer);
  xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *) 
                    "<?xml version='1.0' encoding='UTF-8'?>\n"             \
                    "<stream:stream\n"                                      \
                    "  xmlns='jabber:client'\n"                             \
                    "  xmlns:stream='http://etherx.jabber.org/streams'");

  if (to != NULL) {
    xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *)"\n  to=\"");
    xmlTextWriterFlush(priv->xmlwriter);
    xmlAttrSerializeTxtContent(priv->buffer, NULL, NULL, (xmlChar *)to);
    xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *)"\"");
  }

  if (from != NULL) {
    xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *)"\n  from=\"");
    xmlTextWriterFlush(priv->xmlwriter);
    xmlAttrSerializeTxtContent(priv->buffer, NULL, NULL, (xmlChar *)from);
    xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *)"\"");
  }

  if (version != NULL) {
    xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *)"\n  version=\"");
    xmlTextWriterFlush(priv->xmlwriter);
    xmlAttrSerializeTxtContent(priv->buffer, NULL, NULL, (xmlChar *)version);
    xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *)"\"");
  }

  xmlTextWriterWriteString(priv->xmlwriter, (xmlChar *) ">\n");
  xmlTextWriterFlush(priv->xmlwriter);

  *data = (const guint8 *)priv->buffer->content;
  *length  = priv->buffer->use;

  /* Set the magic known namespaces */
  priv->current_ns = g_quark_from_string("jabber:client");
  priv->stream_ns = g_quark_from_string("http://etherx.jabber.org/streams");
}

void wocky_xmpp_writer_stream_close(WockyXmppWriter *writer,
                                   const guint8 **data, gsize *length) {
  static const guint8 *close = (const guint8 *)"</stream:stream>\n";
  *data = close;
  *length = strlen((gchar *)close);
}

static void
_xml_write_node(WockyXmppWriter *writer, WockyXmppNode *node);

gboolean
_write_attr(const gchar *key, const gchar *value, const gchar *ns,
            gpointer user_data) {
  WockyXmppWriter *self = WOCKY_XMPP_WRITER(user_data);
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (self);
  GQuark attrns = 0;

  if (ns != NULL) {
    attrns = g_quark_from_string(ns);
  }

  if (attrns == 0 || attrns == priv->current_ns) {
    xmlTextWriterWriteAttribute(priv->xmlwriter, 
                                     (const xmlChar *)key, 
                                     (const xmlChar *)value);
  } else if (attrns == priv->stream_ns) {
    xmlTextWriterWriteAttributeNS(priv->xmlwriter, 
                                     (const xmlChar *)"stream",
                                     (const xmlChar *)key, 
                                     (const xmlChar *)NULL,
                                     (const xmlChar *)value);
  } else {
    xmlTextWriterWriteAttributeNS(priv->xmlwriter, 
                                     (const xmlChar *)key,
                                     (const xmlChar *)key, 
                                     (const xmlChar *)ns,
                                     (const xmlChar *)value);
  }
  return TRUE;
}

gboolean 
_write_child(WockyXmppNode *node, gpointer user_data) {
  _xml_write_node(WOCKY_XMPP_WRITER(user_data), node);
  return TRUE;
}


static void
_xml_write_node(WockyXmppWriter *writer, WockyXmppNode *node) {
  const gchar *l;
  GQuark oldns;
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (writer);

  oldns = priv->current_ns;
  
  if (node->ns == 0 || oldns == node->ns) {
    /* Another element in the current namespace */ 
    xmlTextWriterStartElement(priv->xmlwriter, (const xmlChar*) node->name);
  } else if (node->ns == priv->stream_ns) {
    xmlTextWriterStartElementNS(priv->xmlwriter, 
                                (const xmlChar *) "stream",
                                (const xmlChar *) node->name,
                                NULL);

  } else {
    priv->current_ns = node->ns;
    xmlTextWriterStartElementNS(priv->xmlwriter, 
                                NULL,
                                (const xmlChar *) node->name,
                                (const xmlChar *) wocky_xmpp_node_get_ns(node));
  }

  wocky_xmpp_node_each_attribute(node, _write_attr, writer);

  l = wocky_xmpp_node_get_language(node);
  if (l != NULL) {
    xmlTextWriterWriteAttributeNS(priv->xmlwriter, 
                                  (const xmlChar *)"xml", 
                                  (const xmlChar *)"lang", 
                                  NULL,
                                  (const xmlChar *)l);

  }


  wocky_xmpp_node_each_child(node, _write_child, writer);

  if (node->content) {
    xmlTextWriterWriteString(priv->xmlwriter, (const xmlChar*)node->content);
  }
  xmlTextWriterEndElement(priv->xmlwriter);
  priv->current_ns = oldns;
}


gboolean 
wocky_xmpp_writer_write_stanza(WockyXmppWriter *writer, 
                               WockyXmppStanza *stanza,
                               const guint8 **data, gsize *length,
                               GError **error) {
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (writer);

  xmlBufferEmpty(priv->buffer);

  _xml_write_node(writer, stanza->node);
  xmlTextWriterFlush(priv->xmlwriter);

  *data = (const guint8 *)priv->buffer->content;
  *length  = priv->buffer->use;

  return TRUE;
}

void 
wocky_xmpp_writer_flush(WockyXmppWriter *writer) {
  WockyXmppWriterPrivate *priv = WOCKY_XMPP_WRITER_GET_PRIVATE (writer);

  xmlBufferFree(priv->buffer);
  priv->buffer = xmlBufferCreate();
}
