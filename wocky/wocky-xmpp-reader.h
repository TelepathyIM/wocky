/*
 * wocky-xmpp-reader.h - Header for WockyXmppReader
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
#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_XMPP_READER_H__
#define __WOCKY_XMPP_READER_H__

#include <glib-object.h>
#include "wocky-enumtypes.h"
#include "wocky-stanza.h"

G_BEGIN_DECLS

typedef struct _WockyXmppReader WockyXmppReader;

/**
 * WockyXmppReaderClass:
 *
 * The class of a #WockyXmppReader.
 */
typedef struct _WockyXmppReaderClass WockyXmppReaderClass;
typedef struct _WockyXmppReaderPrivate WockyXmppReaderPrivate;


struct _WockyXmppReaderClass {
    /*<private>*/
    GObjectClass parent_class;

    /*<protected>*/
    const gchar *stream_element_name;
    const gchar *stream_element_ns;
};

struct _WockyXmppReader {
    /*<private>*/
    GObject parent;
    WockyXmppReaderPrivate *priv;
};

/**
 * WockyXmppReaderState:
 * @WOCKY_XMPP_READER_STATE_INITIAL : initial state
 * @WOCKY_XMPP_READER_STATE_OPENED  : stream is open
 * @WOCKY_XMPP_READER_STATE_CLOSED  : stream has been closed
 * @WOCKY_XMPP_READER_STATE_ERROR   : stream reader hit an error
 *
 * The possible states a reader can be in.
 */
typedef enum {
  WOCKY_XMPP_READER_STATE_INITIAL,
  WOCKY_XMPP_READER_STATE_OPENED,
  WOCKY_XMPP_READER_STATE_CLOSED,
  WOCKY_XMPP_READER_STATE_ERROR,
} WockyXmppReaderState;

/**
 * WockyXmppReaderError:
 * @WOCKY_XMPP_READER_ERROR_INVALID_STREAM_START : invalid start of xmpp stream
 * @WOCKY_XMPP_READER_ERROR_PARSE_ERROR          : error in parsing the XML
 *
 * The different errors that can occur while reading a stream
 */
typedef enum {
  WOCKY_XMPP_READER_ERROR_INVALID_STREAM_START,
  WOCKY_XMPP_READER_ERROR_PARSE_ERROR,
} WockyXmppReaderError;

GQuark wocky_xmpp_reader_error_quark (void);

/**
 * WOCKY_XMPP_READER_ERROR:
 *
 * Get access to the error quark of the reader.
 */
#define WOCKY_XMPP_READER_ERROR (wocky_xmpp_reader_error_quark ())

GType wocky_xmpp_reader_get_type (void);

/* TYPE MACROS */
#define WOCKY_TYPE_XMPP_READER \
  (wocky_xmpp_reader_get_type ())
#define WOCKY_XMPP_READER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), WOCKY_TYPE_XMPP_READER, \
   WockyXmppReader))
#define WOCKY_XMPP_READER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), WOCKY_TYPE_XMPP_READER,  \
   WockyXmppReaderClass))
#define WOCKY_IS_XMPP_READER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), WOCKY_TYPE_XMPP_READER))
#define WOCKY_IS_XMPP_READER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), WOCKY_TYPE_XMPP_READER))
#define WOCKY_XMPP_READER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_XMPP_READER, \
   WockyXmppReaderClass))


WockyXmppReader * wocky_xmpp_reader_new (void);
WockyXmppReader * wocky_xmpp_reader_new_no_stream (void);
WockyXmppReader * wocky_xmpp_reader_new_no_stream_ns (
    const gchar *default_namespace);


WockyXmppReaderState wocky_xmpp_reader_get_state (WockyXmppReader *reader);

void wocky_xmpp_reader_push (WockyXmppReader *reader,
    const guint8 *data,
    gsize length);

WockyStanza *wocky_xmpp_reader_pop_stanza (WockyXmppReader *reader);
WockyStanza *wocky_xmpp_reader_peek_stanza (WockyXmppReader *reader);

GError *wocky_xmpp_reader_get_error (WockyXmppReader *reader);
void wocky_xmpp_reader_reset (WockyXmppReader *reader);

G_END_DECLS

#endif /* #ifndef __WOCKY_XMPP_READER_H__*/
