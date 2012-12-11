#if !defined (WOCKY_H_INSIDE) && !defined (WOCKY_COMPILATION)
# error "Only <wocky/wocky.h> can be included directly."
#endif

#ifndef __WOCKY_DEBUG_H__
#define __WOCKY_DEBUG_H__

G_BEGIN_DECLS

typedef enum
{
  /*< private > */
  WOCKY_DEBUG_TRANSPORT         = 1 << 0,
  WOCKY_DEBUG_NET               = 1 << 1,
  WOCKY_DEBUG_XMPP_READER       = 1 << 2,
  WOCKY_DEBUG_XMPP_WRITER       = 1 << 3,
  WOCKY_DEBUG_AUTH              = 1 << 4,
  WOCKY_DEBUG_SSL               = 1 << 5,
  WOCKY_DEBUG_RMULTICAST        = 1 << 6,
  WOCKY_DEBUG_RMULTICAST_SENDER = 1 << 7,
  WOCKY_DEBUG_MUC_CONNECTION    = 1 << 8,
  WOCKY_DEBUG_BYTESTREAM        = 1 << 9,
  WOCKY_DEBUG_FILE_TRANSFER     = 1 << 10,
  WOCKY_DEBUG_PORTER            = 1 << 11,
  WOCKY_DEBUG_CONNECTOR         = 1 << 12,
  WOCKY_DEBUG_ROSTER            = 1 << 13,
  WOCKY_DEBUG_TLS               = 1 << 14,
  WOCKY_DEBUG_PUBSUB            = 1 << 15,
  WOCKY_DEBUG_DATA_FORM         = 1 << 16,
  WOCKY_DEBUG_PING              = 1 << 17,
  WOCKY_DEBUG_HEARTBEAT         = 1 << 18,
  WOCKY_DEBUG_PRESENCE          = 1 << 19,
  WOCKY_DEBUG_CONNECTION_FACTORY= 1 << 20,
  WOCKY_DEBUG_JINGLE            = 1 << 21,
} WockyDebugFlags;

#define WOCKY_DEBUG_XMPP (WOCKY_DEBUG_XMPP_READER | WOCKY_DEBUG_XMPP_WRITER)

void wocky_debug_set_flags (WockyDebugFlags flags);

G_END_DECLS

#endif
