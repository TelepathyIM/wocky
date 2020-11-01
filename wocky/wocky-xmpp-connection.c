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

/**
 * SECTION: wocky-xmpp-connection
 * @title: WockyXmppConnection
 * @short_description: Low-level XMPP connection.
 *
 * Sends and receives #WockyStanza<!-- -->s from an underlying #GIOStream.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-xmpp-connection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <gio/gio.h>

#include "wocky-signals-marshal.h"

#include "wocky-xmpp-reader.h"
#include "wocky-xmpp-writer.h"
#include "wocky-stanza.h"
#include "wocky-utils.h"

#define BUFFER_SIZE 1024

static void _xmpp_connection_received_data (GObject *source,
    GAsyncResult *result, gpointer user_data);
static void wocky_xmpp_connection_do_write (WockyXmppConnection *self);

/* properties */
enum
{
  PROP_BASE_STREAM = 1,
};

/* private structure */
struct _WockyXmppConnectionPrivate
{
  gboolean dispose_has_run;
  WockyXmppReader *reader;
  WockyXmppWriter *writer;

  GIOStream *stream;

  /* received open from the input stream */
  gboolean input_open;
  GTask *input_task;
  GCancellable *input_cancellable;

  /* sent open to the output stream */
  gboolean output_open;
  /* sent close to the output stream */
  gboolean output_closed;
  GTask *output_task;
  GCancellable *output_cancellable;

  guint8 input_buffer[BUFFER_SIZE];

  const guint8 *output_buffer;
  gsize offset;
  gsize length;

  GTask *force_close_task;
};

G_DEFINE_TYPE_WITH_CODE (WockyXmppConnection, wocky_xmpp_connection, G_TYPE_OBJECT,
          G_ADD_PRIVATE (WockyXmppConnection))

/**
 * wocky_xmpp_connection_error_quark
 *
 * Get the error quark used by the connection.
 *
 * Returns: the quark for connection errors.
 */
GQuark
wocky_xmpp_connection_error_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("wocky-xmpp-connection-error");

  return quark;
}

static void
wocky_xmpp_connection_init (WockyXmppConnection *self)
{
  WockyXmppConnectionPrivate *priv;

  self->priv = wocky_xmpp_connection_get_instance_private (self);
  priv = self->priv;

  priv->writer = wocky_xmpp_writer_new ();
  priv->reader = wocky_xmpp_reader_new ();
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
      connection->priv;

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
      connection->priv;

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

  object_class->set_property = wocky_xmpp_connection_set_property;
  object_class->get_property = wocky_xmpp_connection_get_property;
  object_class->dispose = wocky_xmpp_connection_dispose;
  object_class->finalize = wocky_xmpp_connection_finalize;

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
      self->priv;

  if (priv->dispose_has_run)
    return;

  g_warn_if_fail (priv->input_task == NULL);
  g_warn_if_fail (priv->output_task == NULL);

  priv->dispose_has_run = TRUE;

  g_clear_object (&(priv->stream));
  g_clear_object (&(priv->reader));
  g_clear_object (&(priv->writer));
  g_clear_object (&(priv->output_task));
  g_clear_object (&(priv->output_cancellable));
  g_clear_object (&(priv->input_task));
  g_clear_object (&(priv->input_cancellable));

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->dispose (object);
}

void
wocky_xmpp_connection_finalize (GObject *object)
{
  G_OBJECT_CLASS (wocky_xmpp_connection_parent_class)->finalize (object);
}

/**
 * wocky_xmpp_connection_new:
 * @stream: GIOStream over wich all the data will be sent/received.
 *
 * Convenience function to create a new #WockyXmppConnection.
 *
 * Returns: a new #WockyXmppConnection.
 */
WockyXmppConnection *
wocky_xmpp_connection_new (GIOStream *stream)
{
  WockyXmppConnection * result;

  result = g_object_new (WOCKY_TYPE_XMPP_CONNECTION,
    "base-stream", stream, NULL);

  return result;
}

