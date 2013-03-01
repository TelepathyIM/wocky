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
#include <stdlib.h>

#include "wocky-disco-identity.h"
#include "wocky-utils.h"
#include "wocky-data-form.h"
#include "wocky-namespaces.h"

#define WOCKY_DEBUG_FLAG WOCKY_DEBUG_PRESENCE
#include "wocky-debug-internal.h"

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
    return 0;
  else if (left_type == NULL && right_type != NULL)
    return -1;
  else if (left_type != NULL && right_type == NULL)
    return 1;
  else /* left_type != NULL && right_type != NULL */
    {
      const gchar *left_value = NULL, *right_value = NULL;

      if (left_type->raw_value_contents != NULL)
        left_value = left_type->raw_value_contents[0];

      if (right_type->raw_value_contents != NULL)
        right_value = right_type->raw_value_contents[0];

      return g_strcmp0 (left_value, right_value);
    }
}

static GPtrArray *
ptr_array_copy (GPtrArray *old)
{
  GPtrArray *new = g_ptr_array_sized_new (old->len);
  guint i;

  for (i = 0 ; i < old->len ; i++)
    g_ptr_array_add (new, g_ptr_array_index (old, i));

  return new;
}

/* see qsort(3) */
static int
cmpstringp (const void *p1,
    const void *p2)
{
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference */

  return strcmp (* (char * const *) p1, * (char * const *) p2);
}

/**
 * wocky_caps_hash_compute_from_lists:
 * @features: a #GPtrArray of strings of features
 * @identities: a #GPtrArray of #WockyDiscoIdentity structures
 * @dataforms: a #GPtrArray of #WockyDataForm objects, or %NULL
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
  gchar *encoded = NULL;
  gsize sha1_buffer_size;
  GHashTable *form_names;

  GPtrArray *features_sorted, *identities_sorted, *dataforms_sorted;

  g_return_val_if_fail (features != NULL, NULL);
  g_return_val_if_fail (identities != NULL, NULL);

  /* not a deep copy, we only need to sort */
  features_sorted = ptr_array_copy (features);
  identities_sorted = ptr_array_copy (identities);

  if (dataforms != NULL)
    dataforms_sorted = ptr_array_copy (dataforms);
  else
    dataforms_sorted = g_ptr_array_new ();

  g_ptr_array_sort (identities_sorted, identity_cmp);
  g_ptr_array_sort (features_sorted, char_cmp);
  g_ptr_array_sort (dataforms_sorted, dataforms_cmp);

  checksum = g_checksum_new (G_CHECKSUM_SHA1);

  form_names = g_hash_table_new (g_str_hash, g_str_equal);

  /* okay go and actually create this caps hash */
  for (i = 0 ; i < identities_sorted->len ; i++)
    {
      const WockyDiscoIdentity *identity = g_ptr_array_index (identities_sorted, i);
      gchar *str = g_strdup_printf ("%s/%s/%s/%s",
          identity->category, identity->type,
          identity->lang ? identity->lang : "",
          identity->name ? identity->name : "");
      g_checksum_update (checksum, (guchar *) str, -1);
      g_checksum_update (checksum, (guchar *) "<", 1);
      g_free (str);
    }

  for (i = 0 ; i < features_sorted->len ; i++)
    {
      g_checksum_update (checksum, (guchar *) g_ptr_array_index (features_sorted, i), -1);
      g_checksum_update (checksum, (guchar *) "<", 1);
    }

  for (i = 0; i < dataforms_sorted->len; i++)
    {
      WockyDataForm *dataform = g_ptr_array_index (dataforms_sorted, i);
      WockyDataFormField *field;
      GSList *fields, *l;
      const gchar *form_name;

      field = g_hash_table_lookup (dataform->fields, "FORM_TYPE");

      if (field == NULL)
        {
          DEBUG ("Data form is missing FORM_TYPE field; ignoring form and "
              "moving onto next one");
          continue;
        }

      if (field->type != WOCKY_DATA_FORM_FIELD_TYPE_HIDDEN)
        {
          DEBUG ("FORM_TYPE field is not hidden; "
              "ignoring form and moving onto next one");
          continue;
        }

      if (field->raw_value_contents == NULL ||
          g_strv_length (field->raw_value_contents) != 1)
        {
          DEBUG ("FORM_TYPE field does not have exactly one value; failing");
          goto cleanup;
        }

      form_name = field->raw_value_contents[0];

      if (g_hash_table_lookup (form_names, form_name) != NULL)
        {
          DEBUG ("error: there are multiple data forms with the "
              "same form type: %s", form_name);
          goto cleanup;
        }

      g_hash_table_insert (form_names,
          (gpointer) form_name, (gpointer) form_name);

      g_checksum_update (checksum, (guchar *) form_name, -1);
      g_checksum_update (checksum, (guchar *) "<", 1);

      /* we need to make a shallow copy to sort the fields */
      fields = g_slist_copy (dataform->fields_list);
      fields = g_slist_sort (fields, (GCompareFunc) wocky_data_form_field_cmp);

      for (l = fields; l != NULL; l = l->next)
        {
          GStrv values = NULL;
          GStrv tmp;

          field = l->data;

          if (field->var == NULL)
            {
              DEBUG ("can't hash form '%s': it has an anonymous field",
                  form_name);
              g_slist_free (fields);
              goto cleanup;
            }

          if (!wocky_strdiff (field->var, "FORM_TYPE"))
            continue;

          g_checksum_update (checksum, (guchar *) field->var, -1);
          g_checksum_update (checksum, (guchar *) "<", 1);

          if (field->raw_value_contents == NULL
              || field->raw_value_contents[0] == NULL)
            {
              DEBUG ("could not get field %s value", field->var);
              g_slist_free (fields);
              goto cleanup;
            }

          /* make a copy so we can sort it */
          values = g_strdupv (field->raw_value_contents);

          qsort (values, g_strv_length (values),
              sizeof (gchar *), cmpstringp);

          for (tmp = values; tmp != NULL && *tmp != NULL; tmp++)
            {
              g_checksum_update (checksum, (guchar *) *tmp, -1);
              g_checksum_update (checksum, (guchar *) "<", 1);
            }

          g_strfreev (values);
        }

      g_slist_free (fields);
    }

  sha1_buffer_size = g_checksum_type_get_length (G_CHECKSUM_SHA1);
  sha1 = g_new0 (guint8, sha1_buffer_size);
  g_checksum_get_digest (checksum, sha1, &sha1_buffer_size);

  encoded = g_base64_encode (sha1, sha1_buffer_size);
  g_free (sha1);

cleanup:
  g_checksum_free (checksum);

  g_hash_table_unref (form_names);

  g_ptr_array_unref (identities_sorted);
  g_ptr_array_unref (features_sorted);
  g_ptr_array_unref (dataforms_sorted);

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
  GPtrArray *features = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_free);
  GPtrArray *identities = wocky_disco_identity_array_new ();
  GPtrArray *dataforms = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_object_unref);
  gchar *str = NULL;
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
      WockyDataForm *dataform  = wocky_data_form_new_from_node (x_node, &error);

      if (error != NULL)
        {
          DEBUG ("Failed to parse data form: %s\n", error->message);
          g_clear_error (&error);
          goto out;
        }

      g_ptr_array_add (dataforms, dataform);
   }

  str = wocky_caps_hash_compute_from_lists (features, identities, dataforms);

out:
  wocky_disco_identity_array_free (identities);
  g_ptr_array_unref (features);
  g_ptr_array_unref (dataforms);

  return str;
}
