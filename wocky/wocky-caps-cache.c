/* * wocky-caps-cache.c - Source for WockyCapsCache
 * Copyright (C) 2010 Collabora Ltd.
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

#include "wocky-caps-cache.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sqlite3.h>

#include "wocky-xmpp-reader.h"
#include "wocky-xmpp-writer.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_PRESENCE
#include "wocky-debug-internal.h"

#define DB_USER_VERSION 2

G_DEFINE_TYPE (WockyCapsCache, wocky_caps_cache, G_TYPE_OBJECT)

static WockyCapsCache *shared_cache = NULL;

struct _WockyCapsCachePrivate
{
  gchar *path;
  sqlite3 *db;
  guint inserts;

  WockyXmppReader *reader;
  WockyXmppWriter *writer;
};

enum
{
  PROP_PATH = 1,
};

static void wocky_caps_cache_constructed (GObject *object);
static gboolean caps_cache_get_one_uint (WockyCapsCache *self,
    const gchar *sql, guint *value);

static void
wocky_caps_cache_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  WockyCapsCache *self = (WockyCapsCache *) object;

  switch (property_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->priv->path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_caps_cache_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  WockyCapsCache *self = (WockyCapsCache *) object;

  switch (property_id)
    {
    case PROP_PATH:
      g_free (self->priv->path);
      self->priv->path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
wocky_caps_cache_dispose (GObject *object)
{
  G_OBJECT_CLASS (wocky_caps_cache_parent_class)->dispose (object);
}

static void
wocky_caps_cache_finalize (GObject *object)
{
  WockyCapsCache *self = WOCKY_CAPS_CACHE (object);

  g_free (self->priv->path);
  self->priv->path = NULL;

  if (self->priv->db != NULL)
    {
      sqlite3_close (self->priv->db);
      self->priv->db = NULL;
    }

  if (self->priv->reader != NULL)
    {
      g_object_unref (self->priv->reader);
      self->priv->reader = NULL;
    }

  if (self->priv->writer != NULL)
    {
      g_object_unref (self->priv->writer);
      self->priv->writer = NULL;
    }

  G_OBJECT_CLASS (wocky_caps_cache_parent_class)->finalize (object);
}

static void
wocky_caps_cache_class_init (WockyCapsCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WockyCapsCachePrivate));

  object_class->constructed = wocky_caps_cache_constructed;
  object_class->get_property = wocky_caps_cache_get_property;
  object_class->set_property = wocky_caps_cache_set_property;
  object_class->dispose = wocky_caps_cache_dispose;
  object_class->finalize = wocky_caps_cache_finalize;

  /**
   * WockyCapsCache:path:
   *
   * The path on disk to the SQLite database where this
   * #WockyCapsCache stores its information.
   */
  g_object_class_install_property (object_class, PROP_PATH,
      g_param_spec_string ("path", "Path", "The path to the cache", NULL,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));
}

static gchar *
get_path (void)
{
  gchar *free_dir = NULL, *ret;
  const gchar *dir, *path;

  /* This should still work if it's the magic value ":memory:". */
  path = g_getenv ("WOCKY_CAPS_CACHE");

  if (path != NULL)
    {
      dir = free_dir = g_path_get_dirname (path);
      ret = g_strdup (path);
    }
  else
    {
      dir = g_getenv ("WOCKY_CACHE_DIR");

      if (dir != NULL)
        {
          ret = g_build_path (G_DIR_SEPARATOR_S, dir, "caps-cache.db", NULL);
        }
      else
        {
          ret = g_build_path (G_DIR_SEPARATOR_S,
              g_get_user_cache_dir (), "wocky", "caps", "caps-cache.db",
              NULL);
          dir = free_dir = g_path_get_dirname (ret);
        }
    }

  /* Any errors are ignored here, on the basis that we'll find out the path is
   * duff when we try to open the database anyway.
   */
  g_mkdir_with_parents (dir, 0755);
  g_free (free_dir);
  return ret;
}