static void
wocky_xmpp_connection_write_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (user_data);
  WockyXmppConnectionPrivate *priv =
      self->priv;
  gssize written;
  GError *error = NULL;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source), res,
      &error);

  if (G_UNLIKELY (written < 0))
    goto finished;

  if (G_UNLIKELY (written == 0))
    {
      g_clear_error (&error);
      error = g_error_new_literal (WOCKY_XMPP_CONNECTION_ERROR,
          WOCKY_XMPP_CONNECTION_ERROR_EOS, "Connection got disconnected" );
      goto finished;
    }

  priv->offset += written;

  if (priv->offset == priv->length)
    {
      /* Done ! */
      goto finished;
    }

  wocky_xmpp_connection_do_write (self);
  return;

finished:
  {
    GTask *t = priv->output_task;

    if (priv->output_cancellable != NULL)
      g_object_unref (priv->output_cancellable);

    priv->output_cancellable = NULL;
    priv->output_task = NULL;

    if (error == NULL)
      g_task_return_boolean (t, TRUE);
    else
      g_task_return_error (t, error);

    g_object_unref (t);
  }
}

static void
wocky_xmpp_connection_do_write (WockyXmppConnection *self)
{
  WockyXmppConnectionPrivate *priv =
      self->priv;
  GOutputStream *output = g_io_stream_get_output_stream (priv->stream);

  g_assert (priv->length != priv->offset);

  g_output_stream_write_async (output,
    priv->output_buffer + priv->offset,
    priv->length - priv->offset,
    G_PRIORITY_DEFAULT,
    priv->output_cancellable,
    wocky_xmpp_connection_write_cb,
    self);
}

/**
 * wocky_xmpp_connection_send_open_async:
 * @connection: a #WockyXmppConnection.
 * @to: destination in the XMPP opening (can be NULL).
 * @from: sender in the XMPP opening (can be NULL).
 * @version: XMPP version sent (can be NULL).
 * @lang: language sent (can be NULL).
 * @id: XMPP Stream ID, if any, or NULL
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Request asynchronous sending of an XMPP stream opening over the stream. When
 * the operation is finished @callback will be called. You can then call
 * wocky_xmpp_connection_send_open_finish() to get the result of the operation.
 *
 */
void
wocky_xmpp_connection_send_open_async (WockyXmppConnection *connection,
    const gchar *to,
    const gchar *from,
    const gchar *version,
    const gchar *lang,
    const gchar *id,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
      connection->priv;

  if (G_UNLIKELY (priv->output_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_open_async, G_IO_ERROR,
          G_IO_ERROR_PENDING, "Another send operation is pending");
      return;
    }

  if (G_UNLIKELY (priv->output_closed))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_open_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
          "Connection is already open");
      return;
    }

  if (G_UNLIKELY (priv->output_open))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_open_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_OPEN,
          "Connection is closed for sending");
      return;
    }

  g_assert (priv->output_task == NULL);
  g_assert (priv->output_cancellable == NULL);

  priv->output_task = g_task_new (G_OBJECT (connection), cancellable,
    callback, user_data);

  if (cancellable != NULL)
    priv->output_cancellable = g_object_ref (cancellable);

  priv->offset = 0;
  priv->length = 0;

  wocky_xmpp_writer_stream_open (priv->writer,
      to, from, version, lang, id, &priv->output_buffer, &priv->length);

  wocky_xmpp_connection_do_write (connection);

  return;
}

/**
 * wocky_xmpp_connection_send_open_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes sending a stream opening.
 *
 * Returns: TRUE if the opening was succesfully sent, FALSE on error.
 */
gboolean
wocky_xmpp_connection_send_open_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  WockyXmppConnectionPrivate *priv = connection->priv;

  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return FALSE;

  priv->output_open = TRUE;

  return TRUE;
}

static void
wocky_xmpp_connection_do_read (WockyXmppConnection *self)
{
  WockyXmppConnectionPrivate *priv =
      self->priv;
  GInputStream *input = g_io_stream_get_input_stream (priv->stream);

  g_input_stream_read_async (input,
    priv->input_buffer, BUFFER_SIZE,
    G_PRIORITY_DEFAULT,
    priv->input_cancellable,
    _xmpp_connection_received_data,
    self);
}

static gboolean
input_is_closed (WockyXmppConnection *self)
{
  WockyXmppConnectionPrivate *priv = self->priv;

  return wocky_xmpp_reader_get_state (priv->reader) >
    WOCKY_XMPP_READER_STATE_OPENED;
}

