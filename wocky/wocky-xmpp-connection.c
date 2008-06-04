/*
 * wocky-xmpp-connection.c - Source for WockyXmppConnection
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "wocky-xmpp-connection.h"
#include "signals-marshal.h"

#include "wocky-xmpp-reader.h"
#include "wocky-xmpp-writer.h"
#include "wocky-transport.h"
#include "wocky-xmpp-stanza.h"

#define XMPP_STREAM_NAMESPACE "http://etherx.jabber.org/streams"

static void _xmpp_connection_received_data (WockyTransport *transport,
    WockyBuffer *buffer, gpointer user_data);

G_DEFINE_TYPE(WockyXmppConnection, wocky_xmpp_connection, G_TYPE_OBJECT)

/* signal enum */
enum
{
  STREAM_OPENED,
  STREAM_CLOSED,
  PARSE_ERROR,
  RECEIVED_STANZA,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

static void _reader_stream_opened_cb (WockyXmppReader *reader,
    const gchar *to, const gchar *from, const gchar *version,
    gpointer user_data);

static void _reader_stream_closed_cb (WockyXmppReader *reader,
    gpointer user_data);

static void _reader_received_stanza_cb (WockyXmppReader *reader,
    WockyXmppStanza *stanza, gpointer user_data);

/* private structure */
typedef struct _WockyXmppConnectionPrivate WockyXmppConnectionPrivate;

struct _WockyXmppConnectionPrivate
{
  WockyXmppReader *reader;
  WockyXmppWriter *writer;
  gboolean dispose_has_run;
  guint last_id;
};

#define WOCKY_XMPP_CONNECTION_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), WOCKY_TYPE_XMPP_CONNECTION, \
    WockyXmppConnectionPrivate))

static GObject *
wocky_xmpp_connection_constructor (GType type,
    guint n_props, GObjectConstructParam *props)
{
  GObject *obj;
  WockyXmppConnectionPrivate *priv;

  obj = G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->
        constructor (type, n_props, props);

  priv = WOCKY_XMPP_CONNECTION_GET_PRIVATE (obj);

  priv->writer = wocky_xmpp_writer_new ();
  priv->reader = wocky_xmpp_reader_new ();

  g_signal_connect (priv->reader, "stream-opened",
      G_CALLBACK (_reader_stream_opened_cb), obj);
  g_signal_connect (priv->reader, "stream-closed",
      G_CALLBACK (_reader_stream_closed_cb), obj);
  g_signal_connect (priv->reader, "received-stanza",
      G_CALLBACK (_reader_received_stanza_cb), obj);

  return obj;
}

static void
wocky_xmpp_connection_init (WockyXmppConnection *obj)
{
  WockyXmppConnectionPrivate *priv = WOCKY_XMPP_CONNECTION_GET_PRIVATE (obj);
  obj->transport = NULL;
  priv->last_id = 0;
}

static void wocky_xmpp_connection_dispose (GObject *object);
static void wocky_xmpp_connection_finalize (GObject *object);

static void
wocky_xmpp_connection_class_init (
    WockyXmppConnectionClass *wocky_xmpp_connection_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_connection_class);

  g_type_class_add_private (wocky_xmpp_connection_class,
      sizeof (WockyXmppConnectionPrivate));

  object_class->dispose = wocky_xmpp_connection_dispose;
  object_class->finalize = wocky_xmpp_connection_finalize;

  object_class->constructor = wocky_xmpp_connection_constructor;

  signals[STREAM_OPENED] = g_signal_new ("stream-opened",
      G_OBJECT_CLASS_TYPE (wocky_xmpp_connection_class),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _wocky_signals_marshal_VOID__STRING_STRING_STRING,
      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  signals[STREAM_CLOSED] = g_signal_new ("stream-closed",
       G_OBJECT_CLASS_TYPE (wocky_xmpp_connection_class),
       G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
       0,
       NULL, NULL,
       g_cclosure_marshal_VOID__VOID,
       G_TYPE_NONE, 0);

  signals[RECEIVED_STANZA] = g_signal_new ("received-stanza",
      G_OBJECT_CLASS_TYPE (wocky_xmpp_connection_class),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, WOCKY_TYPE_XMPP_STANZA);

  signals[PARSE_ERROR] = g_signal_new ("parse-error",
      G_OBJECT_CLASS_TYPE(wocky_xmpp_connection_class),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

void
wocky_xmpp_connection_dispose (GObject *object)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (object);
  WockyXmppConnectionPrivate *priv =
      WOCKY_XMPP_CONNECTION_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;
  if (self->transport != NULL)
    {
      g_object_unref (self->transport);
      self->transport = NULL;
    }

  if (priv->reader != NULL)
    {
      g_object_unref (priv->reader);
      priv->reader = NULL;
    }

  if (priv->writer != NULL)
    {
      g_object_unref (priv->writer);
      priv->writer = NULL;
    }

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->dispose (object);
}

void
wocky_xmpp_connection_finalize (GObject *object)
{
  G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->finalize (object);
}



static WockyXmppConnection *
new_connection (WockyTransport *transport, gboolean stream)
{
  WockyXmppConnection * result;

  result = g_object_new (WOCKY_TYPE_XMPP_CONNECTION, NULL);

  if (transport != NULL)
    {
      wocky_xmpp_connection_engage (result, transport);
    }

  return result;
}

WockyXmppConnection *
wocky_xmpp_connection_new (WockyTransport *transport)
{
  return new_connection (transport, TRUE);
}

WockyXmppConnection *
wocky_xmpp_connection_new_no_stream (WockyTransport *transport)
{
  return new_connection (transport, FALSE);
}

void
wocky_xmpp_connection_open (WockyXmppConnection *connection,
    const gchar *to, const gchar *from, const gchar *version)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);
  const guint8 *data;
  gsize length;

  g_assert ((connection->stream_flags & WOCKY_XMPP_CONNECTION_STREAM_SENT)
      == 0);

  wocky_xmpp_writer_stream_open (priv->writer, to, from, version, &data,
      &length);
  connection->stream_flags |= WOCKY_XMPP_CONNECTION_STREAM_SENT;
  wocky_transport_send (connection->transport, data, length, NULL);
}

