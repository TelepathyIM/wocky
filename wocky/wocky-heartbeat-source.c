/*
 * wocky-heartbeat-source.c: a GSource wrapping libiphb.
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk>
 * Copyright © 2010 Nokia Corporation
 * @author Will Thompson <will.thompson@collabora.co.uk>
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

#include "wocky-heartbeat-source.h"

#include <errno.h>

#define DEBUG_FLAG DEBUG_HEARTBEAT
#include "wocky-debug.h"

#ifdef HAVE_IPHB
# include <iphbd/libiphb.h>
#endif

typedef struct _WockyHeartbeatSource {
    GSource parent;

#ifdef HAVE_IPHB
    iphb_t heartbeat;
    GPollFD fd;
#endif

    guint min_interval;
    guint max_interval;
} WockyHeartbeatSource;

static void
wocky_heartbeat_source_finalize (GSource *source)
{
#ifdef HAVE_IPHB
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

  if (self->heartbeat != NULL)
    {
      g_source_remove_poll ((GSource *) self, &self->fd);

      iphb_close (self->heartbeat);
      self->heartbeat = NULL;
    }
#endif
}

static gboolean
wocky_heartbeat_source_prepare (
    GSource *source,
    gint *timeout_out_out_out)
{
  /* FIXME: be smarter. We can figure out when we might be woken up. */
  return FALSE;
}

static gboolean
wocky_heartbeat_source_check (
    GSource *source)
{
#ifdef HAVE_IPHB
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

  if (self->heartbeat == NULL)
    {
      return FALSE;
    }
  else if ((self->fd.revents & (G_IO_ERR | G_IO_HUP)) != 0)
    {
      DEBUG ("Heartbeat closed unexpectedly: %hu", self->fd.revents);
      g_source_remove_poll (source, &self->fd);
      return FALSE;
    }
  else if ((self->fd.revents & G_IO_IN) != 0)
    {
      DEBUG ("Heartbeat fired");
      return TRUE;
    }
  else
    {
      return FALSE;
    }
#else
  return FALSE;
#endif
}

static gboolean
wocky_heartbeat_source_dispatch (
    GSource *source,
    GSourceFunc callback,
    gpointer user_data)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

#if HAVE_IPHB
  if (self->heartbeat == NULL)
    return FALSE;
#endif

  if (callback == NULL)
    {
      g_warning ("No callback set for WockyHeartbeatSource %p", self);
      return FALSE;
    }

  /* Call our callback. We don't currently allow callbacks to stop future
   * heartbeats from occurring: this source is used for keepalives from the
   * time we're connected until we disconnect.
   */
  ((WockyHeartbeatCallback) callback) (user_data);

#if HAVE_IPHB
  if (iphb_wait (self->heartbeat, self->min_interval, self->max_interval, 0)
          == -1)
    {
      DEBUG ("iphb_wait failed: %s", g_strerror (errno));
      wocky_heartbeat_source_finalize (source);
      return FALSE;
    }
#endif

  return TRUE;
}

static GSourceFuncs wocky_heartbeat_source_funcs = {
    wocky_heartbeat_source_prepare,
    wocky_heartbeat_source_check,
    wocky_heartbeat_source_dispatch,
    wocky_heartbeat_source_finalize,
    NULL,
    NULL
};

GSource *
wocky_heartbeat_source_new (
    guint min_interval,
    guint max_interval)
{
  GSource *source = g_source_new (&wocky_heartbeat_source_funcs,
      sizeof (WockyHeartbeatSource));
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

#if HAVE_IPHB
  iphb_t heartbeat = iphb_open (NULL);

  if (heartbeat == NULL)
    {
      DEBUG ("Couldn't open connection to heartbeat service: %s",
          g_strerror (errno));
      return NULL;
    }

  /* We initially wait for anywhere between (0, max_interval) rather than
   * (min_interval, max_interval) to fall into step with other connections,
   * which may have started waiting at slightly different times.
   */
  if (iphb_wait (heartbeat, 0, max_interval, 0) == -1)
    {
      DEBUG ("Initial call to iphb_wait failed: %s", g_strerror (errno));
      iphb_close (heartbeat);
      return NULL;
    }
#endif

#if HAVE_IPHB
  self->heartbeat = heartbeat;
  self->fd.fd = iphb_get_fd (heartbeat);
  self->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
  g_source_add_poll (source, &self->fd);
#endif

  self->min_interval = min_interval;
  self->max_interval = max_interval;

  return source;
}
