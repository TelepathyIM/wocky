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
  GObject parent;
  GInputStream *input;
  GOutputStream *output;
  gboolean dispose_has_run;
} WockyTestIOStream;

typedef struct {
  GObjectClass parent;
  gboolean dispose_has_run;
} WockyTestIOStreamClass;

typedef struct {
  GOutputStream parent;
  GAsyncQueue *queue;
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
  gboolean dispose_has_run;
} WockyTestInputStream;

typedef struct {
  GOutputStreamClass parent_class;
} WockyTestInputStreamClass;


G_DEFINE_TYPE_WITH_CODE (WockyTestIOStream, wocky_test_io_stream,
  G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (G_TYPE_IO_STREAM, NULL));
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
}

static void wocky_test_stream_dispose (GObject *object);
static void wocky_test_stream_finalize (GObject *object);

static void
wocky_test_stream_class_init (WockyTestStreamClass *wocky_test_stream_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (wocky_test_stream_class);

  g_type_class_add_private (wocky_test_stream_class, \
    sizeof (WockyTestStreamPrivate));

  object_class->dispose = wocky_test_stream_dispose;
  object_class->finalize = wocky_test_stream_finalize;
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

  g_object_unref (self->stream0_input);
  self->stream0_input = NULL;

  g_object_unref (self->stream0_output);
  self->stream0_output = NULL;

  g_object_unref (self->stream1_input);
  self->stream1_input = NULL;

  g_object_unref (self->stream1_output);
  self->stream1_output = NULL;

  if (G_OBJECT_CLASS (wocky_test_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_stream_parent_class)->dispose (object);
}

static void
wocky_test_stream_finalize (GObject *object)
{
  /* free any data held directly by the object here */
  G_OBJECT_CLASS (wocky_test_stream_parent_class)->finalize (object);
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
wocky_test_io_stream_dispose (GObject *object)
{
  WockyTestIOStream *self = WOCKY_TEST_IO_STREAM (object);

  if (self->dispose_has_run)
    return;

  self->dispose_has_run = TRUE;

  self->output = NULL;
  self->input = NULL;

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->dispose (object);
}

static void
wocky_test_io_stream_class_init (
  WockyTestIOStreamClass *wocky_test_io_stream_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (wocky_test_io_stream_class);

  obj_class->dispose = wocky_test_io_stream_dispose;
  obj_class->get_property = wocky_test_io_stream_class_get_property;

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
  gssize len;

  if (self->out_array == NULL)
    {
      g_assert (self->offset == 0);
      self->out_array = g_async_queue_pop (self->queue);
    }

  len = MIN (self->out_array->len - self->offset, count);
  memcpy (buffer, self->out_array->data, len);
  self->offset += len;

  if (self->offset == self->out_array->len)
    {
      g_array_free (self->out_array, TRUE);
      self->out_array = NULL;
      self->offset = 0;
    }

  return len;
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
    g_array_free (self->out_array, TRUE);
  self->out_array = NULL;

  if (self->queue != NULL)
    g_async_queue_unref (self->queue);
  self->queue = NULL;

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->dispose (object);
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
}

/* Output stream */
static gssize
wocky_test_output_stream_write (GOutputStream *stream, const void *buffer,
  gsize count, GCancellable *cancellable, GError **error)
{
  WockyTestOutputStream *self = WOCKY_TEST_OUTPUT_STREAM (stream);
  GArray *data = g_array_sized_new (FALSE, FALSE, sizeof (guint8), count);

  g_array_insert_vals (data, 0, buffer, count);

  g_async_queue_push (self->queue, data);

  return count;
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
  if (G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->dispose)
    G_OBJECT_CLASS (wocky_test_io_stream_parent_class)->dispose (object);
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
}

