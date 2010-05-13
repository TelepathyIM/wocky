
#ifndef __WOCKY_DEBUG_H__
#define __WOCKY_DEBUG_H__

#include "config.h"

#include <glib.h>

#include "wocky-stanza.h"

G_BEGIN_DECLS

#ifdef ENABLE_DEBUG

typedef enum
{
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
} DebugFlags;

#define DEBUG_XMPP (DEBUG_XMPP_READER | DEBUG_XMPP_WRITER)

void wocky_debug_set_flags_from_env (void);
void wocky_debug_set_flags (DebugFlags flags);
gboolean wocky_debug_flag_is_set (DebugFlags flag);
void wocky_debug (DebugFlags flag, const gchar *format, ...)
    G_GNUC_PRINTF (2, 3);
void wocky_debug_stanza (DebugFlags flag, WockyStanza *stanza,
    const gchar *format, ...)
    G_GNUC_PRINTF (3, 4);
void wocky_debug_node_tree (DebugFlags flag, WockyNodeTree *tree,
    const gchar *format, ...)
    G_GNUC_PRINTF (3, 4);

#ifdef DEBUG_FLAG

#define DEBUG(format, ...) \
  wocky_debug (DEBUG_FLAG, "%s: %s: " format, G_STRFUNC, G_STRLOC, \
      ##__VA_ARGS__)

#define DEBUG_STANZA(stanza, format, ...) \
  wocky_debug_stanza (DEBUG_FLAG, stanza, "%s: " format, G_STRFUNC,\
      ##__VA_ARGS__)

#define DEBUG_NODE_TREE(tree, format, ...) \
  wocky_debug_node_tree (DEBUG_FLAG, tree, "%s: " format, G_STRFUNC,\
      ##__VA_ARGS__)

#define DEBUGGING debug_flag_is_set(DEBUG_FLAG)

#endif /* DEBUG_FLAG */

#else /* ENABLE_DEBUG */

#ifdef DEBUG_FLAG

#define DEBUG(format, ...) \
  G_STMT_START { } G_STMT_END

#define DEBUG_STANZA(stanza, format, ...) \
  G_STMT_START { } G_STMT_END

#define DEBUG_NODE_TREE(stanza, format, ...) \
  G_STMT_START { } G_STMT_END

#define DEBUGGING 0

#define NODE_DEBUG(n, s) \
  G_STMT_START { } G_STMT_END

#endif /* DEBUG_FLAG */

#endif /* ENABLE_DEBUG */

G_END_DECLS

#endif
