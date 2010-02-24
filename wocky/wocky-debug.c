
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
  { "sasl",              DEBUG_SASL              },
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
  { "dataforms",         DEBUG_DATA_FORMS        },
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
  if (G_UNLIKELY(!initialized))
    wocky_debug_set_flags_from_env ();
  if (flag & flags)
    {
      va_list args;
      va_start (args, format);
      g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
      va_end (args);
    }
}

void
wocky_debug_stanza (DebugFlags flag,
    WockyXmppStanza *stanza,
    const gchar *format,
    ...)
{
  if (G_UNLIKELY(!initialized))
    wocky_debug_set_flags_from_env ();
  if (flag & flags)
    {
      va_list args;
      gchar *msg, *node_str;

      va_start (args, format);
      msg = g_strdup_vprintf (format, args);
      va_end (args);

      node_str = wocky_xmpp_node_to_string (stanza->node);

      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s\n%s", msg, node_str);

      g_free (msg);
      g_free (node_str);
  }
}

#endif
