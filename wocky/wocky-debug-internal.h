#if !defined (WOCKY_COMPILATION)
# error "This is an internal header."
#endif

#ifndef WOCKY_DEBUG_INTERNAL_H
#define WOCKY_DEBUG_INTERNAL_H

#include "config.h"

#include <glib.h>

#include "wocky-debug.h"
#include "wocky-stanza.h"

G_BEGIN_DECLS

#ifdef ENABLE_DEBUG

void wocky_debug_set_flags_from_env (void);
gboolean wocky_debug_flag_is_set (WockyDebugFlags flag);

void wocky_debug_valist (WockyDebugFlags flag,
    const gchar *format, va_list args);

void wocky_debug (WockyDebugFlags flag, const gchar *format, ...)
    G_GNUC_PRINTF (2, 3);
void wocky_debug_stanza (WockyDebugFlags flag, WockyStanza *stanza,
    const gchar *format, ...)
    G_GNUC_PRINTF (3, 4);
void wocky_debug_node_tree (WockyDebugFlags flag, WockyNodeTree *tree,
    const gchar *format, ...)
    G_GNUC_PRINTF (3, 4);
void wocky_debug_node (WockyDebugFlags flag, WockyNode *node,
    const gchar *format, ...)
    G_GNUC_PRINTF (3, 4);

#ifdef WOCKY_DEBUG_FLAG

#define DEBUG(format, ...) \
  wocky_debug (WOCKY_DEBUG_FLAG, "%s: %s: " format, G_STRFUNC, G_STRLOC, \
      ##__VA_ARGS__)

#define DEBUG_STANZA(stanza, format, ...) \
  wocky_debug_stanza (WOCKY_DEBUG_FLAG, stanza, "%s: " format, G_STRFUNC,\
      ##__VA_ARGS__)

#define DEBUG_NODE_TREE(tree, format, ...) \
  wocky_debug_node_tree (WOCKY_DEBUG_FLAG, tree, "%s: " format, G_STRFUNC,\
      ##__VA_ARGS__)

#define DEBUG_NODE(node, format, ...) \
  wocky_debug_node (WOCKY_DEBUG_FLAG, node, "%s: " format, G_STRFUNC,\
      ##__VA_ARGS__)

#define DEBUGGING wocky_debug_flag_is_set(WOCKY_DEBUG_FLAG)

#endif /* WOCKY_DEBUG_FLAG */

#else /* ENABLE_DEBUG */

#ifdef WOCKY_DEBUG_FLAG

static inline void
DEBUG (
    const gchar *format,
    ...)
{
  /* blah blah blah */
}

static inline void
DEBUG_STANZA (WockyStanza *stanza,
    const gchar *format,
    ...)
{
}

static inline void
DEBUG_NODE_TREE (WockyNodeTree *tree,
    const gchar *format,
    ...)
{
}

static inline void
DEBUG_NODE (WockyNode *node,
    const gchar *format,
    ...)
{
}

#define DEBUGGING 0

#endif /* WOCKY_DEBUG_FLAG */

#endif /* ENABLE_DEBUG */

G_END_DECLS

#endif
