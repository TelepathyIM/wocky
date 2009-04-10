/*
 * wocky-xmpp-connection.c - Source for WockyXmppConnection
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


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "wocky-xmpp-connection.h"
#include "wocky-signals-marshal.h"

#include "wocky-xmpp-reader.h"
#include "wocky-xmpp-writer.h"
#include "wocky-xmpp-stanza.h"

#define BUFFER_SIZE 1024

//static void _xmpp_connection_received_data (GObject *source,
//    GAsyncResult *result, gpointer user_data);

G_DEFINE_TYPE(WockyXmppConnection, wocky_xmpp_connection, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_BASE_STREAM = 1,
};

/* private structure */
typedef struct _WockyXmppConnectionPrivate WockyXmppConnectionPrivate;

struct _WockyXmppConnectionPrivate
{
  WockyXmppReader *reader;
  WockyXmppWriter *writer;
  gboolean dispose_has_run;
  GIOStream *stream;
  GInputStream *input_stream;
  GOutputStream *output_stream;
  guint last_id;
  GCancellable *cancel_read;

  guint8 buffer[BUFFER_SIZE];
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

  return obj;
}

static void
wocky_xmpp_connection_init (WockyXmppConnection *obj)
{
  WockyXmppConnectionPrivate *priv = WOCKY_XMPP_CONNECTION_GET_PRIVATE (obj);

  obj->stream = NULL;
  priv->last_id = 0;

  priv->cancel_read = g_cancellable_new ();
}

static void wocky_xmpp_connection_dispose (GObject *object);
static void wocky_xmpp_connection_finalize (GObject *object);

static void
wocky_xmpp_connection_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (object);
  WockyXmppConnectionPrivate *priv =
      WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);

  switch (property_id)
    {
      case PROP_BASE_STREAM:
        g_assert (priv->stream == NULL);
        priv->stream = g_value_dup_object (value);
        g_assert (priv->stream != NULL);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_connection_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (object);
  WockyXmppConnectionPrivate *priv =
      WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);

  switch (property_id)
    {
      case PROP_BASE_STREAM:
        g_value_set_object (value, priv->stream);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_xmpp_connection_class_init (
    WockyXmppConnectionClass *wocky_xmpp_connection_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_xmpp_connection_class);
  GParamSpec *spec;

  g_type_class_add_private (wocky_xmpp_connection_class,
      sizeof (WockyXmppConnectionPrivate));

  object_class->set_property = wocky_xmpp_connection_set_property;
  object_class->get_property = wocky_xmpp_connection_get_property;
  object_class->dispose = wocky_xmpp_connection_dispose;
  object_class->finalize = wocky_xmpp_connection_finalize;

  object_class->constructor = wocky_xmpp_connection_constructor;

  spec = g_param_spec_object ("base-stream", "base stream",
    "the stream that the XMPP connection communicates over",
    G_TYPE_IO_STREAM,
    G_PARAM_READWRITE |
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_BASE_STREAM, spec);
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
  if (self->stream != NULL)
    {
      g_object_unref (self->stream);
      self->stream = NULL;
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

  if (priv->cancel_read != NULL)
    g_object_unref (priv->cancel_read);
  priv->cancel_read = NULL;

  if (priv->input_stream != NULL)
    g_object_unref (priv->input_stream);
  priv->input_stream = NULL;

  if (priv->output_stream != NULL)
    g_object_unref (priv->output_stream);
  priv->output_stream = NULL;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->dispose (object);
}

void
wocky_xmpp_connection_finalize (GObject *object)
{
  G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->finalize (object);
}



WockyXmppConnection *
wocky_xmpp_connection_new (GIOStream *stream)
{
  WockyXmppConnection * result;

  result = g_object_new (WOCKY_TYPE_XMPP_CONNECTION,
    "base-stream", stream, NULL);

  return result;
}

#if 0
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

  wocky_xmpp_writer_stream_open (priv->writer, to, from, version, NULL,
      &data, &length);
  connection->stream_flags |= WOCKY_XMPP_CONNECTION_STREAM_SENT;

  /* FIXME catch errors */
  g_output_stream_write_all (priv->output_stream,
    data, length, NULL, NULL, NULL);
}

void
wocky_xmpp_connection_restart (WockyXmppConnection *connection)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);

  g_assert (connection->stream_flags
      & WOCKY_XMPP_CONNECTION_STREAM_FULLY_OPEN);

  g_cancellable_reset (priv->cancel_read);
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
  g_output_stream_write_all (priv->output_stream, data, length,
    NULL, NULL, NULL);
}

static void
wocky_xmpp_connection_do_read (WockyXmppConnection *self)
{
  WockyXmppConnectionPrivate *priv =
      WOCKY_XMPP_CONNECTION_GET_PRIVATE (self);

  if (priv->input_stream != NULL &&
      !g_input_stream_has_pending (priv->input_stream))
    g_input_stream_read_async (priv->input_stream,
      priv->buffer, BUFFER_SIZE,
      G_PRIORITY_DEFAULT,
      priv->cancel_read,
      _xmpp_connection_received_data,
      self);
}