static void
_xmpp_connection_received_data (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WockyXmppConnection *self = WOCKY_XMPP_CONNECTION (user_data);
  WockyXmppConnectionPrivate *priv = self->priv;
  gssize size;
  GError *error = NULL;

  size = g_input_stream_read_finish (G_INPUT_STREAM (source),
    result, &error);

  if (G_UNLIKELY (size < 0))
    goto finished;

  if (G_UNLIKELY (size == 0))
    {
      g_clear_error (&error);
      error = g_error_new_literal (WOCKY_XMPP_CONNECTION_ERROR,
          WOCKY_XMPP_CONNECTION_ERROR_EOS,
          "Connection got disconnected" );
      goto finished;
    }

  wocky_xmpp_reader_push (priv->reader, priv->input_buffer, size);

  if (!priv->input_open &&
      (wocky_xmpp_reader_get_state (priv->reader) ==
          WOCKY_XMPP_READER_STATE_OPENED))
    {
      /* stream was opened, can only be as a result of calling recv_open */
      priv->input_open = TRUE;
      goto finished;
    }

  if (wocky_xmpp_reader_peek_stanza (priv->reader) != NULL)
    goto finished;

  switch (wocky_xmpp_reader_get_state (priv->reader))
    {
      case WOCKY_XMPP_READER_STATE_CLOSED:
      case WOCKY_XMPP_READER_STATE_ERROR:
        goto finished;
      default:
        /* Need more data */
        break;
    }

  wocky_xmpp_connection_do_read (self);

  return;

finished:
  {
    GTask *t = priv->input_task;

    if (priv->input_cancellable != NULL)
      g_object_unref (priv->input_cancellable);

    priv->input_cancellable = NULL;
    priv->input_task = NULL;

    if (error == NULL)
      g_task_return_boolean (t, TRUE);
    else
      g_task_return_error (t, error);

    g_object_unref (t);
  }
}

/**
 * wocky_xmpp_connection_recv_open_async:
 * @connection: a #WockyXmppConnection.
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Request asynchronous receiving of an XMPP stream opening over the stream.
 * When the operation is finished @callback will be called. You can then call
 * wocky_xmpp_connection_recv_open_finish() to get the result of the operation.
 *
 */
void
wocky_xmpp_connection_recv_open_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
    connection->priv;

  if (G_UNLIKELY (priv->input_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_recv_open_async, G_IO_ERROR,
          G_IO_ERROR_PENDING, "Another receive operation is pending");
      return;
    }

  if (G_UNLIKELY (input_is_closed (connection)))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_recv_open_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
          "Connection is closed for receiving");
      return;
    }

  if (G_UNLIKELY (priv->input_open))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_recv_open_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_OPEN,
          "Connection has already received open");
      return;
    }

  g_assert (priv->input_task == NULL);
  g_assert (priv->input_cancellable == NULL);

  priv->input_task = g_task_new (G_OBJECT (connection), cancellable,
    callback, user_data);

  if (cancellable != NULL)
    priv->input_cancellable = g_object_ref (cancellable);

  wocky_xmpp_connection_do_read (connection);

  return;
}

/**
 * wocky_xmpp_connection_recv_open_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @to: Optional location to store the to attribute in the XMPP open stanza
 *  will be stored (free after usage).
 * @from: Optional location to store the from attribute in the XMPP open stanza
 *  will be stored (free after usage).
 * @version: Optional location to store the version attribute in the XMPP open
 *  stanza will be stored (free after usage).
 * @lang: Optional location to store the lang attribute in the XMPP open
 *  stanza will be stored (free after usage).
 * @id: Optional location to store the Session ID of the XMPP stream
 *  (free after usage)
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes receiving a stream opening.
 *
 * Returns: TRUE if the opening was succesfully received, FALSE on error.
 */
gboolean
wocky_xmpp_connection_recv_open_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    gchar **to,
    gchar **from,
    gchar **version,
    gchar **lang,
    gchar **id,
    GError **error)
{
  WockyXmppConnectionPrivate *priv;

  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return FALSE;

  priv = connection->priv;
  priv->input_open = TRUE;

  if (to != NULL)
    g_object_get (priv->reader, "to", to, NULL);

  if (from != NULL)
    g_object_get (priv->reader, "from", from, NULL);

  if (version != NULL)
    g_object_get (priv->reader, "version", version, NULL);

  if (lang != NULL)
    g_object_get (priv->reader, "lang", lang, NULL);

  if (id != NULL)
    g_object_get (priv->reader, "id", id, NULL);

  return TRUE;
}