static gboolean
caps_cache_check_version (WockyCapsCache *self)
{
  guint version;

  if (!caps_cache_get_one_uint (self, "PRAGMA user_version;", &version))
    return FALSE;

  if (version == 0)
    {
    /* ______________________________________________________________________
      ( Unfortunately the first incarnation of the caps cache db didn't set  )
      ( user_version, so we can't tell if 0 means this is a new, empty       )
      ( database or an old one.                                              )
      (                                                                      )
      ( So... let's check if the capabilities table exists. If so, we'll     )
      ( pretend user_version was 1.                                          )
      (                                                                      )
      ( When there's nothing left to burn, you have to set yourself on fire. )
       ----------------------------------------------------------------------
                o   ^__^
                 o  (oo)\_______
                    (__)\       )\/\
                        ||----w |
                        ||     ||   */
      guint dummy;

      if (caps_cache_get_one_uint (self, "PRAGMA table_info(capabilities)",
            &dummy))
        {
          DEBUG ("capabilities table exists; this isn't a new database");
          version = 1;
        }
    }

  switch (version)
    {
      case 0:
        DEBUG ("opened new, empty database at %s", self->priv->path);
        return TRUE;

      case DB_USER_VERSION:
        DEBUG ("opened %s, user_version %u", self->priv->path, version);
        return TRUE;

      default:
        DEBUG ("%s is version %u, not our version %u; let's nuke it",
            self->priv->path, version, DB_USER_VERSION);
        return FALSE;
    }
}

static gboolean
caps_cache_open (WockyCapsCache *self)
{
  gint ret;
  gchar *error;

  g_return_val_if_fail (self->priv->db == NULL, FALSE);

  ret = sqlite3_open (self->priv->path, &self->priv->db);

  if (ret != SQLITE_OK)
    {
      DEBUG ("opening database %s failed: %s", self->priv->path,
          sqlite3_errmsg (self->priv->db));
      goto err;
    }

  if (!caps_cache_check_version (self))
    goto err;

  ret = sqlite3_exec (self->priv->db,
      "PRAGMA user_version = " G_STRINGIFY (DB_USER_VERSION) ";"
      "PRAGMA journal_mode = MEMORY;"
      "PRAGMA synchronous = OFF",
      NULL, NULL, &error);

  if (ret != SQLITE_OK)
    {
      DEBUG ("failed to set user_version, turn off fsync() and "
          "turn off on-disk journalling: %s", error);
      sqlite3_free (error);
      goto err;
    }

  ret = sqlite3_exec (self->priv->db,
      "CREATE TABLE IF NOT EXISTS capabilities (\n"
      "  node text PRIMARY KEY,\n"
      "  disco_reply text,\n"
      "  timestamp int)", NULL, NULL, &error);

  if (ret != SQLITE_OK)
    {
      DEBUG ("failed to ensure table exists: %s", error);
      sqlite3_free (error);
      goto err;
    }

  return TRUE;

 err:
  sqlite3_close (self->priv->db);
  self->priv->db = NULL;
  return FALSE;
}

static void
nuke_it_and_try_again (WockyCapsCache *self)
{
  int ret;

  g_return_if_fail (self->priv->path != NULL);
  g_return_if_fail (self->priv->db == NULL);

  ret = unlink (self->priv->path);

  if (ret != 0)
    DEBUG ("removing database failed: %s", g_strerror (errno));
  else
    caps_cache_open (self);
}

static void
close_nuke_and_reopen_database (WockyCapsCache *self)
{
  g_return_if_fail (self->priv->db != NULL);

  DEBUG ("Database seems to be corrupt; blowing it away and reinitializing");

  sqlite3_close (self->priv->db);
  self->priv->db = NULL;

  nuke_it_and_try_again (self);
}

static void
wocky_caps_cache_constructed (GObject *object)
{
  WockyCapsCache *self = WOCKY_CAPS_CACHE (object);

  if (!caps_cache_open (self))
    {
      /* Couldn't open it, or it's got a different user_version. */
      nuke_it_and_try_again (self);
    }

  if (self->priv->db == NULL)
    {
      DEBUG ("couldn't open db; giving up");
      return;
    }

  self->priv->reader = wocky_xmpp_reader_new_no_stream ();
  self->priv->writer = wocky_xmpp_writer_new_no_stream ();
}