void
wocky_xmpp_connection_engage (WockyXmppConnection *connection,
    GIOStream *stream)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);

  g_assert (connection->stream == NULL);

  connection->stream = g_object_ref (stream);

  priv->input_stream = g_io_stream_get_input_stream (stream);
  priv->output_stream = g_io_stream_get_output_stream (stream);

  wocky_xmpp_connection_do_read (connection);
}

void
wocky_xmpp_connection_disengage (WockyXmppConnection *connection)
{
  WockyXmppConnectionPrivate *priv =
    WOCKY_XMPP_CONNECTION_GET_PRIVATE (connection);

  g_assert (connection->stream != NULL);

  g_cancellable_cancel (priv->cancel_read);

  g_object_unref (priv->input_stream);
  priv->input_stream = NULL;

  g_object_unref (priv->output_stream);
  priv->output_stream = NULL;

  g_object_unref (connection->stream);
  connection->stream = NULL;
}
#endif

#if 0
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

  wocky_xmpp_writer_write_stanza (priv->writer, stanza, &data, &length);

  /* FIXME catch errors harder */
  return g_output_stream_write_all (priv->output_stream, data, length,
    NULL, NULL, NULL);
}

static void
_xmpp_connection_received_data (GObject *source, GAsyncResult *result,
  gpointer user_data)
{
  WockyXmppConnection *self;
  WockyXmppConnectionPrivate *priv;
  WockyXmppStanza *stanza;
  /* gboolean ret; */
  gssize size;
  GError *error = NULL;

  size = g_input_stream_read_finish (G_INPUT_STREAM (source),
    result, &error);

  if (size < 0)
    {
      if (g_error_matches (error,
          G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          return;
        }
      g_error ("connect: %s: %d, %s", g_quark_to_string (error->domain),
        error->code, error->message);
    }

  if (size == 0)
    return;

  self = WOCKY_XMPP_CONNECTION (user_data);
  priv = WOCKY_XMPP_CONNECTION_GET_PRIVATE (self);

  /* Ensure we're not disposed inside while running the reader is busy */
  g_object_ref (self);

  wocky_xmpp_reader_push (priv->reader, priv->buffer, size);

  if (!(self->stream_flags & WOCKY_XMPP_CONNECTION_STREAM_RECEIVED)
      && (wocky_xmpp_reader_get_state (priv->reader) ==
          WOCKY_XMPP_READER_STATE_OPENED))
    {
      gchar *to, *from, *version;

      g_object_get (priv->reader,
        "to", &to,
        "from", &from,
        "version", &version,
        NULL);

      self->stream_flags |= WOCKY_XMPP_CONNECTION_STREAM_RECEIVED;
      g_signal_emit (self, signals[STREAM_OPENED], 0, to, from, version);

      g_free (to);
      g_free (from);
      g_free (version);
    }

  while ((stanza = wocky_xmpp_reader_pop_stanza (priv->reader)) != NULL)
    {
      g_signal_emit (self, signals[RECEIVED_STANZA], 0, stanza);
      g_object_unref (stanza);
    }

  switch (wocky_xmpp_reader_get_state (priv->reader))
    {
      case WOCKY_XMPP_READER_STATE_CLOSED:
        self->stream_flags |= WOCKY_XMPP_CONNECTION_CLOSE_RECEIVED;
        g_signal_emit (self, signals[STREAM_CLOSED], 0);
        break;
      case WOCKY_XMPP_READER_STATE_ERROR:
        g_signal_emit (self, signals[PARSE_ERROR], 0);
        break;
      default:
        //wocky_xmpp_connection_do_read (self);
        break;
    }

  g_object_unref (self);
}
#endif

void
wocky_xmpp_connection_send_open_async (WockyXmppConnection *connection,
    const gchar *to,
    const gchar *from,
    const gchar *version,
    const gchar *lang,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  /* stub */
}

gboolean
wocky_xmpp_connection_send_open_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  /* stub */
  return TRUE;
}

void
wocky_xmpp_connection_recv_open_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  /* stub */
}

gboolean
wocky_xmpp_connection_recv_open_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    const gchar **to,
    const gchar **from,
    const gchar **version,
    const gchar **lang,
    GError **error)
{
  /* stub */
  return TRUE;
}

void
wocky_xmpp_connection_send_stanza_async (WockyXmppConnection *connection,
    WockyXmppStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  /* stub */
}

gboolean
wocky_xmpp_connection_send_stanza_async_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  /* stub */
  return TRUE;
}

void
wocky_xmpp_connection_recv_stanza_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  /* stub */
}

WockyXmppStanza *
wocky_xmpp_connection_recv_stanza_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  /* stub */
  return NULL;
}


void
wocky_xmpp_connection_send_close_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  /* stub */
}

gboolean
wocky_xmpp_connection_send_close_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  /* stub */
  return TRUE;
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
