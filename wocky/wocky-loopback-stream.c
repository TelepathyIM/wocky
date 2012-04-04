/*
 * wocky-loopback-stream.c - Source for WockyLoopbackStream
 * Copyright (C) 2009-2011 Collabora Ltd.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wocky-loopback-stream.h"

enum {
  PROP_IO_INPUT_STREAM = 1,
  PROP_IO_OUTPUT_STREAM
};

static GType wocky_loopback_input_stream_get_type (void);
static GType wocky_loopback_output_stream_get_type (void);

struct _WockyLoopbackStreamPrivate
{
  GInputStream *input;
  GOutputStream *output;
};

typedef struct
{
  GOutputStream parent;
  GAsyncQueue *queue;
  GError *write_error /* no, this is not a coding style violation */;
  gboolean dispose_has_run;
} WockyLoopbackOutputStream;

typedef struct
{
  GOutputStreamClass parent_class;
} WockyLoopbackOutputStreamClass;

typedef struct
{
  GInputStream parent;
  GAsyncQueue *queue;
  guint offset;
  GArray *out_array;
  GSimpleAsyncResult *read_result;
  GCancellable *read_cancellable;
  gulong read_cancellable_sig_id;
  void *buffer;
  gsize count;
  GError *read_error /* no, this is not a coding style violation */;
  gboolean dispose_has_run;
} WockyLoopbackInputStream;

typedef struct
{
  GOutputStreamClass parent_class;
} WockyLoopbackInputStreamClass;


G_DEFINE_TYPE (WockyLoopbackStream, wocky_loopback_stream,
    G_TYPE_IO_STREAM);
G_DEFINE_TYPE (WockyLoopbackInputStream, wocky_loopback_input_stream,
  G_TYPE_INPUT_STREAM);
G_DEFINE_TYPE (WockyLoopbackOutputStream, wocky_loopback_output_stream,
  G_TYPE_OUTPUT_STREAM);

#define WOCKY_TYPE_LOOPBACK_INPUT_STREAM (wocky_loopback_input_stream_get_type ())
#define WOCKY_TYPE_LOOPBACK_OUTPUT_STREAM (wocky_loopback_output_stream_get_type ())

#define WOCKY_LOOPBACK_INPUT_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
  WOCKY_TYPE_LOOPBACK_INPUT_STREAM,                                             \
  WockyLoopbackInputStream))

#define WOCKY_LOOPBACK_OUTPUT_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
  WOCKY_TYPE_LOOPBACK_OUTPUT_STREAM,                                             \
  WockyLoopbackOutputStream))

static gboolean wocky_loopback_input_stream_try_read (WockyLoopbackInputStream *self);

static void
output_data_written_cb (GOutputStream *output,
    WockyLoopbackInputStream *input_stream)
{
  wocky_loopback_input_stream_try_read (input_stream);
}

/* connection */
static void
wocky_loopback_stream_init (WockyLoopbackStream *self)
{
  WockyLoopbackStreamPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_LOOPBACK_STREAM,
      WockyLoopbackStreamPrivate);
  priv = self->priv;

  priv->output = g_object_new (WOCKY_TYPE_LOOPBACK_OUTPUT_STREAM, NULL);

  priv->input = g_object_new (WOCKY_TYPE_LOOPBACK_INPUT_STREAM, NULL);
  WOCKY_LOOPBACK_INPUT_STREAM (priv->input)->queue =
    g_async_queue_ref (
        WOCKY_LOOPBACK_OUTPUT_STREAM (priv->output)->queue);

  g_signal_connect (priv->output, "data-written",
      G_CALLBACK (output_data_written_cb), priv->input);
}

