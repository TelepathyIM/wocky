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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-heartbeat-source.h"

#include <errno.h>

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_HEARTBEAT
#include "wocky-debug-internal.h"

#ifdef HAVE_IPHB
# include <iphbd/libiphb.h>
#endif

typedef struct _WockyHeartbeatSource {
    GSource parent;

#ifdef HAVE_IPHB
    iphb_t heartbeat;
    GPollFD fd;
#endif

    guint max_interval;

    gint64 next_wakeup;
} WockyHeartbeatSource;

#if HAVE_IPHB
static void
wocky_heartbeat_source_degrade (WockyHeartbeatSource *self)
{
  /* If we were using the heartbeat before, stop using it. */
  if (self->heartbeat != NULL)
    {
      GSource *source = (GSource *) self;

      /* If this is being called from wocky_heartbeat_source_finalize(), the
       * source has been destroyed (which implicitly removes all polls.
       */
      if (!g_source_is_destroyed (source))
        g_source_remove_poll (source, &self->fd);

      DEBUG ("closing heartbeat connection");
      iphb_close (self->heartbeat);
      self->heartbeat = NULL;
    }
}

static guint
recommended_intervals[] = {
    IPHB_GS_WAIT_10_HOURS,
    IPHB_GS_WAIT_2_HOURS,
    IPHB_GS_WAIT_1_HOUR,
    IPHB_GS_WAIT_30_MINS,
    IPHB_GS_WAIT_10_MINS * 2, /* It aligns with the 1 hour slot. */
    IPHB_GS_WAIT_10_MINS,
    IPHB_GS_WAIT_5_MINS,
    IPHB_GS_WAIT_2_5_MINS,
    IPHB_GS_WAIT_30_SEC};

static guint
get_system_sync_interval (guint max_interval)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (recommended_intervals); i++)
    {
      if (recommended_intervals[i] <= max_interval)
        return recommended_intervals[i];
    }

  return max_interval;
}

static void
wocky_heartbeat_source_wait (
    WockyHeartbeatSource *self,
    guint max_interval)
{
  guint interval;
  int ret;

  if (self->heartbeat == NULL)
    return;

  if (max_interval > 0)
    {
      /* Passing the same minimum and maximum interval to iphb_wait() means
       * that the iphb daemon will wake us up when its internal time is a
       * multiple of the interval.
       * By using recommended intervals across the platform we can get
       * multiple processes waken up at the same time. */
      interval = get_system_sync_interval (max_interval);
      DEBUG ("requested %u as maximum interval; using the recommended %u "
          "interval", max_interval, interval);
      ret = iphb_wait (self->heartbeat, interval, interval, 0);
    }
  else
    {
      ret = iphb_I_woke_up (self->heartbeat);
    }

  if (ret == -1)
    {
      DEBUG ("waiting %u failed: %s; falling back to internal timeouts",
          max_interval, g_strerror (errno));
      wocky_heartbeat_source_degrade (self);
    }
}
#endif

static gboolean
wocky_heartbeat_source_prepare (
    GSource *source,
    gint *msec_to_poll)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;
  gint64 now;

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

  if (self->max_interval == 0)
    return FALSE;

  now = g_source_get_time (source);

  /* If now > self->next_wakeup, it's already time to wake up. */
  if (now > self->next_wakeup)
    {
      DEBUG ("ready to wake up (at %" G_GINT64_FORMAT ")", now);
      return TRUE;
    }

  /* Otherwise, we should only go back to sleep for a period of
   * (self->next_wakeup - now). Inconveniently, g_source_get_time() gives us µs
   * but we need to return ms; hence the scaling.
   *
   * The value calculated here will always be positive. The difference in
   * seconds is non-negative; if it's zero, the difference in microseconds is
   * positive.
   */
  *msec_to_poll = (self->next_wakeup - now) / 1000;

  return FALSE;
}

static gboolean
wocky_heartbeat_source_check (
    GSource *source)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;
  gint64 now;

