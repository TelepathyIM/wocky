/*
 * wocky-caps-hash.c - Computing verification string hash (XEP-0115 v1.5)
 * Copyright (C) 2008-2010 Collabora Ltd.
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

/* Computing verification string hash (XEP-0115 v1.5)
 *
 * Wocky does not do anything with dataforms (XEP-0128) included in
 * capabilities.  However, it needs to parse them in order to compute the hash
 * according to XEP-0115.
 */

/**
 * SECION: wocky-caps-hash
 * @title: WockyCapsHash
 * @short_description: Utilities for computing verification string hash
 *
 * Computes verification string hash according to XEP-0115 v1.5
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wocky-caps-hash.h"

#include <string.h>

#include "wocky-disco-identity.h"
#include "wocky-utils.h"

#define DEBUG_FLAG DEBUG_PRESENCE
#include "wocky-debug.h"

typedef struct _DataFormField DataFormField;

struct _DataFormField {
  gchar *field_name;
  /* array of strings */
  GPtrArray *values;
};

typedef struct _DataForm DataForm;

struct _DataForm {
  gchar *form_type;
  /* array of DataFormField */
  GPtrArray *fields;
};


static gint
char_cmp (gconstpointer a, gconstpointer b)
{
  gchar *left = *(gchar **) a;
  gchar *right = *(gchar **) b;

  return strcmp (left, right);
}

static gint
identity_cmp (gconstpointer a, gconstpointer b)
{
  WockyDiscoIdentity *left = *(WockyDiscoIdentity **) a;
  WockyDiscoIdentity *right = *(WockyDiscoIdentity **) b;
  gint ret;

  if ((ret = strcmp (left->category, right->category)) != 0)
    return ret;
  if ((ret = strcmp (left->type, right->type)) != 0)
    return ret;
  if ((ret = strcmp (left->lang, right->lang)) != 0)
    return ret;
  return strcmp (left->name, right->name);
}

static gint
fields_cmp (gconstpointer a, gconstpointer b)
{
  DataFormField *left = *(DataFormField **) a;
  DataFormField *right = *(DataFormField **) b;

  return strcmp (left->field_name, right->field_name);
}

static gint
dataforms_cmp (gconstpointer a, gconstpointer b)
{
  DataForm *left = *(DataForm **) a;
  DataForm *right = *(DataForm **) b;

  return strcmp (left->form_type, right->form_type);
}

static void
_free_field (gpointer data, gpointer user_data)
{
  DataFormField *field = data;

  g_free (field->field_name);
  g_ptr_array_foreach (field->values, (GFunc) g_free, NULL);
  g_ptr_array_free (field->values, TRUE);

  g_slice_free (DataFormField, field);
}

static void
_free_form (gpointer data, gpointer user_data)
{
  DataForm *form = data;

  g_free (form->form_type);

  g_ptr_array_foreach (form->fields, _free_field, NULL);
  g_ptr_array_free (form->fields, TRUE);

  g_slice_free (DataForm, form);
}

static void
wocky_presence_free_xep0115_hash (
    GPtrArray *features,
    GPtrArray *identities,
    GPtrArray *dataforms)
{
  g_ptr_array_foreach (features, (GFunc) g_free, NULL);
  wocky_disco_identity_array_free (identities);
  g_ptr_array_foreach (dataforms, _free_form, NULL);

  g_ptr_array_free (features, TRUE);
  g_ptr_array_free (dataforms, TRUE);
}

static gchar *
caps_hash_compute (
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

  for (i = 0 ; i < dataforms->len ; i++)
    {
      guint j;
      DataForm *form = g_ptr_array_index (dataforms, i);

      g_assert (form->form_type != NULL);

      g_checksum_update (checksum, (guchar *) form->form_type, -1);
      g_checksum_update (checksum, (guchar *) "<", 1);

      g_ptr_array_sort (form->fields, fields_cmp);

      for (j = 0 ; j < form->fields->len ; j++)
        {
          guint k;
          DataFormField *field = g_ptr_array_index (form->fields, j);

          g_checksum_update (checksum, (guchar *) field->field_name, -1);
          g_checksum_update (checksum, (guchar *) "<", 1);

          g_ptr_array_sort (field->values, char_cmp);

          for (k = 0 ; k < field->values->len ; k++)
            {
              g_checksum_update (checksum, (guchar *) g_ptr_array_index (field->values, k), -1);
              g_checksum_update (checksum, (guchar *) "<", 1);
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
 * parse a XEP-0128 dataform
 *
 * helper function for wocky_caps_hash_compute_from_node
 */
static DataForm *
_parse_dataform (WockyNode *node)
{
  DataForm *form;
  GSList *c;

  form = g_slice_new0 (DataForm);
  form->form_type = NULL;
  form->fields = g_ptr_array_new ();

  for (c = node->children; c != NULL; c = c->next)
    {
      WockyNode *field_node = c->data;
      const gchar *var;

      if (! g_str_equal (field_node->name, "field"))
        continue;

      var = wocky_node_get_attribute (field_node, "var");

      if (NULL == var)
        continue;

      if (g_str_equal (var, "FORM_TYPE"))
        {
          GSList *d;

          for (d = field_node->children; d != NULL; d = d->next)
            {
              WockyNode *value_node = d->data;

              if (wocky_strdiff (value_node->name, "value"))
                continue;

              /* If the stanza is correctly formed, there is only one
               * FORM_TYPE and this check is useless. Otherwise, just
               * use the first one */
              if (form->form_type == NULL)
                form->form_type = g_strdup (value_node->content);
            }
        }
      else
        {
          DataFormField *field = NULL;
          GSList *d;

          field = g_slice_new0 (DataFormField);
          field->values = g_ptr_array_new ();
          field->field_name = g_strdup (var);

          for (d = field_node->children; d != NULL; d = d->next)
            {
              WockyNode *value_node = d->data;

              if (wocky_strdiff (value_node->name, "value"))
                continue;

              g_ptr_array_add (field->values, g_strdup (value_node->content));
            }

            g_ptr_array_add (form->fields, (gpointer) field);
        }
    }

  /* this should not happen if the stanza is correctly formed. */
  if (form->form_type == NULL)
    form->form_type = g_strdup ("");

  return form;
}

/**
 * Compute the hash as defined by the XEP-0115 from a received WockyNode.
 *
 * Returns: the hash. The called must free the returned hash with g_free().
 */
gchar *
wocky_caps_hash_compute_from_node (WockyNode *node)
{
  GPtrArray *features = g_ptr_array_new ();
  GPtrArray *identities = wocky_disco_identity_array_new ();
  GPtrArray *dataforms = g_ptr_array_new ();
  gchar *str;
  GSList *c;

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
      else if (g_str_equal (child->name, "x"))
        {
          const gchar *xmlns;
          const gchar *type;

          xmlns = wocky_node_get_ns (child);
          type = wocky_node_get_attribute (child, "type");

          if (wocky_strdiff (xmlns, "jabber:x:data"))
            continue;

          if (wocky_strdiff (type, "result"))
            continue;

          g_ptr_array_add (dataforms, (gpointer) _parse_dataform (child));
        }
    }

  str = caps_hash_compute (features, identities, dataforms);

  wocky_presence_free_xep0115_hash (features, identities, dataforms);

  return str;
}