static void
wocky_caps_cache_init (WockyCapsCache *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (
      self, WOCKY_TYPE_CAPS_CACHE, WockyCapsCachePrivate);
}

/**
 * wocky_caps_cache_new:
 * @path: full path to where the cache SQLite database should be stored
 *
 * Convenience function to create a new #WockyCapsCache.
 *
 * Returns: a new #WockyCapsCache.
 */
WockyCapsCache *
wocky_caps_cache_new (const gchar *path)
{
  return g_object_new (WOCKY_TYPE_CAPS_CACHE,
      "path", path,
      NULL);
}

/**
 * wocky_caps_cache_dup_shared:
 *
 * Returns a new or existing #WockyCapsCache object.
 *
 * The returned #WockyCapsCache is cached; the same #WockyCapsCache
 * object will be returned by this function repeatedly in the same
 * process. At the end of the process, the caller should call
 * wocky_caps_cache_free_shared() to shared the shared #WockyCapsCache
 * object.
 *
 * Returns: a new, or cached, #WockyCapsCache.
 */
WockyCapsCache *
wocky_caps_cache_dup_shared (void)
{
  if (shared_cache == NULL)
    {
      gchar *path;

      path = get_path ();
      shared_cache = wocky_caps_cache_new (path);
      g_free (path);
    }

  g_object_ref (shared_cache);
  return shared_cache;
}

/**
 * wocky_caps_cache_free_shared:
 *
 * Free the shared #WockyCapsCache instance which was created by
 * calling wocky_caps_cache_dup_shared(), or do nothing if said
 * function was not called.
 */
void
wocky_caps_cache_free_shared (void)
{
  if (shared_cache != NULL)
    {
      g_object_unref (shared_cache);
      shared_cache = NULL;
    }
}

static gboolean
caps_cache_prepare (WockyCapsCache *self,
    const gchar *sql,
    sqlite3_stmt **stmt)
{
  gint ret;

  g_return_val_if_fail (self->priv->db != NULL, FALSE);

  ret = sqlite3_prepare_v2 (self->priv->db, sql, -1, stmt, NULL);

  if (ret != SQLITE_OK)
    {
      g_warning ("preparing statement '%s' failed: %s", sql,
          sqlite3_errmsg (self->priv->db));
      return FALSE;
    }

  g_assert (stmt != NULL);
  return TRUE;
}

/* Finalizes @stmt if an error happens. */
static gboolean
caps_cache_bind_int (WockyCapsCache *self,
    sqlite3_stmt *stmt,
    gint param,
    gint value)
{
  gint ret = sqlite3_bind_int (stmt, param, value);

  if (ret != SQLITE_OK)
    {
      g_warning ("parameter binding failed: %s",
          sqlite3_errmsg (self->priv->db));
      sqlite3_finalize (stmt);
      return FALSE;
    }

  return TRUE;
}

/* Finalizes @stmt if an error happens.
 *
 * Note: the parameter is bound statically, so it mustn't be freed before the
 * statment is finalized.
 */
static gboolean
caps_cache_bind_text (WockyCapsCache *self,
    sqlite3_stmt *stmt,
    gint param,
    gint len,
    const gchar *value)
{
  gint ret = sqlite3_bind_text (stmt, param, value, len, SQLITE_STATIC);

  if (ret != SQLITE_OK)
    {
      g_warning ("parameter binding failed: %s",
          sqlite3_errmsg (self->priv->db));
      sqlite3_finalize (stmt);
      return FALSE;
    }

  return TRUE;
}

/*
 * caps_cache_get_one_uint:
 * @self: the caps cache
 * @sql: a query expected to yield one row with one integer colum
 * @value: location at which to store that single unsigned integer
 *
 * Returns: %TRUE if @value was successfully retrieved; %FALSE otherwise.
 */