#ifdef HAVE_IPHB
  if (self->heartbeat != NULL)
    {
      if ((self->fd.revents & (G_IO_ERR | G_IO_HUP)) != 0)
        {
          DEBUG ("Heartbeat closed unexpectedly: %hu; "
              "falling back to internal timeouts", self->fd.revents);
          wocky_heartbeat_source_degrade (self);
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

  if (self->max_interval == 0)
    return FALSE;

  now = g_source_get_time (source);

  return (now > self->next_wakeup);
}

#if HAVE_IPHB
static inline guint
get_min_interval (
    WockyHeartbeatSource *self)
{
  /* We allow the heartbeat service to wake us up up to a minute early. */
  return self->max_interval > 60 ? self->max_interval - 60 : 0;
}
#endif

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
  if (DEBUGGING)
    {
      gint64 now;

      now = g_source_get_time (source);
      DEBUG ("calling %p (%p) at %" G_GINT64_FORMAT, callback, user_data, now);
    }

  ((WockyHeartbeatCallback) callback) (user_data);

#if HAVE_IPHB
  wocky_heartbeat_source_wait (self, self->max_interval);
#endif

  /* Record the time we next want to wake up. */
  self->next_wakeup = g_source_get_time (source);
  self->next_wakeup += self->max_interval * G_USEC_PER_SEC;
  DEBUG ("next wakeup at %" G_GINT64_FORMAT, self->next_wakeup);

  return TRUE;
}

static void
wocky_heartbeat_source_finalize (GSource *source)
{
#ifdef HAVE_IPHB
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

  wocky_heartbeat_source_degrade (self);
#endif
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

  wocky_heartbeat_source_wait (self, self->max_interval);
}
#endif

/**
 * wocky_heartbeat_source_new:
 * @max_interval: the maximum interval between calls to the source's callback,
 *                in seconds. Pass 0 to prevent the callback being called.
 *
 * Creates a source which calls its callback at least every @max_interval
 * seconds. This is similar to g_timeout_source_new_seconds(), except that the
 * callback may be called slightly earlier than requested, in sync with other
 * periodic network activity (from other XMPP connections, or other
 * applications entirely).
 *
 * When calling g_source_set_callback() on this source, the supplied callback's
 * signature should match #WockyHeartbeatCallback.
 *
 * Returns: the newly-created source.
 */
GSource *
wocky_heartbeat_source_new (
    guint max_interval)
{
  GSource *source = g_source_new (&wocky_heartbeat_source_funcs,
      sizeof (WockyHeartbeatSource));
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

  /* We can't just call wocky_heartbeat_source_update_interval() because it
   * assumes that we're attached to a main context. I think this is probably a
   * reasonable assumption.
   */
  self->max_interval = max_interval;

  self->next_wakeup = g_get_monotonic_time ();
  self->next_wakeup += max_interval * G_USEC_PER_SEC;

#if HAVE_IPHB
  connect_to_heartbeat (self);
#endif

  return source;
}

/**
 * wocky_heartbeat_source_update_interval:
 * @source: a source returned by wocky_heartbeat_source_new()
 * @max_interval: the new maximum interval between calls to the source's
 *                callback, in seconds. Pass 0 to stop the callback being
 *                called.
 *
 * Updates the interval between calls to @source's callback. The new interval
 * may not take effect until after the next call to the callback.
 */
void
wocky_heartbeat_source_update_interval (
    GSource *source,
    guint max_interval)
{
  WockyHeartbeatSource *self = (WockyHeartbeatSource *) source;

  if (self->max_interval == max_interval)
    return;

  /* If we're not using the heartbeat, the new interval takes effect
   * immediately.
   *
   * If we are, we just wait for the next heartbeat to fire as
   * normal, and then use these new values when we ask it to wait again.
   * (Except if the heartbeat was previously disabled, or is being disabled, in
   * which case we have to be sure to schedule a wakeup, or cancel the pending
   * wakeup, respectively.)
   *
   * We could alternatively calculate the time already elapsed since we last
   * called iphb_wait(), and from that calculate how much longer we want to
   * wait with these new values, taking care to deal with the cases where one
   * or both of min_interval and max_interval have already passed. But life is
   * too short.
   */

#ifdef HAVE_IPHB
  /* We specify 0 as the lower bound here to give us a better chance of falling
   * into step with other connections, which may have started waiting at
   * slightly different times.
   */
  if (self->max_interval == 0 || max_interval == 0)
    wocky_heartbeat_source_wait (self, max_interval);
#endif

  /* If we were previously disabled, we need to re-initialize next_wakeup, not
   * just update it.
   */
  if (self->max_interval == 0)
    self->next_wakeup = g_source_get_time (source);

  /* If this moves self->next_wakeup into the past, then we'll wake up ASAP,
   * which is what we want.
   */
  self->next_wakeup += (max_interval - self->max_interval) * G_USEC_PER_SEC;
  self->max_interval = max_interval;

  if (self->max_interval == 0)
    DEBUG ("heartbeat disabled");
  else
    DEBUG ("next wakeup at or before %" G_GINT64_FORMAT, self->next_wakeup);
}
