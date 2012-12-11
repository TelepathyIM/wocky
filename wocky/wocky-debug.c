#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include <glib.h>

#include "wocky-debug-internal.h"

#ifdef ENABLE_DEBUG

static WockyDebugFlags flags = 0;
static gboolean initialized = FALSE;

static GDebugKey keys[] = {
  { "transport",         WOCKY_DEBUG_TRANSPORT         },
  { "net",               WOCKY_DEBUG_NET               },
  { "xmpp",              WOCKY_DEBUG_XMPP              },
  { "xmpp-reader",       WOCKY_DEBUG_XMPP_READER       },
  { "xmpp-writer",       WOCKY_DEBUG_XMPP_WRITER       },
  { "auth",              WOCKY_DEBUG_AUTH              },
  { "ssl",               WOCKY_DEBUG_SSL               },
  { "rmulticast",        WOCKY_DEBUG_RMULTICAST        },
  { "rmulticast-sender", WOCKY_DEBUG_RMULTICAST_SENDER },
  { "muc-connection",    WOCKY_DEBUG_MUC_CONNECTION    },
  { "bytestream",        WOCKY_DEBUG_BYTESTREAM        },
  { "ft",                WOCKY_DEBUG_FILE_TRANSFER     },
  { "porter",            WOCKY_DEBUG_PORTER            },
  { "connector",         WOCKY_DEBUG_CONNECTOR         },
  { "roster",            WOCKY_DEBUG_ROSTER            },
  { "tls",               WOCKY_DEBUG_TLS               },
  { "pubsub",            WOCKY_DEBUG_PUBSUB            },
  { "dataform",          WOCKY_DEBUG_DATA_FORM         },
  { "ping",              WOCKY_DEBUG_PING              },
  { "heartbeat",         WOCKY_DEBUG_HEARTBEAT         },
  { "presence",          WOCKY_DEBUG_PRESENCE          },
  { "connection-factory",WOCKY_DEBUG_CONNECTION_FACTORY},
  { "media",             WOCKY_DEBUG_JINGLE            },
  { 0, },
};

void wocky_debug_set_flags_from_env ()
{
  guint nkeys;
  const gchar *flags_string;

  for (nkeys = 0; keys[nkeys].value; nkeys++);

  flags_string = g_getenv ("WOCKY_DEBUG");

  if (flags_string)
    wocky_debug_set_flags (g_parse_debug_string (flags_string, keys, nkeys));

  initialized = TRUE;
}

void wocky_debug_set_flags (WockyDebugFlags new_flags)
{
  flags |= new_flags;
  initialized = TRUE;
}

gboolean
wocky_debug_flag_is_set (WockyDebugFlags flag)
{
  return flag & flags;
}

void wocky_debug (WockyDebugFlags flag,
                   const gchar *format,
                   ...)
{
  va_list args;
  va_start (args, format);
  wocky_debug_valist (flag, format, args);
  va_end (args);
}

void wocky_debug_valist (WockyDebugFlags flag,
    const gchar *format,
    va_list args)
{
  if (G_UNLIKELY(!initialized))
    wocky_debug_set_flags_from_env ();

  if (flag & flags)
    g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
}

static void
wocky_debug_node_va (WockyDebugFlags flag,
    WockyNode *node,
    const gchar *format,
    va_list args)
{
  if (G_UNLIKELY(!initialized))
    wocky_debug_set_flags_from_env ();
  if (flag & flags)
    {
      gchar *msg, *node_str;

      msg = g_strdup_vprintf (format, args);

      node_str = wocky_node_to_string (node);

      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s\n%s", msg, node_str);

      g_free (msg);
      g_free (node_str);
  }
}

void
wocky_debug_node (WockyDebugFlags flag,
    WockyNode *node,
    const gchar *format,
    ...)
{
  va_list args;

  va_start (args, format);
  wocky_debug_node_va (flag, node, format, args);
  va_end (args);
}

static void
wocky_debug_node_tree_va (WockyDebugFlags flag,
    WockyNodeTree *tree,
    const gchar *format,
    va_list args)
{
  wocky_debug_node_va (flag, wocky_node_tree_get_top_node (tree), format, args);
}

void
wocky_debug_node_tree (WockyDebugFlags flag,
    WockyNodeTree *tree,
    const gchar *format,
    ...)
{
  va_list args;

  va_start (args, format);
  wocky_debug_node_tree_va (flag, tree, format, args);
  va_end (args);
}

void
wocky_debug_stanza (WockyDebugFlags flag,
    WockyStanza *stanza,
    const gchar *format,
    ...)
{
  va_list args;

  va_start (args, format);
  wocky_debug_node_tree_va (flag, (WockyNodeTree *) stanza, format, args);
  va_end (args);
}
#else /* !ENABLE_DEBUG */

void
wocky_debug_set_flags (WockyDebugFlags flags)
{
}

#endif
