/*
 * wocky-test-stream.c - Source for WockyTestStream
 * Copyright (C) 2009 Collabora Ltd.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wocky-test-stream.h"

G_DEFINE_TYPE (WockyTestStream, wocky_test_stream, G_TYPE_OBJECT);

struct _WockyTestStreamPrivate {
  gboolean dispose_has_run;
};

enum {
  PROP_IO_INPUT_STREAM = 1,
  PROP_IO_OUTPUT_STREAM
};

static GType wocky_test_io_stream_get_type (void);
static GType wocky_test_input_stream_get_type (void);
static GType wocky_test_output_stream_get_type (void);

typedef struct {
  GIOStream parent;
  GInputStream *input;
  GOutputStream *output;
} WockyTestIOStream;

typedef struct {
  GIOStreamClass parent;
} WockyTestIOStreamClass;

typedef struct {
  GOutputStream parent;
  GAsyncQueue *queue;
  WockyTestStreamWriteMode mode;
  GError *write_error /* no, this is not a coding style violation */;
  gboolean dispose_has_run;
} WockyTestOutputStream;

typedef struct {
  GOutputStreamClass parent_class;
} WockyTestOutputStreamClass;

typedef struct {
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
  WockyTestStreamReadMode mode;
  WockyTestStreamDirectReadCb direct_read_cb;
  gpointer direct_read_user_data;
  gboolean corked;
} WockyTestInputStream;

typedef struct {
  GOutputStreamClass parent_class;
} WockyTestInputStreamClass;


G_DEFINE_TYPE (WockyTestIOStream, wocky_test_io_stream, G_TYPE_IO_STREAM);
G_DEFINE_TYPE (WockyTestInputStream, wocky_test_input_stream,
  G_TYPE_INPUT_STREAM);
G_DEFINE_TYPE (WockyTestOutputStream, wocky_test_output_stream,
  G_TYPE_OUTPUT_STREAM);

#define WOCKY_TYPE_TEST_IO_STREAM (wocky_test_io_stream_get_type ())
#define WOCKY_TYPE_TEST_INPUT_STREAM (wocky_test_input_stream_get_type ())
#define WOCKY_TYPE_TEST_OUTPUT_STREAM (wocky_test_output_stream_get_type ())

#define WOCKY_TEST_IO_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
  WOCKY_TYPE_TEST_IO_STREAM,                                             \
  WockyTestIOStream))

#define WOCKY_TEST_INPUT_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
  WOCKY_TYPE_TEST_INPUT_STREAM,                                             \
  WockyTestInputStream))

#define WOCKY_TEST_OUTPUT_STREAM(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
  WOCKY_TYPE_TEST_OUTPUT_STREAM,                                             \
  WockyTestOutputStream))

static gboolean wocky_test_input_stream_try_read (WockyTestInputStream *self);

static void
output_data_written_cb (GOutputStream *output,
    WockyTestInputStream *input_stream)
{
  wocky_test_input_stream_try_read (input_stream);
}

static void
wocky_test_stream_init (WockyTestStream *self)
{
  /* allocate any data required by the object here */
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, WOCKY_TYPE_TEST_STREAM,
    WockyTestStreamPrivate);

  self->stream0_output = g_object_new (WOCKY_TYPE_TEST_OUTPUT_STREAM, NULL);

  self->stream1_output = g_object_new (WOCKY_TYPE_TEST_OUTPUT_STREAM, NULL);

  self->stream0_input = g_object_new (WOCKY_TYPE_TEST_INPUT_STREAM, NULL);
  WOCKY_TEST_INPUT_STREAM (self->stream0_input)->queue =
    g_async_queue_ref (
      WOCKY_TEST_OUTPUT_STREAM (self->stream1_output)->queue);

  self->stream1_input = g_object_new (WOCKY_TYPE_TEST_INPUT_STREAM, NULL);
  WOCKY_TEST_INPUT_STREAM (self->stream1_input)->queue =
    g_async_queue_ref (
      WOCKY_TEST_OUTPUT_STREAM (self->stream0_output)->queue);

  self->stream0 = g_object_new (WOCKY_TYPE_TEST_IO_STREAM, NULL);
  WOCKY_TEST_IO_STREAM (self->stream0)->input = self->stream0_input;
  WOCKY_TEST_IO_STREAM (self->stream0)->output = self->stream0_output;

  self->stream1 = g_object_new (WOCKY_TYPE_TEST_IO_STREAM, NULL);
  WOCKY_TEST_IO_STREAM (self->stream1)->input = self->stream1_input;
  WOCKY_TEST_IO_STREAM (self->stream1)->output = self->stream1_output;

  g_signal_connect (self->stream0_output, "data-written",
      G_CALLBACK (output_data_written_cb), self->stream1_input);
  g_signal_connect (self->stream1_output, "data-written",
      G_CALLBACK (output_data_written_cb), self->stream0_input);
}