static void
wocky_loopback_stream_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyLoopbackStream *self = WOCKY_LOOPBACK_STREAM (object);
  WockyLoopbackStreamPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_IO_INPUT_STREAM:
        g_value_set_object (value, priv->input);
        break;
      case PROP_IO_OUTPUT_STREAM:
        g_value_set_object (value, priv->output);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wocky_loopback_stream_dispose (GObject *object)
{
  WockyLoopbackStream *self = WOCKY_LOOPBACK_STREAM (object);
  WockyLoopbackStreamPrivate *priv = self->priv;

  if (G_OBJECT_CLASS (wocky_loopback_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_loopback_stream_parent_class)->dispose (object);

  g_object_unref (priv->input);
  g_object_unref (priv->output);
}

static GInputStream *
wocky_loopback_stream_get_input_stream (GIOStream *stream)
{
  return WOCKY_LOOPBACK_STREAM (stream)->priv->input;
}

static GOutputStream *
wocky_loopback_stream_get_output_stream (GIOStream *stream)
{
  return WOCKY_LOOPBACK_STREAM (stream)->priv->output;
}

static void
wocky_loopback_stream_class_init (
    WockyLoopbackStreamClass *wocky_loopback_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_loopback_stream_class);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (
      wocky_loopback_stream_class);

  g_type_class_add_private (wocky_loopback_stream_class,
      sizeof (WockyLoopbackStreamPrivate));

  obj_class->dispose = wocky_loopback_stream_dispose;
  obj_class->get_property = wocky_loopback_stream_get_property;

  stream_class->get_input_stream = wocky_loopback_stream_get_input_stream;
  stream_class->get_output_stream = wocky_loopback_stream_get_output_stream;

  g_object_class_install_property (obj_class, PROP_IO_INPUT_STREAM,
    g_param_spec_object ("input-stream", "Input stream",
      "the input stream",
      G_TYPE_INPUT_STREAM,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_IO_OUTPUT_STREAM,
    g_param_spec_object ("output-stream", "Output stream", "the output stream",
      G_TYPE_OUTPUT_STREAM,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

GIOStream *
wocky_loopback_stream_new (void)
{
  return g_object_new (WOCKY_TYPE_LOOPBACK_STREAM, NULL);
}

/* Input stream */
static gssize
wocky_loopback_input_stream_read (GInputStream *stream,
    void *buffer,
    gsize count,
    GCancellable *cancellable,
    GError **error)
{
  WockyLoopbackInputStream *self = WOCKY_LOOPBACK_INPUT_STREAM (stream);
  gsize written = 0;

  if (self->out_array == NULL)
    {
      g_assert (self->offset == 0);
      self->out_array = g_async_queue_pop (self->queue);
    }

  do
    {
      gsize towrite;

      if (self->offset == 0)
        {
          towrite = MIN (count - written, MAX (self->out_array->len/2, 1));
        }
      else
        {
          towrite = MIN (count - written, self->out_array->len - self->offset);
        }

      memcpy ((guchar *) buffer + written,
          self->out_array->data + self->offset,
          towrite);

      self->offset += towrite;
      written += towrite;

      if (self->offset == self->out_array->len)
        {
          g_array_unref (self->out_array);
          self->out_array = g_async_queue_try_pop (self->queue);
          self->offset = 0;
        }
      else
        {
          break;
        }
    }
  while (written < count && self->out_array != NULL);

  return written;
}

static void
read_async_complete (WockyLoopbackInputStream *self)
{
  GSimpleAsyncResult *r = self->read_result;

  if (self->read_cancellable != NULL)
    {
      g_signal_handler_disconnect (self->read_cancellable,
          self->read_cancellable_sig_id);
      g_object_unref (self->read_cancellable);
      self->read_cancellable = NULL;
    }

  self->read_result = NULL;

  g_simple_async_result_complete_in_idle (r);
  g_object_unref (r);
}

static void
read_cancelled_cb (GCancellable *cancellable,
    WockyLoopbackInputStream *self)
{
  g_simple_async_result_set_error (self->read_result,
      G_IO_ERROR, G_IO_ERROR_CANCELLED, "Reading cancelled");

  self->buffer = NULL;
  read_async_complete (self);
}

static void
wocky_loopback_input_stream_read_async (GInputStream *stream,
    void *buffer,
    gsize count,
    int io_priority,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyLoopbackInputStream *self = WOCKY_LOOPBACK_INPUT_STREAM (stream);

  g_assert (self->buffer == NULL);
  g_assert (self->read_result == NULL);
  g_assert (self->read_cancellable == NULL);

  self->buffer = buffer;
  self->count = count;

  self->read_result = g_simple_async_result_new (G_OBJECT (stream),
      callback, user_data, wocky_loopback_input_stream_read_async);

  if (self->read_error != NULL)
    {
      g_simple_async_result_set_from_error (self->read_result,
          self->read_error);

      g_error_free (self->read_error);
      self->read_error = NULL;
      read_async_complete (self);
      return;
    }

  if (cancellable != NULL)
    {
      self->read_cancellable = g_object_ref (cancellable);
      self->read_cancellable_sig_id = g_signal_connect (cancellable,
          "cancelled", G_CALLBACK (read_cancelled_cb), self);
    }

  wocky_loopback_input_stream_try_read (self);
}

static gssize
wocky_loopback_input_stream_read_finish (GInputStream *stream,
    GAsyncResult *result,
    GError **error)
{
  WockyLoopbackInputStream *self = WOCKY_LOOPBACK_INPUT_STREAM (stream);
  gssize len = -1;

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    goto out;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), wocky_loopback_input_stream_read_async), -1);

  len = wocky_loopback_input_stream_read (stream, self->buffer, self->count, NULL,
      error);

 out:
  self->buffer = NULL;

  return len;
}

static gboolean
wocky_loopback_input_stream_try_read (WockyLoopbackInputStream *self)
{
  if (self->read_result == NULL)
    /* No pending read operation */
    return FALSE;

  if (self->out_array == NULL
      && g_async_queue_length (self->queue) == 0)
    return FALSE;

  read_async_complete (self);
  return TRUE;
}

static void
wocky_loopback_input_stream_init (WockyLoopbackInputStream *self)
{
}

static void
wocky_loopback_input_stream_dispose (GObject *object)
{
  WockyLoopbackInputStream *self = WOCKY_LOOPBACK_INPUT_STREAM (object);

  if (self->dispose_has_run)
    return;

  self->dispose_has_run = TRUE;

  if (self->out_array != NULL)
    g_array_unref (self->out_array);
  self->out_array = NULL;

  if (self->queue != NULL)
    g_async_queue_unref (self->queue);
  self->queue = NULL;

  g_warn_if_fail (self->read_result == NULL);
  g_warn_if_fail (self->read_cancellable == NULL);

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_loopback_input_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_loopback_input_stream_parent_class)->dispose (object);
}