static gboolean
caps_cache_get_one_uint (WockyCapsCache *self,
    const gchar *sql,
    guint *value)
{
  sqlite3_stmt *stmt;
  int ret;

  if (!caps_cache_prepare (self, sql, &stmt))
    return FALSE;

  ret = sqlite3_step (stmt);

  switch (ret)
    {
      case SQLITE_ROW:
        *value = sqlite3_column_int (stmt, 0);
        sqlite3_finalize (stmt);
        return TRUE;

      case SQLITE_DONE:
        DEBUG ("'%s' returned no results", sql);
        break;

      default:
        DEBUG ("executing '%s' failed: %s", sql,
            sqlite3_errmsg (self->priv->db));
    }

  sqlite3_finalize (stmt);
  return FALSE;
}

/* Update cache entry timestmp. */
static void
caps_cache_touch (WockyCapsCache *self,
    const gchar *node)
{
  gint ret = SQLITE_OK;
  sqlite3_stmt *stmt;

  if (!caps_cache_prepare (self,
        "UPDATE capabilities SET timestamp=? WHERE node=?", &stmt))
    return;

  if (!caps_cache_bind_int (self, stmt, 1, time (NULL)))
    return;

  if (!caps_cache_bind_text (self, stmt, 2, -1, node))
    return;

  ret = sqlite3_step (stmt);

  if (ret != SQLITE_DONE)
    {
      DEBUG ("statement execution failed: %s",
          sqlite3_errmsg (self->priv->db));
    }

  sqlite3_finalize (stmt);

  if (ret == SQLITE_CORRUPT)
    close_nuke_and_reopen_database (self);
}

/**
 * wocky_caps_cache_lookup:
 * @self: a #WockyCapsCache
 * @node: the node to look up in the cache
 *
 * Look up @node in the caps cache @self. The caller is responsible
 * for unreffing the returned #WockyNodeTree.
 *
 * Returns: a #WockyNodeTree if @node was found in the cache, or %NULL
 * if a match was not found
 */
WockyNodeTree *
wocky_caps_cache_lookup (WockyCapsCache *self,
    const gchar *node)
{
  gint ret;
  sqlite3_stmt *stmt;
  const guchar *value;
  int bytes;
  WockyNodeTree *query_node;

  if (!self->priv->db)
    /* DB open failed. */
    return NULL;

  if (!caps_cache_prepare (self,
        "SELECT disco_reply FROM capabilities WHERE node=?", &stmt))
    return NULL;

  if (!caps_cache_bind_text (self, stmt, 1, -1, node))
    return NULL;

  ret = sqlite3_step (stmt);

  if (ret == SQLITE_DONE)
    {
      /* No result. */
      DEBUG ("caps cache miss: %s", node);
      sqlite3_finalize (stmt);
      return NULL;
    }

  if (ret != SQLITE_ROW)
    {
      DEBUG ("statement execution failed: %s",
          sqlite3_errmsg (self->priv->db));
      sqlite3_finalize (stmt);
      return NULL;
    }

  DEBUG ("caps cache hit: %s", node);
  value = sqlite3_column_text (stmt, 0);
  bytes = sqlite3_column_bytes (stmt, 0);
  wocky_xmpp_reader_push (self->priv->reader, value, bytes);
  query_node = (WockyNodeTree *)
      wocky_xmpp_reader_pop_stanza (self->priv->reader);

  sqlite3_finalize (stmt);

  if (query_node == NULL)
    {
      GError *error = wocky_xmpp_reader_get_error (self->priv->reader);

      g_warning ("could not parse query_node of %s: %s", node,
          (error != NULL ? error->message : "no error; incomplete xml?"));

      if (error != NULL)
        g_error_free (error);

      /* Destroy the town in order to save it. */
      close_nuke_and_reopen_database (self);
    }
  else
    {
      caps_cache_touch (self, node);
    }

  wocky_xmpp_reader_reset (self->priv->reader);

  return query_node;
}