static void wocky_test_stream_dispose (GObject *object);

static void
wocky_test_stream_class_init (WockyTestStreamClass *wocky_test_stream_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_test_stream_class);

  g_type_class_add_private (wocky_test_stream_class, \
    sizeof (WockyTestStreamPrivate));

  object_class->dispose = wocky_test_stream_dispose;
}

static void
wocky_test_stream_dispose (GObject *object)
{
  WockyTestStream *self = WOCKY_TEST_STREAM (object);

  if (self->priv->dispose_has_run)
    return;

  self->priv->dispose_has_run = TRUE;

  g_object_unref (self->stream0);
  self->stream0 = NULL;

  g_object_unref (self->stream1);
  self->stream1 = NULL;

  if (G_OBJECT_CLASS (wocky_test_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_stream_parent_class)->dispose (object);
}

/* IO stream */
static void
wocky_test_io_stream_init (WockyTestIOStream *self)
{
}

static void
wocky_test_io_stream_class_get_property (GObject *object, guint property_id,
  GValue *value, GParamSpec *pspec)
{
  WockyTestIOStream *self = WOCKY_TEST_IO_STREAM (object);

  switch (property_id)
      {
        case PROP_IO_INPUT_STREAM:
          g_value_set_object (value, self->input);
          break;
        case PROP_IO_OUTPUT_STREAM:
          g_value_set_object (value, self->output);
          break;
        default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      }
}

static void
wocky_test_io_stream_finalize (GObject *object)
{
  WockyTestIOStream *self = WOCKY_TEST_IO_STREAM (object);

  g_object_unref (self->input);
  g_object_unref (self->output);

  if (G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->finalize)
    G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->finalize (object);
}

static GInputStream *
wocky_test_io_stream_get_input_stream (GIOStream *stream)
{
  return WOCKY_TEST_IO_STREAM (stream)->input;
}

static GOutputStream *
wocky_test_io_stream_get_output_stream (GIOStream *stream)
{
  return WOCKY_TEST_IO_STREAM (stream)->output;
}

static void
wocky_test_io_stream_class_init (
  WockyTestIOStreamClass *wocky_test_io_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_test_io_stream_class);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS
    (wocky_test_io_stream_class);

  obj_class->finalize = wocky_test_io_stream_finalize;
  obj_class->get_property = wocky_test_io_stream_class_get_property;

  stream_class->get_input_stream = wocky_test_io_stream_get_input_stream;
  stream_class->get_output_stream = wocky_test_io_stream_get_output_stream;

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

/* Input stream */
static gssize
wocky_test_input_stream_read (GInputStream *stream, void *buffer, gsize count,
  GCancellable *cancellable, GError  **error)
{
  WockyTestInputStream *self = WOCKY_TEST_INPUT_STREAM (stream);
  gsize written = 0;

  if (self->out_array == NULL)
    {
      g_assert (self->offset == 0);
      self->out_array = g_async_queue_pop (self->queue);
    }

  do {
    gsize towrite;

    if (self->mode == WOCK_TEST_STREAM_READ_COMBINE_SLICE && self->offset == 0)
      towrite = MIN (count - written, MAX (self->out_array->len/2, 1));
    else
      towrite = MIN (count - written, self->out_array->len - self->offset);

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
    else if (self->mode == WOCK_TEST_STREAM_READ_COMBINE_SLICE)
      {
        break;
      }
  } while (self->mode != WOCK_TEST_STREAM_READ_EXACT
    && written < count && self->out_array != NULL);

  if (self->direct_read_cb != NULL)
    self->direct_read_cb (buffer, written, self->direct_read_user_data);

  return written;
}

static void
read_async_complete (WockyTestInputStream *self)
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
    WockyTestInputStream *self)
{
  g_simple_async_result_set_error (self->read_result,
      G_IO_ERROR, G_IO_ERROR_CANCELLED, "Reading cancelled");

  self->buffer = NULL;
  read_async_complete (self);
}

