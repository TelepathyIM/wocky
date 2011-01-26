/*
 * wocky-caps-hash.c - Computing verification string hash (XEP-0115 v1.5)
 * Copyright (C) 2008-2011 Collabora Ltd.
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

/**
 * SECTION: wocky-caps-hash
 * @title: WockyCapsHash
 * @short_description: Utilities for computing verification string hash
 *
 * Computes verification string hashes according to XEP-0115 v1.5
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-caps-hash.h"

#include <string.h>

#include "wocky-disco-identity.h"
#include "wocky-utils.h"
#include "wocky-data-form.h"
#include "wocky-namespaces.h"

#define DEBUG_FLAG DEBUG_PRESENCE
#include "wocky-debug.h"

static gint
char_cmp (gconstpointer a,
    gconstpointer b)
{
  gchar *left = *(gchar **) a;
  gchar *right = *(gchar **) b;

  return strcmp (left, right);
}

static gint
identity_cmp (gconstpointer a,
    gconstpointer b)
{
  WockyDiscoIdentity *left = *(WockyDiscoIdentity **) a;
  WockyDiscoIdentity *right = *(WockyDiscoIdentity **) b;

  return wocky_disco_identity_cmp (left, right);
}

static gint
dataforms_cmp (gconstpointer a,
    gconstpointer b)
{
  WockyDataForm *left = *(WockyDataForm **) a;
  WockyDataForm *right = *(WockyDataForm **) b;
  WockyDataFormField *left_type, *right_type;

  left_type = g_hash_table_lookup (left->fields, "FORM_TYPE");
  right_type = g_hash_table_lookup (right->fields, "FORM_TYPE");

  if (left_type == NULL && right_type == NULL)
    {
      return 0;
    }
  else if (left_type == NULL && right_type != NULL)
    {
      return -1;
    }
  else if (left_type != NULL && right_type == NULL)
    {
      return 0;
    }
  else
    {
      return strcmp (g_value_get_string (left_type->default_value),
          g_value_get_string (left_type->default_value));
    }
}

static void
wocky_presence_free_xep0115_hash (
    GPtrArray *features,
    GPtrArray *identities,
    GPtrArray *dataforms)
{
  g_ptr_array_foreach (features, (GFunc) g_free, NULL);
  wocky_disco_identity_array_free (identities);
  g_ptr_array_foreach (dataforms, (GFunc) g_object_unref, NULL);

  g_ptr_array_free (features, TRUE);
  g_ptr_array_free (dataforms, TRUE);
}

/**
 * wocky_caps_hash_compute_from_lists:
 * @features: a #GPtrArray of strings
 * @identities: a #GPtrArray of #WockyDiscoIdentity structures
 * @dataforms: a #GPtrArray of #WockyDataForm objects
 *
 * Compute the hash as defined by the XEP-0115 from a list of
 * features, identities and dataforms.
 *
 * Returns: a newly allocated string of the caps hash which should be
 *          freed using g_free()
 */