static void
wocky_loopback_input_stream_class_init (
    WockyLoopbackInputStreamClass *wocky_loopback_input_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_loopback_input_stream_class);
  GInputStreamClass *stream_class =
    G_INPUT_STREAM_CLASS (wocky_loopback_input_stream_class);

  obj_class->dispose = wocky_loopback_input_stream_dispose;
  stream_class->read_fn = wocky_loopback_input_stream_read;
  stream_class->read_async = wocky_loopback_input_stream_read_async;
  stream_class->read_finish = wocky_loopback_input_stream_read_finish;
}

/* Output stream */
enum
{
  OUTPUT_DATA_WRITTEN,
  LAST_SIGNAL
};

static guint output_signals[LAST_SIGNAL] = {0};

static gssize
wocky_loopback_output_stream_write (GOutputStream *stream,
    const void *buffer,
    gsize count,
    GCancellable *cancellable,
    GError **error)
{
  WockyLoopbackOutputStream *self = WOCKY_LOOPBACK_OUTPUT_STREAM (stream);
  GArray *data;

  data = g_array_sized_new (FALSE, FALSE, sizeof (guint8), count);

  g_array_insert_vals (data, 0, buffer, count);

  g_async_queue_push (self->queue, data);
  g_signal_emit (self, output_signals[OUTPUT_DATA_WRITTEN], 0);

  return count;
}

static void
wocky_loopback_output_stream_write_async (GOutputStream *stream,
    const void *buffer,
    gsize count,
    int io_priority,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  GError *error = NULL;
  gssize result;

  result = wocky_loopback_output_stream_write (stream, buffer, count, cancellable,
      &error);

  simple = g_simple_async_result_new (G_OBJECT (stream), callback, user_data,
      wocky_loopback_output_stream_write_async);

  if (result == -1)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gssize (simple, result);
    }

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static gssize
wocky_loopback_output_stream_write_finish (GOutputStream *stream,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return -1;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (stream), wocky_loopback_output_stream_write_async), -1);

  return g_simple_async_result_get_op_res_gssize (
      G_SIMPLE_ASYNC_RESULT (result));
}

static void
wocky_loopback_output_stream_dispose (GObject *object)
{
  WockyLoopbackOutputStream *self = WOCKY_LOOPBACK_OUTPUT_STREAM (object);

  if (self->dispose_has_run)
    return;

  self->dispose_has_run = TRUE;

  g_async_queue_push (self->queue,
      g_array_sized_new (FALSE, FALSE, sizeof (guint8), 0));
  g_async_queue_unref (self->queue);

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_loopback_output_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_loopback_output_stream_parent_class)->dispose (object);
}

static void
queue_destroyed (gpointer data)
{
  g_array_free ((GArray *) data, TRUE);
}

static void
wocky_loopback_output_stream_init (WockyLoopbackOutputStream *self)
{
  self->queue = g_async_queue_new_full (queue_destroyed);
}

static void
wocky_loopback_output_stream_class_init (
    WockyLoopbackOutputStreamClass *wocky_loopback_output_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_loopback_output_stream_class);
  GOutputStreamClass *stream_class =
    G_OUTPUT_STREAM_CLASS (wocky_loopback_output_stream_class);

  obj_class->dispose = wocky_loopback_output_stream_dispose;

  stream_class->write_fn = wocky_loopback_output_stream_write;
  stream_class->write_async = wocky_loopback_output_stream_write_async;
  stream_class->write_finish = wocky_loopback_output_stream_write_finish;

  output_signals[OUTPUT_DATA_WRITTEN] = g_signal_new ("data-written",
      G_OBJECT_CLASS_TYPE(wocky_loopback_output_stream_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}