static void
wocky_test_input_stream_read_async (GInputStream *stream,
    void *buffer,
    gsize count,
    int io_priority,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  WockyTestInputStream *self = WOCKY_TEST_INPUT_STREAM (stream);

  g_assert (self->buffer == NULL);
  g_assert (self->read_result == NULL);
  g_assert (self->read_cancellable == NULL);

  self->buffer = buffer;
  self->count = count;

  self->read_result = g_simple_async_result_new (G_OBJECT (stream),
      callback, user_data, wocky_test_input_stream_read_async);

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

  wocky_test_input_stream_try_read (self);
}

static gssize
wocky_test_input_stream_read_finish (GInputStream *stream,
    GAsyncResult *result,
    GError **error)
{
  WockyTestInputStream *self = WOCKY_TEST_INPUT_STREAM (stream);
  gssize len = -1;

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    goto out;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), wocky_test_input_stream_read_async), -1);

  len = wocky_test_input_stream_read (stream, self->buffer, self->count, NULL,
      error);

out:
  self->buffer = NULL;

  return len;
}

static gboolean
wocky_test_input_stream_try_read (WockyTestInputStream *self)
{
  if (self->read_result == NULL)
    /* No pending read operation */
    return FALSE;

  if (self->out_array == NULL && g_async_queue_length (self->queue) == 0)
    return FALSE;

  if (self->corked)
    return FALSE;

  read_async_complete (self);
  return TRUE;
}

static void
wocky_test_input_stream_init (WockyTestInputStream *self)
{
}

static void
wocky_test_input_stream_dispose (GObject *object)
{
  WockyTestInputStream *self = WOCKY_TEST_INPUT_STREAM (object);

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
  if (G_OBJECT_CLASS (wocky_test_input_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_input_stream_parent_class)->dispose (object);
}

static void
wocky_test_input_stream_class_init (
  WockyTestInputStreamClass *wocky_test_input_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_test_input_stream_class);
  GInputStreamClass *stream_class =
    G_INPUT_STREAM_CLASS (wocky_test_input_stream_class);

  obj_class->dispose = wocky_test_input_stream_dispose;
  stream_class->read_fn = wocky_test_input_stream_read;
  stream_class->read_async = wocky_test_input_stream_read_async;
  stream_class->read_finish = wocky_test_input_stream_read_finish;
}

/* Output stream */
enum
{
    OUTPUT_DATA_WRITTEN,
    LAST_SIGNAL
};

static guint output_signals[LAST_SIGNAL] = {0};

static gssize
wocky_test_output_stream_write (GOutputStream *stream, const void *buffer,
  gsize count, GCancellable *cancellable, GError **error)
{
  WockyTestOutputStream *self = WOCKY_TEST_OUTPUT_STREAM (stream);
  GArray *data;
  gsize written = count;

  if (self->mode == WOCKY_TEST_STREAM_WRITE_INCOMPLETE)
    written = MAX (count/2, 1);

  if (self->write_error != NULL)
    {
      *error = self->write_error;
      self->write_error = NULL;
      return -1;
    }

  data = g_array_sized_new (FALSE, FALSE, sizeof (guint8), written);

  g_array_insert_vals (data, 0, buffer, written);

  g_async_queue_push (self->queue, data);
  g_signal_emit (self, output_signals[OUTPUT_DATA_WRITTEN], 0);

  return written;
}