/**
 * wocky_xmpp_connection_send_stanza_async:
 * @connection: a #WockyXmppConnection
 * @stanza: #WockyStanza to send.
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Request asynchronous sending of a #WockyStanza. When the operation is
 * finished @callback will be called. You can then call
 * wocky_xmpp_connection_send_stanza_finish() to get the result of
 * the operation.
 *
 * Can only be called after wocky_xmpp_connection_send_open_async has finished
 * its operation.
 *
 */
void
wocky_xmpp_connection_send_stanza_async (WockyXmppConnection *connection,
    WockyStanza *stanza,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
      connection->priv;

  if (G_UNLIKELY (priv->output_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_stanza_async, G_IO_ERROR,
          G_IO_ERROR_PENDING, "Another send operation is pending");
      return;
    }


  if (G_UNLIKELY (!priv->output_open))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_stanza_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN,
          "Connections hasn't been opened for sending");
      return;
    }

  if (G_UNLIKELY (priv->output_closed))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_stanza_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
          "Connections has been closed for sending");
      return;
    }

  g_assert (!priv->output_closed);
  g_assert (priv->output_task == NULL);
  g_assert (priv->output_cancellable == NULL);

  priv->output_task = g_task_new (G_OBJECT (connection), cancellable,
      callback, user_data);

  if (cancellable != NULL)
    priv->output_cancellable = g_object_ref (cancellable);
  priv->offset = 0;
  priv->length = 0;

  wocky_xmpp_writer_write_stanza (priv->writer, stanza, &priv->output_buffer,
      &priv->length);

  wocky_xmpp_connection_do_write (connection);

  return;
}

/**
 * wocky_xmpp_connection_send_stanza_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes sending a stanza.
 *
 * Returns: TRUE if the stanza was succesfully sent, FALSE on error.
 */
gboolean
wocky_xmpp_connection_send_stanza_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * wocky_xmpp_connection_recv_stanza_async:
 * @connection: a #WockyXmppConnection
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Asynchronous receive a #WockyStanza. When the operation is
 * finished @callback will be called. You can then call
 * wocky_xmpp_connection_recv_stanza_finish() to get the result of
 * the operation.
 *
 * Can only be called after wocky_xmpp_connection_recv_open_async has finished
 * its operation.
 */
void
wocky_xmpp_connection_recv_stanza_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
    connection->priv;

  if (G_UNLIKELY (priv->input_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_recv_stanza_async, G_IO_ERROR,
          G_IO_ERROR_PENDING, "Another receive operation is pending");
      return;
    }

  if (G_UNLIKELY (!priv->input_open))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_recv_stanza_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN,
          "Connection hasn't been opened for reading stanzas");
      return;
    }

  if (G_UNLIKELY (input_is_closed (connection)))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_recv_stanza_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
          "Connection has been closed for reading stanzas");
      return;
    }

  g_assert (priv->input_task == NULL);
  g_assert (priv->input_cancellable == NULL);

  priv->input_task = g_task_new (G_OBJECT (connection), cancellable,
    callback, user_data);

  /* There is already a stanza waiting, no need to read */
  if (wocky_xmpp_reader_peek_stanza (priv->reader) != NULL)
    {
      GTask *t = priv->input_task;

      priv->input_task = NULL;

      g_task_return_boolean (t, TRUE);
      g_object_unref (t);
      return;
    }

  if (cancellable != NULL)
    priv->input_cancellable = g_object_ref (cancellable);

  wocky_xmpp_connection_do_read (connection);
  return;
}

