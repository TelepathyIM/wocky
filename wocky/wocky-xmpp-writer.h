/*
 * wocky-xmpp-writer.h - Header for WockyXmppWriter
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_XMPP_WRITER_H__
#define __WOCKY_XMPP_WRITER_H__

#include <glib-object.h>

#include "wocky-stanza.h"
#include "wocky-node-tree.h"

G_BEGIN_DECLS

typedef struct _WockyXmppWriter WockyXmppWriter;

/**
 * WockyXmppWriterClass:
 *
 * The class of a #WockyXmppWriter.
 */
typedef struct _WockyXmppWriterClass WockyXmppWriterClass;
typedef struct _WockyXmppWriterPrivate WockyXmppWriterPrivate;


struct _WockyXmppWriterClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _WockyXmppWriter {
    /*<private>*/
    GObject parent;

    WockyXmppWriterPrivate *priv;
};

GType wocky_xmpp_writer_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_WRITER \
  (wocky_xmpp_writer_get_type ())
#define WOCKY_XMPP_WRITER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_WRITER, \
  WockyXmppWriter))
#define WOCKY_XMPP_WRITER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_WRITER, \
   WockyXmppWriterClass))
#define WOCKY_IS_XMPP_WRITER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_WRITER))
#define WOCKY_IS_XMPP_WRITER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_WRITER))
#define WOCKY_XMPP_WRITER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_WRITER, \
   WockyXmppWriterClass))


WockyXmppWriter *wocky_xmpp_writer_new (void);
WockyXmppWriter *wocky_xmpp_writer_new_no_stream (void);

void wocky_xmpp_writer_stream_open (WockyXmppWriter *writer,
    const gchar *to,
    const gchar *from,
    const gchar *version,
    const gchar *lang,
    const gchar *id,
    const guint8 **data,
    gsize *length);

void wocky_xmpp_writer_stream_close (WockyXmppWriter *writer,
    const guint8 **data, gsize *length);

void wocky_xmpp_writer_write_stanza (WockyXmppWriter *writer,
    WockyStanza *stanza,
    const guint8 **data,
    gsize *length);

void wocky_xmpp_writer_write_node_tree (WockyXmppWriter *writer,
    WockyNodeTree *tree,
    const guint8 **data,
    gsize *length);

void wocky_xmpp_writer_flush (WockyXmppWriter *writer);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_WRITER_H__*/
