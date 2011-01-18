
#include <stdarg.h>

#include <glib.h>

#include "wocky-debug.h"

#ifdef ENABLE_DEBUG

static DebugFlags flags = 0;
static gboolean initialized = FALSE;

static GDebugKey keys[] = {
  { "transport",         DEBUG_TRANSPORT         },
  { "net",               DEBUG_NET               },
  { "xmpp",              DEBUG_XMPP              },
  { "xmpp-reader",       DEBUG_XMPP_READER       },
  { "xmpp-writer",       DEBUG_XMPP_WRITER       },
  { "auth",              DEBUG_AUTH              },
  { "ssl",               DEBUG_SSL               },
  { "rmulticast",        DEBUG_RMULTICAST        },
  { "rmulticast-sender", DEBUG_RMULTICAST_SENDER },
  { "muc-connection",    DEBUG_MUC_CONNECTION    },
  { "bytestream",        DEBUG_BYTESTREAM        },
  { "ft",                DEBUG_FILE_TRANSFER     },
  { "porter",            DEBUG_PORTER            },
  { "connector",         DEBUG_CONNECTOR         },
  { "roster",            DEBUG_ROSTER            },
  { "tls",               DEBUG_TLS               },
  { "pubsub",            DEBUG_PUBSUB            },
  { "dataform",          DEBUG_DATA_FORM         },
  { "ping",              DEBUG_PING              },
  { "heartbeat",         DEBUG_HEARTBEAT         },
  { "presence",          DEBUG_PRESENCE          },
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

void wocky_debug_set_flags (DebugFlags new_flags)
{
  flags |= new_flags;
  initialized = TRUE;
}

gboolean
wocky_debug_flag_is_set (DebugFlags flag)
{
  return flag & flags;
}

void wocky_debug (DebugFlags flag,
                   const gchar *format,
                   ...)
{
  va_list args;
  va_start (args, format);
  wocky_debug_valist (flag, format, args);
  va_end (args);
}

void wocky_debug_valist (DebugFlags flag,
    const gchar *format,
    va_list args)
{
  if (G_UNLIKELY(!initialized))
    wocky_debug_set_flags_from_env ();

  if (flag & flags)
    g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
}

static void
wocky_debug_node_tree_va (DebugFlags flag,
    WockyNodeTree *tree,
    const gchar *format,
    va_list args)
{
  if (G_UNLIKELY(!initialized))
    wocky_debug_set_flags_from_env ();
  if (flag & flags)
    {
      gchar *msg, *node_str;

      msg = g_strdup_vprintf (format, args);

      node_str = wocky_node_to_string (
          wocky_node_tree_get_top_node (tree));

      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s\n%s", msg, node_str);

      g_free (msg);
      g_free (node_str);
  }
}

void
wocky_debug_node_tree (DebugFlags flag,
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
wocky_debug_stanza (DebugFlags flag,
    WockyStanza *stanza,
    const gchar *format,
    ...)
{
  va_list args;

  va_start (args, format);
  wocky_debug_node_tree_va (flag, (WockyNodeTree *) stanza, format, args);
  va_end (args);
}

#endif