static void
caps_cache_insert (WockyCapsCache *self,
    const gchar *node,
    WockyNodeTree *query_node)
{
  const guint8 *val;
  gsize len;
  gint ret = SQLITE_OK;
  sqlite3_stmt *stmt;

  if (!caps_cache_prepare (self,
        "INSERT INTO capabilities (node, disco_reply, timestamp) "
        "VALUES (?, ?, ?)", &stmt))
    return;

  if (!caps_cache_bind_text (self, stmt, 1, -1, node))
    return;

  wocky_xmpp_writer_write_node_tree (self->priv->writer, query_node,
      &val, &len);

  if (!caps_cache_bind_text (self, stmt, 2, len, (const gchar *) val))
    return;

  if (!caps_cache_bind_int (self, stmt, 3, time (NULL)))
    return;

  ret = sqlite3_step (stmt);

  if (ret == SQLITE_CONSTRAINT)
    /* Presumably the error is because the key already exists. Ignore it. */
    goto OUT;

  if (ret != SQLITE_DONE)
    {
      DEBUG ("statement execution failed: %s",
          sqlite3_errmsg (self->priv->db));
    }

OUT:
  sqlite3_finalize (stmt);

  if (ret == SQLITE_CORRUPT)
    close_nuke_and_reopen_database (self);
}

static gboolean
caps_cache_count_entries (WockyCapsCache *self,
    guint *count)
{
  if (!self->priv->db)
    return FALSE;

  return caps_cache_get_one_uint (self, "SELECT COUNT(*) FROM capabilities",
      count);
}

/* If the number of entries is above @high_threshold, remove entries older
 * than @max_age while the cache is bigger than @low_threshold.
 */
static void
caps_cache_gc (WockyCapsCache *self,
    guint high_threshold,
    guint low_threshold)
{
  gint ret;
  guint count;
  sqlite3_stmt *stmt;

  if (!caps_cache_count_entries (self, &count))
    return;

  if (count <= high_threshold)
    return;

  /* This emulates DELETE ... ORDER ... LIMIT because some Sqlites (e.g.
   * Debian) ship without SQLITE_ENABLE_UPDATE_DELETE_LIMIT unabled.
   */

  if (!caps_cache_prepare (self,
        "DELETE FROM capabilities WHERE oid IN ("
        "  SELECT oid FROM capabilities"
        "    ORDER BY timestamp ASC, oid ASC"
        "    LIMIT ?)", &stmt))
    return;

  if (!caps_cache_bind_int (self, stmt, 1, count - low_threshold))
    return;

  ret = sqlite3_step (stmt);

  if (ret == SQLITE_DONE)
    DEBUG ("cache reduced from %d to %d items",
        count, count - sqlite3_changes (self->priv->db));
  else
      DEBUG ("statement execution failed: %s", sqlite3_errmsg (self->priv->db));

  sqlite3_finalize (stmt);

  if (ret == SQLITE_CORRUPT)
    close_nuke_and_reopen_database (self);
}

static guint
get_size (void)
{
  static gboolean ready = FALSE;
  static guint size = 1000;

  if (G_UNLIKELY (!ready))
    {
      const gchar *str = g_getenv ("WOCKY_CAPS_CACHE_SIZE");

      if (str != NULL)
        {
          /* Ignoring return code; size will retain default value on failure.
           */
          sscanf (str, "%u", &size);
        }

      ready = TRUE;
      /* DEBUG ("caps cache size = %d", size); */
    }

  return size;
}

/**
 * wocky_caps_cache_insert:
 * @self: a #WockyCapsCache
 * @node: the capability node
 * @query_node: the query #WockyNodeTree associated with @node
 *
 * Adds a new item to the caps cache. @node is associated with
 * @query_node so that subsequent calls to wocky_caps_cache_lookup()
 * with the same @node value will return @query_node.
 */
void
wocky_caps_cache_insert (WockyCapsCache *self,
    const gchar *node,
    WockyNodeTree *query_node)
{
  guint size = get_size ();

  if (!self->priv->db)
    /* DB open failed. */
    return;

  DEBUG ("caps cache insert: %s", node);
  caps_cache_insert (self, node, query_node);

  /* Remove old entries after every 50th insert. */

  if (self->priv->inserts % 50 == 0)
    caps_cache_gc (self, size, MAX (1, 0.95 * size));

  self->priv->inserts++;
}