static WockyStanza *
retrieve_stanza (WockyXmppConnection *connection,
    WockyStanza *(*getter)(WockyXmppReader *),
    GError **error)
{
  WockyXmppConnectionPrivate *priv;
  WockyStanza *stanza = NULL;

  priv = connection->priv;

  switch (wocky_xmpp_reader_get_state (priv->reader))
    {
      case WOCKY_XMPP_READER_STATE_INITIAL:
        g_assert_not_reached ();
        break;
      case WOCKY_XMPP_READER_STATE_OPENED:
        stanza = getter (priv->reader);
        break;
      case WOCKY_XMPP_READER_STATE_CLOSED:
        g_set_error_literal (error, WOCKY_XMPP_CONNECTION_ERROR,
          WOCKY_XMPP_CONNECTION_ERROR_CLOSED,
          "Stream closed");
        break;
      case WOCKY_XMPP_READER_STATE_ERROR:
        {
          GError *e /* default coding style checker */;

          e = wocky_xmpp_reader_get_error (priv->reader);

          g_assert (e != NULL);

          g_propagate_error (error, e);

          break;
        }
    }

  return stanza;
}

/**
 * wocky_xmpp_connection_peek_stanza_async:
 * @connection: a #WockyXmppConnection
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Asynchronous receive a #WockyStanza. When the operation is
 * finished @callback will be called. You can then call
 * wocky_xmpp_connection_peek_stanza_finish() to get the result of
 * the operation.
 *
 * Can only be called after wocky_xmpp_connection_peek_open_async has finished
 * its operation.
 */
void
wocky_xmpp_connection_peek_stanza_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  wocky_xmpp_connection_recv_stanza_async (connection, cancellable, callback,
      user_data);
}

/**
 * wocky_xmpp_connection_peek_stanza_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes receiving a stanza
 *
 * Returns: A #WockyStanza or NULL on error (transfer none)
 */

const WockyStanza *
wocky_xmpp_connection_peek_stanza_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), NULL);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return NULL;

  return retrieve_stanza (connection, wocky_xmpp_reader_peek_stanza, error);
}

/**
 * wocky_xmpp_connection_recv_stanza_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes receiving a stanza
 *
 * Returns: A #WockyStanza or NULL on error (unref after usage)
 */

WockyStanza *
wocky_xmpp_connection_recv_stanza_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), NULL);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return NULL;

  return retrieve_stanza (connection, wocky_xmpp_reader_pop_stanza, error);
}

/**
 * wocky_xmpp_connection_send_close_async:
 * @connection: a #WockyXmppConnection.
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Request asynchronous sending of an XMPP stream close. When
 * the operation is finished @callback will be called. You can then call
 * wocky_xmpp_connection_send_close_finish() to get the result of the
 * operation.
 *
 * Can only be called after wocky_xmpp_connection_send_open_async has finished
 * its operation.
 *
 */
void
wocky_xmpp_connection_send_close_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
      connection->priv;

  if (G_UNLIKELY (priv->output_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_close_async, G_IO_ERROR,
          G_IO_ERROR_PENDING, "Another send operation is pending");
      return;
    }

  if (G_UNLIKELY (priv->output_closed))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_close_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
          "Connections has been closed sending");
      return;
    }

  if (G_UNLIKELY (!priv->output_open))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_close_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN,
          "Connections hasn't been opened for sending");
      return;
    }

  g_assert (priv->output_task == NULL);
  g_assert (priv->output_cancellable == NULL);

  priv->output_task = g_task_new (G_OBJECT (connection), cancellable,
    callback, user_data);

  if (cancellable != NULL)
    priv->output_cancellable = g_object_ref (cancellable);

  priv->offset = 0;
  priv->length = 0;

  wocky_xmpp_writer_stream_close (priv->writer, &priv->output_buffer,
      &priv->length);

  wocky_xmpp_connection_do_write (connection);

  return;
}

/**
 * wocky_xmpp_connection_send_close_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes send the xmpp stream close.
 *
 * Returns: TRUE on success or FALSE on error.
 */
gboolean
wocky_xmpp_connection_send_close_finish (WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  WockyXmppConnectionPrivate *priv = connection->priv;

  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return FALSE;

  priv->output_closed = TRUE;

  return TRUE;
}

/**
 * wocky_xmpp_connection_send_whitespace_ping_async:
 * @connection: a #WockyXmppConnection
 * @cancellable: optional GCancellable object, NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Request asynchronous sending of a whitespace ping. When the operation is
 * finished @callback will be called. You can then call
 * wocky_xmpp_connection_send_whitespace_ping_finish() to get the result of
 * the operation.
 *
 * Can only be called after wocky_xmpp_connection_send_open_async has finished
 * its operation.
 */
