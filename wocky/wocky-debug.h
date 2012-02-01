#if !defined (WOCKY_COMPILATION)
# error "This is an internal header."
#endif

#ifndef __WOCKY_DEBUG_H__
#define __WOCKY_DEBUG_H__

#include "config.h"

G_BEGIN_DECLS

#ifdef ENABLE_DEBUG

typedef enum
{
  /*< private > */
  DEBUG_TRANSPORT         = 1 << 0,
  DEBUG_NET               = 1 << 1,
  DEBUG_XMPP_READER       = 1 << 2,
  DEBUG_XMPP_WRITER       = 1 << 3,
  DEBUG_AUTH              = 1 << 4,
  DEBUG_SSL               = 1 << 5,
  DEBUG_RMULTICAST        = 1 << 6,
  DEBUG_RMULTICAST_SENDER = 1 << 7,
  DEBUG_MUC_CONNECTION    = 1 << 8,
  DEBUG_BYTESTREAM        = 1 << 9,
  DEBUG_FILE_TRANSFER     = 1 << 10,
  DEBUG_PORTER            = 1 << 11,
  DEBUG_CONNECTOR         = 1 << 12,
  DEBUG_ROSTER            = 1 << 13,
  DEBUG_TLS               = 1 << 14,
  DEBUG_PUBSUB            = 1 << 15,
  DEBUG_DATA_FORM         = 1 << 16,
  DEBUG_PING              = 1 << 17,
  DEBUG_HEARTBEAT         = 1 << 18,
  DEBUG_PRESENCE          = 1 << 19,
  DEBUG_CONNECTION_FACTORY= 1 << 20,
} WockyDebugFlags;

#define DEBUG_XMPP (DEBUG_XMPP_READER | DEBUG_XMPP_WRITER)

void wocky_debug_set_flags (WockyDebugFlags flags);

#else /* ENABLE_DEBUG */

#endif /* ENABLE_DEBUG */

G_END_DECLS

#endif
