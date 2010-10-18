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

    GTimeVal next_wakeup;
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

#if HAVE_IPHB
static void
wocky_heartbeat_source_wait (
    WockyHeartbeatSource *self,
    guint min_interval,
    guint max_interval)
{
  if (self->heartbeat != NULL &&
      iphb_wait (self->heartbeat, min_interval, max_interval, 0)
          == -1)
    {
      DEBUG ("iphb_wait failed: %s; falling back to internal timeouts",
          g_strerror (errno));
      wocky_heartbeat_source_finalize ((GSource *) self);
    }
}
#endif

static gboolean
wocky_heartbeat_source_prepare (
    GSource *source,
    gint *msec_to_poll)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;
  GTimeVal now;

#if HAVE_IPHB
  /* If we're listening to the system heartbeat, always rely on it to wake us
   * up.
   */
  if (self->heartbeat != NULL)
    {
      *msec_to_poll = -1;
      return FALSE;
    }
#endif

  g_source_get_current_time (source, &now);

  /* If now > self->next_wakeup, it's already time to wake up. */
  if (now.tv_sec > self->next_wakeup.tv_sec ||
      (now.tv_sec == self->next_wakeup.tv_sec &&
       now.tv_usec >= self->next_wakeup.tv_usec))
    return TRUE;

  /* Otherwise, we should only go back to sleep for a period of
   * (self->next_wakeup - now). Inconveniently, GTimeVal gives us µs but we
   * need to return ms; hence the scaling.
   *
   * The value calculated here will always be positive. The difference in
   * seconds is non-negative; if it's zero, the difference in microseconds is
   * positive.
   */
  *msec_to_poll = (self->next_wakeup.tv_sec - now.tv_sec) * 1000
      + (self->next_wakeup.tv_usec - now.tv_usec) / 1000;

  return FALSE;
}

static gboolean
wocky_heartbeat_source_check (
    GSource *source)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;
  GTimeVal now;

#ifdef HAVE_IPHB
  if (self->heartbeat != NULL)
    {
      if ((self->fd.revents & (G_IO_ERR | G_IO_HUP)) != 0)
        {
          DEBUG ("Heartbeat closed unexpectedly: %hu; "
              "falling back to internal timeouts", self->fd.revents);
          wocky_heartbeat_source_finalize (source);
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
    }
#endif

  g_source_get_current_time (source, &now);

  return (now.tv_sec > self->next_wakeup.tv_sec ||
      (now.tv_sec == self->next_wakeup.tv_sec &&
       now.tv_usec >= self->next_wakeup.tv_usec));
}

static gboolean
wocky_heartbeat_source_dispatch (
    GSource *source,
    GSourceFunc callback,
    gpointer user_data)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

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
  wocky_heartbeat_source_wait (self, self->min_interval, self->max_interval);
#endif

  /* Record the time we next want to wake up. */
  g_source_get_current_time (source, &self->next_wakeup);
  self->next_wakeup.tv_sec += self->max_interval;

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

#if HAVE_IPHB
static void
connect_to_heartbeat (
    WockyHeartbeatSource *self)
{
  GSource *source = (GSource *) self;

  self->heartbeat = iphb_open (NULL);

  if (self->heartbeat == NULL)
    {
      DEBUG ("Couldn't open connection to heartbeat service: %s",
          g_strerror (errno));
      return;
    }

  self->fd.fd = iphb_get_fd (self->heartbeat);
  self->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
  g_source_add_poll (source, &self->fd);

  /* We initially wait for anywhere between (0, max_interval) rather than
   * (min_interval, max_interval) to fall into step with other connections,
   * which may have started waiting at slightly different times.
   */
  wocky_heartbeat_source_wait (self, 0, self->max_interval);
}
#endif

GSource *
wocky_heartbeat_source_new (
    guint min_interval,
    guint max_interval)
{
  GSource *source = g_source_new (&wocky_heartbeat_source_funcs,
      sizeof (WockyHeartbeatSource));
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

  self->min_interval = min_interval;
  self->max_interval = max_interval;

  g_get_current_time (&self->next_wakeup);
  self->next_wakeup.tv_sec += max_interval;

#if HAVE_IPHB
  connect_to_heartbeat (self);
#endif

  return source;
}