void
wocky_xmpp_connection_send_whitespace_ping_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
      connection->priv;

  if (G_UNLIKELY (priv->output_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_whitespace_ping_async, G_IO_ERROR,
          G_IO_ERROR_PENDING, "Another send operation is pending");
      return;
    }

  if (G_UNLIKELY (!priv->output_open))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_whitespace_ping_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_NOT_OPEN,
          "Connections hasn't been opened for sending");
      return;
    }

  if (G_UNLIKELY (priv->output_closed))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_send_whitespace_ping_async,
          WOCKY_XMPP_CONNECTION_ERROR, WOCKY_XMPP_CONNECTION_ERROR_IS_CLOSED,
          "Connections has been closed for sending");
      return;
    }

  g_assert (!priv->output_closed);
  g_assert (priv->output_task == NULL);
  g_assert (priv->output_cancellable == NULL);

  priv->output_task = g_task_new (G_OBJECT (connection), cancellable,
      callback, user_data);

  if (cancellable != NULL)
    priv->output_cancellable = g_object_ref (cancellable);

  priv->output_buffer = (guint8 *) " ";
  priv->length = 1;
  priv->offset = 0;

  wocky_xmpp_connection_do_write (connection);

  return;
}

/**
 * wocky_xmpp_connection_send_whitespace_ping_finish:
 * @connection: a #WockyXmppConnection.
 * @result: a GAsyncResult.
 * @error: a GError location to store the error occuring, or NULL to ignore.
 *
 * Finishes sending a whitespace ping.
 *
 * Returns: TRUE if the ping was succesfully sent, FALSE on error.
 */
gboolean
wocky_xmpp_connection_send_whitespace_ping_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * wocky_xmpp_connection_reset:
 * @connection: a #WockyXmppConnection.
 *
 * Reset the XMPP Connection. After the reset the connection is back in its
 * initial state (as if wocky_xmpp_connection_send_open_async() and
 * wocky_xmpp_connection_recv_open_async() were never called).
 */
void
wocky_xmpp_connection_reset (WockyXmppConnection *connection)
{
  WockyXmppConnectionPrivate *priv =
    connection->priv;

  /* There can't be any pending operations */
  g_assert (priv->input_task == NULL);
  g_assert (priv->output_task == NULL);

  priv->input_open = FALSE;

  priv->output_open = FALSE;
  priv->output_closed = FALSE;

  wocky_xmpp_reader_reset (priv->reader);
}

/**
 * wocky_xmpp_connection_new_id:
 * @self: a #WockyXmppConnection.
 *
 * Returns: A short unique string for usage as the id attribute on a stanza
 * (free after usage).
 */
gchar *
wocky_xmpp_connection_new_id (WockyXmppConnection *self)
{
  return g_uuid_string_random ();
}

static void
stream_close_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  WockyXmppConnection *connection = WOCKY_XMPP_CONNECTION (user_data);
  WockyXmppConnectionPrivate *priv =
    connection->priv;
  GError *error = NULL;
  GTask *t = priv->force_close_task;

  if (!g_io_stream_close_finish (G_IO_STREAM (source), res, &error))
    g_task_return_error (t, error);
  else
    g_task_return_boolean (t, TRUE);

  priv->force_close_task = NULL;
  g_object_unref (t);
}

void
wocky_xmpp_connection_force_close_async (WockyXmppConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyXmppConnectionPrivate *priv =
    connection->priv;

  if (G_UNLIKELY (priv->force_close_task != NULL))
    {
      g_task_report_new_error (G_OBJECT (connection), callback, user_data,
          wocky_xmpp_connection_force_close_async,
          G_IO_ERROR, G_IO_ERROR_PENDING, "Another close operation is pending");
      return;
    }

  priv->force_close_task = g_task_new (G_OBJECT (connection), cancellable,
      callback, user_data);

  g_io_stream_close_async (priv->stream, G_PRIORITY_HIGH, cancellable,
      stream_close_cb, connection);
}

gboolean
wocky_xmpp_connection_force_close_finish (
    WockyXmppConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