gchar *
wocky_caps_hash_compute_from_lists (
    GPtrArray *features,
    GPtrArray *identities,
    GPtrArray *dataforms)
{
  GChecksum *checksum;
  guint8 *sha1;
  guint i;
  gchar *encoded;
  gsize sha1_buffer_size;

  g_ptr_array_sort (identities, identity_cmp);
  g_ptr_array_sort (features, char_cmp);
  g_ptr_array_sort (dataforms, dataforms_cmp);

  checksum = g_checksum_new (G_CHECKSUM_SHA1);

  for (i = 0 ; i < identities->len ; i++)
    {
      const WockyDiscoIdentity *identity = g_ptr_array_index (identities, i);
      gchar *str = g_strdup_printf ("%s/%s/%s/%s",
          identity->category, identity->type,
          identity->lang ? identity->lang : "",
          identity->name ? identity->name : "");
      g_checksum_update (checksum, (guchar *) str, -1);
      g_checksum_update (checksum, (guchar *) "<", 1);
      g_free (str);
    }

  for (i = 0 ; i < features->len ; i++)
    {
      g_checksum_update (checksum, (guchar *) g_ptr_array_index (features, i), -1);
      g_checksum_update (checksum, (guchar *) "<", 1);
    }

  for (i = 0; i < dataforms->len; i++)
    {
      WockyDataForm *dataform = g_ptr_array_index (dataforms, i);
      WockyDataFormField *field;
      GSList *l;

      field = g_hash_table_lookup (dataform->fields, "FORM_TYPE");
      g_assert (field != NULL);

      g_checksum_update (checksum, (guchar *) g_value_get_string (field->default_value), -1);
      g_checksum_update (checksum, (guchar *) "<", 1);

      for (l = dataform->fields_list; l != NULL; l = l->next)
        {
          field = l->data;

          if (!wocky_strdiff (field->var, "FORM_TYPE"))
            continue;

          g_checksum_update (checksum, (guchar *) field->var, -1);
          g_checksum_update (checksum, (guchar *) "<", 1);

          if (field->type == WOCKY_DATA_FORM_FIELD_TYPE_TEXT_SINGLE)
            {
              g_checksum_update (checksum, (guchar *) g_value_get_string (field->default_value), -1);
              g_checksum_update (checksum, (guchar *) "<", 1);
            }
          else if (field->type == WOCKY_DATA_FORM_FIELD_TYPE_TEXT_MULTI)
            {
              GStrv values = g_value_get_boxed (field->default_value);
              GStrv tmp;

              for (tmp = values; tmp != NULL && *tmp != NULL; tmp++)
                {
                  g_checksum_update (checksum, (guchar *) *tmp, -1);
                  g_checksum_update (checksum, (guchar *) "<", 1);
                }
            }
        }
    }

  sha1_buffer_size = g_checksum_type_get_length (G_CHECKSUM_SHA1);
  sha1 = g_new0 (guint8, sha1_buffer_size);
  g_checksum_get_digest (checksum, sha1, &sha1_buffer_size);

  encoded = g_base64_encode (sha1, sha1_buffer_size);

  g_checksum_free (checksum);

  return encoded;
}

/**
 * wocky_caps_hash_compute_from_node:
 * @node: a #WockyNode
 *
 * Compute the hash as defined by the XEP-0115 from a received
 * #WockyNode.
 *
 * @node should be the top-level node from a disco response such as
 * the example given in XEP-0115 §5.3 "Complex Generation Example".
 *
 * Returns: the hash. The called must free the returned hash with
 *          g_free().
 */
gchar *
wocky_caps_hash_compute_from_node (WockyNode *node)
{
  GPtrArray *features = g_ptr_array_new ();
  GPtrArray *identities = wocky_disco_identity_array_new ();
  GPtrArray *dataforms = g_ptr_array_new ();
  gchar *str;
  GSList *c;
  WockyNodeIter iter;
  WockyNode *x_node = NULL;

  for (c = node->children; c != NULL; c = c->next)
    {
      WockyNode *child = c->data;

      if (g_str_equal (child->name, "identity"))
        {
          const gchar *category;
          const gchar *name;
          const gchar *type;
          const gchar *xmllang;
          WockyDiscoIdentity *identity;

          category = wocky_node_get_attribute (child, "category");
          name = wocky_node_get_attribute (child, "name");
          type = wocky_node_get_attribute (child, "type");
          xmllang = wocky_node_get_language (child);

          if (NULL == category)
            continue;
          if (NULL == name)
            name = "";
          if (NULL == type)
            type = "";
          if (NULL == xmllang)
            xmllang = "";

          identity = wocky_disco_identity_new (category, type, xmllang, name);
          g_ptr_array_add (identities, identity);
        }
      else if (g_str_equal (child->name, "feature"))
        {
          const gchar *var;
          var = wocky_node_get_attribute (child, "var");

          if (NULL == var)
            continue;

          g_ptr_array_add (features, g_strdup (var));
        }
    }

  wocky_node_iter_init (&iter, node, "x", WOCKY_XMPP_NS_DATA);
  while (wocky_node_iter_next (&iter, &x_node))
    {
      GError *error = NULL;
      WockyDataForm *dataform  = wocky_data_form_new_from_form (node, &error);

      if (error != NULL)
        {
          DEBUG ("Failed to parse data form: %s\n", error->message);
          g_clear_error (&error);
          continue;
        }

      g_ptr_array_add (dataforms, dataform);
   }

  str = wocky_caps_hash_compute_from_lists (features, identities, dataforms);

  wocky_presence_free_xep0115_hash (features, identities, dataforms);

  return str;
}