void
wocky_xmpp_connection_restart (WockyXmppConnection *connection)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);

  g_assert (connection->stream_flags
      & WOCKY_XMPP_CONNECTION_STREAM_FULLY_OPEN);
  wocky_xmpp_reader_reset (priv->reader);
  connection->stream_flags = 0;
}

void
wocky_xmpp_connection_close (WockyXmppConnection *connection)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);
  const guint8 *data;
  gsize length;

  connection->stream_flags |= WOCKY_XMPP_CONNECTION_CLOSE_SENT;

  wocky_xmpp_writer_stream_close (priv->writer, &data, &length);
  wocky_transport_send (connection->transport, data, length, NULL);
}

void
wocky_xmpp_connection_engage (WockyXmppConnection *connection,
    WockyTransport *transport)
{
  g_assert (connection->transport == NULL);

  connection->transport = g_object_ref (transport);
  wocky_transport_set_handler (transport, _xmpp_connection_received_data,
      connection);
}

void
wocky_xmpp_connection_disengage (WockyXmppConnection *connection)
{
  g_assert (connection->transport != NULL);

  wocky_transport_set_handler (connection->transport, NULL, NULL);

  g_object_unref (connection->transport);
  connection->transport = NULL;
}

gboolean
wocky_xmpp_connection_send (WockyXmppConnection *connection,
    WockyXmppStanza *stanza, GError **error)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);
  const guint8 *data;
  gsize length;
  const gchar *id;

  id = wocky_xmpp_node_get_attribute (stanza->node, "id");
  if (id == NULL)
    {
      gchar *tmp = wocky_xmpp_connection_new_id (connection);
      wocky_xmpp_node_set_attribute (stanza->node, "id", tmp);
      g_free (tmp);
    }

  if (!wocky_xmpp_writer_write_stanza (priv->writer, stanza,
      &data, &length, error))
    {
      return FALSE;
    }

  return wocky_transport_send (connection->transport, data, length, error);
}

static void
_xmpp_connection_received_data (WockyTransport *transport,
    WockyBuffer *buffer, gpointer user_data)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (user_data);
  WockyXmppConnectionPrivate *priv =
      WOCKY_XMPP_CONNECTION_GET_PRIVATE (self);
  gboolean ret;
  GError *error = NULL;

  g_assert (buffer->length > 0);

  /* Ensure we're not disposed inside while running the reader is busy */
  g_object_ref (self);
  ret = wocky_xmpp_reader_push (priv->reader, buffer->data, buffer->length,
      &error);

  if (!ret)
    {
      g_signal_emit (self, signals[PARSE_ERROR], 0);
    }
  g_object_unref (self);
}

static void
_reader_stream_opened_cb (WockyXmppReader *reader, const gchar *to,
    const gchar *from, const gchar *version, gpointer user_data)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (user_data);

  self->stream_flags |= WOCKY_XMPP_CONNECTION_STREAM_RECEIVED;

  g_signal_emit (self, signals[STREAM_OPENED], 0, to, from, version);
}

static void
_reader_stream_closed_cb (WockyXmppReader *reader, gpointer user_data)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (user_data);

  self->stream_flags |= WOCKY_XMPP_CONNECTION_CLOSE_RECEIVED;

  g_signal_emit (self, signals[STREAM_CLOSED], 0);
}

static void
_reader_received_stanza_cb (WockyXmppReader *reader, WockyXmppStanza *stanza,
    gpointer user_data)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (user_data);
  g_signal_emit (self, signals[RECEIVED_STANZA], 0, stanza);
}

gchar *
wocky_xmpp_connection_new_id (WockyXmppConnection *self)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (self);
  GTimeVal tv;
  glong val;

  g_get_current_time (&tv);
  val = (tv.tv_sec & tv.tv_usec) + priv->last_id++;

  return g_strdup_printf ("%ld%ld", val, tv.tv_usec);
}