static void
wocky_test_output_stream_write_async (GOutputStream *stream,
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

  result = wocky_test_output_stream_write (stream, buffer, count, cancellable,
    &error);

  simple = g_simple_async_result_new (G_OBJECT (stream), callback, user_data,
    wocky_test_output_stream_write_async);

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
wocky_test_output_stream_write_finish (GOutputStream *stream,
  GAsyncResult *result,
  GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return -1;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (stream), wocky_test_output_stream_write_async), -1);

  return g_simple_async_result_get_op_res_gssize (
    G_SIMPLE_ASYNC_RESULT (result));
}

static void
wocky_test_output_stream_dispose (GObject *object)
{
  WockyTestOutputStream *self = WOCKY_TEST_OUTPUT_STREAM (object);

  if (self->dispose_has_run)
    return;

  self->dispose_has_run = TRUE;

  g_async_queue_push (self->queue,
    g_array_sized_new (FALSE, FALSE, sizeof (guint8), 0));
  g_async_queue_unref (self->queue);

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_test_output_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_output_stream_parent_class)->dispose (object);
}

static void
queue_destroyed (gpointer data)
{
  g_array_free ((GArray *) data, TRUE);
}

static void
wocky_test_output_stream_init (WockyTestOutputStream *self)
{
  self->queue = g_async_queue_new_full (queue_destroyed);
}

static void
wocky_test_output_stream_class_init (
  WockyTestOutputStreamClass *wocky_test_output_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_test_output_stream_class);
  GOutputStreamClass *stream_class =
    G_OUTPUT_STREAM_CLASS (wocky_test_output_stream_class);

  obj_class->dispose = wocky_test_output_stream_dispose;

  stream_class->write_fn = wocky_test_output_stream_write;
  stream_class->write_async = wocky_test_output_stream_write_async;
  stream_class->write_finish = wocky_test_output_stream_write_finish;

  output_signals[OUTPUT_DATA_WRITTEN] = g_signal_new ("data-written",
      G_OBJECT_CLASS_TYPE(wocky_test_output_stream_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

void
wocky_test_input_stream_set_read_error (GInputStream *stream)
{
  WockyTestInputStream *self = WOCKY_TEST_INPUT_STREAM (stream);

  if (self->read_result == NULL)
    {
      /* No pending read operation. Set the error so next read will fail */
      self->read_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
          "read error");
      return;
    }

  g_simple_async_result_set_error (self->read_result, G_IO_ERROR,
          G_IO_ERROR_FAILED, "read error");
  read_async_complete (self);
}

void
wocky_test_output_stream_set_write_error (GOutputStream *stream)
{
  WockyTestOutputStream *self = WOCKY_TEST_OUTPUT_STREAM (stream);

   self->write_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
       "write error");
}

void
wocky_test_stream_set_mode (GInputStream *stream,
  WockyTestStreamReadMode mode)
{
  WOCKY_TEST_INPUT_STREAM (stream)->mode = mode;
}

void
wocky_test_stream_cork (GInputStream *stream,
  gboolean cork)
{
  WockyTestInputStream *tstream = WOCKY_TEST_INPUT_STREAM (stream);
  tstream->corked = cork;

  if (cork == FALSE)
    wocky_test_input_stream_try_read (tstream);

}

void
wocky_test_stream_set_direct_read_callback (GInputStream *stream,
  WockyTestStreamDirectReadCb cb,
  gpointer user_data)
{
  WockyTestInputStream *tstream = WOCKY_TEST_INPUT_STREAM (stream);

  tstream->direct_read_cb = cb;
  tstream->direct_read_user_data = user_data;
}

void
wocky_test_stream_set_write_mode (GOutputStream *stream,
  WockyTestStreamWriteMode mode)
{
  WOCKY_TEST_OUTPUT_STREAM (stream)->mode = mode;
}
